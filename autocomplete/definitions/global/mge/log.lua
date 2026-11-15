return {
	type = "function",
	description = [[Writes a line to MGE XE's log file, mgeXE.log. The message is written as it is: it takes no format arguments. Without MGE XE, or when MGE XE does not draw the game, the call does nothing.]],
	arguments = {
		{ name = "message", type = "string", description = "The line to write." },
	},
}
