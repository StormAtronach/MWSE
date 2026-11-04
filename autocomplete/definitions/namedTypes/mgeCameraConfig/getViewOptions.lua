return {
	type = "function",
	description = [[Returns the options that `mge.camera.setViewOptions` set for a view, or `nil` if the key has none. With the flag `intoTextures`, a `niCamera` key returns the camera's default for the textures that it draws into. A texture's result is its own options only: it is `nil` for a texture that takes its camera's default.

This function exists when the MGE XE API version is 4 or higher.]],
	arguments = {
		{ name = "key", type = "niRenderedTexture|niCamera" },
		{ name = "flags", type = "table", optional = true, tableParams = {
			{ name = "intoTextures", type = "boolean", optional = true, default = false, description = "With a `niCamera` key: read the camera's default for every `niRenderedTexture` that it draws into. With a `niRenderedTexture` key this flag is an error." },
		} },
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
