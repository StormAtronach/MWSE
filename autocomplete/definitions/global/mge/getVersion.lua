return {
	type = "function",
	description = [[Gets the version of the installed MGE XE. Returns a table with keys `major`, `minor`, and `patch`, or nil when MGE XE is not installed. When MGE XE's renderer is not in use (`mge.enabled()` is false), the version is still given: the version that MGE XE gave before it stopped its renderer, else the version of MGEXEgui.exe.]],
	returns = "mgeVersionTable",
}