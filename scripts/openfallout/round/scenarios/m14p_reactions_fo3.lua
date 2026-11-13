-- M14 part two (Fallout 3): the same checks as m14p_reactions.lua on the characters of a raider camp of the Capital Wasteland: a blow
-- throws a character back, two crippled legs slow it and make it limp, a character thrown off the ground falls and is hurt by the
-- fall, a shot flashes at the muzzle and its blow leaves an impact on the body. The characters stay where the game put them (the
-- player goes to them) and are never relocated except to be put up in the air for the fall.
-- Phases: S setup, K a blow and the knockback, L two crippled legs and how fast the character goes now, F a fall.
local M = 70
-- Two placed raiders of the Capital Wasteland (cell -3,0, 6 m apart): LvlRaider2SpecialVar (a) and LvlRaiderMelee (b). The player
-- has no weapon (the round's give does not reach an inventory of this engine's items), so the blows are made with the form of the
-- 10mm pistol named for each blow: the damage, the impact and the knockback come from that record.
local SITE = { x = -10600, y = 1560, z = 14158, a = 0x52367, b = 0x52363 }
local PISTOL = 'FormId:0x100434f'  -- Weap10mmPistol (the plugin is the first of the load order: numbers 0x01......)

return { limit = 700, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local function lookup(id)
        local ok, object = pcall(r.lookup, { 'Fallout3.esm', id })
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
    r.camera('ThirdPerson')
    r.teleport{ pos = { SITE.x, SITE.y - 6 * M, SITE.z + 300 }, yaw = 0, onGround = true, label = 'the raiders of cell -3,0' }
    r.wait(12)
    r.settle(3, 30)
    r.eye('camp')
    local g1, g2 = lookup(SITE.a), lookup(SITE.b)
    if g1 == nil or g2 == nil then
        error('S the placed raiders were not found: there is nothing to hit, and a round with no character must not pass')
    end
    r.log(string.format('S raider a at (%.0f, %.0f, %.0f), raider b at (%.0f, %.0f, %.0f)', g1.position.x, g1.position.y,
        g1.position.z, g2.position.x, g2.position.y, g2.position.z))
    r.peace(g1)
    r.peace(g2)
    r.track({ { label = 'a', actor = g1 }, { label = 'b', actor = g2 } }, 0.5)

    ---------------------------------------------------------------- K: a blow throws a character back
    phase('K knockback')
    local before = { x = g2.position.x, y = g2.position.y, z = g2.position.z }
    r.teleport{ pos = { before.x, before.y - 5 * M, before.z + 200 }, yaw = 0, onGround = true, label = 'five metres from b' }
    r.wait(1.5)
    r.log(string.format('K b before the blow at (%.0f, %.0f, %.0f)', before.x, before.y, before.z))
    -- three blows with the pistol's form, one after the other
    for i = 1, 3 do
        local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(g2), PISTOL, -1)
        r.log('K blow ' .. i .. ': ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage)) or ('error ' .. tostring(hit))))
        r.wait(0.3)
    end
    for i = 1, 6 do
        r.wait(0.4)
        local p = g2.position
        r.log(string.format('K b %.1f s after the blows: (%.0f, %.0f, %.0f), %.0f units from where it stood', i * 0.4, p.x, p.y, p.z,
            distance(p, before)))
    end
    r.state('b after the blows', g2)
    r.peace(g2)
    r.peace(g1)

    ---------------------------------------------------------------- L: crippled legs
    phase('L crippled legs')
    -- a weak weapon needs many blows to cripple a leg: each leg is struck until the blow says it is crippled (at most ten times), and
    -- a character that dies of them fails the round (the comparison needs it alive)
    local function cripple(target, label)
        for _, limb in ipairs({ 7, 10 }) do
            local legCrippled = false
            for blow = 1, 10 do
                local ok, result = pcall(r.core.combat.strike, r.form(player), r.form(target), PISTOL, limb)
                local okd, dead = pcall(r.core.combat.isDead, r.form(target))
                dead = okd and dead
                local crippled = ok and result and result.crippled
                r.log(string.format('L %s limb %d blow %d: %s crippled=%s dead=%s', label, limb, blow,
                    ok and 'struck' or ('error ' .. tostring(result)), tostring(crippled), tostring(dead)))
                if dead then error('L ' .. label .. ' died of the blows at its legs: the comparison needs it alive') end
                if crippled then
                    legCrippled = true
                    break
                end
                r.wait(0.2)
            end
            -- (a round that did not cripple both legs must not compare the speeds, so it fails)
            if not legCrippled then
                error('L ' .. label .. ' limb ' .. limb .. ' was not crippled by ten blows: the comparison needs two crippled legs')
            end
        end
    end
    r.teleport{ pos = { g1.position.x, g1.position.y - 5 * M, g1.position.z + 200 }, yaw = 0, onGround = true, label = 'five metres from a' }
    r.wait(1.5)
    cripple(g1, 'a')
    r.wait(2.5)
    r.peace(g1)
    -- the player goes 35 m south of the two, which then go for it: a has two crippled legs, b has none
    local mid = { x = (g1.position.x + g2.position.x) / 2, y = math.min(g1.position.y, g2.position.y) }
    r.teleport{ pos = { mid.x, mid.y - 35 * M, g1.position.z + 200 }, yaw = 0, onGround = true, label = '35 m from them' }
    r.wait(3)
    r.inspect('a', g1)
    r.inspect('b', g2)
    local start1 = { x = g1.position.x, y = g1.position.y, z = g1.position.z }
    local start2 = { x = g2.position.x, y = g2.position.y, z = g2.position.z }
    r.log('L both characters go for the player from 35 m: a has two crippled legs, b has none')
    r.fight(g1, player, 'a -> player')
    r.fight(g2, player, 'b -> player')
    local t = 0
    for i = 1, 8 do
        r.wait(0.5)
        t = t + 0.5
        r.log(string.format('L %.1f s: a has gone %.0f units, b has gone %.0f units', t, distance(g1.position, start1),
            distance(g2.position, start2)))
    end
    r.state('a', g1)
    r.state('b', g2)
    r.peace(g1)
    r.peace(g2)

    ---------------------------------------------------------------- F: a fall
    phase('F fall')
    local function fall(target, label, up)
        local p0 = target.position
        r.teleport{ pos = { p0.x, p0.y - 5 * M, p0.z + 200 }, yaw = 0, onGround = true, label = 'five metres from ' .. label }
        r.wait(1.5)
        local stands = height(target)
        r.log(string.format('F %s stands at height %.0f', label, stands))
        local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(target), PISTOL, -1)
        r.log('F blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit)) or ('error ' .. tostring(hit))))
        r.wait(0.15)
        local p = target.position
        -- (not taken to the ground: it has to be in the air)
        r.putAt(target, { pos = { p.x, p.y, p.z + up }, yaw = math.pi, onGround = false,
            label = label .. ' ' .. up .. ' units up while it is thrown' })
        for i = 1, 8 do
            r.wait(0.5)
            r.log(string.format('F %s %.1f s later at height %.0f (it stood at %.0f)', label, i * 0.5, height(target), stands))
        end
        r.state(label .. ' after the fall', target)
    end
    fall(g2, 'b', 905)
    r.shot('F1_after_fall', false)
    r.peace(g1)
    fall(g1, 'a', 1100)
    r.untrack()
    r.log('PHASE done')
end }
