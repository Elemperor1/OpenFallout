-- M16 law2 (Fallout 3): the blows of the player, on the real data. Numbers are logged, never text.
-- Phases: S setup (the open ground outside the main settlement), B a blow at a placed character with nobody about,
-- N the same character struck again with a weapon that has the minor-crime flag (flags2 0x10: a minor crime that only the victim minds),
-- and F the character of the first-wave beggar case struck while a neighbour looks (the friendly-fire rule let that blow pass and the
-- law was never asked; the law is asked now, 0x898E26), and C a blow at a creature (the radroach case of the first birthday scene: the
-- hit routine 0x8987F0 makes no attack crime for one; an owner of it would only be warned).
local M = 70
local FO3 = 'Fallout3.esm'
local PISTOL = 'FormId:0x100434f'
local ARENA = { x = -2700, y = -20500, z = 13160 }
local RESIDENTS = { 0x3b26, 0xc6e91, 0x1942f, 0x3b46 }
-- a placed creature of the settlement's outskirts (a brahmin: it is a creature, whatever its owner); the blow names the pistol
local CREATURE = 0x21491
-- a weapon with the minor-crime flag that does little harm (the Mister Gutsy's plasma gun, which has it too, killed the resident)
local MINOR = 0x4339

return { limit = 500, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local function lookup(id)
        local ok, object = pcall(r.lookup, { FO3, id })
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
    local function blow(label, victim, weapon)
        local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(victim), weapon or PISTOL, -1)
        r.log(label .. ' blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage)
            .. ' killed=' .. tostring(hit and hit.killed)) or ('error ' .. tostring(hit))))
        return ok and hit or nil
    end

    phase('S setup')
    r.god(true)
    r.camera('ThirdPerson')
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'outside the settlement' }
    r.wait(12)
    r.settle(3, 30)
    local found = {}
    for _, id in ipairs(RESIDENTS) do
        if #found < 2 then
            local object = lookup(id)
            if object ~= nil then found[#found + 1] = object end
        end
    end
    if #found < 2 then error('S fewer than two characters') end
    local va, vb = found[1], found[2]
    local minor = tostring(r.core.getFormId(FO3, MINOR))
    snap('S start')

    phase('B a blow at a character with nobody about')
    r.putAt(vb, { pos = { ARENA.x - 330 * M, ARENA.y + 300 * M, ARENA.z }, label = 'resident b away' })
    r.putAt(va, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'resident a 12 m ahead' })
    r.wait(4)
    r.peace(va)
    snap('B before', va)
    blow('B', va)
    for i = 1, 3 do r.wait(1) snap('B +' .. i .. ' s', va) end
    r.peace(va)
    r.putAt(va, { pos = { ARENA.x + 330 * M, ARENA.y - 300 * M, ARENA.z }, label = 'resident a away' })

    phase('N a weapon with the minor-crime flag')
    r.wait(2)
    r.putAt(va, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'resident a 12 m ahead' })
    r.wait(4)
    r.peace(va)
    snap('N before', va)
    blow('N', va, minor)
    for i = 1, 3 do r.wait(1) snap('N +' .. i .. ' s', va) end
    r.peace(va)
    r.putAt(va, { pos = { ARENA.x + 330 * M, ARENA.y - 300 * M, ARENA.z }, label = 'resident a away' })

    phase('F the beggar case: struck with a neighbour looking')
    r.wait(1.5)
    r.putAt(vb, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'resident b 12 m ahead' })
    r.putAt(va, { pos = { ARENA.x + 6 * M, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'neighbour beside it' })
    r.wait(4)
    r.peace(vb)
    r.peace(va)
    snap('F before victim', vb)
    snap('F before neighbour', va)
    blow('F', vb)
    for i = 1, 4 do
        r.wait(1)
        snap('F +' .. i .. ' s victim', vb)
        snap('F +' .. i .. ' s neighbour', va)
    end
    phase('C a blow at a creature')
    -- (the creature stands in a cell that is not loaded at the arena: the player goes to it)
    r.teleport{ pos = { -3200, -24850, 11800 }, yaw = 0, onGround = true, label = 'beside the creature' }
    r.wait(12)
    r.settle(3, 30)
    local creature = lookup(CREATURE)
    if creature ~= nil then
        r.peace(creature)
        snap('C before', creature)
        blow('C', creature)
        for i = 1, 3 do r.wait(1) snap('C +' .. i .. ' s', creature) end
    else
        r.log('C the creature is not available')
    end
    phase('end')
end }
