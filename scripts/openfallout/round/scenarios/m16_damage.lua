-- M16 damage (New Vegas): what happens to a thing that is damaged past its limit. Phases: S setup, D a placed car (MSTT with six
-- destruction stages and a model of the wreck) is damaged through its stages with DamageObject and cleared again, L characters are
-- struck dead and the limbs that the rule of dismemberment took off are read with IsLimbGone, F a script fires a weapon and pushes a
-- character away, E an energy kill: the critical stage and an ash pile. Numbers and ids only.
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }
local ARENA = { x = -69660, y = 1900, z = 8352 }
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3 }
-- Car02 (MSTT, 325 health, six stages): a placed reference in the open
local CAR = { x = -42912, y = 38672, z = 4300, ref = 0x13da2e }
local M = 70

return { limit = 900, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local cmd = r.core.combat.command
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { NV, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local function weapon(id) return r.core.getFormId(NV, id) end
    local PISTOL, LASER = weapon(0x434f), weapon(0x4336)

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    if not exterior() then
        r.activate(DOOR_OUT, 'door out')
        r.waitFor(exterior, 40, 'exterior')
        r.wait(8)
    end
    r.camera('ThirdPerson')

    ---------------------------------------------------------------- D: a car through its stages
    phase('D destruction')
    r.teleport{ pos = { CAR.x, CAR.y - 4 * M, CAR.z + 100 }, yaw = 0, onGround = true, label = 'next to the car' }
    r.wait(12)
    r.settle(3, 30)
    local car = lookup(CAR.ref)
    if car == nil then error('D the car was not found: there is nothing to damage, and a round with no thing must not pass') end
    local id = r.form(car)
    r.log(string.format('D stage before any blow: %s, health %s', tostring(cmd('GetDestructionStage', id)),
        tostring(r.core.combat.destructionHealth(id))))
    r.shot('D1_car_whole', false)
    for i, amount in ipairs({ 30, 40, 60, 80, 200 }) do
        cmd('DamageObject', id, amount)
        r.wait(1.5)
        r.log(string.format('D blow %d of %d: stage %s, health %s, destroyed %s, enabled %s', i, amount,
            tostring(cmd('GetDestructionStage', id)), tostring(r.core.combat.destructionHealth(id)),
            tostring(cmd('GetDestroyed', id)), tostring(car.enabled)))
        if i == 3 or i == 5 then r.shot('D' .. (i - 1) .. '_car_after_blow_' .. i, false) end
    end
    r.log('D after the blows: ' .. r.describe(car))
    cmd('ClearDestruction', id)
    r.wait(2)
    r.log(string.format('D after ClearDestruction: stage %s, destroyed %s', tostring(cmd('GetDestructionStage', id)),
        tostring(cmd('GetDestroyed', id))))
    r.shot('D4_car_cleared', false)

    ---------------------------------------------------------------- L: limbs
    phase('L limbs')
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then error('L the placed gangers were not found') end
    r.putAt(g1, { pos = { ARENA.x - 140, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = 'g1 to the arena' })
    r.putAt(g2, { pos = { ARENA.x + 140, ARENA.y + 20 * M, ARENA.z }, yaw = math.pi, label = 'g2 to the arena' })
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'to the arena' }
    r.wait(8)
    r.settle(3, 30)
    local pid = r.core.combat.player()
    local function limbs(label, who)
        local gone = {}
        for limb = 0, 14 do
            if r.core.combat.isLimbGone(r.form(who), limb) then gone[#gone + 1] = tostring(limb) end
        end
        r.log(string.format('L %s: dead %s, limb mask %d, gone [%s], cause of death %s, IsLimbGone(1) via command %s', label,
            tostring(r.core.combat.isDead(r.form(who))), r.core.combat.limbMask(r.form(who)), table.concat(gone, ','),
            tostring(cmd('GetCauseofDeath', r.form(who))), tostring(cmd('IsLimbGone', r.form(who), 1))))
    end
    local function kill(who, label, limb, weaponForm)
        for blow = 1, 20 do
            local ok, result = pcall(r.core.combat.strike, pid, r.form(who), weaponForm or '', limb)
            if not ok then r.error('L ' .. label .. ' strike error ' .. tostring(result)) break end
            if result.killed then
                r.log(string.format('L %s killed by blow %d at limb %d (crippled %s)', label, blow, limb, tostring(result.crippled)))
                return true
            end
            r.wait(0.15)
        end
        -- (a target that survives every blow fails the round: a note would let it pass)
        r.error('L ' .. label .. ' was not killed in 20 blows')
        return false
    end
    -- the same two characters are killed again and again (a revived character keeps nothing of the limbs: they are forgotten between
    -- the rounds), at different parts of the body, to see what the roll of the rule gives
    local tally = 0
    for round = 1, 4 do
        for k, who in ipairs({ g1, g2 }) do
            r.core.combat.forgetLimbs(r.form(who))
            local limb = ({ 1, 7, 3, 0 })[round]
            local killed = kill(who, string.format('round %d g%d', round, k), limb, PISTOL)
            r.wait(0.5)
            limbs(string.format('round %d g%d', round, k), who)
            if killed and r.core.combat.limbMask(r.form(who)) ~= 0 then tally = tally + 1 end
            if round == 1 and k == 1 then
                r.log('L GetTimeDead ' .. tostring(cmd('GetTimeDead', r.form(who))) .. ', IsKillerObject(player) ' ..
                    tostring(cmd('IsKillerObject', r.form(who), pid)))
            end
            if not (round == 4 and k == 2) then
                cmd('ResurrectActor', r.form(who), 1)
                r.wait(1)
            end
        end
    end
    r.log('L kills that took a limb off: ' .. tally .. ' of 8')
    r.shot('L1_after_kills', false)

    ---------------------------------------------------------------- F: a script fires a weapon, a script pushes a character away
    phase('F fire and push')
    cmd('ResurrectActor', r.form(g2), 1)
    r.wait(1)
    -- g1 stands in the open between the player and the far wall, so the push has room (a throw is stopped by what is in its way)
    r.putAt(g1, { pos = { ARENA.x, ARENA.y + 300, ARENA.z }, yaw = math.pi, label = 'g1 close to the player' })
    r.wait(1)
    r.fight(g2, player, 'g2 -> player')
    r.wait(1)
    r.log('F FireWeapon with the pistol by g2 (it fights the player)')
    cmd('FireWeapon', r.form(g2), PISTOL)
    r.wait(1)
    r.peace(g2)
    local before = { x = g1.position.x, y = g1.position.y, z = g1.position.z }
    r.log(string.format('F g1 before the push at (%.0f, %.0f, %.0f)', before.x, before.y, before.z))
    cmd('PushActorAway', r.form(player), r.form(g1), 120)
    for i = 1, 5 do
        r.wait(0.4)
        local p = g1.position
        r.log(string.format('F g1 %.1f s after the push: (%.0f, %.0f, %.0f)', i * 0.4, p.x, p.y, p.z))
    end

    ---------------------------------------------------------------- E: an energy kill and the ash pile
    phase('E energy kill')
    cmd('ResurrectActor', r.form(g1), 1)
    r.wait(1)
    if not kill(g1, 'laser g1', -1, LASER) then r.error('E the laser kill of g1 did not happen') end
    r.core.combat.forgetLimbs(r.form(g1))
    r.log('E critical stage before: ' .. tostring(cmd('IsInCriticalStage', r.form(g1), 3)))
    cmd('SetCriticalStage', r.form(g1), 3)
    r.log('E in stage 3: ' .. tostring(cmd('IsInCriticalStage', r.form(g1), 3)) .. ', in stage 0: ' ..
        tostring(cmd('IsInCriticalStage', r.form(g1), 0)))
    cmd('AttachAshPile', r.form(g1))
    r.wait(2)
    r.shot('E1_ash_pile', false)
    -- both characters are alive again, and the call kills the ones that are in the loaded cells but not the player
    cmd('ResurrectActor', r.form(g1), 1)
    cmd('ResurrectActor', r.form(g2), 1)
    r.wait(1)
    r.log('E before KillAllActors: g1 dead ' .. tostring(r.core.combat.isDead(r.form(g1))) .. ', g2 dead ' ..
        tostring(r.core.combat.isDead(r.form(g2))))
    cmd('KillAllActors', '')
    r.wait(1.5)
    r.log('E after KillAllActors: g1 dead ' .. tostring(r.core.combat.isDead(r.form(g1))) .. ', g2 dead ' ..
        tostring(r.core.combat.isDead(r.form(g2))) .. ', the player dead ' .. tostring(r.core.combat.isDead(pid)))
end }
