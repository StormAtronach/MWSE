return {
	type = "function",
	description = [[Returns the options that `mge.camera.setViewOptions` set for a view, or `nil` if the key has none.

This function exists when the MGE XE API version is 4 or higher.]],
	arguments = {
		{ name = "key", type = "niRenderedTexture|niCamera" },
	},
	returns = {
		{ name = "options", type = "table|nil", tableParams = {
			{ name = "distantLand", type = "boolean" },
			{ name = "distantStatics", type = "boolean" },
			{ name = "grass", type = "boolean" },
			{ name = "distantWater", type = "boolean" },
			{ name = "shaders", type = "boolean" },
			{ name = "glows", type = "boolean" },
		} },
	},
}
