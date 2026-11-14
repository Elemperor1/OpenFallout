-- M16 world: the commands about the sky, the clock, line of sight, cells and forms, asked through the interpreter on real
-- data (core.worldCommands runs a command by its name on the frame's thread, as a condition would). The scenario sets two
-- weathers one after the other and reads the weather questions at each point, sets the time multiplier, asks the line of
-- sight between the player and a character near it with the camera on it and turned away, and asks the form types and cell
-- of the player. It logs numbers and ids only.
return { limit = 360, delay = 4, run = function(r)
    local W = r.core.worldCommands
    local function phase(name) r.log('PHASE ' .. name) end

    -- Runs a command and waits for the frame that answers it
    local function ask(name, reference, arguments)
        local ok, id = pcall(W.run, name, reference, arguments)
        if not ok then
            r.log('ASK ' .. name .. ' failed: ' .. tostring(id))
            return nil
        end
        local value
        r.waitFor(function()
            value = W.result(id)
            return value ~= nil
        end, 10, name)
        r.log(string.format('ASK %s %s -> %s', name, reference or '-', tostring(value)))
        return value
    end

    r.god(true)
    r.wait(6)
    local player = r.player()
    -- the player as the scripts know it (the Lua id of the object is not a form id)
    local me = W.player()
    r.log('PLAYER id=' .. me)

    phase('A maths')
    ask('Sin', nil, { 30 })
    ask('Cos', nil, { 60 })
    ask('Tan', nil, { 45 })
    ask('Sqrt', nil, { 144 })
    ask('Log', nil, { 8, 2 })
    ask('Sin', nil, { 0.5, 1 })

    phase('B the machine')
    ask('IsWin32')
    ask('GetDLCCount')
    ask('IsGoreDisabled')
    ask('IsPC1stPerson')
    ask('GetPlayerControlsDisabled')
    ask('PersistentRefCount')
    ask('IsTimePassing')
    ask('WhichServiceMenu')

    phase('C forms and cells')
    ask('GetIsObjectType', me, { 0x2A })
    ask('GetIsFormType', me, { 'NPC_' })
    ask('GetIsFormType', me, { 'DOOR' })
    local cell = player.cell
    r.log('CELL id=' .. tostring(cell.id) .. ' exterior=' .. tostring(cell.isExterior))
    ask('GetInCellParam', me, { tostring(cell.id), me })
    ask('GetStartingPos', me, { 'X' })

    phase('D time')
    r.log('SCALE before=' .. tostring(r.core.getSimulationTimeScale()))
    ask('SetGlobalTimeMultiplier', nil, { 0.5 })
    r.wait(1)
    r.log('SCALE set=' .. tostring(r.core.getSimulationTimeScale()))
    ask('SetGlobalTimeMultiplier')
    r.wait(1)
    r.log('SCALE reset=' .. tostring(r.core.getSimulationTimeScale()))

    phase('E sky')
    local ids = {}
    local okRecords, errRecords = pcall(function()
        local records = r.core.weather.records
        for i = 1, #records do
            local id = tostring(records[i].recordId)
            if id:find('^FormId:') then ids[#ids + 1] = id end
            if #ids >= 2 then break end
        end
    end)
    if not okRecords then r.log('weather records failed: ' .. tostring(errRecords)) end
    r.log('WEATHERS ' .. table.concat(ids, ' '))
    local function sky(label)
        for _, name in ipairs({ 'GetCurrentWeatherPercent', 'IsPleasant', 'IsCloudy', 'IsSnowing', 'GetWindSpeed' }) do
            ask(name)
        end
        for _, id in ipairs(ids) do ask('GetIsCurrentWeather', nil, { id }) end
        r.log('SKY ' .. label)
    end
    sky('start')
    if #ids == 2 then
        ask('SetWeather', nil, { ids[1], 1 })
        r.wait(8)
        sky('first set')
        ask('SetWeather', nil, { ids[2], 1 })
        r.wait(2)
        sky('second set, 2 s in')
        r.wait(40)
        sky('second set, 42 s in')
    end

    phase('F line of sight')
    local near, apart
    for _, actor in ipairs(r.world.activeActors) do
        if actor.id ~= player.id then
            local d = (actor.position - player.position):length()
            if d > 30 and (apart == nil or d < apart) then near, apart = actor, d end
        end
    end
    if near == nil then
        r.log('no character near the player: line of sight is not asked')
    else
        local other = tostring(near.id)
        r.log(string.format('NEAR id=%s dist=%.0f', other, apart))
        r.aimAt(near, 'at it')
        r.wait(1)
        ask('GetLineOfSight', me, { other })
        ask('GetLineOfSight', other, { me })
        local d = near.position - player.position
        r.look(math.atan2(d.x, d.y) + math.pi, 0)
        r.wait(1)
        ask('GetLineOfSight', me, { other })
        ask('SetAllVisible', nil, { 1 })
        ask('GetLineOfSight', other, { me })
        ask('SetAllVisible', nil, { 0 })
    end
    r.eye('end')
end }
