-- The menu that opens with Escape in a game of Fallout 3 or New Vegas, drawn on the game's own menu file
-- (options/start_menu.xml) instead of the engine's buttons. A game that is running opens it as the PAUSE menu; the file's tiles
-- are the ones the game's code (the class StartMenu, read from the executable in round 12V) fades in and out by setting
-- the `_alpha` of their containers: the list of the main menu, the list of saves with the panel of the save that is lit,
-- and the question that asks to be sure. What the buttons do is the engine's (openfallout.menu).
--
-- The list, as the code of the game builds it (with the conditions of the pause menu): Continue (the same as Escape: there
-- is no item for resuming), Save (when the game can be saved), Load (when there is a save), Settings, Quit (which asks).
-- Settings opens the engine's own window until the game's settings screens are drawn from the file; the Help item of the
-- game, and the item that goes to the title screen, are not here (the title screen is the engine's too, because no
-- content, and so no words of the game, is loaded there).
--
-- The script says every frame that it draws the menu (claimMainMenu), so that a script that fails gives it back to the
-- engine, and keeps Escape and the B button for itself in a screen of the menu below the list (they go back one step);
-- in the list the engine closes the menu with them, as the game's menu does.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local engine = require('openfallout.menu')
local menuModule = require('scripts.omw.falloutmenu')

