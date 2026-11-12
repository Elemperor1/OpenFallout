-- M15 round 15C: the ambient sound of New Vegas. Nobody can hear a hidden round: what is checked is what the engine logs
-- ("Ambient: ..."), with the master volume at 0. The player stands on open ground of Goodsprings (an acoustic space with a loop of
-- the time of day and a region with sounds), goes into the doctor's house (an interior space) and comes out again, and then goes
-- to a far place of the Mojave to see the space and the region change.
local M = 70
local NV = 'FalloutNV.esm'
local ARENA = { x = -69660, y = 1900, z = 8352 }   -- open flat ground
-- the door of the doctor's house seen from outside (0x103e61 is the one inside, the way out) and the place in front of it
local HOUSE_DOOR = { NV, 0x103e69 }
local HOUSE_FRONT = { x = -73163, y = 1312, z = 8690 }
local SITE_X = { x = -21860, y = -38900, z = 8100 }
local MOJAVE_CELLS = { 0x0daeb9, 0x0daeb8, 0x0daebb }

return { limit = 420, delay = 4, run = function(r)
    local player = r.player()
    local function phase(name) r.log('PHASE ' .. name) end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { NV, id })
        return ok and object or nil
    end
    local mojave
    for _, id in ipairs(MOJAVE_CELLS) do
        local ok, cell = pcall(function() return r.world.getCellById(r.core.getFormId(NV, id)) end)
        if ok and cell then mojave = cell break end
    end
    if mojave == nil then error('no cell of the Mojave was found') end
    r.god(true)
    phase('A open ground of Goodsprings')
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, worldspace = mojave, yaw = 0, onGround = true, label = 'arena' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('arena')
    r.wait(35)
    phase('B the doctor\'s house')
    r.teleport{ pos = { HOUSE_FRONT.x, HOUSE_FRONT.y, HOUSE_FRONT.z }, worldspace = mojave, yaw = 0, onGround = true, label = 'house front' }
    r.wait(4)
    r.activate(HOUSE_DOOR, 'door in')
    r.waitFor(function() return player.cell ~= nil and not player.cell.isExterior end, 40, 'interior')
    r.wait(10)
    r.settle(3, 30)
    r.eye('house')
    r.wait(30)
    phase('C out again')
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, worldspace = mojave, yaw = 0, onGround = true, label = 'arena again' }
    r.wait(10)
    r.settle(3, 30)
    r.wait(25)
    phase('D a far place')
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(45)
    r.settle(3, 30)
    r.eye('end')
end }
