-- M16 law2 (New Vegas): the blows of the player at creatures near the first settlement, on the real data. Numbers are logged, never text.
-- The first wave made an attack crime of every blow at a creature; the hit routine 0x8987F0 makes none. Phases: S setup, C a blow at a
-- creature of the settlement with a resident looking, D a blow at a resident (the control: it is still a crime).
local M = 70
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }
local ARENA = { x = -69660, y = 1900, z = 8352 }
local RESIDENTS = { 0x104f0a, 0x104f03, 0x104f06, 0x104c80 }
-- a placed creature of the settlement's outskirts (a bighorner of the settlement's animal faction)
local CREATURE = 0x107077

return { limit = 400, delay = 4, run = function(r)
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
    local function snap(label, actor)
        local ok, law = pcall(r.core.combat.law, actor and r.form(actor) or nil)
        local okp, stats = pcall(r.core.pipboy.stats)
        local karma = okp and stats and stats.karma or nil
        if not ok or law == nil then r.log('SNAP ' .. label .. ' law error ' .. tostring(law)) return end
        local text = string.format('SNAP %s: karma=%s minor=%s major=%s murderer=%s crimes=%s', label, tostring(karma), tostring(law.minor),
            tostring(law.major), tostring(law.murderer), tostring(law.crimes))
        if actor then
            local okc, fighting = pcall(r.core.combat.inCombat, r.form(actor))
            local okd, dead = pcall(r.core.combat.isDead, r.form(actor))
            text = text .. string.format(' | actor: factions=%s enemy=%s factionMinor=%s factionMajor=%s inCombat=%s dead=%s',
                tostring(law.factions), tostring(law.enemy), tostring(law.factionMinor), tostring(law.factionMajor),
                tostring(okc and fighting), tostring(okd and dead))
        end
        r.log(text)
    end
    local function blow(label, victim)
        local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(victim), '', -1)
        r.log(label .. ' blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage)
            .. ' killed=' .. tostring(hit and hit.killed)) or ('error ' .. tostring(hit))))
    end

    phase('S setup')
    r.god(true)
    r.activate(DOOR_OUT, 'door out')
    r.waitFor(exterior, 40, 'exterior')
    r.wait(8)
    r.equip(nil, 1)
    r.weapon('equipped')
    r.camera('ThirdPerson')
    r.drawn(true)
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'arena' }
    r.wait(10)
    r.settle(3, 30)
    local found = {}
    for _, id in ipairs(RESIDENTS) do
        if #found < 2 then
            local object = lookup(id)
            if object ~= nil then found[#found + 1] = object end
        end
    end
    if #found < 2 then error('S fewer than two of the residents were there') end
    local va, w = found[1], found[2]
    snap('S start', w)

    phase('C a creature near the settlement, a resident looking')
    r.putAt(va, { pos = { ARENA.x + 300 * M, ARENA.y + 300 * M, ARENA.z }, label = 'resident a away' })
    r.putAt(w, { pos = { ARENA.x + 6 * M, ARENA.y + 12 * M, ARENA.z }, yaw = math.pi, label = 'resident beside the creature' })
    local creature = lookup(CREATURE)
    if creature then
        r.putAt(creature, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z }, yaw = math.pi, label = 'creature 12 m ahead' })
    end
    r.wait(4)
    r.peace(w)
    snap('C before resident', w)
    if creature then
        r.peace(creature)
        snap('C before creature', creature)
        blow('C', creature)
        for i = 1, 4 do
            r.wait(1)
            snap('C +' .. i .. ' s resident', w)
            snap('C +' .. i .. ' s creature', creature)
        end
    else
        r.log('C the creature is not available')
    end

    phase('D the control: a blow at a resident')
    r.peace(w)
    r.putAt(va, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z }, yaw = math.pi, label = 'resident a 12 m ahead' })
    r.wait(4)
    r.peace(va)
    snap('D before', va)
    blow('D', va)
    for i = 1, 3 do r.wait(1) snap('D +' .. i .. ' s', va) end
    phase('end')
end }
