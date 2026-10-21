#include "NativeApi.h"

#include "Log.h"
#include "MemoryUtil.h"
#include "MWSEDefs.h"
#include "BuildDate.h"

#include "TES3DataHandler.h"
#include "TES3PlayerAnimationController.h"
#include "TES3WorldController.h"

namespace mwse::native {

	//
	// CallSites
	//

	CallSites::CallSites(std::initializer_list<DWORD> sites, DWORD engineFunction) :
		m_sites(sites),
		m_engineFunction(engineFunction)
	{

	}

	bool CallSites::areUnchanged() const {
		return findChangedSite() == 0;
	}

	void CallSites::redirect(DWORD to) const {
		for (const auto site : m_sites) {
			se::memory::genCallEnforced(site, m_engineFunction, to);
		}
	}

	DWORD CallSites::findChangedSite() const {
		for (const auto site : m_sites) {
			if (se::memory::getCallAddress(site) != m_engineFunction) {
				return site;
			}
		}
		return 0;
	}

	DWORD CallSites::getEngineFunction() const {
		return m_engineFunction;
	}

	//
	// The engine places.
	//

	static const CallSites cameraTransformsSites({ 0x41C029, 0x45CDC3, 0x48F64A, 0x567A17, 0x56871C }, 0x542E60);
	static const CallSites uiConfigureSites({ 0x40E55E }, 0x40F2A0);
	static const CallSites dynamicLightTestSites({ 0x485B60, 0x4D2C9C, 0x4D2D04, 0x4D2D9F, 0x4D2F10, 0x4D3350 }, 0x4D2F40);

	// The two reads of the back buffer size in NiRenderer::MapWindowPointToBufferPoint, and the reads of the
	// UI's view size that replace them.
	constexpr DWORD PICK_RAY_SIZE_READS = 0x6F5157;
	constexpr BYTE PICK_RAY_BUFFER_SIZE[] = {
		0x8B, 0x06,             // mov eax, [esi]
		0xFF, 0x50, 0x54,       // call [eax+54h]: getBackBufferWidth
		0x8B, 0x16,             // mov edx, [esi]
		0x8B, 0xCE,             // mov ecx, esi
		0x8B, 0xF8,             // mov edi, eax
		0xFF, 0x52, 0x58,       // call [edx+58h]: getBackBufferHeight
	};
	constexpr BYTE PICK_RAY_VIEW_SIZE[] = {
		0xA1, 0xDC, 0x67, 0x7C, 0x00, // mov eax, [WorldController]
		0x8B, 0x78, 0x78,             // mov edi, [eax+78h]: viewWidth
		0x8B, 0x40, 0x7C,             // mov eax, [eax+7Ch]: viewHeight
		0x90, 0x90, 0x90,
	};
	static_assert(sizeof(PICK_RAY_BUFFER_SIZE) == sizeof(PICK_RAY_VIEW_SIZE));

	static void logChangedSite(const char* name, DWORD site) {
		log::getLog() << "[NativeApi] " << name << ": the engine code at 0x" << std::hex << site << std::dec << " is not the expected code. Another module changed it." << std::endl;
	}

	static MwseNativeStatus redirectCallSites(const char* name, const CallSites& sites, DWORD to) {
		const auto changed = sites.findChangedSite();
		if (changed) {
			logChangedSite(name, changed);
			return MWSE_NATIVE_BYTES_CHANGED;
		}
		sites.redirect(to);
		return MWSE_NATIVE_OK;
	}

	static MwseNativeStatus installCameraTransformsUpdated() {
		auto function = &TES3::PlayerAnimationController::updateCameraTransforms;
		return redirectCallSites("the camera transforms update", cameraTransformsSites, *reinterpret_cast<DWORD*>(&function));
	}

	// The configuration of the UI scale. The event and the request of the UI scale share it.
	static MwseNativeStatus installUIConfigure() {
		static bool installed = false;
		if (installed) {
			return MWSE_NATIVE_OK;
		}

		auto function = &TES3::WorldController::initializeUIScaling;
		const auto status = redirectCallSites("the UI configuration", uiConfigureSites, *reinterpret_cast<DWORD*>(&function));
		installed = status == MWSE_NATIVE_OK;
		return status;
	}

	static bool isUIMade() {
		const auto worldController = TES3::WorldController::get();
		return worldController && worldController->menuController;
	}

