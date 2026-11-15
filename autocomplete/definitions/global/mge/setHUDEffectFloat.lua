return {
	type = "function",
	deprecated = true,
	description = [[With the old MGE XE, this set a float variable of a HUD element's effect. Now it does nothing: MWSE does not call MGE XE's HUD. The first call of one of the 15 HUD functions in a session writes a warning to MWSE.log.]],
	arguments = { {
		name = "params",
		type = "table",
		optional = true,
		tableParams = {
			{ name = "hud", type = "string", optional = true, description = "The HUD element's name." },
			{ name = "variable", type = "string", description = "The name of the effect's variable." },
			{ name = "value", type = "number", description = "The value." },
		},
	} },
	returns = {
		{ name = "result", type = "boolean", description = "Always false." },
	},
}
