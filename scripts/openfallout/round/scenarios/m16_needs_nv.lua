-- M16 needs (New Vegas): the rule of waiting, sleeping and travelling (a place that forbids waiting, a hostile near, an
-- ordinary wait, a bed) and the hardcore mode (needs rising with the game's time, the stage spells, the tiles of the display).
-- It logs numbers: the reason of a refusal is a word in the engine's log ("Rest refused: ..."), never a text of the game.
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }                                   -- the door of the doctor's house, out to Goodsprings
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4 } -- placed Powder Gangers (cell -6,-10)
local BED = { NV, 0x1389b7 }                                        -- a queen bed of NiptonHouse5 (no owner of its own)
local NIPTON = { cell = 'NiptonHouse5', pos = { 180, -540, 2420 } }
local NOWAIT = { cell = 'Gomorrah01', pos = { 5376, 2672, 6540 } }   -- by a door, just above the floor  -- a cell with the flag that forbids waiting

return { limit = 1500, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { NV, id })
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
    r.needs('start')
    r.activate(DOOR_OUT, 'door out')
    r.waitFor(exterior, 40, 'exterior')
    r.wait(6)
    r.settle(3, 30)
    r.eye('outside')

    phase('W a wait in the open (hardcore off)')
    r.rest{ label = 'open wait 1h', hours = 1 }

    phase('H a hostile near')
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(10)
    r.settle(3, 30)
    local g1 = lookup(SITE_X.gun1)
    if g1 == nil then error('H the placed Powder Ganger was not found: a round without one proves nothing') end
    r.fight(g1, player, 'g1 -> player')
    r.wait(2)
    r.rest{ label = 'wait with a hostile near', hours = 1 }
    r.peace(g1)
    r.wait(2)
    r.rest{ label = 'wait after the peace', hours = 1 }

    phase('T fast travel (hardcore off)')
    r.travel('from the site')
    r.wait(6)
    r.settle(3, 30)
    r.eye('after the travel')

    phase('C the hardcore mode')
    -- a player that cannot be hurt is not worn down by the needs: the god mode of the setup comes off here and stays off
    r.god(false)
    r.hardcore(true, 'turned on')
    r.needs('on, at once')
    -- three days: dehydration passes its stages (the fourth day would take all the health: the last stage is fatal)
    for i = 1, 3 do
        r.rest{ label = 'hardcore wait ' .. i, hours = 24, timeout = 400 }
        r.needs('after wait ' .. i)
        r.shot('hud_needs_' .. i)
    end
    r.hardcore(false, 'turned off')
    r.needs('off')
    r.hardcore(true, 'turned on again')

    phase('B a bed')
    interior(NIPTON, 'Nipton house')
    r.activate(BED, 'bed')
    r.wait(1)
    r.rest{ label = 'sleep 8h', hours = 8, activated = true, timeout = 300 }
    r.needs('after the sleep')

    phase('N a place that forbids waiting')
    interior(NOWAIT, 'Gomorrah')
    r.wait(3)
    r.rest{ label = 'wait where waiting is forbidden', hours = 1 }
    r.needs('end')
    r.log('done')
end }
