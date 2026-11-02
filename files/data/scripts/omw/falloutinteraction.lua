-- The screens of the locks, the terminals, and waiting and sleeping in Fallout 3 and New Vegas. What is locked, what a
-- terminal holds and how the hours pass is the engine's (core.interact); this script shows it and gives the choices
-- of the player back: the lock of a door or container with the bobby pins that open it, the menu of a terminal and
-- the game of hacking that opens a closed one, and the hours to wait or sleep.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

-- A message box drawn over the screen (falloutmessage.lua) has the keys, the pad and the wheel
local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end
local pad = require('scripts.omw.falloutpad')
local widgetModule = require('scripts.omw.falloutwidgets')
local menuModule = require('scripts.omw.falloutmenu')
local terminalMenu = require('scripts.omw.falloutterminal')
local hackMenu = require('scripts.omw.falloutterminalhack')

local interact = core.interact
local pipboy = core.pipboy

if not pipboy.available() then
    return {}
end

local v2 = util.vector2
local colors = widgetModule
local textSize = widgetModule.textSize

-- 'lock', 'terminal', 'rest' (the hours are chosen) or 'resting' (the time passes); nil when nothing is shown
local screen = nil
local element = nil
local modeOpen = false
local widgets
-- The screen of the hours to wait or sleep when it is drawn from the game's own menu file (sleep_wait_menu.xml)
local restMenu = nil

-- What the screen was drawn for, so that it is drawn again when that changes
local shownSerial = nil
local shownPins = nil
local shownRestSerial = nil
local shownTenths = nil
-- A line the screen shows besides what the engine says: what the last bobby pin did, why the wait was refused
local notice = ''
local noticeColor = colors.badColor
-- The hours chosen for waiting or sleeping, and the number of rests that had ended when this one began
local hours = 8
local endedBefore = 0

local render

local function update()
    if element then element:update() end
end

local function destroyElement()
    if element then
        element:destroy()
        element = nil
    end
    if restMenu then
        restMenu:close()
        restMenu = nil
    end
end

-- The menus drawn from the files of the terminals (the screen of the menu and the one of the game of hacking) stay while
-- the screen is drawn again for a change of the engine's; they go with the screen
local function closeTerminalMenus()
    terminalMenu.close()
    hackMenu.close()
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

-- What the engine has open is closed, whichever screen it was
local function release()
    if screen == 'lock' then
        interact.closeLock()
    elseif screen == 'terminal' then
        interact.closeTerminal()
    elseif screen == 'rest' or screen == 'resting' then
        interact.cancelRest()
    end
end

-- The player leaves the screen with its button: what was open is closed here, because the mode is no longer ours
-- when the event that tells of it comes
local function leave()
    release()
    destroyElement()
    closeTerminalMenus()
    screen = nil
    shownSerial, shownPins, shownRestSerial, shownTenths = nil, nil, nil, nil
    notice = ''
    closeMode()
