-- M16 law (Fallout 3): how the world judges the player, on the real data of the Capital Wasteland. Numbers are logged, never text.
-- Phases: S setup (the open ground outside the main settlement), A1 a blow at a character with nobody about, A2 a blow at a character with a
-- neighbour looking (the character of A1, which is only two in reach), M a character killed with a neighbour looking, T1 an item taken in a house whose owner is not in it, R1 the
-- trespass warnings and the alarm in a house whose owner is in it, R2 an item of that house taken under the owner's eyes.
-- The player has no weapon of its own here, so the blows name the form of the 10mm pistol (as m14p_reactions_fo3 does).
local M = 70
local FO3 = 'Fallout3.esm'
local PISTOL = 'FormId:0x100434f'
local ARENA = { x = -2700, y = -20500, z = 13160 }
-- placed characters of the settlement's outskirts (two citizens of one record, a beggar); the beggar is struck in A2 and killed in M
-- (the first two of the list that are there are taken as victim a and victim b)
local RESIDENTS = { 0x3b26, 0xc6e91, 0x1942f, 0x3b46 }
-- a house whose owner is not in it and a loose item that is the cell's; a house whose owner is in it, and an item
local HOUSE_A, ITEM_A, ENTRY_A = 'MegatonMisterBurkesHouse', 0x45004, { 700, -20, -190 }
local HOUSE_B, OWNER_B, ITEM_B, ENTRY_B = 'MegatonLucyWestsHouse', 0x3b5b, 0x44fd7, { -151, -32, 5 }

