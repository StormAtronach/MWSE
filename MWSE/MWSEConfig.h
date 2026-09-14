#pragma once

namespace mwse {
	class Configuration {
	public:
		static bool LogWarningsWithLuaStack;
		static bool EnableLegacyLuaMods;
		static bool KeepAllNetImmerseObjectsAlive;
		static bool RunInBackground;
		static bool PatchNiFlipController;
		static bool LetterboxMovies;
		static bool EnableLogColors;
		static bool EnableLogLineNumbers;
		static bool EnableDependencyChecks;
		static bool ReplaceDialogueFiltering;
		static bool EnableLuaErrorNotifications;
		static bool UseSkinnedAccurateActivationRaytests;
		static bool UsePhysicsOptimizations;
		static bool SuppressUselessWarnings;
		static bool UseGlobalAudio;
		static bool NonExclusiveKeyboard;
		static bool ReplaceLightSorting;
		static bool CreateFullMinidumps;
		static UINT BackgroundLoadPollIntervalMs;
		// Render-skip probe (local instrument, see moreFPS/docs/plans/active-grid-batching-plan.md Phase 0b).
		static bool EnableRenderSkipProbe;
		static UINT RenderSkipPercent;
		static UINT RenderSkipMinTriangles;
		static UINT RenderSkipMaxTriangles;
		// Static batching prototype (local experiment, see the same plan, Phase 1).
		static bool EnableStaticBatching;
		static UINT StaticBatchingBinSize;
		static UINT StaticBatchingTypes;
		static UINT StaticBatchingMaxLights;
		static UINT BuildNumber;

		static sol::table getDefaults();

		static void bindToLua();
	};
}
