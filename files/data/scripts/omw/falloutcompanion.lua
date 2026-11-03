-- The wheel of commands of a companion in Fallout 3 and New Vegas, drawn from the game's own menu file
-- (companion_wheel_menu.xml): the player activates a character that follows it and gets eight slices round the screen
-- (be aggressive or passive, use a stimpak, wait or follow, talk, back up, stay close or keep a distance, open its
-- inventory, use ranged or melee weapons). The engine keeps the character and its state (core.pipboy.companion*); this
-- script draws the wheel, tells which slice the pointer is on and gives the choices back.
--
-- The wheel keeps nothing of its own (FalloutNV.exe 0x754DE0): it reads four variables of the companion's script when it
-- opens, each click flips the flag it shows and makes the companion say a topic of the commands, and the results of that
-- response (the plugin's) are what changes the companion. Not there yet: what Back Up does (the executable calls a
-- function that is not read), the fade the wheel closes with.
local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local pad = require('scripts.omw.falloutpad')

local pipboy = core.pipboy

if not (pipboy and pipboy.available() and pipboy.companion) then
    return {}
end

local FILE = 'companion_wheel_menu.xml'
-- The sounds the wheel makes: when it opens, when the pointer comes to another slice, when a slice is clicked
local OPEN_SOUND, HOVER_SOUND, CLICK_SOUND = 'UIPopUpMessageGeneral', 'UIPipBoyScroll', 'UIMenuOK'
-- The size the file gives the picture of the wheel (the rings of the slices are in these units)
local WHEEL_SIZE = 760

-- The slices as the file numbers them: the tile and the word of the setting that says what a click does
local SLICES = {
    [0] = { tile = 'CWM_Button_Aggressive_Passive' },
    [1] = { tile = 'CWM_Button_Use_Stim' },
    [2] = { tile = 'CWM_Button_Stay_Follow' },
    [3] = { tile = 'CWM_Button_Talk_To' },
    [4] = { tile = 'CWM_Button_Back_Up' },
    [5] = { tile = 'CWM_Button_Near_Far' },
    [6] = { tile = 'CWM_Button_Open_Inventory' },
    [7] = { tile = 'CWM_Button_Ranged_Melee' },
}
local SLICE_COUNT = 8

local shown = nil
local modeOpen = false
local shownSerial = nil
-- What the companion's script said when the wheel opened, flipped by the clicks that flip it (the wheel's own flags)
local flags = {}
-- The slice that is lit (nil for none) and where the pointer was when it was last looked at
local hover = nil
local lastPointer = nil
-- The rings and angles of the slices as the file gives them (read when the wheel opens)
local geometry = nil

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function isOpen()
    return shown ~= nil
end

local function openMode()
    if not modeOpen then
        modeOpen = true
        I.UI.addMode('Interface', { windows = {} })
    end
end

local function closeMode()
    if modeOpen then
        modeOpen = false
        I.UI.removeMode('Interface')
    end
end

local function destroyMenu()
    if shown then
        shown:close()
        shown = nil
    end
    hover, lastPointer, geometry = nil, nil, nil
end

-- What a slice says it does now (the word of the setting; the flags that the clicks flipped decide which)
local function labelOf(id)
    if id == 0 then
        return flags.aggressive and word('sCWheelBePassive', 'Be Passive') or word('sCWheelBeAggressive', 'Be Aggressive')
    elseif id == 1 then
        return word('sCWheelUseStimpak', 'Use Stimpak')
    elseif id == 2 then
        return flags.following and word('sCWheelWaitHere', 'Wait Here') or word('sCWheelFollowMe', 'Follow Me')
    elseif id == 3 then
        return word('sCWheelTalkTo', 'Talk To')
    elseif id == 4 then
        return word('sCWheelBackUp', 'Back Up')
    elseif id == 5 then
        return flags.staysFar and word('sCWheelStayClose', 'Stay Close') or word('sCWheelKeepDistance', 'Keep Distance')
    elseif id == 6 then
        return word('sCWheelOpenInventory', 'Open Inventory')
    elseif id == 7 then
        return flags.ranged and word('sCWheelUseMelee', 'Use Melee') or word('sCWheelUseRanged', 'Use Ranged')
    end
    return ''
end

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