return { limit = 800, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { FO3, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local function need(object, what)
        if object == nil then error(what .. ' was not found: the round cannot show anything without it') end
        return object
    end
    -- one line of what the world holds against the player and, for a character, how it stands
    local function snap(label, actor)
        local ok, law = pcall(r.core.combat.law, actor and r.form(actor) or nil)
        local karma = nil
        local okp, stats = pcall(r.core.pipboy.stats)
        if okp and stats then karma = stats.karma end
        if not ok or law == nil then
            r.log('SNAP ' .. label .. ' law error ' .. tostring(law))
            return
        end
        local text = string.format('SNAP %s: karma=%s minor=%s major=%s murderer=%s crimes=%s', label, tostring(karma), tostring(law.minor),
            tostring(law.major), tostring(law.murderer), tostring(law.crimes))
        if actor then
            local okc, fighting = pcall(r.core.combat.inCombat, r.form(actor))
            local okd, dead = pcall(r.core.combat.isDead, r.form(actor))
            text = text .. string.format(' | actor: factions=%s enemy=%s factionMinor=%s factionMajor=%s disposition=%s trespass=%s inCombat=%s dead=%s',
                tostring(law.factions), tostring(law.enemy), tostring(law.factionMinor), tostring(law.factionMajor),
                tostring(law.disposition), tostring(law.trespassLevel), tostring(okc and fighting), tostring(okd and dead))
        end
        r.log(text)
    end
    local function distance(a, b)
        local dx, dy = a.x - b.x, a.y - b.y
        return math.sqrt(dx * dx + dy * dy)
    end

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    r.camera('ThirdPerson')
    r.teleport{ pos = { ARENA.x, ARENA.y, ARENA.z }, yaw = 0, onGround = true, label = 'outside the settlement' }
    r.wait(12)
    r.settle(3, 30)
    r.actors('arena', 3000, 12)
    local found = {}
    for _, id in ipairs(RESIDENTS) do
        if #found < 2 then
            local object = lookup(id)
            if object ~= nil then found[#found + 1] = object end
        end
    end
    if #found < 2 then error('S fewer than two characters: the round needs a victim and another one') end
    -- (only two non-hostile characters are in reach: the first one, already struck once, stands beside the second as the neighbour)
    local va, vb = found[1], found[2]
    local w = va
    local vm = vb
    snap('S start')
    local function blow(label, victim)
        local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(victim), PISTOL, -1)
        r.log(label .. ' blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage)
            .. ' killed=' .. tostring(hit and hit.killed)) or ('error ' .. tostring(hit))))
        return ok and hit or nil
    end

    ---------------------------------------------------------------- A1: a blow at a character with nobody about
    phase('A1 blow, nobody about')
    r.putAt(vb, { pos = { ARENA.x - 300 * M, ARENA.y + 300 * M, ARENA.z }, label = 'victim b far away' })
    r.putAt(va, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'victim a 12 m ahead' })
    r.wait(4)
    r.peace(va)
    snap('A1 before', va)
    blow('A1', va)
    for i = 1, 4 do
        r.wait(1)
        snap('A1 +' .. i .. ' s', va)
    end
    r.peace(va)
    r.putAt(va, { pos = { ARENA.x + 330 * M, ARENA.y - 300 * M, ARENA.z }, label = 'victim a away' })

    ---------------------------------------------------------------- A2: a blow at a character with a neighbour looking
    phase('A2 blow, a neighbour looking')
    r.wait(1.5)
    r.putAt(vb, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'victim b 12 m ahead' })
    r.putAt(w, { pos = { ARENA.x + 6 * M, ARENA.y + 12 * M, ARENA.z + 100 }, yaw = math.pi, label = 'witness beside it' })
    r.wait(4)
    r.peace(vb)
    r.peace(w)
    snap('A2 before victim', vb)
    snap('A2 before witness', w)
    blow('A2', vb)
    for i = 1, 5 do
        r.wait(1)
        snap('A2 +' .. i .. ' s victim', vb)
        snap('A2 +' .. i .. ' s witness', w)
    end
    r.peace(vb)
    r.peace(w)

    ---------------------------------------------------------------- M: a character killed with a neighbour looking
    phase('M murder')
    r.putAt(vm, { pos = { ARENA.x, ARENA.y + 10 * M, ARENA.z + 100 }, yaw = math.pi, label = 'victim m 10 m ahead' })
    r.putAt(w, { pos = { ARENA.x + 6 * M, ARENA.y + 10 * M, ARENA.z + 100 }, yaw = math.pi, label = 'witness beside it' })
    r.wait(4)
    r.peace(vm)
    r.peace(w)
    snap('M before victim', vm)
    snap('M before witness', w)
    for n = 1, 14 do
        blow('M ' .. n, vm)
        local okd, dead = pcall(r.core.combat.isDead, r.form(vm))
        if okd and dead then break end
        r.wait(0.4)
    end
    for i = 1, 4 do
        r.wait(1)
        snap('M +' .. i .. ' s witness', w)
    end
    snap('M after victim', vm)
    r.peace(w)

    ---------------------------------------------------------------- T1: an item taken in a house whose owner is not in it
    phase('T1 theft, nobody about')
    r.teleport{ cell = HOUSE_A, pos = ENTRY_A, yaw = 0, onGround = true, label = 'into the house of the first owner' }
    r.wait(6)
    local item = need(lookup(ITEM_A), 'the item of the first house')
    snap('T1 before')
    r.activate({ FO3, ITEM_A }, 'take the item')
    r.wait(2.5)
    snap('T1 after')
    r.log('T1 ' .. r.describe(item))

    ---------------------------------------------------------------- R1: a house that is someone's, the owner in it
    phase('R1 trespass')
    r.teleport{ cell = HOUSE_B, pos = ENTRY_B, yaw = 0, onGround = true, label = 'into the house of the second owner' }
    r.wait(6)
    local owner = need(lookup(OWNER_B), 'the owner of the second house')
    r.peace(owner)
    snap('R1 entered', owner)
    for i = 1, 24 do
        r.wait(2)
        snap('R1 +' .. 2 * i .. ' s', owner)
        local okc, fighting = pcall(r.core.combat.inCombat, r.form(owner))
        if okc and fighting then break end
    end

    ---------------------------------------------------------------- R2: an item of the house taken under the owner's eyes
    phase('R2 theft in the house')
    r.peace(owner)
    local item2 = need(lookup(ITEM_B), 'an item of the second house')
    snap('R2 before', owner)
    r.activate({ FO3, ITEM_B }, 'take the item of the second house')
    for i = 1, 5 do
        r.wait(1)
        snap('R2 +' .. i .. ' s', owner)
    end
    r.log('R2 ' .. r.describe(item2))
    r.log('PHASE done')
end }
