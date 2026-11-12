-- M15 round 15B: the music of New Vegas. Nobody can hear a hidden round: what is checked is what the engine logs ("Music: ..."), with
-- the master volume at 0. The player stands on open ground east of the doctor's house of Goodsprings (an audio marker of the town's
-- media location controller), goes into the doctor's house (a cell that names its own music, or none), comes out again, and then
-- fights two Powder Gangers (the combat music is on while they hunt the player and goes off a while after the fight is over).
local M = 70
local NV = 'FalloutNV.esm'
local ARENA = { x = -69660, y = 1900, z = 8352 }   -- open flat ground
-- the door of the doctor's house seen from outside (0x103e61 is the one inside, the way out) and the place in front of it
local HOUSE_DOOR = { NV, 0x103e69 }
local HOUSE_FRONT = { x = -73163, y = 1312, z = 8690 }
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3 }
local MOJAVE_CELLS = { 0x0daeb9, 0x0daeb8, 0x0daebb }

return { limit = 600, delay = 4, run = function(r)
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
    if not r.waitFor(function() return player.cell ~= nil and not player.cell.isExterior end, 40, 'interior') then
        error('B the door of the doctor\'s house did not take the player indoors')
    end
    r.wait(10)
    r.settle(3, 30)
    r.eye('house')
    r.wait(30)
    phase('C out again')
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, worldspace = mojave, yaw = 0, onGround = true, label = 'arena again' }
    r.wait(10)
    r.settle(3, 30)
    r.wait(25)
    phase('D a fight')
    r.drawn(true)
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then
        error('D the placed gangers were not found: there is nothing to fight, and a round with no fight must not pass')
    end
    r.putAt(g1, { pos = { ARENA.x - 70, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = 'ganger1 to the arena' })
    r.putAt(g2, { pos = { ARENA.x + 170, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = 'ganger2 to the arena' })
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'to the arena' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('before the fight')
    r.fight(g1, player, 'g1 -> player')
    r.fight(g2, player, 'g2 -> player')
    r.wait(30)
    r.log('D the fight, state of g1 and g2')
    r.state('g1', g1)
    r.state('g2', g2)
    phase('E after the fight')
    r.drawn(false)
    -- the player goes far from the fight (the fighters are still at it: the music stops when none of them is near)
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'away from the fight' }
    r.wait(45)
    r.eye('end')
end }
