-- M16 spawn: what comes into the world, goes out of it and comes back, on the real data of Fallout 3 and New Vegas. The script
-- commands of the plugins are called through core.spawn.run (the interpreter's own handlers, as a script or a condition calls
-- them); nobody touches the screen and the report holds numbers and ids, never the text of the games.
-- Phases: A characters made from a levelled list at different levels (the zone, the level modifier), B an entry that a script
-- adds to a list is rolled afterwards, C RemoveAllTypedItems takes the items of one kind of an inventory, D ResetInventory,
-- E a character that died comes back when its cell is left for long enough and loaded again.
local GAMES = {
    fo3 = {
        file = 'Fallout3.esm',
        -- EncRaiderGun: entries VarRaider1Gun (level 1), VarRaider2Gun (3), VarRaider3Gun (5)
        list = 0x29627,
        -- encounter zones by the lowest level they say: Vault101Zone 1, Shalebridge01 7, DeathclawSanctuary 14
        zones = { low = 0x35a98, mid = 0x35a93, high = 0x35a70 },
        -- a character that is in none of the lists of EncRaiderRandom and joins it
        added = 0x26361,
        addedList = 0x377e6,
        -- RCSecurityFoodContainer: a container that fills again
        box = 0x1f1d4,
        away = 40000,
    },
    nv = {
        file = 'FalloutNV.esm',
        -- EncVHDGreatKhan: entries at levels 1, 13, 16, 19, 21, 24, 27, 30, 33
        list = 0x130c4c,
        zones = { low = 0x35aaa, mid = 0x131f33, high = 0x10cc90 },
        -- VHDGreatKhan1 joins VarGoodspringsPowderGanger
        added = 0x130c05,
        addedList = 0x15f313,
        -- VendorContainerChet: a container that fills again
        box = 0x104c7b,
        away = 40000,
        doorOut = 0x103e61,
    },
}

