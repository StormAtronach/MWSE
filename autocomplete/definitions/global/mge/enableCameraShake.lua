return {
	type = "function",
	deprecated = true,
	description = [[Enables the camera shake. A magnitude or an acceleration that is given and not 0 is set first. Use `mge.camera.shakeEnable`, `mge.camera.shakeMagnitude` and `mge.camera.shakeAcceleration` instead.]],
	arguments = { {
		name = "params",
		type = "table",
		optional = true,
		tableParams = {
			{ name = "magnitude", type = "number", optional = true, description = "The magnitude of the shake. When it is missing or 0, the current magnitude stays." },
			{ name = "acceleration", type = "number", optional = true, description = "The acceleration of the shake. When it is missing or 0, the current acceleration stays." },
		},
	} },
}
