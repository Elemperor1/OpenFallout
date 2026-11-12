-- M15 round 15D: the radio of New Vegas. Nobody can hear a hidden round: what is checked is what the engine logs ("Radio: ..."
-- and "Music track N plays ..."), with the master volume at 0. The player stands where the save was made (the stations that
-- reach everywhere are in reach at once), the page of the Pip-Boy's radio tab is asked for its stations, one is tuned, and
-- what it says is waited for; then the radio is turned off.
return { limit = 420, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local function stations()
        local ok, list = pcall(function() return r.core.pipboy.radio() end)
        if not ok then
            r.log('pipboy.radio failed: ' .. tostring(list))
            return {}
        end
        return list
    end
    r.god(true)
    phase('A the stations in reach')
    r.wait(8)
    local list = stations()
    for _, station in ipairs(list) do
        r.log('STATION ' .. tostring(station.id) .. ' ' .. tostring(station.name) .. ' tuned ' .. tostring(station.tuned))
    end
    phase('B tune')
    local pick
    for _, station in ipairs(list) do
        if pick == nil or station.name == 'Radio New Vegas' then pick = station end
    end
    if pick == nil then error('no station is in reach: there is nothing to tune') end
    r.log('TUNE ' .. tostring(pick.id) .. ' ' .. tostring(pick.name))
    local ok, result = pcall(function() return r.core.pipboy.tuneRadio(pick.id) end)
    r.log('tuneRadio -> ' .. tostring(ok) .. ' ' .. tostring(result))
    r.wait(150)
    for _, station in ipairs(stations()) do
        r.log('STATION ' .. tostring(station.name) .. ' tuned ' .. tostring(station.tuned))
    end
    phase('C off')
    pcall(function() return r.core.pipboy.tuneRadio(nil) end)
    r.wait(5)
    r.eye('end')
end }
