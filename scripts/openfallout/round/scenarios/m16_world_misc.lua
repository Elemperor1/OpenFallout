-- M16 world: the odd corners asked through the interpreter on real data (core.worldCommands): the map markers shown by
-- ShowAllMapMarkers, the codes, the screen splatters and the shudder, the state of the demigod switch, the sleep and wait
-- menu, a video in a window nobody sees, the terminal that is left, and the questions about the player's actions and
-- controls. It logs numbers and ids only.
return { limit = 240, delay = 4, run = function(r)
    local W = r.core.worldCommands
    local function phase(name) r.log('PHASE ' .. name) end
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
    -- the player as the scripts know it (the Lua id of the object is not a form id)
    local me = W.player()

    phase('A map markers')
    -- A marker of the base game (the first town of New Vegas) and one of a far place
    -- (the numbers carry the index of the plugin: the player's number says which one the base game is)
    local index = tonumber(me:sub(3, 4), 16)
    local markers = {}
    for _, local_ in ipairs({ 0x09289B, 0x0CDF13, 0x13A66A }) do
        markers[#markers + 1] = string.format('0x%02x%06x', index, local_)
    end
    for _, marker in ipairs(markers) do ask('GetMapMarkerVisible', marker) end
    ask('ShowAllMapMarkers', nil, { 1, 1, 0 })
    for _, marker in ipairs(markers) do ask('GetMapMarkerVisible', marker) end
    ask('ShowAllMapMarkers', nil, { 0, 1, 0 })
    for _, marker in ipairs(markers) do ask('GetMapMarkerVisible', marker) end

    phase('B codes')
    ask('IsCodeRedeemed', nil, { 'M16-Code' })
    ask('RedeemCode', nil, { 'M16-Code' })
    ask('IsCodeRedeemed', nil, { 'm16-code' })
    ask('RevokeCode', nil, { 'M16-CODE' })
    ask('IsCodeRedeemed', nil, { 'M16-Code' })

    phase('C screen effects')
    ask('TriggerScreenSplatter', nil, { 4, 2, 1, 1 })
    ask('TriggerScreenBlood', nil, { 3 })
    ask('SetScreenSplatterFade', nil, { 1, 1 })
    ask('SetScreenSplatterTexture', nil, { 'splat.dds', 1 })
    r.wait(3)
    ask('ClearScreenSplatter')
    ask('TriggerHUDShudder')
    r.wait(1)

    phase('D player state')
    ask('GetPlayerControlsDisabled')
    for i = 1, 7 do
        local flags = { 0, 0, 0, 0, 0, 0, 0 }
        flags[i] = 1
        ask('GetPlayerControlsDisabled', nil, flags)
    end
    ask('IsPlayerActionActive', nil, { 1 })
    ask('GetLastPlayerAction')
    ask('IsPC1stPerson')
    ask('IsTimePassing')
    ask('ShowSleepWaitMenu', nil, { 0, 0 })
    r.wait(2)
    ask('IsTimePassing')
    local menu = r.core.menus
    r.log('menus table present=' .. tostring(menu ~= nil))

    phase('E video and terminal')
    ask('PlayBink', nil, { 'none.bik', 1 })
    ask('ForceTerminalBack')
    ask('IsWin32')
    ask('ToggleDemigodMode')
    ask('ToggleDemigodMode')

    phase('F reference state')
    ask('GetStartingPos', me, { 88 })
    ask('GetStartingPos', me, { 89 })
    ask('GetStartingAngle', me, { 90 })
    ask('GetDefaultOpen', me)
    ask('GetParentRef', me)
    ask('IsWaterObject', me)
    ask('IsInDangerousWater', me)
    r.eye('end')
end }