-- The line under the wheel for a slice (FalloutNV.exe 0x755DC0): the stimpaks of the player and the health of the
-- companion, what it carries of what it can, the weapon in its hands; the other slices have none
local function contextOf(id)
    if id == 1 then
        local status = pipboy.companionStatus()
        return word('sStimpak', 'Stimpak') .. ' ' .. status.stimpaks .. '  ' .. word('sHitPointsShort', 'HP') .. ' '
            .. number(status.health) .. '/' .. number(status.maxHealth)
    elseif id == 6 then
        local status = pipboy.companionStatus()
        return string.format('%s  %s/%s', word('sInventoryWeight', 'Weight'), number(status.carried),
            number(status.capacity))
    elseif id == 7 then
        return pipboy.companionStatus().weapon
    end
    return ''
end

-- The slices that have two pictures show the second while their flag is set: the flag of a slice that the wheel keeps
-- is the file's `user11`. (The executable gives the slice that waits or follows the flag of the script when the wheel
-- opens and the wheel's own flag after, so does this.)
local function setAlternates(waitingFlag)
    shown:batch(function()
        shown:set(SLICES[0].tile, 'user11', flags.aggressive and 1 or 0)
        shown:set(SLICES[2].tile, 'user11', waitingFlag and 1 or 0)
        shown:set(SLICES[5].tile, 'user11', flags.staysFar and 1 or 0)
        shown:set(SLICES[7].tile, 'user11', flags.ranged and 1 or 0)
    end)
end

-- Light a slice, or none, and write its words under the wheel
local function light(id, silent)
    local changed = id ~= hover
    shown:batch(function()
        if hover ~= nil and SLICES[hover] then
            shown:set(SLICES[hover].tile, 'user10', 0)
        end
        hover = id
        if id ~= nil and SLICES[id] then
            shown:set(SLICES[id].tile, 'user10', 1)
        end
        shown:set('CWM_ButtonPreviewText', 'string', id ~= nil and labelOf(id) or '')
        shown:set('CWM_ButtonContextText', 'string', id ~= nil and contextOf(id) or '')
        if id ~= nil then
            setAlternates(flags.following)
        end
    end)
    if changed and id ~= nil and not silent then
        shown:playSound(HOVER_SOUND)
    end
end

-- The rings and angles the file gives each slice (user2 and user3 are angles in radians clockwise from the top, user4
-- and user5 the nearest and farthest the pointer may be from the middle, in the units of the picture)
local function readGeometry()
    geometry = {}
    for id, slice in pairs(SLICES) do
        local tile = shown:find(slice.tile)
        if tile then
            geometry[id] = {
                from = shown:get(tile, 'user2'),
                to = shown:get(tile, 'user3'),
                near = shown:get(tile, 'user4'),
                far = shown:get(tile, 'user5'),
            }
        end
    end
end

-- The slice a place in the window is on: nil when it is none (the middle with the Exit button, the outside, the two
-- gaps at the top and the bottom). The middle of the wheel is the middle of its picture: the file puts the middle of its
-- rings in the middle of the screen, 55 units lower than the picture, which only matters for a pointer; the buttons of
-- the pad give the direction.
local function sliceAt(x, y)
    if not geometry then return nil end
    local main = shown:find('CWM_MainRect')
    local rect = main and shown:rect(main)
    if not rect or rect.width <= 0 then return nil end
    local unit = rect.width / WHEEL_SIZE
    local dx, dy = x - (rect.x + rect.width / 2), y - (rect.y + rect.height / 2)
    local distance = math.sqrt(dx * dx + dy * dy) / unit
    local angle = math.atan2(dx, -dy)
    if angle < 0 then angle = angle + 2 * math.pi end
    for id, g in pairs(geometry) do
        if distance >= g.near and distance <= g.far and angle >= g.from and angle < g.to then
            return id
        end
    end
    return nil
end

-- Whether a place in the window is on the Exit button in the middle (which a pad does not have)
local function overExit(x, y)
    local tile = shown:find('CWM_Button_Exit')
    if not tile or shown:get(tile, 'visible') == 0 then return false end
    local rect = shown:rect(tile)
    return rect ~= nil and x >= rect.x and x <= rect.x + rect.width and y >= rect.y and y <= rect.y + rect.height
end

local function leave()
    destroyMenu()
    closeMode()
    shownSerial = nil
end

