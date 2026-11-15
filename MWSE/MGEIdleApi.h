#pragma once

#include "MGEApi.h"

namespace mge {
	// The mge API that MWSE uses when no real API of MGE XE is in use: MGE XE is not there, its
	// renderer is not in use, or MGE XE withdrew its API (MWSE_NATIVE_REQUEST_MGE_API). Each call
	// does nothing and gives a value of its type. Where the game has the true answer (the field of
	// view), it gives that. The first call writes one line to MWSE.log.
	class IdleAPI final : public MGEAPIv4 {
	public:
		// The one idle API.
		static IdleAPI* get();

		// Puts the idle API in the place of MGE XE's API, or in the place of no API. The Lua
		// values that were copied from MGE XE's API before (the macros, the distant land
		// configuration) stay as they are. The version of MGE XE's API is kept for getMGEVersion.
		static void takeOver();

		// True when an API of MGE XE is in use: not null and not the idle API.
		static bool isRealAPIInUse();

		int getAPIVersion() const override;
		int getMGEVersion() const override;

		bool loadConfig() override;
		bool saveConfig() override;

		bool featureGetEnabled(RenderFeature feature) override;
		void featureSetEnabled(RenderFeature feature, bool enable) override;

		float guiGetScale() override;
		void guiSetScale(float scale) override;

		const MacroFunctions* macroFunctions() override;
		bool reloadDistantLand() override;
		DistantLandRenderConfig* getDistantLandRenderConfig() override;
		LightingMode lightingModeGet() override;
		void lightingModeSet(LightingMode mode) override;

		float cameraGetFoV() override;
		void cameraSetFoV(float fov) override;
		void cameraThirdPersonGetOffset(float* out_values) override;
		void cameraThirdPersonSetOffset(float* values) override;

		bool zoomGetEnabled() override;
		void zoomSetEnabled(bool enable) override;
		void zoomIn(float amount) override;
		void zoomOut(float amount) override;
		float zoomGetZoom() const override;
		void zoomSetZoom(float amount) override;
		void zoomSetZoomContinuous(float rate, float rateTarget) override;
		void zoomStop() const override;

		bool cameraShakeGetEnabled() override;
		void cameraShakeSetEnabled(bool enable) override;
		float cameraShakeGetAcceleration() override;
		void cameraShakeSetAcceleration(float value) override;
		float cameraShakeGetMagnitude() override;
		void cameraShakeSetMagnitude(float value) override;

		void hudReset() override;
		bool hudLoad(const char* name, const char* texture) override;
		void hudFree(const char* name) override;
		bool hudGetEnabled(const char* name) override;
		void hudSetEnabled(const char* name, bool enable) override;
		void hudPosition(const char* name, float x, float y) override;
		void hudScale(const char* name, float scaleX, float scaleY) override;
		void hudFullscreen(const char* name) override;
		void hudSetEffect(const char* name, const char* effectPath) override;
		void hudSetFloat(const char* name, const char* variableName, float value) override;
		void hudSetInt(const char* name, const char* variableName, int value) override;
		void hudSetVector4(const char* name, const char* variableName, float value[4]) override;
		void hudSetTexture(const char* name, const char* texturePath) override;

