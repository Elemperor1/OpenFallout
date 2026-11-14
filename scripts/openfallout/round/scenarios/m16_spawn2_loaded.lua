-- M16 spawn2 (second round): the game that m16_spawn2_placed.lua saved is loaded and shows the same state. (The round loads the
-- newest save of the profile, which is the one the placement round wrote: run that round first and this one right after it.) The level that the actors a cell places are rolled for, on the real data of Fallout 3 and New Vegas. The plugins
-- of neither game place a reference whose base is a levelled list; their characters and creatures take a levelled list as their
-- template, which the executable resolves once when the actor is made, for the level of its zone (or the player) turned one
-- higher, one lower or not in turn, times the multiplier of the level modifier of the reference. Nobody touches the screen; the
-- report holds numbers and ids, never text of the games.
-- Phases: P1 a cell entered at player level A, P2 another cell of the same game at level B, P3 a third at level C, P4 the first
-- cell again (its levels must not change), P5 the first cell reset (ResetInterior), left and entered again at level C (its
-- levels follow the new level of the player), S the state is saved with core.spawn.quickSave (a second round loads it).
local GAMES = {
    nv = {
        file = 'FalloutNV.esm',
        -- cells[i] = { local id, name for the log, zone's lowest level }, entered in turn at the levels below
        cells = {
            { 0x1206d8, 'HooverDamIntPowerPlant01', 10 },
            { 0xe6a6e, 'Vault11c', 6 },
            { 0x10d629, 'Gomorrah00', 22 },
        },
        levels = { 1, 25, 40 },
        -- list VarGoodspringsPowderGanger joined by VHDGreatKhan1; the zone of the first cell
        list = 0x15f313, added = 0x130c05, zone = 0x131f33,
        -- BoxHull20Ga (its script adds the hulls and calls RemoveMe), ArmorNVPGSimple (its script calls DropMe)
        removeItem = 0x173f82, dropItem = 0xa4731,
    },
    fo3 = {
        file = 'Fallout3.esm',
        cells = {
            { 0x236db, 'CapitolBuilding01', 7 },
            { 0x1a28c, 'MetroCenter01', 5 },
            { 0x1853b, 'RavenRock01', 5 },
        },
        levels = { 1, 20, 35 },
        -- list EncRaiderRandom joined by a character that is in none of the lists; the zone of the first cell
        list = 0x377e6, added = 0x26361, zone = 0x35a6a,
        -- SchematicsRockItLauncherItem (its script calls RemoveMe); no item of the first game's files calls DropMe
        removeItem = 0xbafff,
    },
}

return { limit = 900, delay = 4, run = function(r)
    local core = r.core
    local PLAYER = core.spawn.player()
    local game = 'nv'
    local okFiles, has = pcall(function() return core.contentFiles.has('FalloutNV.esm') end)
    if not (okFiles and has) then game = 'fo3' end
    local G = GAMES[game]
    r.log('GAME ' .. game)
    local function hex(n) return string.format('0x%x', n) end
    local function call(name, reference, args)
        local ok, value = pcall(core.spawn.run, name, reference, args)
        if not ok then r.log('CALL ' .. name .. ' failed: ' .. tostring(value)) return nil end
        return value
    end
    local function phase(name) r.log('PHASE ' .. name) end

    r.god(true)

    -- Enters a cell and tells what the actors in it were rolled for. Returns the table id -> level of the actors with a
    -- levelled template.
    local seen = {}
    local function visit(label, index)
        local cell = G.cells[index]
        local cellId = core.getFormId(G.file, cell[1])
        r.teleport{ cellId = cellId, label = label }
        r.wait(6)
        local total, templated, placed = 0, 0, 0
        local levels, mods, chains = {}, {}, {}
        local lo, hi = nil, nil
        local byMod = {}
        local examples = 0
        for _, actor in ipairs(r.world.activeActors) do
            local id = tostring(actor.id)
            if id ~= tostring(r.world.players[1].id) then
                total = total + 1
                local chain = core.spawn.template(id)
                local placement = core.spawn.placement(id)
                if placement ~= nil then placed = placed + 1 end
                if #chain > 1 and placement ~= nil then
                    templated = templated + 1
                    levels[id] = placement.level
                    lo = lo == nil and placement.level or math.min(lo, placement.level)
                    hi = hi == nil and placement.level or math.max(hi, placement.level)
                    mods[placement.levelMod] = (mods[placement.levelMod] or 0) + 1
                    local m = byMod[placement.levelMod]
                    if m == nil then m = { lo = placement.level, hi = placement.level } byMod[placement.levelMod] = m end
                    m.lo = math.min(m.lo, placement.level)
                    m.hi = math.max(m.hi, placement.level)
                    chains[chain[2]] = true
                    if examples < 3 then
                        examples = examples + 1
                        r.log(string.format('  example %s base=%s template=%s level=%d mod=%d', id, tostring(core.spawn.base(id)),
                            chain[2], placement.level, placement.levelMod))
                    end
                end
            end
        end
        local distinct = 0
        for _ in pairs(chains) do distinct = distinct + 1 end
        local modText = {}
        for mod, m in pairs(byMod) do modText[#modText + 1] = string.format('mod%d:%d..%d(%d)', mod, m.lo, m.hi, mods[mod]) end
        table.sort(modText)
        r.log(string.format('VISIT %s cell=%s zoneMin=%d playerLevel=%s actors=%d withPlacement=%d templated=%d levels=%s..%s distinctTemplates=%d %s',
            label, cell[2], cell[3], tostring(call('GetLevel', PLAYER, {})), total, placed, templated, tostring(lo), tostring(hi), distinct,
            table.concat(modText, ' ')))
        return levels, templated
    end
    local function same(a, b)
        local n, differ = 0, 0
        for id, level in pairs(a) do
            if b[id] ~= nil then
                n = n + 1
                if b[id] ~= level then differ = differ + 1 end
            end
        end
        return n, differ
    end

    local function form(id) return core.getFormId(G.file, id) end
    phase('L')
    r.wait(6)
    local function dump(tag)
        local ids = {}
        for _, actor in ipairs(r.world.activeActors) do
            local id = tostring(actor.id)
            if id ~= tostring(r.world.players[1].id) and core.spawn.placement(id) ~= nil then ids[#ids + 1] = id end
        end
        table.sort(ids)
        for _, id in ipairs(ids) do
            local p = core.spawn.placement(id)
            r.log(string.format('ACT %s %s level=%d mod=%d template=%s', tag, id, p.level, p.levelMod, tostring(core.spawn.template(id)[2])))
        end
        local list, zone = form(G.list), form(G.zone)
        local item = form(G.dropItem or G.removeItem)
        r.log(string.format('STATE %s actors=%d additions=%s zoneNeverResets=%s itemValue=%s', tag, #ids, tostring(core.spawn.additions(list)),
            tostring(core.spawn.zoneNeverResets(zone)), tostring(core.spawn.itemValue(item))))
    end
    dump('after')
    r.eye('end')
end }
