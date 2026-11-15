return {
	type = "function",
	deprecated = true,
	description = [[With the old MGE XE, this scaled a HUD element. Now it does nothing: MWSE does not call MGE XE's HUD. The first call of one of the 15 HUD functions in a session writes a warning to MWSE.log.]],
	arguments = { {
		name = "params",
		type = "table",
		optional = true,
		tableParams = {
			{ name = "hud", type = "string", optional = true, description = "The HUD element's name." },
			{ name = "x", type = "number", description = "The horizontal scale." },
			{ name = "y", type = "number", description = "The vertical scale." },
		},
	} },
	returns = {
		{ name = "result", type = "boolean", description = "Always false." },
	},
}
