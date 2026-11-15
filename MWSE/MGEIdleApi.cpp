#include "MGEIdleApi.h"

#include "TES3WorldController.h"

#include "Log.h"

namespace mge {
	IdleAPI* IdleAPI::get() {
		static IdleAPI idle;
		return &idle;
	}

	void IdleAPI::takeOver() {
		const auto idle = get();
		if (isRealAPIInUse()) {
			idle->withdrawnMGEVersion = api->getMGEVersion();
			const int version = idle->withdrawnMGEVersion - 0x40000;
			mwse::log::getLog() << "The idle mge API keeps the version of MGE XE's API: " << ((version >> 16) & 0xFF) << "." << ((version >> 8) & 0xFF) << "." << (version & 0xFF) << std::endl;
		}
		api = idle;
		apiVersion = idle->getAPIVersion();
		macros = idle->macroFunctions();
	}

	bool IdleAPI::isRealAPIInUse() {
		return api != nullptr && api != get();
	}

	void IdleAPI::noteCall() {
		static std::atomic_flag written = ATOMIC_FLAG_INIT;
		if (!written.test_and_set()) {
			mwse::log::getLog() << "MGE XE's renderer is not in use: the mge calls do nothing." << std::endl;
		}
	}

	const char* IdleAPI::getShaderName(ShaderHandle handle) {
		std::lock_guard lock(shadersMutex);
		for (const auto& [name, shader] : shaders) {
			if (shader.get() == handle) {
				return shader->name.c_str();
			}
		}
		return "";
	}

	void IdleAPI::ignoreMacro() {
		noteCall();
	}

	// The newest version that MWSE knows, so that every name of the mge table is bound and no
	// wrapper takes the path of an older MGE XE.
	int IdleAPI::getAPIVersion() const {
		return 4;
	}

	// The version of the MGE XE that is installed, so that a mod's check for MGE XE passes as with
	// MGE XE's API: the version that MGE XE's API gave before its withdrawal, else the version of
	// MGEXEgui.exe, else 0 (no MGE XE). The number is packed as MGE XE packs it.
	int IdleAPI::getMGEVersion() const {
		if (withdrawnMGEVersion != 0) {
			return withdrawnMGEVersion;
		}
		if (guiVersion.valid()) {
			return 0x40000 + (guiVersion.major << 16 | guiVersion.minor << 8 | guiVersion.patch);
		}
		return 0;
	}

	// A load and a save answer "done": MWSE's wrappers raise an error on "failed".
	bool IdleAPI::loadConfig() {
		noteCall();
		return true;
	}

	bool IdleAPI::saveConfig() {
		noteCall();
		return true;
	}

	bool IdleAPI::featureGetEnabled(RenderFeature) {
		noteCall();
		return false;
	}

	void IdleAPI::featureSetEnabled(RenderFeature, bool) {
		noteCall();
	}

	// The scale of the game's own UI.
	float IdleAPI::guiGetScale() {
		noteCall();
		return 1.0f;
	}

	void IdleAPI::guiSetScale(float) {
		noteCall();
	}

	const MacroFunctions* IdleAPI::macroFunctions() {
		if (idleMacros.TakeScreenshot == nullptr) {
			auto slots = reinterpret_cast<MacroFunctions::Macro*>(&idleMacros);
			for (size_t i = 0; i < sizeof(MacroFunctions) / sizeof(MacroFunctions::Macro); ++i) {
				slots[i] = &IdleAPI::ignoreMacro;
			}
		}
		return &idleMacros;
	}

	bool IdleAPI::reloadDistantLand() {
		noteCall();
		return false;
	}

	// A settings object of zeros.
	DistantLandRenderConfig* IdleAPI::getDistantLandRenderConfig() {
		return &distantLandConfig;
	}

	LightingMode IdleAPI::lightingModeGet() {
		noteCall();
		return LightingMode::Vertex;
	}

	void IdleAPI::lightingModeSet(LightingMode) {
		noteCall();
	}

