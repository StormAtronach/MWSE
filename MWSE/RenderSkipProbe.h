#pragma once

// Render-skip probe: measures what one static draw costs by hiding a
// deterministic fraction of batchable static shapes from the render walk and
// nothing else. Picks, collision, lighting and line of sight keep the
// originals. Local instrument for the active-grid batching investigation;
// not for shipping.

namespace mwse::patch::renderskip {
	// Private render-only bit on NiAVObject::flags, honoured by the
	// NiNode::Display detour: a child carrying it is never offered to CullShow.
	// Bits 0-7 are engine-defined, 0x200 is MWSE's SoftwareSkinningFlag on
	// tri-shapes and 0x400 is MWSE's flagPickProxy on NiCollisionSwitch nodes.
	// Set on shapes only, never on nodes. Shared with the static batching
	// prototype, which hides batched members with it.
	constexpr unsigned short HIDE_MASK = 0x1000;

	// Detours NiNode::Display. Safe to call once; a no-op when the probe is
	// compiled out.
	void install();

	// Per-frame maintenance: re-evaluates the skip set when the config or the
	// active cells change, and emits the periodic stats line.
	void onFrame();
}
