return {
	type = "function",
	deprecated = true,
	description = [[With the old MGE XE, this positioned a HUD element. Now it does nothing: MWSE does not call MGE XE's HUD. The first call of one of the 15 HUD functions in a session writes a warning to MWSE.log.]],
	arguments = { {
		name = "params",
		type = "table",
		optional = true,
		tableParams = {
			{ name = "hud", type = "string", optional = true, description = "The HUD element's name." },
			{ name = "x", type = "number", description = "The horizontal position." },
			{ name = "y", type = "number", description = "The vertical position." },
		},
	} },
	returns = {
		{ name = "result", type = "boolean", description = "Always false." },
	},
}
