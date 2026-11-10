-- The first door in the player's cell.
local door
for reference in tes3.player.cell:iterateReferences(tes3.objectType.door, false) do
	door = reference
	break
end

if door then
	tes3.player:activate(door)
end
