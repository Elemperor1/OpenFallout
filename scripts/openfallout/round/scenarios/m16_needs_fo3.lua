-- M16 needs (Fallout 3): the rule of waiting, sleeping and travelling (an ordinary wait, a hostile near, a bed, a place that
-- forbids waiting) and the fact that the game has no hardcore mode (asking for it changes nothing). It logs numbers: the reason of
-- a refusal is a word in the engine's log ("Rest refused: ..."), never a text of the game.
local M = 70
local FO3 = 'Fallout3.esm'
local SITE = { x = -10600, y = 1560, z = 14158, a = 0x52367 }       -- a placed raider of the Capital Wasteland (cell -3,0)
local BED = { FO3, 0x9dcfa }                                         -- a mattress on the floor of MamaDolces01
local MAMA = { cell = 'MamaDolces01', pos = { 1631, -6664, -690 } }
local NOWAIT = { cell = 'Vault101a', pos = { 10804, 7575, 2860 } }   -- by a door, just above the floor  -- a cell with the flag that forbids waiting

return { limit = 900, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local function lookup(id)
        local ok, object = pcall(r.lookup, { FO3, id })
        return ok and object or nil
    end
    local function interior(place, label)
        r.teleport{ cell = place.cell, pos = place.pos, yaw = 0, onGround = place.ground == true, label = label }
        r.wait(8)
        r.settle(3, 30)
        r.eye(label)
    end

    phase('S setup')
    r.god(true)
    r.eye('start')
    r.hardcore(nil, 'start')
    r.hardcore(true, 'asked for the mode')

    phase('W a wait in the open')
    r.rest{ label = 'open wait 1h', hours = 1 }

    phase('H a hostile near')
    r.teleport{ pos = { SITE.x, SITE.y - 6 * M, SITE.z + 300 }, yaw = 0, onGround = true, label = 'the raiders of cell -3,0' }
    r.wait(10)
    r.settle(3, 30)
    local g1 = lookup(SITE.a)
    if g1 == nil then error('H the placed raider was not found: a round without one proves nothing') end
    r.fight(g1, player, 'raider -> player')
    r.wait(2)
    r.rest{ label = 'wait with a hostile near', hours = 1 }
    r.peace(g1)
    r.wait(2)
    r.rest{ label = 'wait after the peace', hours = 1 }

    phase('T fast travel')
    r.travel('from the camp')

    phase('B a bed')
    interior(MAMA, 'Mama Dolce')
    r.activate(BED, 'bed')
    r.wait(1)
    r.rest{ label = 'sleep 8h', hours = 8, activated = true, timeout = 300 }

    phase('N a place that forbids waiting')
    interior(NOWAIT, 'Vault 101')
    r.wait(3)
    r.rest{ label = 'wait where waiting is forbidden', hours = 1 }
    r.log('done')
end }
