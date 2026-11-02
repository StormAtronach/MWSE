return {
	type = "method",
	description = [[This method returns the last rendered frame in the form of niPixelData. It returns `nil` when the renderer cannot read its back buffer. The game's device does not ask for a lockable back buffer, so that is the result on plain Direct3D 8, without MGE XE.]],
	arguments = {
		{ name = "bounds", optional = true, type = "integer[]", description = "These four values are used to take only a specific sub region (in pixels) from the framebuffer, in the order left, right, top, bottom. The right and bottom are inclusive. If not provided, the taken screenshot will include the whole screen." },
	},
	returns = {
		{ name = "screenshot", type = "niPixelData|nil" },
	},
}
