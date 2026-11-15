return {
	type = "function",
	deprecated = true,
	description = [[Turns the picture about the view's axis at a rate. Without a rate the rate is 0, which stops the rotation, as the old call without arguments did. Without MGE XE, or when MGE XE does not draw the game, the call does nothing.]],
	arguments = { {
		name = "params",
		type = "table",
		optional = true,
		tableParams = {
			{ name = "rate", type = "number", optional = true, default = 0, description = "The rate of the rotation, in radians in a second." },
		},
	} },
}
