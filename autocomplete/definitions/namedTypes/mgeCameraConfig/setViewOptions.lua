return {
	type = "function",
	description = [[Sets the options of a view that a mod draws with a camera or a rendered texture. A view with options gets the parts that are named in the table, in addition to what a view of its kind draws without options (the full tier of the wgpu renderer of MGE XE). A part that is off in `mge.render` stays off in every view. A part that the table does not name stays off in this view. A name in the table that is not one of these, or a value that is not a boolean (`1` is an error, not `true`), throws an error that names it.

The key is the object that names the view: a `niRenderedTexture` (the view that renders into that texture), or a `niCamera` other than the world camera (that camera into the back buffer). The world camera is refused, because it is the main view.

The options last until you change them, until you pass `nil`, or until the key's object is freed. A view that is idle keeps them. With the Direct3D 9 renderer MGE XE cannot see the free: an address that the engine reuses keeps the old options, and `getViewOptions` can return them for a new object.

The player's setting `views.full_tier_max` limits how many views of a frame get the options. With the Direct3D 9 renderer the call stores the options and draws nothing more.

This function exists when the MGE XE API version is 4 or higher (check `mge.camera.setViewOptions ~= nil`).]],
	arguments = {
		{ name = "key", type = "niRenderedTexture|niCamera" },
		{ name = "options", type = "table|nil", tableParams = {
			{ name = "distantLand", type = "boolean", optional = true, default = false },
			{ name = "distantStatics", type = "boolean", optional = true, default = false },
			{ name = "grass", type = "boolean", optional = true, default = false },
			{ name = "distantWater", type = "boolean", optional = true, default = false, description = "MGE XE's water and its reflection for this view." },
			{ name = "shaders", type = "boolean", optional = true, default = false, description = "The post-processing chain for this view." },
			{ name = "glows", type = "boolean", optional = true, default = false },
		} },
	},
	returns = {
		{ name = "accepted", type = "boolean", description = "`false` if MGE XE refuses the key, for example the world camera." },
	},
}
