-- M15 round 15F: the sound of water near the player (New Vegas). Nobody can hear a hidden round: what is checked is what the engine
-- logs ("Water: ..."), with the master volume at 0. The player stands in the cells whose water type names a loop (the cells of
-- the lake of the east and of the river), a little above the water.
local NV = 'FalloutNV.esm'
local MOJAVE_CELLS = { 0x0daeb9, 0x0daeb8, 0x0daebb }
-- the middle of a cell (a cell is 4096 units) of the grid, and a height above the water of the cell
local PLACES = {
    -- the lake of the east (NVCleanWater, water level 2600, AMBWaterLakeLP): the middle of a cell above the water, the player falls on it
    { name = 'lake 13,12', x = 13 * 4096 + 2048, y = 12 * 4096 + 2048, z = 2900, ground = false },
    { name = 'lake 12,10', x = 12 * 4096 + 2048, y = 10 * 4096 + 2048, z = 2900, ground = false },
    -- a cell of the same water type whose land is high above the water: the sound must be quiet or not there at all
    { name = 'dry land 14,7', x = 14 * 4096 + 2048, y = 7 * 4096 + 2048, z = 2900, ground = true },
}

return { limit = 300, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local mojave
    for _, id in ipairs(MOJAVE_CELLS) do
        local ok, cell = pcall(function() return r.world.getCellById(r.core.getFormId(NV, id)) end)
        if ok and cell then mojave = cell break end
    end
    if mojave == nil then error('no cell of the Mojave was found') end
    r.god(true)
    for _, place in ipairs(PLACES) do
        phase('W ' .. place.name)
        r.teleport{ pos = { place.x, place.y, place.z }, worldspace = mojave, yaw = 0, onGround = place.ground, label = place.name }
        r.wait(10)
        r.settle(3, 30)
        r.eye(place.name)
        r.wait(20)
    end
    r.eye('end')
end }
