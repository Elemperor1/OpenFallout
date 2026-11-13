-- M14 part two: the player's own body swims (New Vegas, third person). The player is put over the lake of the east (water level
-- 2600, cells 13,12) and falls into it, then swims forward for a few seconds. Checked by the engine log ("Movement of ... swimmt...").
local NV = 'FalloutNV.esm'
local MOJAVE_CELLS = { 0x0daeb9, 0x0daeb8, 0x0daebb }

return { limit = 200, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local mojave
    for _, id in ipairs(MOJAVE_CELLS) do
        local ok, cell = pcall(function() return r.world.getCellById(r.core.getFormId(NV, id)) end)
        if ok and cell then mojave = cell break end
    end
    if mojave == nil then error('no cell of the Mojave was found') end
    r.god(true)
    r.camera('ThirdPerson')
    phase('W over the lake')
    r.teleport{ pos = { 13 * 4096 + 2048, 12 * 4096 + 2048, 2900 }, worldspace = mojave, yaw = 0, onGround = false, label = 'over the lake' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('in the water')
    phase('W swims forward')
    r.move{ forward = 1, seconds = 4 }
    r.eye('after swimming forward')
    r.move{ forward = 1, run = true, seconds = 3 }
    r.eye('after swimming fast')
    r.wait(2)
    r.shot('W1_swimming', false)
    r.log('PHASE done')
end }