	// The field of view of the game's world camera.
	float IdleAPI::cameraGetFoV() {
		noteCall();
		const auto worldController = TES3::WorldController::get();
		return worldController ? worldController->worldCamera.cameraData.getFOV() : 0.0f;
	}

	void IdleAPI::cameraSetFoV(float) {
		noteCall();
	}

	void IdleAPI::cameraThirdPersonGetOffset(float* out_values) {
		noteCall();
		out_values[0] = 0.0f;
		out_values[1] = 0.0f;
		out_values[2] = 0.0f;
	}

	void IdleAPI::cameraThirdPersonSetOffset(float*) {
		noteCall();
	}

	bool IdleAPI::zoomGetEnabled() {
		noteCall();
		return false;
	}

	void IdleAPI::zoomSetEnabled(bool) {
		noteCall();
	}

	void IdleAPI::zoomIn(float) {
		noteCall();
	}

	void IdleAPI::zoomOut(float) {
		noteCall();
	}

	// The game's own view: no zoom.
	float IdleAPI::zoomGetZoom() const {
		noteCall();
		return 1.0f;
	}

	void IdleAPI::zoomSetZoom(float) {
		noteCall();
	}

	void IdleAPI::zoomSetZoomContinuous(float, float) {
		noteCall();
	}

	void IdleAPI::zoomStop() const {
		noteCall();
	}

	bool IdleAPI::cameraShakeGetEnabled() {
		noteCall();
		return false;
	}

	void IdleAPI::cameraShakeSetEnabled(bool) {
		noteCall();
	}

	float IdleAPI::cameraShakeGetAcceleration() {
		noteCall();
		return 0.0f;
	}

	void IdleAPI::cameraShakeSetAcceleration(float) {
		noteCall();
	}

	float IdleAPI::cameraShakeGetMagnitude() {
		noteCall();
		return 0.0f;
	}

	void IdleAPI::cameraShakeSetMagnitude(float) {
		noteCall();
	}

	void IdleAPI::hudReset() {
		noteCall();
	}

	bool IdleAPI::hudLoad(const char*, const char*) {
		noteCall();
		return false;
	}

	void IdleAPI::hudFree(const char*) {
		noteCall();
	}

	bool IdleAPI::hudGetEnabled(const char*) {
		noteCall();
		return false;
	}

	void IdleAPI::hudSetEnabled(const char*, bool) {
		noteCall();
	}

	void IdleAPI::hudPosition(const char*, float, float) {
		noteCall();
	}

	void IdleAPI::hudScale(const char*, float, float) {
		noteCall();
	}

	void IdleAPI::hudFullscreen(const char*) {
		noteCall();
	}

	void IdleAPI::hudSetEffect(const char*, const char*) {
		noteCall();
	}

	void IdleAPI::hudSetFloat(const char*, const char*, float) {
		noteCall();
	}

	void IdleAPI::hudSetInt(const char*, const char*, int) {
		noteCall();
	}

	void IdleAPI::hudSetVector4(const char*, const char*, float[4]) {
		noteCall();
	}

	void IdleAPI::hudSetTexture(const char*, const char*) {
		noteCall();
	}

	float IdleAPI::shaderGetHDRReactionSpeed() {
		noteCall();
		return 0.0f;
	}

	void IdleAPI::shaderSetHDRReactionSpeed(float) {
		noteCall();
	}

	// The same object for each name: a mod can load its shader at each frame.
	ShaderHandle IdleAPI::shaderLoad(const char* id) {
		noteCall();
		if (id == nullptr) {
			return nullptr;
		}

		std::lock_guard lock(shadersMutex);
		auto& shader = shaders[id];
		if (!shader) {
			shader = std::make_unique<Shader>(Shader{ id });
		}
		return shader.get();
	}

	bool IdleAPI::shaderReload(ShaderHandle) {
		noteCall();
		return false;
	}

	// A search finds nothing, as the empty list of shaders says.
	ShaderHandle IdleAPI::shaderGetShader(const char*) {
		noteCall();
		return nullptr;
	}

