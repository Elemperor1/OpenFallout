-- M16 damage (Fallout 3): the checks of m16_damage.lua on the content of the Capital Wasteland. Phases: S setup, D a placed electric box
-- (an activator with destruction stages) is damaged through its stages with DamageObject and cleared again, L two raiders are struck
-- dead at different parts of the body and the limbs the rule of dismemberment took off are read, F a script fires a weapon and pushes a
-- raider away, E an energy kill, the critical stage, an ash pile, and KillAllActors. Numbers and ids only.
local FO3 = 'Fallout3.esm'
local M = 70
-- ElectricBox01 of the Evergreen Mills area: a placed reference in the open
local BOX = { x = -71719, y = -25290, z = 13348, ref = 0x1b462 }
-- Two placed raiders of the Capital Wasteland (cell -3,0, 6 m apart)
local SITE = { x = -10600, y = 1560, z = 14158, a = 0x52367, b = 0x52363 }

return { limit = 900, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local cmd = r.core.combat.command
    local function lookup(id)
        local ok, object = pcall(r.lookup, { FO3, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local function weapon(id) return r.core.getFormId(FO3, id) end
    local PISTOL, LASER = weapon(0x434f), weapon(0x4336)

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    r.camera('ThirdPerson')

    ---------------------------------------------------------------- D: an electric box through its stages
    phase('D destruction')
    r.teleport{ pos = { BOX.x, BOX.y - 4 * M, BOX.z + 300 }, yaw = 0, onGround = true, label = 'next to the box' }
    r.wait(12)
    r.settle(3, 30)
    local box = lookup(BOX.ref)
    if box == nil then error('D the box was not found: there is nothing to damage, and a round with no thing must not pass') end
    local id = r.form(box)
    r.log(string.format('D stage before any blow: %s, health %s', tostring(cmd('GetDestructionStage', id)),
        tostring(r.core.combat.destructionHealth(id))))
    r.shot('D1_box_whole', false)
    for i, amount in ipairs({ 15, 20, 30, 60 }) do
        cmd('DamageObject', id, amount)
        r.wait(1.5)
        r.log(string.format('D blow %d of %d: stage %s, health %s, destroyed %s, enabled %s', i, amount,
            tostring(cmd('GetDestructionStage', id)), tostring(r.core.combat.destructionHealth(id)),
            tostring(cmd('GetDestroyed', id)), tostring(box.enabled)))
        if i == 2 or i == 4 then r.shot('D' .. i .. '_box_after_blow_' .. i, false) end
    end
    r.log('D after the blows: ' .. r.describe(box))
    cmd('ClearDestruction', id)
    r.wait(2)
    r.log(string.format('D after ClearDestruction: stage %s, destroyed %s', tostring(cmd('GetDestructionStage', id)),
        tostring(cmd('GetDestroyed', id))))
    r.shot('D5_box_cleared', false)

    ---------------------------------------------------------------- L: limbs
    phase('L limbs')
    r.teleport{ pos = { SITE.x, SITE.y - 6 * M, SITE.z + 300 }, yaw = 0, onGround = true, label = 'the raiders of cell -3,0' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE.a), lookup(SITE.b)
    if g1 == nil or g2 == nil then error('L the placed raiders were not found') end
    r.peace(g1)
    r.peace(g2)
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
        for blow = 1, 25 do
            local ok, result = pcall(r.core.combat.strike, pid, r.form(who), weaponForm or '', limb)
            if not ok then r.error('L ' .. label .. ' strike error ' .. tostring(result)) break end
            if result.killed then
                r.log(string.format('L %s killed by blow %d at limb %d (crippled %s)', label, blow, limb, tostring(result.crippled)))
                return true
            end
            r.wait(0.15)
        end
        r.log('L ' .. label .. ' was not killed in 25 blows')
        return false
    end
    local tally = 0
    for round = 1, 4 do
        for k, who in ipairs({ g1, g2 }) do
            r.core.combat.forgetLimbs(r.form(who))
            local limb = ({ 1, 7, 3, 0 })[round]
            local killed = kill(who, string.format('round %d r%d', round, k), limb, PISTOL)
            r.wait(0.5)
            limbs(string.format('round %d r%d', round, k), who)
            if killed and r.core.combat.limbMask(r.form(who)) ~= 0 then tally = tally + 1 end
            if round == 1 and k == 1 then
                r.log('L GetTimeDead ' .. tostring(cmd('GetTimeDead', r.form(who))) .. ', IsKillerObject(player) ' ..
                    tostring(cmd('IsKillerObject', r.form(who), pid)))
            end
            if not (round == 4 and k == 2) then
                cmd('ResurrectActor', r.form(who), 1)
                r.peace(who)
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
    r.fight(g2, player, 'r2 -> player')
    r.wait(1)
    r.log('F FireWeapon with the pistol by raider b (it fights the player)')
    cmd('FireWeapon', r.form(g2), PISTOL)
    r.wait(1)
    r.peace(g2)
    -- the player stands 3 m south of raider a, so the push goes north along the camp
    local at = g1.position
    r.teleport{ pos = { at.x, at.y - 3 * M, at.z + 200 }, yaw = 0, onGround = true, label = '3 m from raider a' }
    r.wait(1.5)
    local before = { x = g1.position.x, y = g1.position.y, z = g1.position.z }
    r.log(string.format('F r1 before the push at (%.0f, %.0f, %.0f)', before.x, before.y, before.z))
    cmd('PushActorAway', r.form(player), r.form(g1), 120)
    for i = 1, 5 do
        r.wait(0.4)
        local p = g1.position
        r.log(string.format('F r1 %.1f s after the push: (%.0f, %.0f, %.0f)', i * 0.4, p.x, p.y, p.z))
    end

    ---------------------------------------------------------------- E: an energy kill, the ash pile, KillAllActors
    phase('E energy kill')
    cmd('ResurrectActor', r.form(g1), 1)
    r.wait(1)
    if not kill(g1, 'laser r1', -1, LASER) then r.error('E the laser kill of r1 did not happen') end
    r.core.combat.forgetLimbs(r.form(g1))
    r.log('E critical stage before: ' .. tostring(cmd('IsInCriticalStage', r.form(g1), 3)))
    cmd('SetCriticalStage', r.form(g1), 3)
    r.log('E in stage 3: ' .. tostring(cmd('IsInCriticalStage', r.form(g1), 3)) .. ', in stage 0: ' ..
        tostring(cmd('IsInCriticalStage', r.form(g1), 0)))
    cmd('AttachAshPile', r.form(g1))
    r.wait(2)
    r.shot('E1_ash_pile', false)
    cmd('ResurrectActor', r.form(g1), 1)
    cmd('ResurrectActor', r.form(g2), 1)
    r.wait(1)
    r.log('E before KillAllActors: r1 dead ' .. tostring(r.core.combat.isDead(r.form(g1))) .. ', r2 dead ' ..
        tostring(r.core.combat.isDead(r.form(g2))))
    cmd('KillAllActors', '')
    r.wait(1.5)
    r.log('E after KillAllActors: r1 dead ' .. tostring(r.core.combat.isDead(r.form(g1))) .. ', r2 dead ' ..
        tostring(r.core.combat.isDead(r.form(g2))) .. ', the player dead ' .. tostring(r.core.combat.isDead(pid)))
end }
