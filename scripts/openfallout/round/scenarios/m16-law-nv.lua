-- M16 law (New Vegas): how the world judges the player, on the real data. Numbers and form ids are logged, never text.
-- Phases: S setup (out of the doctor's house onto open ground), A1 a blow at a character with nobody about, A2 a blow at a character
-- with a neighbour looking, M a character killed with a neighbour looking, T1 an owned item taken with nobody about (the owner is
-- switched off), T2 another one taken with the owner four metres away, then the player goes into a room that is someone's (cell
-- owner present): R1 the trespass warnings and the alarm, R2 an item of that room taken under the owner's eyes.
local M = 70
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }
local ARENA = { x = -69660, y = 1900, z = 8352 }
-- The gas station (an interior, in whose cell the placed references are only there when the player is): two 9mm round pickups that
-- belong to a character who stands four metres from them
local STATION, STATION_POS = 'GSGasStation', { 700, -5793, 7576 }
local AMMO1, AMMO2, OWNER = 0x10aba7, 0x10aba6, 0x104c7d
-- placed residents of the first settlement
-- (the ones of the list that are there are taken in turn as victim a, victim b (also the one that is killed in M) and the neighbour)
local RESIDENTS = { 0x104f0a, 0x104f03, 0x104f06, 0x104c80 }
-- a room (an interior owned by a character, who is in it) and a loose item in it
local ROOM, ROOM_OWNER, ITEM_IN_ROOM = 'NovacBoonesRoom', 0x96bce, 0xced3d
local ROOM_ENTRY = { 2362, 1352, 8963 }

return { limit = 700, delay = 4, run = function(r)
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
        if #found < 3 then
            local object = lookup(id)
            if object ~= nil then found[#found + 1] = object end
        end
    end
    if #found < 3 then error('S fewer than three of the residents were there: the round needs a victim, a neighbour and another victim') end
    local va, vb, w = found[1], found[2], found[3]
    local vm = vb
    snap('S start')

    ---------------------------------------------------------------- A1: a blow at a character with nobody about
    phase('A1 blow, nobody about')
    r.putAt(va, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z }, yaw = math.pi, label = 'victim a 12 m ahead' })
    r.putAt(w, { pos = { ARENA.x + 250 * M, ARENA.y - 150 * M, ARENA.z }, label = 'witness far away' })
    r.wait(4)
    r.peace(va)
    snap('A1 before', va)
    local ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(va), '', -1)
    r.log('A1 blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage) .. ' killed=' .. tostring(hit and hit.killed))
        or ('error ' .. tostring(hit))))
    for i = 1, 4 do
        r.wait(1)
        snap('A1 +' .. i .. ' s', va)
    end
    r.peace(va)
    r.putAt(va, { pos = { ARENA.x + 300 * M, ARENA.y + 300 * M, ARENA.z }, label = 'victim a away' })

    ---------------------------------------------------------------- A2: a blow at a character with a neighbour looking
    phase('A2 blow, a neighbour looking')
    r.putAt(vb, { pos = { ARENA.x, ARENA.y + 12 * M, ARENA.z }, yaw = math.pi, label = 'victim b 12 m ahead' })
    r.putAt(w, { pos = { ARENA.x + 6 * M, ARENA.y + 12 * M, ARENA.z }, yaw = math.pi, label = 'witness beside it' })
    r.wait(4)
    r.peace(vb)
    r.peace(w)
    snap('A2 before victim', vb)
    snap('A2 before witness', w)
    ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(vb), '', -1)
    r.log('A2 blow: ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage) .. ' killed=' .. tostring(hit and hit.killed))
        or ('error ' .. tostring(hit))))
    for i = 1, 5 do
        r.wait(1)
        snap('A2 +' .. i .. ' s victim', vb)
        snap('A2 +' .. i .. ' s witness', w)
    end
    r.peace(vb)
    r.peace(w)

    ---------------------------------------------------------------- M: a character killed with a neighbour looking
    phase('M murder')
    r.putAt(vm, { pos = { ARENA.x, ARENA.y + 10 * M, ARENA.z }, yaw = math.pi, label = 'victim m 10 m ahead' })
    r.putAt(w, { pos = { ARENA.x + 6 * M, ARENA.y + 10 * M, ARENA.z }, yaw = math.pi, label = 'witness beside it' })
    r.wait(4)
    r.peace(vm)
    r.peace(w)
    snap('M before victim', vm)
    snap('M before witness', w)
    for blow = 1, 12 do
        ok, hit = pcall(r.core.combat.strike, r.form(player), r.form(vm), '', -1)
        local okd, dead = pcall(r.core.combat.isDead, r.form(vm))
        r.log('M blow ' .. blow .. ': ' .. (ok and ('hit=' .. tostring(hit and hit.hit) .. ' damage=' .. tostring(hit and hit.damage)
            .. ' killed=' .. tostring(hit and hit.killed)) or ('error ' .. tostring(hit))) .. ' dead=' .. tostring(okd and dead))
        if okd and dead then break end
        r.wait(0.4)
    end
    for i = 1, 4 do
        r.wait(1)
        snap('M +' .. i .. ' s witness', w)
    end
    snap('M after victim', vm)

    ---------------------------------------------------------------- T1: an owned item taken with nobody about
    phase('T1 theft, nobody about')
    r.peace(w)
    r.teleport{ cell = STATION, pos = STATION_POS, yaw = 0, onGround = true, label = 'into the gas station' }
    r.wait(6)
    local keeper = need(lookup(OWNER), 'the owner of the items')
    local ammo1, ammo2 = need(lookup(AMMO1), 'ammo 1'), need(lookup(AMMO2), 'ammo 2')
    keeper.enabled = false
    r.wait(2)
    snap('T1 before')
    r.activate({ NV, AMMO1 }, 'take ammo 1')
    r.wait(2.5)
    snap('T1 after')
    r.log('T1 ' .. r.describe(ammo1))

    ---------------------------------------------------------------- T2: an owned item taken with the owner four metres away
    phase('T2 theft, owner near')
    keeper.enabled = true
    r.wait(4)
    r.peace(keeper)
    snap('T2 before', keeper)
    r.activate({ NV, AMMO2 }, 'take ammo 2')
    for i = 1, 6 do
        r.wait(1)
        snap('T2 +' .. i .. ' s', keeper)
    end
    r.log('T2 ' .. r.describe(ammo2))
    r.peace(keeper)

    ---------------------------------------------------------------- R1: a room that is someone's, the owner in it
    phase('R1 trespass')
    r.teleport{ cell = ROOM, pos = ROOM_ENTRY, yaw = 0, onGround = true, label = 'into the room' }
    r.wait(6)
    local owner = need(lookup(ROOM_OWNER), 'the owner of the room')
    r.peace(owner)
    snap('R1 entered', owner)
    for i = 1, 24 do
        r.wait(2)
        snap('R1 +' .. 2 * i .. ' s', owner)
        local okc, fighting = pcall(r.core.combat.inCombat, r.form(owner))
        if okc and fighting then break end
    end

    ---------------------------------------------------------------- R2: an item of the room taken under the owner's eyes
    phase('R2 theft in the room')
    r.peace(owner)
    local item = need(lookup(ITEM_IN_ROOM), 'an item of the room')
    snap('R2 before', owner)
    r.activate({ NV, ITEM_IN_ROOM }, 'take the item of the room')
    for i = 1, 5 do
        r.wait(1)
        snap('R2 +' .. i .. ' s', owner)
    end
    r.log('R2 ' .. r.describe(item))
    r.log('PHASE done')
end }