	ShaderHandle IdleAPI::shaderListShaders(size_t) {
		noteCall();
		return nullptr;
	}

	const char* IdleAPI::shaderGetName(ShaderHandle handle) {
		noteCall();
		return getShaderName(handle);
	}

	bool IdleAPI::shaderGetEnabled(ShaderHandle) {
		noteCall();
		return false;
	}

	void IdleAPI::shaderSetEnabled(ShaderHandle, bool) {
		noteCall();
	}

	int IdleAPI::shaderGetPriority(ShaderHandle) {
		noteCall();
		return 0;
	}

	void IdleAPI::shaderSetPriority(ShaderHandle, int) {
		noteCall();
	}

	// A shader of the idle API has no variables.
	bool IdleAPI::shaderGetVariableInfo(ShaderHandle, size_t, ShaderVariableInfo*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetBool(ShaderHandle, const char*, bool*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetFloat(ShaderHandle, const char*, float*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetInt(ShaderHandle, const char*, int*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetString(ShaderHandle, const char*, const char**) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetFloatArray(ShaderHandle, const char*, float*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetVector(ShaderHandle, const char*, float*, size_t) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetMatrix(ShaderHandle, const char*, float*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetBool(ShaderHandle, const char*, bool) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetFloat(ShaderHandle, const char*, float) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetInt(ShaderHandle, const char*, int) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetString(ShaderHandle, const char*, const char*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetFloatArray(ShaderHandle, const char*, const float*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetVector(ShaderHandle, const char*, const float*, size_t) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetMatrix(ShaderHandle, const char*, const float*) {
		noteCall();
		return false;
	}

	void IdleAPI::weatherScatteringGet(float* inscatter, float* outscatter) {
		noteCall();
		for (size_t i = 0; i < 3; ++i) {
			inscatter[i] = 0.0f;
			outscatter[i] = 0.0f;
		}
	}

	void IdleAPI::weatherScatteringSet(float[3], float[3]) {
		noteCall();
	}

	void IdleAPI::weatherDistantFogGet(int, float* fogDistMult, float* fogOffset) {
		noteCall();
		*fogDistMult = 0.0f;
		*fogOffset = 0.0f;
	}

	void IdleAPI::weatherDistantFogSet(int, float, float) {
		noteCall();
	}

	void IdleAPI::weatherPerPixelLightGet(int, float* sunMult, float* ambMult) {
		noteCall();
		*sunMult = 0.0f;
		*ambMult = 0.0f;
	}

	void IdleAPI::weatherPerPixelLightSet(int, float, float) {
		noteCall();
	}

	void IdleAPI::saveScreenshot(const char*, bool) {
		noteCall();
	}

	// The colour of the sky's scattering and its mix: four values.
	void IdleAPI::weatherScatteringSkylightGet(float* skylight) {
		noteCall();
		for (size_t i = 0; i < 4; ++i) {
			skylight[i] = 0.0f;
		}
	}

	void IdleAPI::weatherScatteringSkylightSet(float[3]) {
		noteCall();
	}

	float IdleAPI::nearRenderDistanceGet() {
		noteCall();
		return 0.0f;
	}

	void IdleAPI::nearRenderDistanceSet(float) {
		noteCall();
	}

	bool IdleAPI::shaderGetBoolArray(ShaderHandle, const char*, int*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetIntArray(ShaderHandle, const char*, int*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderGetVectorArray(ShaderHandle, const char*, float*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetBoolArray(ShaderHandle, const char*, const int*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetIntArray(ShaderHandle, const char*, const int*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::shaderSetVectorArray(ShaderHandle, const char*, const float*, size_t*) {
		noteCall();
		return false;
	}

	bool IdleAPI::viewOptionsSet(ViewKeyKind, void*, const ViewOptions*) {
		noteCall();
		return false;
	}

	bool IdleAPI::viewOptionsGet(ViewKeyKind, void*, ViewOptions*) {
		noteCall();
		return false;
	}
}
