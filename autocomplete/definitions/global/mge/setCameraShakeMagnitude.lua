return {
	type = "function",
	deprecated = true,
	description = [[Sets the magnitude of the camera shake. Use `mge.camera.shakeMagnitude` instead.]],
	arguments = { {
		name = "params",
		type = "table",
		tableParams = {
			{ name = "magnitude", type = "number", description = "The magnitude of the shake." },
		},
	} },
}