local FILE = 'options/start_menu.xml'
-- The engine's own menu is in a layer of the window manager that the layer of the windows is below, so the menu is drawn
-- in the layer above it (the one the engine's settings window is in). The console's layer is above that one, so the
-- console is still seen over the menu (the layer of the message boxes is above the console's)
local LAYER = 'Settings'
local MAX_SAVES = 1000
-- The room that the file gives the question and its choices: three rows of the height of an item, at the list's side
local CONFIRM_ROWS = 3

local shown = nil
local backdrop = nil
local lists = {}
-- What is on show: 'list' (the main menu), 'save', 'load' or 'confirm'
local screen = 'list'
-- The screen the question was asked from, and what it asks
local confirmFrom = 'list'
local items = {}
local saveRows = {}
local lastSaveRow = 1
-- A click is carried out in the next frame, where the engine's calls for saving and loading are allowed
local pending = nil
-- The script cannot draw the menu (the file is not there): the engine's own menu is used and nothing is claimed
local failed = false
local claiming = false
-- The save that the panel shows, so the panel is made again only when it changes
local panelFor = nil

-- What the game's settings say (the name of the setting when the install has none)
local function word(name)
    local ok, text = pcall(core.pipboy.text, name)
    if ok and type(text) == 'string' and text ~= '' then return text end
    return name
end

-- A tile of the menu is shown (its container is faded in) or hidden: the code of the game sets the `_alpha` of a
-- container, and the file shows it when that is more than 0
local function fade(name, on)
    shown:set(name, '_alpha', on and 255 or 0)
    -- A list that is faded in takes the pointer and lights the row it is on; the file keeps it off until the code says
    if name ~= 'main_back_button' then shown:set(name, '_enabled', on and 1 or 0) end
end

local function closeShown()
    pending = nil
    if shown then
        shown:close()
        shown = nil
    end
    if backdrop then
        backdrop:destroy()
        backdrop = nil
    end
    lists = {}
    screen, items, saveRows, panelFor = 'list', {}, {}, nil
end

local function release()
    if claiming then
        engine.claimMainMenu(false)
        claiming = false
    end
end

-- Every save of the game that is loaded or, for `everything`, of every character, the newest first; the number of a save
-- counts from the oldest
local function collectSaves(everything)
    local found = {}
    local function add(directory, saves)
        for file, info in pairs(saves) do
            found[#found + 1] = { dir = directory, file = file, info = info }
        end
    end
    if everything then
        for directory, saves in pairs(engine.getAllSaves()) do add(directory, saves) end
    else
        local directory = engine.getCurrentSaveDir()
        if directory then add(directory, engine.getSaves(directory)) end
    end
    table.sort(found, function(a, b) return (a.info.creationTime or 0) > (b.info.creationTime or 0) end)
    for index, save in ipairs(found) do save.number = #found - index + 1 end
    return found
end

-- The two parts of a row of the list of saves: the prefix (the number of the save, or the abbreviation for the quick
-- and the automatic one) and the place it was made in
local function rowOf(save)
    local file = (save.file or ''):lower()
    local description = save.info.description or ''
    local lowered = description:lower()
    local prefix, location = nil, description
    if file:find('quicksave', 1, true) or lowered == 'quicksave' then
        prefix, location = word('sQuicksaveAbbrev'), lowered == 'quicksave' and '' or description
    elseif file:find('autosave', 1, true) or lowered == 'autosave' then
        prefix, location = word('sAutosaveAbbrev'), lowered == 'autosave' and '' or description
    else
        prefix = string.format('#%04d', save.number)
    end
    return prefix, location
end

-- The time played as days, hours and minutes (the game keeps the text in the save's header)
local function playTime(seconds)
    seconds = math.floor(seconds or 0)
    local days = math.floor(seconds / 86400)
    local hours = math.floor((seconds % 86400) / 3600)
    local minutes = math.floor((seconds % 3600) / 60)
    return string.format('%03d.%02d.%02d', days, hours, minutes)
end

local function showScreen(name)
    screen = name
    shown:batch(function()
        fade('main_container', name == 'list')
        fade('saveload_container', name == 'save' or name == 'load')
        fade('confirm_container', name == 'confirm')
        -- the question has its own way out (its last choice), so Back is only in the lists of saves
        fade('main_back_button', name == 'save' or name == 'load')
    end)
end

local function hidePanel()
    panelFor = nil
    shown:batch(function()
        -- (the version of the save, which New Vegas has a tile for, and the picture of the save are not shown)
        for _, tile in ipairs({ 'saveload_playername', 'saveload_playerlevel', 'saveload_playtime',
            'saveload_screenshot' }) do
            shown:set(tile, 'visible', 0)
        end
    end)
end

local showList, showSaves, ask

-- The main list, with the items that the state of the game allows
showList = function()
    items = {}
    items[#items + 1] = { label = word('sContinue'), action = function() engine.resume() end }
    if engine.canSave() then
        items[#items + 1] = { label = word('sSave'), action = function() showSaves('save') end }
    end
    if #collectSaves(true) > 0 then
        items[#items + 1] = { label = word('sLoad'), action = function() showSaves('load') end }
    end
    items[#items + 1] = { label = word('sSettings'), action = function() engine.openSettings() end }
    items[#items + 1] = {
        label = word('sQuit'),
        action = function()
            ask(word('sConfirmQuit'), 'list', {
                { label = word('sExitGameAffirm'), action = function() engine.quit() end },
                { label = word('sCancel'), action = function() showList() end },
            })
        end,
    }
    local rows = {}
    for index, item in ipairs(items) do rows[index] = { string = item.label, user0 = 0 } end
    shown:batch(function()
        shown:set('main_container', '_number_of_visible_items', #rows)
        lists.main.rows(rows)
        showScreen('list')
        hidePanel()
        lists.main.select(1)
    end)
end

-- The list of saves: a row for a new save (when saving and there is room for one) and a row for each save
showSaves = function(kind, keepRow)
    local saves = collectSaves(kind == 'load')
    saveRows = {}
    local rows = {}
    if kind == 'save' and #saves < MAX_SAVES then
        saveRows[#saveRows + 1] = { new = true }
        rows[#rows + 1] = {
            _template = 'lb_newsave_template',
            string = string.format('%s %u/%d %s', word('sNewSave'), #saves, MAX_SAVES, word('sSavesRemaining')),
        }
    end
    if #saves == 0 and kind == 'load' then
        rows[#rows + 1] = { _template = 'lb_newsave_template', string = word('sNoSaves') }
        saveRows[#saveRows + 1] = { none = true }
    end
    for _, save in ipairs(saves) do
        local prefix, location = rowOf(save)
        saveRows[#saveRows + 1] = { save = save }
        rows[#rows + 1] = { _template = 'lb_saveload_template', user0 = prefix, user1 = location }
    end
    shown:batch(function()
        lists.saves.rows(rows)
        showScreen(kind)
        hidePanel()
        -- The row that was lit before a question (or the first) is lit again and shown: the rows are made again, and the
        -- scrolling of the list stays where it was
        lists.saves.select(math.max(1, math.min(keepRow or 1, #rows)), true)
    end)
    screen = kind
end

-- A question with its choices, in the container of the file; `from` is the screen that Back goes to
ask = function(question, from, choices)
    confirmFrom = from
    items = choices
    local rows = {}
    for index, choice in ipairs(choices) do rows[index] = { string = choice.label, user0 = 0 } end
    shown:batch(function()
        local menu = shown.menu
        local width = menu:getNumber(menu:find('NOGLOW_BRANCH'), 'width') or 0
        local height = menu:getNumber(menu:find('NOGLOW_BRANCH'), 'height') or 0
        local listWidth = menu:getNumber(menu:find('main_container'), 'width') or 375
        local rowHeight = menu:getNumber(menu:root(), '_item_height') or 48
        local count = math.min(#rows, CONFIRM_ROWS)
        -- Where the container is, the code of the game says (the file leaves its place to it): at the side of the screen
        -- that the list of the menu is at, in the middle of the window's height
        shown:set('confirm_container', 'x', menu:getNumber(menu:find('main_container'), 'x') or (width - listWidth))
        shown:set('confirm_container', 'y', (height - count * rowHeight) / 2)
        shown:set('confirm_container', 'width', listWidth)
        shown:set('confirm_container', '_number_of_visible_items', count)
        shown:set('confirm_question', 'string', question)
        lists.confirm.rows(rows)
        showScreen('confirm')
        lists.confirm.select(1)
    end)
end

-- Go back one step: from a question to the screen it came from, from a list of saves to the main list
local function back()
    -- what was picked and not carried out yet (it is, in the next frame) is given up
    pending = nil
    if screen == 'confirm' then
        if confirmFrom == 'list' then showList() else showSaves(confirmFrom, lastSaveRow) end
    elseif screen == 'save' or screen == 'load' then
        showList()
    end
end

-- A save is picked: a new one is made at once, one that is written over (or loaded) asks first
local function pickSave(row)
    local entry = saveRows[row]
    if not entry or entry.none then return end
    local kind = screen
    lastSaveRow = row
    if kind == 'save' then
        local place = ''
        local ok, info = pcall(function() return core.interact.mapInfo() end)
        if ok and type(info) == 'table' then place = info.location or '' end
        local description = place ~= '' and place or word('sSave')
        if entry.new then
            pending = function()
                engine.saveGame(description)
                showList()
            end
        else
            ask(word('sConfirmSave'), kind, {
                { label = word('sYes'), action = function()
                    engine.saveGame(description, entry.save.file)
                    showList()
                end },
                { label = word('sNo'), action = function() showSaves(kind, lastSaveRow) end },
            })
        end
    else
        ask(word('sConfirmLoad'), kind, {
            { label = word('sYes'), action = function() engine.loadGame(entry.save.dir, entry.save.file) end },
            { label = word('sNo'), action = function() showSaves(kind, lastSaveRow) end },
        })
    end
end

-- The choice of the question or the item of the list that was picked
local function pickItem(row)
    local item = items[row]
    if not item then return end
    -- carried out in the next frame: the engine's calls that save and load are only allowed there
    pending = item.action
end

local function updatePanel()
    if screen ~= 'save' and screen ~= 'load' then return end
    local row = lists.saves.selected()
    local entry = row and row > 0 and saveRows[row] or nil
    local save = entry and entry.save or nil
    if save == panelFor then return end
    panelFor = save
    shown:batch(function()
        local on = save ~= nil and 1 or 0
        if save then
            shown:set('saveload_playername', 'string', save.info.playerName or '')
            shown:set('saveload_playerlevel', 'string',
                string.format('%s %s', word('sLevelAbbrev'), tostring(save.info.playerLevel or '')))
            shown:set('saveload_playtime', 'string',
                string.format('%s: %s', word('sPlayTime'), playTime(save.info.timePlayed)))
        end
        shown:set('saveload_playername', 'visible', on)
        shown:set('saveload_playerlevel', 'visible', on)
        shown:set('saveload_playtime', 'visible', on)
    end)
end

local function open()
    -- The paused game shows through the menu, darkened a little: the game draws the pause menu over a picture of its own.
    -- It is made first, so that the element of the menu is above it
    local dark = ui.create {
        layer = LAYER,
        type = ui.TYPE.Image,
        props = {
            relativeSize = util.vector2(1, 1),
            resource = ui.texture { path = 'white' },
            color = util.color.rgb(0, 0, 0),
            alpha = 0.45,
            ignorePointerEvents = true,
        },
    }
    local opened = menuModule.open(FILE, { layer = LAYER })
    local made = opened and {
        main = opened:list('main_container', 'lb_item_template'),
        saves = opened:list('saveload_container', 'lb_saveload_template'),
        confirm = opened:list('confirm_container', 'lb_item_template'),
    }
    if not (made and made.main and made.saves and made.confirm) then
        if opened then
            opened:close()
            print('start menu: the file ' .. FILE .. ' does not have what the menu needs; the engine\'s own menu is used')
        end
        dark:destroy()
        failed = true
        return false
    end
    shown, backdrop, lists = opened, dark, made
    lists.main.onClick(pickItem)
    lists.confirm.onClick(pickItem)
    lists.saves.onClick(pickSave)
    shown:onClick('main_back_button', back)
    shown:batch(function()
        shown:set('main_back_button', 'string', word('sBack'))
    end)
    showList()
    return true
end

-- A message box that is open (one that opened before the menu did) has the keys and the buttons of the pad, and so has the
-- console (the keys that are typed in it reach the menu's script as well)
local function inputTaken()
    if engine.consoleOpen() then return true end
    local box = core.messageBox
    if box == nil then return false end
    local ok, current = pcall(box.current)
    return ok and current ~= nil
end

local function onKey(key)
    if not shown or inputTaken() then return end
    local code = key.code
    local KEY = input.KEY
    local list = screen == 'list' and lists.main or screen == 'confirm' and lists.confirm or lists.saves
    if code == KEY.Escape then
        -- Escape closes the menu from the list (the engine does that); in a screen below it goes back
        if screen ~= 'list' then back() end
        return
    end
    local count = list.count()
    if code == KEY.UpArrow or code == KEY.DownArrow then
        local at = list.selected()
        local step = code == KEY.UpArrow and -1 or 1
        if at == 0 then at = step > 0 and 0 or count + 1 end
        list.select(math.max(1, math.min(count, at + step)), true)
        return
    end
    if code == KEY.Enter or code == KEY.Space then
        local at = list.selected()
        at = at > 0 and at or 1
        if screen == 'save' or screen == 'load' then pickSave(at) else pickItem(at) end
    end
end

-- The buttons of the pad: the engine does not give them to its own menu while the script draws this one, and takes B only
-- in the list (to close the menu), so the pad moves the lit row, picks it, goes back and closes the menu here
local function onPad(button)
    if not shown or inputTaken() then return end
    local PAD = input.CONTROLLER_BUTTON
    if button == PAD.DPadUp then
        onKey({ code = input.KEY.UpArrow })
    elseif button == PAD.DPadDown then
        onKey({ code = input.KEY.DownArrow })
    elseif button == PAD.A then
        onKey({ code = input.KEY.Enter })
    elseif button == PAD.B then
        if screen ~= 'list' then back() end
    elseif button == PAD.Start then
        pending = function() engine.resume() end
    end
end

return {
    engineHandlers = {
        onFrame = function()
            -- The engine's own window for the settings is the menu while it is open
            local function drawnNow()
                return engine.getState() == engine.STATE.Running and not failed and not engine.settingsOpen()
            end
            if pending then
                -- carried out in this frame while the menu is still the one that picked it, given up when it is not
                local action = pending
                pending = nil
                if drawnNow() and engine.mainMenuOpen() then action() end
            end
            local drawn = drawnNow()
            if not drawn then
                closeShown()
                release()
                return
            end
            if engine.mainMenuOpen() then
                if not shown and not open() then
                    release()
                    return
                end
            elseif shown then
                closeShown()
            end
            -- The script says it every frame; Escape and B are its own in a screen below the list
            engine.claimMainMenu(true, shown ~= nil and screen ~= 'list')
            claiming = true
            if shown then
                shown:checkSize()
                updatePanel()
            end
        end,
        onKeyPress = onKey,
        onControllerButtonPress = onPad,
        onStateChanged = function()
            closeShown()
            release()
            failed = false
        end,
    },
}
