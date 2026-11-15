return {
	type = "function",
	description = [[Determines if the MGE XE interface is enabled. It is false when MGE XE is not installed, or when its renderer is not in use. Then every other function of the mge library still exists and does nothing, and a shader that is loaded does nothing.]],
	returns = {
		{ name = "enabled", type = "boolean" },
	}
}