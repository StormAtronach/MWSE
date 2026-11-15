return {
	type = "function",
	deprecated = true,
	description = [[Adds an angle to the rotation of the picture about the view's axis. Without MGE XE, or when MGE XE does not draw the game, the call does nothing.]],
	arguments = { {
		name = "params",
		type = "table",
		tableParams = {
			{ name = "rotation", type = "number", description = "The angle to add, in radians." },
		},
	} },
}