		float shaderGetHDRReactionSpeed() override;
		void shaderSetHDRReactionSpeed(float speed) override;
		ShaderHandle shaderLoad(const char* id) override;
		bool shaderReload(ShaderHandle handle) override;
		ShaderHandle shaderGetShader(const char* id) override;
		ShaderHandle shaderListShaders(size_t index) override;
		const char* shaderGetName(ShaderHandle handle) override;
		bool shaderGetEnabled(ShaderHandle handle) override;
		void shaderSetEnabled(ShaderHandle handle, bool enable) override;
		int shaderGetPriority(ShaderHandle handle) override;
		void shaderSetPriority(ShaderHandle handle, int value) override;
		bool shaderGetVariableInfo(ShaderHandle handle, size_t index, ShaderVariableInfo* out_info) override;
		bool shaderGetBool(ShaderHandle handle, const char* variableName, bool* out_value) override;
		bool shaderGetFloat(ShaderHandle handle, const char* variableName, float* out_value) override;
		bool shaderGetInt(ShaderHandle handle, const char* variableName, int* out_value) override;
		bool shaderGetString(ShaderHandle handle, const char* variableName, const char** out_value) override;
		bool shaderGetFloatArray(ShaderHandle handle, const char* variableName, float* out_values, size_t* count) override;
		bool shaderGetVector(ShaderHandle handle, const char* variableName, float* out_values, size_t count) override;
		bool shaderGetMatrix(ShaderHandle handle, const char* variableName, float* out_values) override;
		bool shaderSetBool(ShaderHandle handle, const char* variableName, bool value) override;
		bool shaderSetFloat(ShaderHandle handle, const char* variableName, float value) override;
		bool shaderSetInt(ShaderHandle handle, const char* variableName, int value) override;
		bool shaderSetString(ShaderHandle handle, const char* variableName, const char* value) override;
		bool shaderSetFloatArray(ShaderHandle handle, const char* variableName, const float* values, size_t* count) override;
		bool shaderSetVector(ShaderHandle handle, const char* variableName, const float* values, size_t count) override;
		bool shaderSetMatrix(ShaderHandle handle, const char* variableName, const float* values) override;

		void weatherScatteringGet(float* inscatter, float* outscatter) override;
		void weatherScatteringSet(float inscatter[3], float outscatter[3]) override;
		void weatherDistantFogGet(int weatherID, float* fogDistMult, float* fogOffset) override;
		void weatherDistantFogSet(int weatherID, float fogDistMult, float fogOffset) override;
		void weatherPerPixelLightGet(int weatherID, float* sunMult, float* ambMult) override;
		void weatherPerPixelLightSet(int weatherID, float sunMult, float ambMult) override;

		void saveScreenshot(const char* path, bool captureWithUI) override;
		void weatherScatteringSkylightGet(float* skylight) override;
		void weatherScatteringSkylightSet(float skylight[3]) override;

		float nearRenderDistanceGet() override;
		void nearRenderDistanceSet(float distance) override;
		bool shaderGetBoolArray(ShaderHandle handle, const char* variableName, int* values, size_t* count) override;
		bool shaderGetIntArray(ShaderHandle handle, const char* variableName, int* values, size_t* count) override;
		bool shaderGetVectorArray(ShaderHandle handle, const char* variableName, float* values, size_t* count) override;
		bool shaderSetBoolArray(ShaderHandle handle, const char* variableName, const int* values, size_t* count) override;
		bool shaderSetIntArray(ShaderHandle handle, const char* variableName, const int* values, size_t* count) override;
		bool shaderSetVectorArray(ShaderHandle handle, const char* variableName, const float* values, size_t* count) override;

		bool viewOptionsSet(ViewKeyKind kind, void* key, const ViewOptions* options) override;
		bool viewOptionsGet(ViewKeyKind kind, void* key, ViewOptions* options) override;

	private:
		IdleAPI() = default;

		// A shader that does nothing: only its name. One object for each name, kept for the
		// session, so that a mod that loads its shader at each frame gets the same one.
		struct Shader {
			std::string name;
		};

		// Writes the line of the idle API to MWSE.log at the first call.
		static void noteCall();

		// The name of a shader of this API, or an empty name for a handle of another API (a mod
		// can hold a handle of MGE XE's API from before a withdrawal).
		const char* getShaderName(ShaderHandle handle);

		// A macro that does nothing.
		static void ignoreMacro();

		std::mutex shadersMutex;
		std::unordered_map<std::string, std::unique_ptr<Shader>> shaders;
		MacroFunctions idleMacros = {};
		DistantLandRenderConfig distantLandConfig = {};

		// The version that MGE XE's API gave before the idle API took its place, or 0 when no API
		// of MGE XE was in use.
		int withdrawnMGEVersion = 0;
	};
}
