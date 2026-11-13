-- M15 round 15E: the music of a region of New Vegas where no audio marker has any. Nobody can hear a hidden round: what is checked
-- is what the engine logs ("Music: ..."), with the master volume at 0. The player stands on open ground that no marker reaches (the
-- sound region of its acoustic space has an incidental media set whose stingers are heard while nothing else plays), and then two
-- Powder Gangers fight the player there (the battle sets of the region play, not the default battle music); last the player goes to
-- the marker of a controller whose looping option is "random", whose tracks are started without looping.
local M = 70
local NV = 'FalloutNV.esm'
local PLACE = { x = -44000, y = 4000, z = 6000 }   -- no audio marker near (8500 units to the nearest edge); dropped to the ground
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3 }
local MOJAVE_CELLS = { 0x0daeb9, 0x0daeb8, 0x0daebb }

return { limit = 480, delay = 4, run = function(r)
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
    phase('A open ground with no marker')
    r.teleport{ pos = { PLACE.x, PLACE.y, PLACE.z }, worldspace = mojave, yaw = 0, onGround = true, label = 'region place' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('region place')
    r.wait(60)
    phase('B a fight')
    r.drawn(true)
    -- the gangers stand at site X: the player goes there first so that they are loaded, and brings them back to the place
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then
        error('B the placed gangers were not found: there is nothing to fight, and a round with no fight must not pass')
    end
    r.putAt(g1, { pos = { PLACE.x - 70, PLACE.y + 20 * M, PLACE.z }, yaw = math.pi, label = 'ganger1 to the place' })
    r.putAt(g2, { pos = { PLACE.x + 170, PLACE.y + 20 * M, PLACE.z }, yaw = math.pi, label = 'ganger2 to the place' })
    r.teleport{ pos = { PLACE.x, PLACE.y, PLACE.z }, worldspace = mojave, yaw = 0, onGround = true, label = 'back to the place' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('before the fight')
    r.fight(g1, player, 'g1 -> player')
    r.fight(g2, player, 'g2 -> player')
    r.wait(35)
    r.log('B the fight, state of g1 and g2')
    r.state('g1', g1)
    r.state('g2', g2)
    phase('C after the fight')
    r.drawn(false)
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'away from the fight' }
    r.wait(40)
    phase('D a controller that plays its set once (the crashed B-29: five location sets, the looping option "random")')
    r.teleport{ pos = { 75775, 59369, 1685 }, worldspace = mojave, yaw = 0, onGround = true, label = 'crashed B-29' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('crashed B-29')
    r.wait(30)
    r.eye('end')
end }
