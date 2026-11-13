-- M14 part two: what a blow does to a character beyond its health (New Vegas): it is thrown back by the force of the blow, a
-- crippled leg slows it and makes it limp, and a character thrown off the ground falls and is hurt by the fall.
-- Phases: S setup (out of the interior), K a blow and the knockback, L two crippled legs and how fast the character goes now
-- (against one with sound legs), F a fall (a character thrown while it is put up in the air: the ground ends under it).
-- The characters are the placed Powder Gangers of the camp at site X, taken to the open ground east of the doctor's house.
local M = 70
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }
local ARENA = { x = -69660, y = 1900, z = 8352 }
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3 }

return { limit = 600, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { NV, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local function distance(a, b)
        local dx, dy = a.x - b.x, a.y - b.y
        return math.sqrt(dx * dx + dy * dy)
    end
    local function height(object) return object.position.z end

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    r.activate(DOOR_OUT, 'door out')
    r.waitFor(exterior, 40, 'exterior')
    r.wait(8)
    r.equip(nil, 1)
    r.weapon('equipped')
    r.camera('ThirdPerson')
    r.drawn(true)
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then
        error('S the placed gangers were not found: there is nothing to hit, and a round with no character must not pass')
    end
    -- the arena is open ground; the characters stand 20 m north of the player, 4 m apart
    r.putAt(g1, { pos = { ARENA.x - 140, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = 'ganger1 to the arena' })
    r.putAt(g2, { pos = { ARENA.x + 140, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = 'ganger2 to the arena' })
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'to the arena' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('arena')
    r.track({ { label = 'g1', actor = g1 }, { label = 'g2', actor = g2 } }, 0.5)

    ---------------------------------------------------------------- K: a blow throws a character back
    phase('K knockback')
    -- they are at peace: g2 is struck by the player's own weapon from a few metres, and its place is read before and after
    local before = { x = g2.position.x, y = g2.position.y, z = g2.position.z }
    r.teleport{ pos = { before.x - 280, before.y - 200, before.z }, yaw = 0, onGround = true, label = 'four metres from g2, away from the saloon posts' }
    r.wait(1)
    r.log(string.format('K g2 before the blow at (%.0f, %.0f, %.0f)', before.x, before.y, before.z))
    -- a few shots one after the other, a picture right after each: the flash of the muzzle lasts a fraction of a second (the
    -- variant with OPENFALLOUT_EFFECT_HOLD makes the effects last long enough for a picture to catch them)
    r.camera('FirstPerson')
    r.aimAt(g2, 'K aim at g2', 60)
    r.wait(0.5)
    -- (two shots: a character that has little left of its health runs from a fight, and phase L needs g2 to fight)
    local row
    for i = 1, 2 do
        row = r.fire('K_g2_blow_' .. i, g2)
        r.shot('K' .. i .. '_blow', false)
        r.log('K blow ' .. i .. ': hit=' .. tostring(row and row.hit) .. ' damage=' .. tostring(row and row.damage))
        r.wait(0.3)
    end
    for i = 1, 6 do
        r.wait(0.4)
        local p = g2.position
        r.log(string.format('K g2 %.1f s after the blow: (%.0f, %.0f, %.0f), %.0f units from where it stood', i * 0.4, p.x, p.y, p.z,
            distance(p, before)))
    end
    r.shot('K1_after_blow', false)
    r.state('g2 after the blow', g2)
    r.peace(g2)
    r.peace(g1)

    ---------------------------------------------------------------- L: crippled legs
    phase('L crippled legs')
    -- both of g1's legs are crippled by blows at them (limb types 7 to 9 are the left leg, 10 to 12 the right one)
    -- the blows are those of the player's own weapon (one cripples a leg, two do not kill a character of 45 health unless they are critical)
    -- (each leg is struck until the blow says it is crippled, at most six times: a round that did not cripple both legs must
    -- not compare the speeds, so it fails)
    local function cripple(target, label)
        for _, limb in ipairs({ 7, 10 }) do
            local crippled = false
            for blow = 1, 6 do
                local ok, result = pcall(r.core.combat.strike, r.form(player), r.form(target), '', limb)
                local dead = false
                do
                    local okd, d = pcall(r.core.combat.isDead, r.form(target))
                    dead = okd and d
                end
                crippled = ok and result ~= nil and result.crippled == true
                r.log(string.format('L %s limb %d blow %d: %s crippled=%s dead=%s', label, limb, blow,
                    ok and 'struck' or ('error ' .. tostring(result)), tostring(crippled), tostring(dead)))
                if dead then error('L ' .. label .. ' died of the blows at its legs: the comparison needs it alive') end
                if crippled then break end
                r.wait(0.2)
            end
            if not crippled then
                error('L ' .. label .. ' limb ' .. limb .. ' was not crippled by six blows: the comparison needs two crippled legs')
            end
        end
    end
    -- g1 is put back in line with g2, 35 m from the player, both at peace; the crippling blows throw g1 about, so it is put again after
    r.putAt(g1, { pos = { ARENA.x - 140, ARENA.y + 35 * M, ARENA.z }, yaw = math.pi, label = 'ganger1 35 m away' })
    r.wait(1)
    cripple(g1, 'g1')
    r.wait(2.5)
    r.peace(g1)
    r.putAt(g1, { pos = { ARENA.x - 140, ARENA.y + 35 * M, ARENA.z }, yaw = math.pi, label = 'ganger1 35 m away again' })
    r.putAt(g2, { pos = { ARENA.x + 140, ARENA.y + 35 * M, ARENA.z }, yaw = math.pi, label = 'ganger2 35 m away' })
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'player back at the start' }
    r.wait(2)
    r.inspect('g1', g1)
    r.inspect('g2', g2)
    local start1 = { x = g1.position.x, y = g1.position.y, z = g1.position.z }
    local start2 = { x = g2.position.x, y = g2.position.y, z = g2.position.z }
    r.log('L both characters go for the player from 35 m: g1 has two crippled legs, g2 has none')
    r.fight(g1, player, 'g1 -> player')
    r.fight(g2, player, 'g2 -> player')
    local t = 0
    for i = 1, 8 do
        r.wait(0.5)
        t = t + 0.5
        r.log(string.format('L %.1f s: g1 has gone %.0f units, g2 has gone %.0f units', t, distance(g1.position, start1),
            distance(g2.position, start2)))
        if i == 4 then r.shot('L1_walk_4s', false) end
    end
    r.state('g1', g1)
    r.state('g2', g2)
    r.peace(g1)
    r.peace(g2)

    ---------------------------------------------------------------- F: a fall
    phase('F fall')
    -- a character is struck and put up in the air while it is thrown: the ground ends under it and it falls. There is
    -- something solid about 270 units over the arena (the first run's fall ended there), so the heights below are over
    -- that: 905 up is a fall of about 630 units (over the 600 that the plugins allow before a fall hurts: a few points
    -- of damage), 1100 up one of about 830 (more than the health: it dies of the fall)
    local function fall(target, label, up)
        r.putAt(target, { pos = { ARENA.x + 140, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = label .. ' to the arena again' })
        r.teleport{ pos = { ARENA.x + 140, ARENA.y + 20 * M - 5 * M, ARENA.z }, yaw = 0, onGround = true, label = 'five metres from ' .. label }
        r.wait(1.5)
        local ground = height(target)
        r.log(string.format('F %s stands at height %.0f', label, ground))
        local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(target), '', -1)
        r.log('F blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit)) or ('error ' .. tostring(hit))))
        r.wait(0.15)
        local p = target.position
        -- (not taken to the ground: it has to be in the air)
        r.putAt(target, { pos = { p.x, p.y, p.z + up }, yaw = math.pi, onGround = false,
            label = label .. ' ' .. up .. ' units up while it is thrown' })
        for i = 1, 8 do
            r.wait(0.5)
            r.log(string.format('F %s %.1f s later at height %.0f (the ground is at %.0f)', label, i * 0.5, height(target), ground))
        end
        r.state(label .. ' after the fall', target)
    end
    fall(g2, 'g2', 905)
    r.shot('F1_after_fall', false)
    r.peace(g1)
    fall(g1, 'g1', 1100)
    r.untrack()

    ---------------------------------------------------------------- P: the player's own body in the air (third person)
    phase('P player in the air')
    r.camera('ThirdPerson')
    r.drawn(false)
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'player on the ground' }
    r.wait(3)
    r.track({ { label = 'player', actor = player } }, 0.2)
    r.wait(1)
    local up = player.position
    r.teleport{ pos = { up.x, up.y, up.z + 700 }, yaw = 0, onGround = false, label = 'player 700 units up' }
    for i = 1, 8 do
        r.wait(0.35)
        r.log(string.format('P player %.2f s after being put up: height %.0f', i * 0.35, player.position.z))
    end
    r.wait(2)
    -- turning where it stands: the camera (and the body of the player with it) is turned in small steps, one way and then back
    r.log('P player turns right where it stands')
    for i = 1, 20 do r.look(i * 0.15, 0) r.wait(0.06) end
    r.wait(1)
    r.log('P player turns left where it stands')
    for i = 1, 20 do r.look(3 - i * 0.15, 0) r.wait(0.06) end
    r.wait(1.5)
    r.untrack()
    r.log('PHASE done')
end }
