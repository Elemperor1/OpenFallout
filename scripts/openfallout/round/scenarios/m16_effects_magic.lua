-- M16 effects (New Vegas): active effects and what perks change, on the real data. The commands run through the round-only tool
-- debug.scriptCommand, and the values they give are in the engine log ("Command: Name on 0x14 (args) = value"), in the order of the
-- STEP lines of this scenario.
-- Steps: D1 a spell's effects dispelled by spell, D2 two spells dispelled all at once, D3 an item's effect dispelled, E the
-- elapsed seconds of a script effect (an ability that restores health from them), P a perk's entry point changes the radiation of
-- a food.
local NV = 'FalloutNV.esm'
local AV = { strength = 5, perception = 6, health = 16, radiationResistance = 20, radiationLevel = 54 }

return { limit = 360, delay = 6, run = function(r)
    local function id(n) return tostring(r.core.getFormId(NV, n)) end
    local MEAT_SPELL, SAWBONES_SPELL = id(0x164afd), id(0xbd074)
    local STRENGTH_EFFECT, PERCEPTION_EFFECT = id(0x1515c), id(0x1515d)
    local RADX = id(0x15168)
    local IMPLANT_REGEN = id(0x14cce2)
    local LEAD_BELLY, MEAT = id(0x44ca9), id(0x16d5ba)
    local function step(label) r.log('STEP ' .. label .. string.format(' (simulation time %.2f s)', r.core.getSimulationTime())) end
    local function cmd(name, ref, args) r.command(name, ref, args) end
    local function readout(label)
        step(label)
        cmd('GetActorValue', 'player', { AV.strength })
        cmd('GetActorValue', 'player', { AV.perception })
        cmd('HasMagicEffect', 'player', { STRENGTH_EFFECT })
        cmd('HasMagicEffect', 'player', { PERCEPTION_EFFECT })
    end

    r.god(true)
    r.wait(2)

    -- D1: a spell cast on the player, its usage, and what dispelling it by the spell takes back
    readout('D1 before')
    step('D1 cast the spell of four boosts')
    cmd('CastImmediateOnSelf', 'player', { MEAT_SPELL })
    readout('D1 after the cast')
    cmd('GetSpellUsageNum', 'player', { MEAT_SPELL })
    step('D1 dispel by spell')
    cmd('Dispel', 'player', { MEAT_SPELL })
    readout('D1 after the dispel')
    cmd('GetSpellUsageNum', 'player', { MEAT_SPELL })

    -- D2: two spells, then all of them at once
    step('D2 cast two spells')
    cmd('CastImmediateOnSelf', 'player', { MEAT_SPELL })
    cmd('CastImmediateOnSelf', 'player', { SAWBONES_SPELL })
    readout('D2 after the casts')
    step('D2 dispel all spells')
    cmd('DispelAllSpells', 'player')
    readout('D2 after DispelAllSpells')

    -- D3: an aid item: used from the Pip-Boy, its usage, dispelled by the item
    step('D3 aid item')
    cmd('AddItem', 'player', { RADX, 1 })
    cmd('GetActorValue', 'player', { AV.radiationResistance })
    r.pipboyUse(RADX, 'aid item')
    r.wait(0.5)
    cmd('GetActorValue', 'player', { AV.radiationResistance })
    cmd('GetSpellUsageNum', 'player', { RADX })
    step('D3 dispel by the item')
    cmd('Dispel', 'player', { RADX })
    cmd('GetActorValue', 'player', { AV.radiationResistance })
    cmd('GetSpellUsageNum', 'player', { RADX })

    -- E: an ability whose script restores health by the seconds that passed (ScriptEffectElapsedSeconds / 10 each frame)
    step('E damage the health by 30')
    cmd('DamageActorValue', 'player', { AV.health, 30 })
    cmd('GetActorValue', 'player', { AV.health })
    r.wait(5)
    step('E health after 5 s without the ability')
    cmd('GetActorValue', 'player', { AV.health })
    step('E add the ability')
    cmd('AddSpell', 'player', { IMPLANT_REGEN })
    for i = 1, 4 do
        r.wait(5)
        step('E health ' .. (i * 5) .. ' s after the ability')
        cmd('GetActorValue', 'player', { AV.health })
    end
    step('E remove the ability')
    cmd('RemoveSpell', 'player', { IMPLANT_REGEN })

    -- P: the perk 0x44ca9 halves the radiation a food gives (the entry point 44)
    step('P radiation before')
    cmd('GetActorValue', 'player', { AV.radiationLevel })
    cmd('AddItem', 'player', { MEAT, 2 })
    r.pipboyUse(MEAT, 'meat without the perk')
    r.wait(0.5)
    step('P radiation after the meat, no perk')
    cmd('GetActorValue', 'player', { AV.radiationLevel })
    step('P add the perk')
    cmd('AddPerk', 'player', { LEAD_BELLY, 1 })
    cmd('HasPerk', 'player', { LEAD_BELLY })
    r.pipboyUse(MEAT, 'meat with the perk')
    r.wait(0.5)
    step('P radiation after the meat, with the perk')
    cmd('GetActorValue', 'player', { AV.radiationLevel })
    r.wait(1)
end }
