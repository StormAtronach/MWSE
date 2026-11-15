return {
	type = "function",
	deprecated = true,
	description = [[Sets the acceleration of the camera shake. Use `mge.camera.shakeAcceleration` instead.]],
	arguments = { {
		name = "params",
		type = "table",
		tableParams = {
			{ name = "acceleration", type = "number", description = "The acceleration of the shake." },
		},
	} },
}