	static MwseNativeStatus installUIConfigured() {
		if (isUIMade()) {
			return MWSE_NATIVE_TOO_LATE;
		}
		return installUIConfigure();
	}

	static MwseNativeStatus installDynamicLightTest() {
		return redirectCallSites("the dynamic light test", dynamicLightTestSites, reinterpret_cast<DWORD>(&TES3::DataHandler::testDynamicLight));
	}

	//
	// EventPoint
	//

	EventPoint::EventPoint(uint32_t id, const char* name, Installer installer) :
		m_id(id),
		m_name(name),
		m_installer(installer)
	{

	}

	MwseNativeStatus EventPoint::subscribe(const char* module, MwseNativeListener listener, void* user, int32_t priority, MwseNativeHandle handle) {
		if (!m_installed) {
			const auto status = m_installer();
			if (status != MWSE_NATIVE_OK) {
				return status;
			}
			m_installed = true;
		}

		const auto current = m_listeners.load(std::memory_order_acquire);
		auto list = current ? std::make_unique<ListenerList>(*current) : std::make_unique<ListenerList>();
		list->push_back({ listener, user, priority, handle, module });
		std::stable_sort(list->begin(), list->end(), [](const Listener& a, const Listener& b) {
			return a.priority > b.priority;
		});
		publish(std::move(list));

		log::getLog() << "[NativeApi] " << module << " listens to " << m_name << "." << std::endl;
		return MWSE_NATIVE_OK;
	}

	bool EventPoint::unsubscribe(MwseNativeHandle handle) {
		const auto current = m_listeners.load(std::memory_order_acquire);
		if (current == nullptr) {
			return false;
		}

		auto list = std::make_unique<ListenerList>(*current);
		const auto removed = std::erase_if(*list, [handle](const Listener& listener) {
			return listener.handle == handle;
		});
		if (removed == 0) {
			return false;
		}
		publish(std::move(list));
		return true;
	}

	void EventPoint::fire(const void* packet) const {
		const auto list = m_listeners.load(std::memory_order_acquire);
		if (list == nullptr) {
			return;
		}
		for (const auto& listener : *list) {
			listener.function(listener.user, packet);
		}
	}

	uint32_t EventPoint::getID() const {
		return m_id;
	}

	const char* EventPoint::getName() const {
		return m_name;
	}

	void EventPoint::publish(std::unique_ptr<ListenerList> list) {
		m_lists.push_back(std::move(list));
		m_listeners.store(m_lists.back().get(), std::memory_order_release);
	}

	//
	// OverrideSlot
	//

	OverrideSlot::OverrideSlot(uint32_t id, const char* name, void* engineFunction, Installer installer) :
		m_id(id),
		m_name(name),
		m_engineFunction(engineFunction),
		m_installer(installer)
	{

	}

	MwseNativeStatus OverrideSlot::claim(const char* module, void* function, void** engineFunction, MwseNativeHandle handle) {
		if (m_owner.load(std::memory_order_acquire)) {
			log::getLog() << "[NativeApi] " << module << " cannot own " << m_name << ": " << m_module << " owns it." << std::endl;
			return MWSE_NATIVE_TAKEN;
		}

		if (!m_installed) {
			const auto status = m_installer();
			if (status != MWSE_NATIVE_OK) {
				return status;
			}
			m_installed = true;
		}

		if (engineFunction) {
			*engineFunction = m_engineFunction;
		}
		m_module = module;
		m_handle = handle;
		m_owner.store(function, std::memory_order_release);

		log::getLog() << "[NativeApi] " << module << " owns " << m_name << "." << std::endl;
		return MWSE_NATIVE_OK;
	}

	bool OverrideSlot::release(MwseNativeHandle handle) {
		if (handle == 0 || handle != m_handle) {
			return false;
		}
		m_owner.store(nullptr, std::memory_order_release);
		m_handle = 0;
		log::getLog() << "[NativeApi] " << m_module << " gave " << m_name << " back to the engine." << std::endl;
		m_module.clear();
		return true;
	}

	uint32_t OverrideSlot::getID() const {
		return m_id;
	}

	//
	// UIScale
	//

