return {
	type = "function",
	deprecated = true,
	description = [[Sets a shader's vector variable to 4 values. Use the shader handle from `mge.shaders.find()` instead.]],
	arguments = { {
		name = "params",
		type = "table",
		tableParams = {
			{ name = "shader", type = "string", description = "The name of the shader." },
			{ name = "variable", type = "string", description = "The name of the variable." },
			{ name = "value", type = "number[]", description = "A table of 4 numbers." },
		},
	} },
	returns = {
		{ name = "success", type = "boolean", description = "False when the shader, the variable or the 4 numbers are missing, or when the variable could not be set." },
	},
}