-- A click on a slice: what it does, and the sound
local function activate(id)
    if id == nil then return end
    shown:playSound(CLICK_SOUND)
    if id == 0 then
        flags.aggressive = not flags.aggressive
        pipboy.companionSay(flags.aggressive and 'FollowersTacticsCombatAggressive' or 'FollowersTacticsCombatPassive')
        pipboy.companionSetAggressive(flags.aggressive)
    elseif id == 1 then
        if not pipboy.companionStimpak() then
            print('Companion wheel: the player has no stimpak')
        end
    elseif id == 2 then
        flags.following = not flags.following
        pipboy.companionSay(flags.following and 'FollowersLetsGo' or 'FollowersWait')
    elseif id == 3 then
        leave()
        pipboy.companionTalk()
        return
    elseif id == 4 then
        -- What the executable does here is not read yet: the wheel closes
        print('Companion wheel: Back Up is not done')
        pipboy.closeCompanion()
        leave()
        return
    elseif id == 5 then
        flags.staysFar = not flags.staysFar
        pipboy.companionSay(flags.staysFar and 'FollowersTacticsDistanceLong' or 'FollowersTacticsDistanceDefault')
    elseif id == 6 then
        -- The companion says the topic (its result opens the trade) while the wheel still knows who it is
        pipboy.companionSay('FollowersTrade')
        pipboy.closeCompanion()
        leave()
        return
    elseif id == 7 then
        flags.ranged = not flags.ranged
        pipboy.companionSay(flags.ranged and 'FollowersTacticsCombatRanged' or 'FollowersTacticsCombatMelee')
    end
    -- The wheel stays: its words and pictures are the new state
    light(id, true)
end

local function exitWheel()
    shown:playSound(CLICK_SOUND)
    pipboy.closeCompanion()
    leave()
end

local function openMenu(row)
    local opened = menuModule.open(FILE, { pad = row.pad == true })
    if not opened then return false end
    shown = opened
    readGeometry()
    flags = {
        aggressive = row.aggressive, staysFar = row.staysFar, ranged = row.ranged, following = row.following,
    }
    setAlternates(row.waiting)
    -- A click is on the slice under the pointer, whichever of the stacked tiles of the file took it (the slices are as
    -- large as the wheel, and the Exit button in the middle is under them)
    local function clicked()
        local at = shown:pointerAt()
        if not at then return end
        local id = sliceAt(at.x, at.y)
        if id ~= nil then
            activate(id)
        elseif overExit(at.x, at.y) then
            exitWheel()
        end
    end
    for _, slice in pairs(SLICES) do
        shown:onClick(slice.tile, clicked)
    end
    shown:onClick('CWM_MainRect', clicked)
    shown:onClick('CWM_Button_Exit', clicked)
    shown:playSound(OPEN_SOUND)
    return true
end

local function sync()
    local row = pipboy.companion()
    if not row then
        if shown or modeOpen then
            leave()
        end
        return
    end
    if row.serial ~= shownSerial then
        shownSerial = row.serial
        destroyMenu()
        openMode()
        if not openMenu(row) then
            -- The game's files have no such menu: the player talks to the companion instead
            print('companion wheel: the file ' .. FILE .. ' is not there; the wheel is not shown')
            leave()
            pipboy.companionTalk()
            return
        end
    end
    if shown then
        shown:checkSize()
        -- The slice under the pointer, when the pointer moved
        local at = shown:pointerAt()
        if at and (not lastPointer or lastPointer.x ~= at.x or lastPointer.y ~= at.y) then
            lastPointer = { x = at.x, y = at.y }
            local id = sliceAt(at.x, at.y)
            if id ~= hover then light(id) end
        end
    end
end

-- The keys and the buttons of the pad: the arrows go round the wheel, Enter does what the lit slice does
local function press(code)
    if not shown then return end
    if code == input.KEY.RightArrow or code == input.KEY.DownArrow then
        light(hover == nil and 0 or (hover + 1) % SLICE_COUNT)
    elseif code == input.KEY.LeftArrow or code == input.KEY.UpArrow then
        light(hover == nil and SLICE_COUNT - 1 or (hover - 1) % SLICE_COUNT)
    elseif code == input.KEY.Enter then
        activate(hover)
    end
end

local function onPad(button)
    if not isOpen() or I.UI.getMode() ~= 'Interface' then return end
    local code = pad.key(button)
    if code ~= nil then
        pad.log('companion', button, code)
        press(code)
    end
end

return {
    engineHandlers = {
        onFrame = sync,
        onKeyPress = function(key)
            press(key.code)
        end,
        onControllerButtonPress = onPad,
    },
    eventHandlers = {
        -- However the wheel is left (Escape, the pad's B), what was open is closed
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyMenu()
                pipboy.closeCompanion()
                shownSerial = nil
            end
        end,
    },
}
