return {
	type = "function",
	deprecated = true,
	description = [[With the old MGE XE, this loaded a HUD element. Now it does nothing: MWSE does not call MGE XE's HUD. The first call of one of the 15 HUD functions in a session writes a warning to MWSE.log.]],
	arguments = { {
		name = "params",
		type = "table",
		optional = true,
		tableParams = {
			{ name = "hud", type = "string", description = "The HUD element's name." },
			{ name = "texture", type = "string", description = "The texture's path." },
			{ name = "enable", type = "boolean", optional = true, description = "If true, the element was enabled too." },
		},
	} },
	returns = {
		{ name = "result", type = "boolean", description = "Always false." },
	},
}