end

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function newRows(width)
    local rows = { { props = { size = v2(width, 0) } } }
    local function add(layout)
        rows[#rows + 1] = { props = { size = widgetModule.gap } }
        rows[#rows + 1] = layout
    end
    return rows, add
end

local function widthOf(share, least)
    return math.max(least, math.floor(ui.screenSize().x * share))
end

-- A bobby pin in the lock
local function pickLock()
    local result = interact.pick()
    if result == 'opened' then
        leave()
        return
    elseif result == 'broke' then
        notice = 'Your bobby pin broke.'
    elseif result == 'nopins' then
        notice = 'You have no bobby pins.'
    end
    shownPins = nil
    render()
end

-- The lock of a door or a container
local function renderLock()
    local lock = interact.lock()
    if not lock then
        return
    end
    widgets = widgetModule.new(update)
    local rows, add = newRows(widthOf(0.3, 420))
    add(widgets.label(lock.name ~= '' and lock.name or 'Lock', colors.titleColor, textSize + 4))
    add(widgets.label(lock.keyOnly and lock.word or string.format('%s (%d)', lock.word, lock.level), colors.focusColor))
    if not lock.keyOnly then
        add(widgets.label(string.format('Lockpick %d   Bobby pins %d', math.floor(lock.skill + 0.5), lock.pins),
            colors.hintColor))
        add(widgets.label(string.format('Chance for each pin: %d%%', math.floor(lock.chance * 100 + 0.5)),
            colors.hintColor))
    end
    if notice ~= '' then
        add(widgets.label(notice, noticeColor))
    end
    add(widgets.row(
        widgets.cell(widgets.clickable('Pick the lock', pickLock, { enabled = not lock.keyOnly and lock.pins > 0, color = colors.titleColor }), 200),
        widgets.clickable(word('sLeave', 'Leave'), leave, { color = colors.titleColor })))
    element = ui.create(widgets.panel(rows))
end

-- A line of the menu is chosen, or the player goes back (or leaves from the first menu), on the screen of the game's own
-- file: the screen that follows is typed again
local function chooseLine(index)
    interact.choose(index)
    shownSerial = interact.terminalSerial()
    render()
end

local function goBack()
    interact.back()
    shownSerial = interact.terminalSerial()
    if shownSerial == 0 then
        leave()
    else
        render()
    end
end

-- The menu of a terminal, or the game of hacking it while it is closed
local function renderTerminal()
    local terminal = interact.terminal()
    if not terminal then
        return
    end
    -- Both are drawn from the game's own files; the widgets below stand in for an install that lacks them
    if terminal.hacking then
        terminalMenu.close()
        if hackMenu.start(terminal, { leave = leave }) then
            return
        end
    else
        hackMenu.close()
        if terminalMenu.start(terminal, { choose = chooseLine, back = goBack }) then
            return
        end
    end
    closeTerminalMenus()
    widgets = widgetModule.new(update)
    local width = widthOf(0.5, 560)
    local rows, add = newRows(width)
    if terminal.hacking then
        add(widgets.label('ENTER PASSWORD NOW', colors.titleColor, textSize + 4))
        if terminal.lockedOut then
            add(widgets.label('TERMINAL LOCKED. PLEASE CONTACT AN ADMINISTRATOR.', colors.badColor))
        else
            add(widgets.label(string.format('ATTEMPTS LEFT: %d', terminal.attempts), colors.pickedColor))
        end
        local guessed = {}
        for _, guess in ipairs(terminal.guesses) do guessed[guess.word] = true end
        for index, text in ipairs(terminal.words) do
            local place = index
            add(widgets.clickable(text, function()
                interact.guess(place)
                render()
            end, { enabled = not terminal.lockedOut and not guessed[text] and terminal.attempts > 0 }))
        end
        for _, guess in ipairs(terminal.guesses) do
            add(widgets.label(string.format('%s  Entry denied. Likeness=%d', guess.word, guess.likeness),
                colors.hintColor, widgetModule.textSize - 4))
        end
        add(widgets.clickable(word('sExit', 'Exit'), leave, { color = colors.titleColor }))
    else
        if terminal.name ~= '' then
            add(widgets.label(terminal.name, colors.titleColor, textSize + 4))
        end
        if terminal.note ~= '' then
            -- A note that a line showed: its text, and Back to the menu
            for _, text in ipairs(widgetModule.wrap(terminal.note, width)) do
                add(widgets.label(text, colors.focusColor))
            end
        else
            for _, text in ipairs(widgetModule.wrap(terminal.body, width)) do
                add(widgets.label(text, colors.focusColor))
            end
            for _, line in ipairs(terminal.lines) do
                local index = line.index
                add(widgets.clickable('> ' .. line.text, function()
                    interact.choose(index)
                    render()
                end))
            end
            if terminal.result ~= '' then
                add(widgets.label('> ' .. terminal.result, colors.pickedColor))
            end
        end
        add(widgets.clickable((terminal.canGoBack or terminal.note ~= '') and word('sBack', 'Back') or word('sExit', 'Exit'),
            function()
                if terminal.canGoBack or terminal.note ~= '' then
                    interact.back()
                    render()
                else
                    leave()
                end
            end, { color = colors.titleColor }))
    end
    element = ui.create(widgets.panel(rows))
end

-- The wait or sleep of the hours that were chosen begins
local function startRest()
    local why = interact.startRest(hours)
    if why ~= '' then
        notice = why
        if restMenu then
            -- (the menu of the game has no line for it: the words go to the messages)
            ui.showMessage(why)
            return
        end
    else
        notice = ''
        endedBefore = interact.restProgress().ended
        screen = 'resting'
        shownTenths = nil
    end
    render()
end

local function hoursText(count)
    return string.format('%d %s', count, count == 1 and word('sHour', 'hour') or word('sHours', 'hours'))
end

-- The hours are chosen on the screen of the game's own file: the prompt, the word of the button and the hours are
-- what the game says there, the rest is the file's
local function renderRestMenu(request)
    local menu = menuModule.open('sleep_wait_menu.xml')
    if not menu then return false end
    -- a file that lacks one of the tiles this screen fills is not the game's: the widgets take over
    for _, name in ipairs({ 'SWM_HowManyText', 'SWM_WaitButton', 'SWM_HoursChosen', 'SWM_Scrollbar' }) do
        if not menu:find(name) then
            menu:close()
            return false
        end
    end
    restMenu = menu
    local title = request.sleeping and word('sSleep', 'Sleep') or word('sWait', 'Wait')
    -- (sHowMany is the "how many?" of the quantity screen, not of the hours)
    menu:set('SWM_HowManyText', 'string', word('sHowManyHours', 'How many hours?'))
    menu:set('SWM_WaitButton', 'string', title)
    menu:set('SWM_HoursChosen', 'string', hoursText(hours))
    menu:setScrollbar('SWM_Scrollbar', hours - 1)
    menu:onClick('SWM_WaitButton', function() startRest() end)
    menu:onClick('SWM_CancelButton', function() leave() end)
    return true
end

-- The hours to wait or sleep
local function renderRest(request)
    if renderRestMenu(request) then return end
    widgets = widgetModule.new(update)
    local rows, add = newRows(widthOf(0.3, 420))
    local title = request.sleeping and word('sSleep', 'Sleep') or word('sWait', 'Wait')
    add(widgets.label(title, colors.titleColor, textSize + 4))
    add(widgets.row(
        widgets.cell(widgets.label('Hours', colors.focusColor), 100),
        widgets.cell(widgets.clickable('-', function()
            hours = math.max(1, hours - 1)
            render()
        end, { enabled = hours > 1 }), 30),
        widgets.cell(widgets.label(string.format('%d', hours), colors.pickedColor), 50),
        widgets.clickable('+', function()
            hours = math.min(24, hours + 1)
            render()
        end, { enabled = hours < 24 })))
    if notice ~= '' then
        add(widgets.label(notice, noticeColor))
    end
    add(widgets.row(
        widgets.cell(widgets.clickable(title, startRest, { color = colors.titleColor }), 200),
        widgets.clickable(word('sCancel', 'Cancel'), leave, { color = colors.titleColor })))
    element = ui.create(widgets.panel(rows))
end

-- The time passes
local function renderResting()
    local progress = interact.restProgress()
    widgets = widgetModule.new(update)
    local rows, add = newRows(widthOf(0.3, 420))
    add(widgets.label(progress.sleeping and 'Sleeping...' or 'Waiting...', colors.titleColor, textSize + 4))
    add(widgets.label(string.format('%d of %d hours', math.floor(progress.done + 0.001), math.floor(progress.total + 0.5)),
        colors.focusColor))
    add(widgets.clickable(word('sCancel', 'Cancel'), function()
        interact.cancelRest()
    end, { color = colors.titleColor }))
    element = ui.create(widgets.panel(rows))
end

function render()
    destroyElement()
    if screen ~= 'terminal' then
        closeTerminalMenus()
    end
    if screen == 'lock' then
        renderLock()
    elseif screen == 'terminal' then
        renderTerminal()
    elseif screen == 'rest' then
        local request = interact.rest()
        if request then renderRest(request) end
    elseif screen == 'resting' then
        renderResting()
    end
end

local function show(kind)
    screen = kind
    notice = ''
    openMode()
    render()
end

local function sync()
    if screen == nil then
        -- Something the engine has open that is not on the screen yet: a lock first, then a terminal, then a rest
        local lock = interact.lock()
        if lock then
            shownSerial = lock.serial
            shownPins = lock.pins
            show('lock')
            return
        end
        local terminal = interact.terminalSerial()
        if terminal ~= 0 then
            shownSerial = terminal
            show('terminal')
            return
        end
        local request = interact.rest()
        if request and request.serial ~= shownRestSerial then
            shownRestSerial = request.serial
            show('rest')
        end
        return
    end

    if screen == 'lock' then
        local lock = interact.lock()
        if not lock then
            leave()
        elseif lock.serial ~= shownSerial or lock.pins ~= shownPins then
            shownSerial = lock.serial
            shownPins = lock.pins
            render()
        end
    elseif screen == 'terminal' then
        local serial = interact.terminalSerial()
        if serial == 0 then
            leave()
        else
            if serial ~= shownSerial then
                shownSerial = serial
                render()
            end
            local seconds = core.getRealFrameDuration()
            terminalMenu.update(seconds)
            hackMenu.update(seconds)
        end
    elseif screen == 'rest' then
        if not interact.rest() then
            leave()
        elseif restMenu then
            -- The hours follow the marker, which the pointer, the wheel and the keys move
            local chosen = math.max(1, math.min(24, math.floor(restMenu:get('SWM_Scrollbar', '_current_value') + 1.5)))
            if chosen ~= hours then
                hours = chosen
                restMenu:set('SWM_HoursChosen', 'string', hoursText(hours))
            end
            restMenu:checkSize()
            restMenu:refresh()
        end
    elseif screen == 'resting' then
        local progress = interact.restProgress()
        if not progress.active and progress.ended ~= endedBefore then
            -- It ended: the hours were over, or something stopped it (the line of messages says which)
            leave()
        else
            local tenths = math.floor(progress.done * 4)
            if tenths ~= shownTenths then
                shownTenths = tenths
                render()
            end
        end
    end
end

-- What a key (or a button of the pad that stands for one) does: on the screen of the hours left and right change them
-- and Enter begins the wait; on the screen of a lock Enter puts a bobby pin in
local function press(code)
    -- (a menu opened over the screen takes the keys and the buttons)
    if I.UI.getMode() ~= 'Interface' or messageOpen() then
        return
    end
    if screen == 'rest' then
        if code == input.KEY.LeftArrow or code == input.KEY.RightArrow then
            hours = math.max(1, math.min(24, hours + (code == input.KEY.LeftArrow and -1 or 1)))
            if restMenu then
                restMenu:setScrollbar('SWM_Scrollbar', hours - 1)
                restMenu:set('SWM_HoursChosen', 'string', hoursText(hours))
            else
                render()
            end
        elseif code == input.KEY.Enter then
            startRest()
        end
    elseif screen == 'terminal' then
        if not terminalMenu.press(code) then
            hackMenu.press(code)
        end
    elseif screen == 'lock' and code == input.KEY.Enter then
        local lock = interact.lock()
        if lock and not lock.keyOnly and lock.pins > 0 then
            pickLock()
        end
    end
end

local onPad = pad.handler('interaction',
    function() return (screen == 'rest' or screen == 'lock') and I.UI.getMode() == 'Interface' and not messageOpen() end,
    press)

return {
    engineHandlers = {
        onFrame = sync,
        onKeyPress = function(key)
            press(key.code)
        end,
        onControllerButtonPress = onPad,
    },
    eventHandlers = {
        -- A button of the pad, sent by a script (the buttons of a real pad come as onControllerButtonPress)
        PadButton = function(data)
            onPad(data.button)
        end,
        -- However the screen is left (the button, Escape, a script), what was open is closed
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                release()
                destroyElement()
                closeTerminalMenus()
                screen = nil
                shownSerial, shownPins, shownTenths = nil, nil, nil
                notice = ''
            end
        end,
    },
}
