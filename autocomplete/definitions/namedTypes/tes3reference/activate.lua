return {
	type = "method",
	description = [[Causes this reference to activate another. This reference is the activator: it must be an actor with a mobile (the player, or a loaded NPC or creature). This will lead it to go through doors, pick up items, etc.]],
	arguments = {
		{ name = "reference", type = "tes3reference", description = "The reference to activate." },
	},
	examples = {
		["activateDoor"] = {
			title = "The player activates a door",
			description = "The player is the activator. The door in the player's cell opens, or takes the player through it.",
		},
	},
}