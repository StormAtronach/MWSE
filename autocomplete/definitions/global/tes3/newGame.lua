return {
	type = "function",
	description = [[Starts a new game. Returns false, and changes nothing, when called off the main thread or inside a cell load (for example from `referenceSceneNodeCreated` or `referenceActivated`); the call is logged.]],
	returns = {{ name = "executed", type = "boolean" }},
}