	MwseNativeStatus UIScale::request(const char* module, float scale) {
		if (!(scale > 0.0f)) {
			return MWSE_NATIVE_BAD_ARGUMENT;
		}

		if (!m_configureInstalled && !isUIMade()) {
			const auto status = installUIConfigure();
			if (status != MWSE_NATIVE_OK) {
				return status;
			}
			m_configureInstalled = true;
		}

		const auto status = installPickRay();
		if (status != MWSE_NATIVE_OK) {
			return status;
		}

		m_scale = scale;
		m_requested = true;
		log::getLog() << "[NativeApi] " << module << " requests the UI scale " << scale << "." << std::endl;

		if (isUIMade()) {
			TES3::WorldController::get()->applyUIScale(scale);
		}
		return MWSE_NATIVE_OK;
	}

	void UIScale::query(MwseNativeUIScaleV1* packet) const {
		packet->scale = m_scale;
		packet->viewWidth = 0;
		packet->viewHeight = 0;
		if (isUIMade()) {
			const auto worldController = TES3::WorldController::get();
			packet->viewWidth = worldController->viewWidth;
			packet->viewHeight = worldController->viewHeight;
		}
		packet->pickRayInViewSize = m_pickRayInViewSize ? 1 : 0;
	}

	bool UIScale::isRequested() const {
		return m_requested;
	}

	float UIScale::getScale() const {
		return m_scale;
	}

	MwseNativeStatus UIScale::installPickRay() {
		if (m_pickRayInViewSize) {
			return MWSE_NATIVE_OK;
		}

		const auto code = reinterpret_cast<const BYTE*>(PICK_RAY_SIZE_READS);
		if (memcmp(code, PICK_RAY_VIEW_SIZE, sizeof(PICK_RAY_VIEW_SIZE)) != 0) {
			if (memcmp(code, PICK_RAY_BUFFER_SIZE, sizeof(PICK_RAY_BUFFER_SIZE)) != 0) {
				logChangedSite("the pick ray", PICK_RAY_SIZE_READS);
				return MWSE_NATIVE_BYTES_CHANGED;
			}
			se::memory::writeBytesUnprotected(PICK_RAY_SIZE_READS, PICK_RAY_VIEW_SIZE, sizeof(PICK_RAY_VIEW_SIZE));
		}
		m_pickRayInViewSize = true;
		return MWSE_NATIVE_OK;
	}

	//
	// Registry
	//

	Registry::Registry() :
		cameraTransformsUpdated(MWSE_NATIVE_EVENT_CAMERA_TRANSFORMS_UPDATED, "the camera transforms update", installCameraTransformsUpdated),
		uiConfigured(MWSE_NATIVE_EVENT_UI_CONFIGURED, "the UI configuration", installUIConfigured),
		dynamicLightTest(MWSE_NATIVE_OVERRIDE_DYNAMIC_LIGHT_TEST, "the dynamic light test", reinterpret_cast<void*>(dynamicLightTestSites.getEngineFunction()), installDynamicLightTest)
	{

	}

	Registry& Registry::get() {
		static Registry instance;
		return instance;
	}

	const MwseNativeApiV1* Registry::getTable(uint32_t version) {
		if (version > MWSE_NATIVE_API_VERSION) {
			return nullptr;
		}

		static const std::string mwseVersion = std::to_string(MWSE_VERSION_MAJOR) + "." + std::to_string(MWSE_VERSION_MINOR) + "." + std::to_string(MWSE_VERSION_PATCH) + " (" + std::to_string(MWSE_BUILD_DATE) + ")";
		static const MwseNativeApiV1 table = {
			sizeof(MwseNativeApiV1),
			MWSE_NATIVE_API_VERSION,
			mwseVersion.c_str(),
			&Registry::subscribe,
			&Registry::unsubscribe,
			&Registry::claim,
			&Registry::release,
			&Registry::request,
			&Registry::query,
		};
		return &table;
	}

	void Registry::setMainThread(DWORD threadID) {
		m_mainThread = threadID;
	}

	bool Registry::isMainThread() const {
		return m_mainThread == 0 || m_mainThread == GetCurrentThreadId();
	}

