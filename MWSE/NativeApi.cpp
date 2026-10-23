#include "NativeApi.h"

#include "Log.h"
#include "MemoryUtil.h"
#include "MWSEDefs.h"
#include "BuildDate.h"

#include "NIAccumulator.h"

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
	static const CallSites cameraRootCullShowSites({ 0x6CC874 }, 0x6EB480);
	static const CallSites mapTileReadSites({ 0x42E4FD }, 0x42F6F0);
	static const CallSites texturePurgeSites({ 0x41A8F7, 0x48535B, 0x486F2B, 0x489745, 0x489CC1, 0x4B52B0, 0x4C4B87, 0x5048A4 }, 0x485490);
	static const CallSites mapCellPaintSites({ 0x4E32FE }, 0x4C81C0);
	static const CallSites savePictureSites({ 0x4B586D }, 0x4B4F80);
	static const CallSites renderTargetLockSites({ 0x42DFBC, 0x42F70B, 0x42FEA7 }, 0x42F3F0);
	static const CallSites renderTargetUnlockSites({ 0x42E180, 0x42F756, 0x42FEF9 }, 0x42F620);
	static const CallSites fogRenderSites({ 0x41F52A }, 0x42FC60);
	static const CallSites fogBlobSites({ 0x42082C, 0x4208C2, 0x420A7F, 0x420BB9, 0x420CC8, 0x421CE4, 0x42FD7C }, 0x42FA50);
	static const CallSites gameIdsResolvedSites({ 0x419AC4, 0x4C601D, 0x5FB11A, 0x5FE929 }, 0x40FC40);
	static const CallSites playerCellChangeSites({ 0x45C9A2, 0x45D356, 0x45D3C4, 0x4637B0, 0x4EA137, 0x50497D, 0x5062FD, 0x568360, 0x56A735, 0x56B7D2, 0x619BBF }, 0x45CEF0);
	static const CallSites playerCellGameLoadSites({ 0x4C4B4F, 0x4C51AE }, 0x45CEF0);
	static const CallSites playerCellCompanionSites({ 0x4EA0E8, 0x619B84 }, 0x45C9B0);
	static const CallSites lightPointSites({ 0x485BF0, 0x485C2A, 0x4D2C07, 0x4EB9E4 }, 0x4D2C20);
	static const CallSites lightHelperSites({ 0x485DD8, 0x485E2D }, 0x4D32F0);

	// The slot of `FinishAccumulating` in the virtual tables of NiAlphaAccumulator and
	// NiClusterAccumulator, and the engine function that both hold.
	constexpr DWORD ACCUMULATOR_VTABLES[] = { 0x7468D0, 0x74FB4C };
	constexpr DWORD ACCUMULATOR_FINISH_SLOT = 0x30;
	constexpr DWORD ACCUMULATOR_FINISH = 0x6CF0E0;

	// The operands of the engine's uses of `timeGetTime`: `mov ebx, [timeGetTime]` at 0x403B50 and
	// `call [timeGetTime]` at 0x4535FB, 0x453613 and 0x453636. Each names the import slot 0x7463E8.
	constexpr DWORD FRAME_TIMER_OPERANDS[] = { 0x403B52, 0x4535FD, 0x453615, 0x453638 };
	constexpr DWORD TIME_GET_TIME_IMPORT = 0x7463E8;
	// The operands point here once a module owns the frame timer.
	static uint32_t(__stdcall* frameTimerTarget)() = &Dispatch::frameTimer;

	// The local light limit in NiNode::PushLocalEffects: `cmp ebp, <limit>; ja`.
	constexpr DWORD LIGHT_LIMIT_COMPARE = 0x6C8FF0;
	// The read of the ripple limit in WaterController::createRipple, and the code that makes it 0.
	constexpr DWORD RIPPLES_READ = 0x51C2D4;
	constexpr DWORD RIPPLES_ON = 0x33504D8B; // mov ecx, [ebp+50h]; (xor ...)
	constexpr DWORD RIPPLES_OFF = 0x3390C931; // xor ecx, ecx; nop; (xor ...)
	// The starts of the two intro movies, and the jump over each.
	constexpr DWORD INTRO_MOVIE_SITES[] = { 0x418EF0, 0x5FC8F7 };
	constexpr BYTE INTRO_MOVIE_CODE[][2] = { { 0x8B, 0x8D }, { 0x8B, 0x88 } };
	constexpr BYTE INTRO_MOVIE_SKIP[] = { 0xEB, 0x16 };
	// The jz over the emissive colour of a light's particles in sg_stopParticlesAndSetEmissiveMaterialToBlack.
	constexpr DWORD LIGHT_PARTICLES_JUMP = 0x4D2789;
	// The jz to the visible branch in WeatherController::sunGlareTests, and the nops that replace it.
	constexpr DWORD SUN_GLARE_JUMP = 0x4404FB;
	constexpr BYTE SUN_GLARE_ON[] = { 0x74, 0x51 };
	constexpr BYTE SUN_GLARE_OFF[] = { 0x90, 0x90 };

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

	static MwseNativeStatus installGameIdsResolved() {
		auto function = &TES3::WorldController::resolveScriptInternalIDs;
		return redirectCallSites("the resolve of the game's ids", gameIdsResolvedSites, *reinterpret_cast<DWORD*>(&function));
	}

	// Both virtual tables, or none: a part would leave some flushes without the event.
	static MwseNativeStatus installAccumulatorFinished() {
		for (const auto vtable : ACCUMULATOR_VTABLES) {
			if (*reinterpret_cast<const DWORD*>(vtable + ACCUMULATOR_FINISH_SLOT) != ACCUMULATOR_FINISH) {
				logChangedSite("the draw of an accumulator's queue", vtable + ACCUMULATOR_FINISH_SLOT);
				return MWSE_NATIVE_BYTES_CHANGED;
			}
		}
		for (const auto vtable : ACCUMULATOR_VTABLES) {
			se::memory::overrideVirtualTableEnforced(vtable, ACCUMULATOR_FINISH_SLOT, ACCUMULATOR_FINISH, reinterpret_cast<DWORD>(&Dispatch::finishAccumulating));
		}
		return MWSE_NATIVE_OK;
	}

	// The three groups of calls, or none: a part would leave some changes of cell without the event.
	static MwseNativeStatus installPlayerCellChanged() {
		for (const auto sites : { &playerCellChangeSites, &playerCellGameLoadSites, &playerCellCompanionSites }) {
			const auto changed = sites->findChangedSite();
			if (changed) {
				logChangedSite("the player's change of cell", changed);
				return MWSE_NATIVE_BYTES_CHANGED;
			}
		}
		playerCellChangeSites.redirect(reinterpret_cast<DWORD>(&Dispatch::changePlayerCell));
		playerCellGameLoadSites.redirect(reinterpret_cast<DWORD>(&Dispatch::changePlayerCellAtGameLoad));
		playerCellCompanionSites.redirect(reinterpret_cast<DWORD>(&Dispatch::changePlayerCellWithCompanions));
		return MWSE_NATIVE_OK;
	}

	// Both groups of calls, or none: a part would attach lights at two radii.
	static MwseNativeStatus installLightAttachRadius() {
		for (const auto sites : { &lightPointSites, &lightHelperSites }) {
			const auto changed = sites->findChangedSite();
			if (changed) {
				logChangedSite("the light attach radius", changed);
				return MWSE_NATIVE_BYTES_CHANGED;
			}
		}
		lightPointSites.redirect(reinterpret_cast<DWORD>(&Dispatch::updateDynamicLightingForPointLight));
		lightHelperSites.redirect(reinterpret_cast<DWORD>(&Dispatch::updateLightHelper));
		return MWSE_NATIVE_OK;
	}

	static MwseNativeStatus installFrameTimer() {
		for (const auto operand : FRAME_TIMER_OPERANDS) {
			if (*reinterpret_cast<const DWORD*>(operand) != TIME_GET_TIME_IMPORT) {
				logChangedSite("the frame timer", operand);
				return MWSE_NATIVE_BYTES_CHANGED;
			}
		}
		for (const auto operand : FRAME_TIMER_OPERANDS) {
			se::memory::writeDoubleWordUnprotected(operand, reinterpret_cast<DWORD>(&frameTimerTarget));
		}
		return MWSE_NATIVE_OK;
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

	OverrideSlot::OverrideSlot(uint32_t id, const char* name, const CallSites* sites, DWORD dispatcher) :
		m_id(id),
		m_name(name),
		m_sites(sites),
		m_dispatcher(dispatcher),
		m_engineFunction(reinterpret_cast<void*>(sites->getEngineFunction()))
	{

	}

	OverrideSlot::OverrideSlot(uint32_t id, const char* name, void* engineFunction, Installer installer) :
		m_id(id),
		m_name(name),
		m_installer(installer),
		m_engineFunction(engineFunction)
	{

	}

	MwseNativeStatus OverrideSlot::claim(const char* module, void* function, void** engineFunction, MwseNativeHandle handle) {
		if (m_owner.load(std::memory_order_acquire)) {
			log::getLog() << "[NativeApi] " << module << " cannot own " << m_name << ": " << m_module << " owns it." << std::endl;
			return MWSE_NATIVE_TAKEN;
		}

		if (!m_installed) {
			const auto status = m_installer ? m_installer() : redirectCallSites(m_name, *m_sites, m_dispatcher);
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
	// Dispatch
	//

	void __fastcall Dispatch::cameraRootCullShow(void* root, DWORD, void* camera) {
		Registry::get().cameraRootCullShow.getFunction<MwseNativeCameraRootCullShow>()(root, nullptr, camera);
	}

	void __fastcall Dispatch::readMapTile(void* renderTarget, DWORD, bool skip) {
		Registry::get().mapTileRead.getFunction<MwseNativeMapTileRead>()(renderTarget, nullptr, skip);
	}

	void __fastcall Dispatch::purgeTextures(void* dataHandler, DWORD) {
		Registry::get().texturePurge.getFunction<MwseNativeTexturePurge>()(dataHandler, nullptr);
	}

	void __fastcall Dispatch::paintMapCell(void* recordsHandler, DWORD, void* cell) {
		Registry::get().mapCellPaint.getFunction<MwseNativeMapCellPaint>()(recordsHandler, nullptr, cell);
	}

	void* __cdecl Dispatch::takeSavePicture(const char* fileName) {
		return Registry::get().savePicture.getFunction<MwseNativeSavePicture>()(fileName);
	}

	D3DLOCKED_RECT* __fastcall Dispatch::lockRenderTarget(void* renderTarget, DWORD, void* texture) {
		return static_cast<D3DLOCKED_RECT*>(Registry::get().renderTargetLock.getFunction<MwseNativeRenderTargetLock>()(renderTarget, nullptr, texture));
	}

	void __fastcall Dispatch::unlockRenderTarget(void* renderTarget, DWORD, D3DLOCKED_RECT* lockedRect, int copyBack) {
		Registry::get().renderTargetUnlock.getFunction<MwseNativeRenderTargetUnlock>()(renderTarget, nullptr, lockedRect, copyBack);
	}

	void __fastcall Dispatch::renderFogOfWar(void* renderTarget, DWORD, void* texture, void* bitmap, bool smallBlobs) {
		Registry::get().fogRender.getFunction<MwseNativeFogRender>()(renderTarget, nullptr, texture, bitmap, smallBlobs);
	}

	void __fastcall Dispatch::drawFogBlob(void* renderTarget, DWORD, void* texture, int x, int y, int size) {
		Registry::get().fogBlob.getFunction<MwseNativeFogBlob>()(renderTarget, nullptr, texture, x, y, size);
	}

	void __cdecl Dispatch::updateDynamicLightingForPointLight(void* light, void* cell, int radius, int lightFlags, int isTorch) {
		const auto attachRadius = Registry::get().lightAttachRadius.getFunction<MwseNativeLightAttachRadius>();
		TES3::DataHandler::updateDynamicLightingForPointLight(static_cast<NI::PointLight*>(light), static_cast<TES3::Cell*>(cell), attachRadius(light, radius), lightFlags, isTorch);
	}

	void __cdecl Dispatch::updateLightHelper(void* light, void* reference, int radius, int lightFlags, int highPriority) {
		const auto attachRadius = Registry::get().lightAttachRadius.getFunction<MwseNativeLightAttachRadius>();
		TES3::DataHandler::updateLightHelper(static_cast<NI::PointLight*>(light), static_cast<TES3::Reference*>(reference), attachRadius(light, radius), lightFlags, highPriority);
	}

	int32_t __cdecl Dispatch::engineLightAttachRadius(const void*, int32_t radius) {
		return radius;
	}

	void __fastcall Dispatch::finishAccumulating(void* accumulator, DWORD) {
		const auto camera = static_cast<NI::Accumulator*>(accumulator)->camera.get();
		reinterpret_cast<void(__thiscall*)(void*)>(ACCUMULATOR_FINISH)(accumulator);

		MwseNativeAccumulatorFinishedV1 packet = {};
		packet.structSize = sizeof(packet);
		packet.accumulator = accumulator;
		packet.camera = camera;
		Registry::get().accumulatorFinished.fire(&packet);
	}

	void __cdecl Dispatch::changePlayerCell(NI::Point3 position, NI::Point3 orientation, void* cell, int updateCollision) {
		TES3::DataHandler::changePlayerCell(position, orientation, static_cast<TES3::Cell*>(cell), updateCollision != 0);
	}

	void __cdecl Dispatch::changePlayerCellAtGameLoad(NI::Point3 position, NI::Point3 orientation, void* cell, int updateCollision) {
		TES3::DataHandler::changePlayerCell(position, orientation, static_cast<TES3::Cell*>(cell), updateCollision != 0, true);
	}

	void __cdecl Dispatch::changePlayerCellWithCompanions(NI::Point3 position, NI::Point3 orientation, void* cell) {
		TES3::DataHandler::changePlayerCellWithCompanions(position, orientation, static_cast<TES3::Cell*>(cell));
	}

	uint32_t __stdcall Dispatch::frameTimer() {
		return Registry::get().frameTimer.getFunction<MwseNativeFrameTimer>()();
	}

	uint32_t __stdcall Dispatch::engineFrameTimer() {
		const auto timeGetTime = *reinterpret_cast<uint32_t(__stdcall**)()>(TIME_GET_TIME_IMPORT);
		return timeGetTime();
	}

	//
	// EngineSettings
	//

	MwseNativeStatus EngineSettings::requestLightLimit(const char* module, uint32_t limit) {
		if (limit < 1 || limit > 127) {
			return MWSE_NATIVE_BAD_ARGUMENT;
		}
		const auto code = reinterpret_cast<const BYTE*>(LIGHT_LIMIT_COMPARE);
		if (code[0] != 0x83 || code[1] != 0xFD || code[3] != 0x77 || code[4] != 0x0A) {
			logChangedSite("the light limit", LIGHT_LIMIT_COMPARE);
			return MWSE_NATIVE_BYTES_CHANGED;
		}
		se::memory::writeByteUnprotected(LIGHT_LIMIT_COMPARE + 2, BYTE(limit));
		log::getLog() << "[NativeApi] " << module << " requests a light limit of " << limit << " for each node." << std::endl;
		return MWSE_NATIVE_OK;
	}

	MwseNativeStatus EngineSettings::requestRipples(const char* module, bool enabled) {
		const auto current = *reinterpret_cast<const DWORD*>(RIPPLES_READ);
		if (current != RIPPLES_ON && current != RIPPLES_OFF) {
			logChangedSite("the ripples", RIPPLES_READ);
			return MWSE_NATIVE_BYTES_CHANGED;
		}
		se::memory::writeDoubleWordUnprotected(RIPPLES_READ, enabled ? RIPPLES_ON : RIPPLES_OFF);
		log::getLog() << "[NativeApi] " << module << " requests the ripples " << (enabled ? "on" : "off") << "." << std::endl;
		return MWSE_NATIVE_OK;
	}

	MwseNativeStatus EngineSettings::requestIntroMovies(const char* module, bool play) {
		for (size_t i = 0; i < std::size(INTRO_MOVIE_SITES); ++i) {
			const auto code = reinterpret_cast<const BYTE*>(INTRO_MOVIE_SITES[i]);
			if (memcmp(code, INTRO_MOVIE_CODE[i], 2) != 0 && memcmp(code, INTRO_MOVIE_SKIP, 2) != 0) {
				logChangedSite("the intro movies", INTRO_MOVIE_SITES[i]);
				return MWSE_NATIVE_BYTES_CHANGED;
			}
		}
		for (size_t i = 0; i < std::size(INTRO_MOVIE_SITES); ++i) {
			se::memory::writeBytesUnprotected(INTRO_MOVIE_SITES[i], play ? INTRO_MOVIE_CODE[i] : INTRO_MOVIE_SKIP, 2);
		}
		log::getLog() << "[NativeApi] " << module << " requests the intro movies " << (play ? "played" : "skipped") << "." << std::endl;
		return MWSE_NATIVE_OK;
	}

	MwseNativeStatus EngineSettings::requestLightParticlesFix(const char* module) {
		const auto code = *reinterpret_cast<const BYTE*>(LIGHT_PARTICLES_JUMP);
		if (code != 0x74 && code != 0xEB) { // jz, or the jmp of the fix
			logChangedSite("the light particles fix", LIGHT_PARTICLES_JUMP);
			return MWSE_NATIVE_BYTES_CHANGED;
		}
		se::memory::writeByteUnprotected(LIGHT_PARTICLES_JUMP, 0xEB);
		log::getLog() << "[NativeApi] " << module << " requests the light particles fix." << std::endl;
		return MWSE_NATIVE_OK;
	}

	MwseNativeStatus EngineSettings::requestSunGlare(const char* module, bool enabled) {
		const auto code = reinterpret_cast<const BYTE*>(SUN_GLARE_JUMP);
		if (memcmp(code, SUN_GLARE_ON, 2) != 0 && memcmp(code, SUN_GLARE_OFF, 2) != 0) {
			logChangedSite("the sun glare", SUN_GLARE_JUMP);
			return MWSE_NATIVE_BYTES_CHANGED;
		}
		se::memory::writeBytesUnprotected(SUN_GLARE_JUMP, enabled ? SUN_GLARE_ON : SUN_GLARE_OFF, 2);
		log::getLog() << "[NativeApi] " << module << " requests the sun glare " << (enabled ? "on" : "off") << "." << std::endl;
		return MWSE_NATIVE_OK;
	}

	//
	// Registry
	//

	Registry::Registry() :
		cameraTransformsUpdated(MWSE_NATIVE_EVENT_CAMERA_TRANSFORMS_UPDATED, "the camera transforms update", installCameraTransformsUpdated),
		uiConfigured(MWSE_NATIVE_EVENT_UI_CONFIGURED, "the UI configuration", installUIConfigured),
		gameIdsResolved(MWSE_NATIVE_EVENT_GAME_IDS_RESOLVED, "the resolve of the game's ids", installGameIdsResolved),
		accumulatorFinished(MWSE_NATIVE_EVENT_ACCUMULATOR_FINISHED, "the draw of an accumulator's queue", installAccumulatorFinished),
		playerCellChanged(MWSE_NATIVE_EVENT_PLAYER_CELL_CHANGED, "the player's change of cell", installPlayerCellChanged),
		dynamicLightTest(MWSE_NATIVE_OVERRIDE_DYNAMIC_LIGHT_TEST, "the dynamic light test", &dynamicLightTestSites, reinterpret_cast<DWORD>(&TES3::DataHandler::testDynamicLight)),
		cameraRootCullShow(MWSE_NATIVE_OVERRIDE_CAMERA_ROOT_CULL_SHOW, "the cull show of a camera's root", &cameraRootCullShowSites, reinterpret_cast<DWORD>(&Dispatch::cameraRootCullShow)),
		mapTileRead(MWSE_NATIVE_OVERRIDE_MAP_TILE_READ, "the read of a map tile", &mapTileReadSites, reinterpret_cast<DWORD>(&Dispatch::readMapTile)),
		texturePurge(MWSE_NATIVE_OVERRIDE_TEXTURE_PURGE, "the texture purge", &texturePurgeSites, reinterpret_cast<DWORD>(&Dispatch::purgeTextures)),
		mapCellPaint(MWSE_NATIVE_OVERRIDE_MAP_CELL_PAINT, "the paint of a cell on the world map", &mapCellPaintSites, reinterpret_cast<DWORD>(&Dispatch::paintMapCell)),
		savePicture(MWSE_NATIVE_OVERRIDE_SAVE_PICTURE, "the picture of a save", &savePictureSites, reinterpret_cast<DWORD>(&Dispatch::takeSavePicture)),
		renderTargetLock(MWSE_NATIVE_OVERRIDE_RENDER_TARGET_LOCK, "the lock of a render target", &renderTargetLockSites, reinterpret_cast<DWORD>(&Dispatch::lockRenderTarget)),
		renderTargetUnlock(MWSE_NATIVE_OVERRIDE_RENDER_TARGET_UNLOCK, "the unlock of a render target", &renderTargetUnlockSites, reinterpret_cast<DWORD>(&Dispatch::unlockRenderTarget)),
		fogRender(MWSE_NATIVE_OVERRIDE_FOG_RENDER, "the render of the fog of war", &fogRenderSites, reinterpret_cast<DWORD>(&Dispatch::renderFogOfWar)),
		fogBlob(MWSE_NATIVE_OVERRIDE_FOG_BLOB, "a blob of the fog of war", &fogBlobSites, reinterpret_cast<DWORD>(&Dispatch::drawFogBlob)),
		lightAttachRadius(MWSE_NATIVE_OVERRIDE_LIGHT_ATTACH_RADIUS, "the light attach radius", reinterpret_cast<void*>(&Dispatch::engineLightAttachRadius), installLightAttachRadius),
		frameTimer(MWSE_NATIVE_OVERRIDE_FRAME_TIMER, "the frame timer", reinterpret_cast<void*>(&Dispatch::engineFrameTimer), installFrameTimer)
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
		case MWSE_NATIVE_REQUEST_LIGHT_LIMIT:
		{
			const auto lightLimit = static_cast<const MwseNativeLightLimitRequestV1*>(packet);
			if (lightLimit->structSize < sizeof(MwseNativeLightLimitRequestV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			return registry.engineSettings.requestLightLimit(module, lightLimit->limit);
		}
		case MWSE_NATIVE_REQUEST_RIPPLES:
		{
			const auto ripples = static_cast<const MwseNativeRipplesRequestV1*>(packet);
			if (ripples->structSize < sizeof(MwseNativeRipplesRequestV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			return registry.engineSettings.requestRipples(module, ripples->enabled != 0);
		}
		case MWSE_NATIVE_REQUEST_INTRO_MOVIES:
		{
			const auto introMovies = static_cast<const MwseNativeIntroMoviesRequestV1*>(packet);
			if (introMovies->structSize < sizeof(MwseNativeIntroMoviesRequestV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			return registry.engineSettings.requestIntroMovies(module, introMovies->play != 0);
		}
		case MWSE_NATIVE_REQUEST_LIGHT_PARTICLES_FIX:
		{
			const auto fix = static_cast<const MwseNativeLightParticlesFixRequestV1*>(packet);
			if (fix->structSize < sizeof(MwseNativeLightParticlesFixRequestV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			return registry.engineSettings.requestLightParticlesFix(module);
		}
		case MWSE_NATIVE_REQUEST_SUN_GLARE:
		{
			const auto sunGlare = static_cast<const MwseNativeSunGlareRequestV1*>(packet);
			if (sunGlare->structSize < sizeof(MwseNativeSunGlareRequestV1)) {
				return MWSE_NATIVE_BAD_ARGUMENT;
			}
			return registry.engineSettings.requestSunGlare(module, sunGlare->enabled != 0);
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
		for (auto event : { &cameraTransformsUpdated, &uiConfigured, &gameIdsResolved, &accumulatorFinished, &playerCellChanged }) {
			if (event->getID() == id) {
				return event;
			}
		}
		return nullptr;
	}

	OverrideSlot* Registry::findOverride(uint32_t id) {
		for (auto slot : { &dynamicLightTest, &cameraRootCullShow, &mapTileRead, &texturePurge, &mapCellPaint, &savePicture, &renderTargetLock, &renderTargetUnlock, &fogRender, &fogBlob, &lightAttachRadius, &frameTimer }) {
			if (slot->getID() == id) {
				return slot;
			}
		}
		return nullptr;
	}

	MwseNativeHandle Registry::nextHandle() {
		return ++m_lastHandle;
	}
}
