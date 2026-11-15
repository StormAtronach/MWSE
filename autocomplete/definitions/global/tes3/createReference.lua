return {
	type = "function",
	description = [[Similar to mwscript's PlaceAtPC or PlaceAtMe, this creates a new reference in the game world.

A leveled creature list is resolved to one creature, as the game does when it places a creature from a list: the [`leveledCreaturePicked`](https://mwse.github.io/MWSE/events/leveledCreaturePicked) event fires with the source `"script"`, and the function creates a reference of the picked creature and returns it. When the list picks nothing (its chance for nothing, no entry at the player's level, or the event blocks the pick), no reference is created and the function returns `nil`.]],
	arguments = {{
		name = "params",
		type = "table",
		tableParams = {
			{ name = "object", type = "tes3physicalObject|string", description = "The object to create a reference of. A leveled creature list creates a reference of the creature that it picks." },
			{ name = "position", type = "tes3vector3|number[]", description = "The location to create the reference at." },
			{ name = "orientation", type = "tes3vector3|number[]", description = "The new orientation for the created reference." },
			{ name = "cell", type = "tes3cell|string|table", optional = true, description = "The cell to create the reference in. This is only needed for interior cells." },
			{ name = "scale", type = "number", optional = true, default = 1.0, description = "A scale for the reference." },
			{ name = "updateCollisionGroups", type = "boolean", optional = true, default = true, description = "Only applies to activators, statics, containers, and lights that can't be carried. If false, collision will not be updated. Collision will need to be manually updated by calling `tes3.dataHandler:updateCollisionGroupsForActiveCells()`." },
		},
	}},
	returns = {{ name = "newReference", type = "tes3reference|nil", description = "The new reference. `nil` when a leveled creature list picks nothing." }},
}
