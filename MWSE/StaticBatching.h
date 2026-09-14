#pragma once

// Active-grid static batching prototype (Phase 1 of
// moreFPS/docs/plans/active-grid-batching-plan.md). Merges the batchable
// shapes of the active cells' references into per-(render state, spatial bin)
// NiTriShapes under a batch root beneath "worldRoot", hides the originals from
// the render walk with the render-skip probe's private flag bit, and rebuilds
// when the config or the active cells change. Local experiment; not for a PR.
//
// Config: EnableStaticBatching, StaticBatchingBinSize, StaticBatchingTypes
// (bit 0 static, 1 activator, 2 container, 3 door), StaticBatchingMaxLights.

namespace mwse::patch::staticbatching {
	// Per-frame maintenance from EnterFrame: (re)builds or tears down as the
	// config and the active cells change. Relies on the NiNode::Display detour
	// installed by renderskip::install().
	void onFrame();
}