	MwseNativeStatus Registry::subscribe(const char* module, uint32_t eventId, MwseNativeListener listener, void* user, int32_t priority, MwseNativeHandle* handle) {
		auto& registry = get();
		if (module == nullptr || listener == nullptr || handle == nullptr) {
			return MWSE_NATIVE_BAD_ARGUMENT;
		}
		*handle = 0;
		if (!registry.isMainThread()) {
			return MWSE_NATIVE_WRONG_THREAD;
		}

		const auto event = registry.findEvent(eventId);
		if (event == nullptr) {
			log::getLog() << "[NativeApi] " << module << " asked for event " << eventId << ": this MWSE does not have it." << std::endl;
			return MWSE_NATIVE_UNKNOWN_ID;
		}

		const auto newHandle = registry.nextHandle();
		const auto status = event->subscribe(module, listener, user, priority, newHandle);
		if (status == MWSE_NATIVE_OK) {
			registry.m_eventHandles[newHandle] = event;
			*handle = newHandle;
		}
		return status;
	}

	void Registry::unsubscribe(MwseNativeHandle handle) {
		auto& registry = get();
		if (!registry.isMainThread()) {
			return;
		}

		const auto found = registry.m_eventHandles.find(handle);
		if (found == registry.m_eventHandles.end()) {
			return;
		}
		found->second->unsubscribe(handle);
		registry.m_eventHandles.erase(found);
	}

	MwseNativeStatus Registry::claim(const char* module, uint32_t overrideId, void* function, void** engineFunction, MwseNativeHandle* handle) {
		auto& registry = get();
		if (module == nullptr || function == nullptr || handle == nullptr) {
			return MWSE_NATIVE_BAD_ARGUMENT;
		}
		*handle = 0;
		if (!registry.isMainThread()) {
			return MWSE_NATIVE_WRONG_THREAD;
		}

		const auto slot = registry.findOverride(overrideId);
		if (slot == nullptr) {
			log::getLog() << "[NativeApi] " << module << " asked for override " << overrideId << ": this MWSE does not have it." << std::endl;
			return MWSE_NATIVE_UNKNOWN_ID;
		}

		const auto newHandle = registry.nextHandle();
		const auto status = slot->claim(module, function, engineFunction, newHandle);
		if (status == MWSE_NATIVE_OK) {
			registry.m_overrideHandles[newHandle] = slot;
			*handle = newHandle;
		}
		return status;
	}

	void Registry::release(MwseNativeHandle handle) {
		auto& registry = get();
		if (!registry.isMainThread()) {
			return;
		}

		const auto found = registry.m_overrideHandles.find(handle);
		if (found == registry.m_overrideHandles.end()) {
			return;
		}
		found->second->release(handle);
		registry.m_overrideHandles.erase(found);
	}

	MwseNativeStatus Registry::request(const char* module, uint32_t requestId, const void* packet) {
		auto& registry = get();
		if (module == nullptr || packet == nullptr) {
			return MWSE_NATIVE_BAD_ARGUMENT;
		}
		if (!registry.isMainThread()) {
			return MWSE_NATIVE_WRONG_THREAD;
		}

		switch (requestId) {
		case MWSE_NATIVE_REQUEST_UI_SCALE:
		{
			const auto uiScale = static_cast<const MwseNativeUIScaleRequestV1*>(packet);
			if (uiScale->structSize < sizeof(MwseNativeUIScaleRequestV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			return registry.uiScale.request(module, uiScale->scale);
		}
		}

		log::getLog() << "[NativeApi] " << module << " made request " << requestId << ": this MWSE does not have it." << std::endl;
		return MWSE_NATIVE_UNKNOWN_ID;
	}

	MwseNativeStatus Registry::query(uint32_t queryId, void* packet) {
		auto& registry = get();
		if (packet == nullptr) {
			return MWSE_NATIVE_BAD_ARGUMENT;
		}

		switch (queryId) {
		case MWSE_NATIVE_QUERY_UI_SCALE:
		{
			const auto uiScale = static_cast<MwseNativeUIScaleV1*>(packet);
			if (uiScale->structSize < sizeof(MwseNativeUIScaleV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			registry.uiScale.query(uiScale);
			return MWSE_NATIVE_OK;
		}
		}

		return MWSE_NATIVE_UNKNOWN_ID;
	}

	EventPoint* Registry::findEvent(uint32_t id) {
		for (auto event : { &cameraTransformsUpdated, &uiConfigured }) {
			if (event->getID() == id) {
				return event;
			}
		}
		return nullptr;
	}

	OverrideSlot* Registry::findOverride(uint32_t id) {
		if (dynamicLightTest.getID() == id) {
			return &dynamicLightTest;
		}
		return nullptr;
	}

	MwseNativeHandle Registry::nextHandle() {
		return ++m_lastHandle;
	}
}