return { limit = 900, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local core = r.core
    local PLAYER = core.spawn.player()
    r.log('PLAYER reference ' .. PLAYER)
    local game = 'nv'
    local okFiles, has = pcall(function() return core.contentFiles.has('FalloutNV.esm') end)
    if not (okFiles and has) then game = 'fo3' end
    local G = GAMES[game]
    r.log('GAME ' .. game)
    local function form(id) return core.getFormId(G.file, id) end
    local function hex(n) return string.format('0x%x', n) end
    local function call(name, reference, args)
        local ok, value = pcall(core.spawn.run, name, reference, args)
        if not ok then
            r.log('CALL ' .. name .. ' failed: ' .. tostring(value))
            return nil
        end
        return value
    end
    local function countItems(holder)
        local list = core.spawn.items(holder)
        if list == nil then return nil, nil end
        local kinds, total = 0, 0
        for _, row in ipairs(list) do
            kinds = kinds + 1
            total = total + row.count
        end
        return kinds, total
    end
    local function describeItems(holder)
        local list = core.spawn.items(holder)
        if list == nil then return 'unknown' end
        local parts = {}
        for _, row in ipairs(list) do parts[#parts + 1] = row.id .. 'x' .. tostring(row.count) end
        return table.concat(parts, ' ')
    end
    -- Makes a character from the list and tells what was made: the id scripts know it by, the record the list chose, the
    -- level it was placed at and what it carries
    local function spawn(label, list, mod, zone)
        local args = { form(list), mod or 4 }
        if zone ~= nil then args[3] = form(zone) end
        local made = call('PlaceLeveledActorAtMe', PLAYER, args)
        if made == nil or made == 0 then
            r.log(string.format('SPAWN %s: nothing was made', label))
            return nil
        end
        local id = hex(made)
        r.wait(0.8)
        local exists = call('Exists', id, { 0 })
        local base = core.spawn.base(id)
        local placement = core.spawn.placement(id)
        local kinds, total = countItems(id)
        r.log(string.format('SPAWN %s: id=%s exists=%s base=%s level=%s mod=%s kinds=%s total=%s respawns=%s', label, id,
            tostring(exists), tostring(base), placement and tostring(placement.level) or '-',
            placement and tostring(placement.levelMod) or '-', tostring(kinds), tostring(total),
            tostring(core.spawn.respawns(id))))
        return id
    end

    r.god(true)
    if G.doorOut then
        r.activate({ G.file, G.doorOut }, 'door out')
        r.waitFor(function() local p = r.player(); return p.cell ~= nil and p.cell.isExterior end, 40, 'exterior')
    end
    r.wait(10)
    r.settle(3, 30)
    r.eye('start')
    r.log('PLAYER level ' .. tostring(call('GetLevel', PLAYER, {})))

    ---------------------------------------------------------------- A: the level a character is placed at
    phase('A levels')
    r.log(string.format('A list %s zones low=%s mid=%s high=%s', hex(G.list), hex(G.zones.low), hex(G.zones.mid),
        hex(G.zones.high)))
    local cases = {
        { 'no zone, no modifier', nil, nil },
        { 'low zone, no modifier', 4, G.zones.low },
        { 'mid zone, no modifier', 4, G.zones.mid },
        { 'mid zone, easy', 0, G.zones.mid },
        { 'mid zone, medium', 1, G.zones.mid },
        { 'mid zone, hard', 2, G.zones.mid },
        { 'mid zone, boss', 3, G.zones.mid },
        { 'high zone, no modifier', 4, G.zones.high },
        { 'high zone, boss', 3, G.zones.high },
    }
    local made = {}
    for round = 1, 2 do
        for _, case in ipairs(cases) do
            local id = spawn(string.format('%s #%d', case[1], round), G.list, case[2], case[3])
            if id then made[#made + 1] = id end
        end
    end
    r.log('A made ' .. tostring(#made))

    ---------------------------------------------------------------- B: an entry that a script added
    phase('B added entries')
    local before = {}
    for i = 1, 12 do
        local id = spawn('before ' .. i, G.addedList, 4, G.zones.low)
        if id then before[#before + 1] = core.spawn.base(id) end
    end
    local list = form(G.addedList)
    local addedHex = hex(tonumber(string.match(form(G.added), '0x(%x+)'), 16))
    r.log('B additions before: ' .. tostring(core.spawn.additions(list)))
    -- AddNPCToLeveledList list, actor, level: the character joins the list at level 1
    call('AddNPCToLeveledList', nil, { list, form(G.added), 1 })
    r.log('B additions after: ' .. tostring(core.spawn.additions(list)))
    local hits, total = 0, 0
    for i = 1, 30 do
        local id = spawn('after ' .. i, G.addedList, 4, G.zones.low)
        if id then
            total = total + 1
            if core.spawn.base(id) == addedHex then
                hits = hits + 1
            end
        end
    end
    r.log(string.format('B the added character was chosen %d times in %d', hits, total))

    ---------------------------------------------------------------- C: RemoveAllTypedItems
    phase('C typed removal')
    local holder = spawn('C holder', G.list, 4, G.zones.mid)
    if holder then
        r.log('C before: ' .. describeItems(holder))
        r.log('C player has before: ' .. describeItems(PLAYER))
        -- RemoveAllTypedItems destination, retainOwnership, suppressMessage, formType: weapons (40) go to the player
        call('RemoveAllTypedItems', holder, { PLAYER, 0, 1, 40 })
        r.log('C after weapons: ' .. describeItems(holder))
        r.log('C player has ' .. describeItems(PLAYER))
        -- ammunition (41) goes out of the game
        call('RemoveAllTypedItems', holder, { 0, 0, 1, 41 })
        r.log('C after ammunition: ' .. describeItems(holder))
        -- everything (-1)
        call('RemoveAllTypedItems', holder, { 0, 0, 1, -1 })
        r.log('C after all: ' .. describeItems(holder))
    end

    -- the same command on a container: the aid items (47) and the ammunition (41) go out of the game, the rest stays
    local chest = call('PlaceAtMe', PLAYER, { form(G.box), 1 })
    if chest ~= nil and chest ~= 0 then
        local id = hex(chest)
        r.wait(1)
        local kinds, total = countItems(id)
        r.log(string.format('C container %s before: %s kinds, %s items', id, tostring(kinds), tostring(total)))
        for _, kind in ipairs({ 47, 41 }) do
            call('RemoveAllTypedItems', id, { 0, 0, 1, kind })
            local k, t = countItems(id)
            r.log(string.format('C container after type %d: %s kinds, %s items', kind, tostring(k), tostring(t)))
        end
    end

    ---------------------------------------------------------------- C2: Drop
    phase('C2 drop')
    local dropper = spawn('C2 dropper', G.list, 4, G.zones.mid)
    if dropper then
        local list = core.spawn.items(dropper)
        local pick
        for _, row in ipairs(list or {}) do
            if pick == nil or row.count > pick.count then pick = row end
        end
        if pick then
            local before = core.spawn.count(dropper, pick.id)
            -- Drop item, count: the holder puts the stack into the world (one object with the count, next to it)
            call('Drop', dropper, { pick.id, pick.count })
            r.wait(1.5)
            r.log(string.format('C2 drop of %s x%d: the holder had %d, has %d now', pick.id, pick.count, before,
                core.spawn.count(dropper, pick.id)))
        end
    end

    ---------------------------------------------------------------- D: ResetInventory
    phase('D reset inventory')
    local carrier = spawn('D carrier', G.list, 3, G.zones.high)
    if carrier then
        for i = 1, 3 do
            r.log(string.format('D round %d generation %s: %s', i, tostring(core.spawn.generation(carrier)), describeItems(carrier)))
            call('ResetInventory', carrier, {})
        end
    end

    ---------------------------------------------------------------- E: a character that died comes back
    phase('E respawn')
    local actor
    for i = 1, 12 do
        local id = spawn('E candidate ' .. i, G.list, 4, G.zones.low)
        if id and core.spawn.respawns(id) then
            actor = id
            break
        end
    end
    if actor == nil then
        r.log('E no character that comes back was made: nothing to test')
    else
        r.log('E actor ' .. actor .. ' has the flag that makes it come back')
        call('KillActor', actor, {})
        r.wait(2)
        r.log(string.format('E dead=%s deathHour=%s', tostring(core.spawn.dead(actor)), tostring(core.spawn.deathHour(actor))))
        -- the containers of the cell that fill again: emptied now, looked at when the cell has been left and loaded again
        local boxes = {}
        for i = 1, 2 do
            local made = call('PlaceAtMe', PLAYER, { form(G.box), 1 })
            if made ~= nil and made ~= 0 then
                local id = hex(made)
                r.wait(1)
                local items = core.spawn.items(id)
                r.log(string.format('E container %s placed: respawns=%s kinds=%s', id, tostring(core.spawn.containerRespawns(id)),
                    items and tostring(#items) or 'nil'))
                if items ~= nil and #items > 0 then boxes[#boxes + 1] = { id = id, kinds = #items } end
            end
        end
        for _, box in ipairs(boxes) do
            local removed = call('RemoveAllTypedItems', box.id, { 0, 0, 1, -1 })
            if removed == nil then call('RemoveAllItems', box.id, { 0 }) end
            local kinds, total = countItems(box.id)
            r.log(string.format('E container %s had %d kinds, now %s kinds (%s items), generation %s', box.id, box.kinds,
                tostring(kinds), tostring(total), tostring(core.spawn.generation(box.id))))
        end
        local days = r.global('GameDaysPassed')
        r.log('E clock before ' .. tostring(days))
        local p = r.player()
        local start = p.position
        -- away from the cell for a long stay: far enough that its cell is unloaded
        r.teleport{ pos = { start.x + G.away, start.y, start.z }, onGround = true, label = 'away' }
        r.wait(8)
        r.global('GameDaysPassed', days + 4)
        r.wait(2)
        r.log('E clock after ' .. tostring(r.global('GameDaysPassed')))
        r.teleport{ pos = { start.x, start.y, start.z }, onGround = true, label = 'back' }
        r.wait(10)
        r.log(string.format('E back: dead=%s deathHour=%s generation=%s', tostring(core.spawn.dead(actor)),
            tostring(core.spawn.deathHour(actor)), tostring(core.spawn.generation(actor))))
        for _, box in ipairs(boxes) do
            local kinds, total = countItems(box.id)
            r.log(string.format('E container %s after the reset: %s kinds (%s items), generation %s', box.id, tostring(kinds),
                tostring(total), tostring(core.spawn.generation(box.id))))
        end
    end
    r.eye('end')
end }
