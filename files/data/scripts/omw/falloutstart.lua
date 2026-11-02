-- The menu that opens with Escape in a game of Fallout 3 or New Vegas, drawn on the game's own menu file
-- (options/start_menu.xml) instead of the engine's buttons. A game that is running opens it as the PAUSE menu; the file's tiles
-- are the ones the game's code (the class StartMenu, read from the executable in round 12V) fades in and out by setting
-- the `_alpha` of their containers: the list of the main menu, the list of saves with the panel of the save that is lit,
-- and the question that asks to be sure. What the buttons do is the engine's (openfallout.menu).
--
-- The list, as the code of the game builds it (with the conditions of the pause menu): Continue (the same as Escape: there
-- is no item for resuming), Save (when the game can be saved), Load (when there is a save), Settings, Quit (which asks).
-- Settings opens the game's own settings screens: the list of the four submenus (Gameplay, Display, Audio, Controls) and
-- the options of the one that is picked, toggles and meters (falloutsettingsitems.lua has the items). A change is made
-- at once, as in the game, and written to the user's file when the screens are left. The list of the submenus ends with a
-- row of the port's own that opens the engine's window of settings, which has what the game's screens do not (the size
-- and the mode of the window, the shaders, the mods). The Help item of the game, and the item that goes to the title
-- screen, are not here (the title screen is the engine's too, because no content, and so no words of the game, is loaded
-- there).
--
-- The script says every frame that it draws the menu (claimMainMenu), so that a script that fails gives it back to the
-- engine, and keeps Escape and the B button for itself in a screen of the menu below the list (they go back one step);
-- in the list the engine closes the menu with them, as the game's menu does.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local engine = require('openfallout.menu')
local storage = require('openfallout.storage')
local menuModule = require('scripts.omw.falloutmenu')
local settingsItems = require('scripts.omw.falloutsettingsitems')

local FILE = 'options/start_menu.xml'
-- The engine's own menu is in a layer of the window manager that the layer of the windows is below, so the menu is drawn
-- in the layer above it (the one the engine's settings window is in). The console's layer is above that one, so the
-- console is still seen over the menu (the layer of the message boxes is above the console's)
local LAYER = 'Settings'
local MAX_SAVES = 1000
-- The row of the port that opens the engine's own window of settings (the game has no such row)
local ENGINE_WINDOW = 'Engine'
-- The room that the file gives the question and its choices: three rows of the height of an item, at the list's side
local CONFIRM_ROWS = 3

local shown = nil
local backdrop = nil
local lists = {}
-- What is on show: 'list' (the main menu), 'save', 'load', 'confirm', 'settings' (the submenus) or 'options' (the items of
-- one submenu)
local screen = 'list'
-- The screen the question was asked from, and what it asks
local confirmFrom = 'list'
local items = {}
local saveRows = {}
local lastSaveRow = 1
-- The submenu that is shown, the items of its rows (the rows are in the order of the items) and whether one was changed
-- since the settings were written
local optionGroup = nil
local optionItems = {}
local optionsChanged = false
local lastGroupRow = 1
-- What the player picked for the items that the engine has no setting for (the permanent storage keeps it)
local optionStore = nil
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

-- The settings are written to the user's file once, when the screens of the settings are left
local function saveOptions()
    if optionsChanged then
        -- kept for the next time the screens are left when the engine could not write them
        optionsChanged = not pcall(engine.saveOptions)
    end
end

local function closeShown()
    pending = nil
    saveOptions()
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
    optionGroup, optionItems = nil, {}
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
        fade('settings_container', name == 'settings')
        fade('options_container', name == 'options')
        -- the question has its own way out (its last choice), so Back is only in the lists of saves and of the settings
        fade('main_back_button', name == 'save' or name == 'load' or name == 'settings' or name == 'options')
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

local showList, showSaves, ask, showSettings, showOptions

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
    items[#items + 1] = { label = word('sSettings'), action = function() showSettings() end }
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

-- The storage of the items that have no setting in the engine
local function store()
    if optionStore == nil then
        local ok, section = pcall(storage.playerSection, 'FalloutOptions')
        optionStore = ok and section or false
    end
    return optionStore or nil
end

-- The traits of a row that change with its value: the word of a toggle (the number of the position of a meter) and which
-- arrows can be used
local function optionTraits(item, index)
    local traits = { user2 = settingsItems.arrows(item, index) }
    if item.values then
        traits.user0 = word(item.values[index + 1])
    else
        traits.user0 = index
    end
    return traits
end

-- The submenus: Gameplay, Display, Audio and Controls, and the row of the port that opens the engine's own window of
-- settings
showSettings = function()
    saveOptions()
    optionGroup, optionItems = nil, {}
    items = {}
    local rows = {}
    for _, group in ipairs(settingsItems.groups) do
        items[#items + 1] = { label = word(group.word), group = group }
        rows[#rows + 1] = { string = word(group.word), user0 = 0 }
    end
    items[#items + 1] = { label = ENGINE_WINDOW, action = function() engine.openSettings() end }
    rows[#rows + 1] = { string = ENGINE_WINDOW, user0 = 0 }
    shown:batch(function()
        shown:set('settings_container', '_number_of_visible_items', #rows)
        lists.settings.rows(rows)
        showScreen('settings')
        lists.settings.select(math.max(1, math.min(lastGroupRow, #rows)), true)
    end)
end

-- The items of a submenu, as toggles and meters in the container of the options; `keepRow` is the row to light
showOptions = function(group, keepRow)
    optionGroup = group
    optionItems = settingsItems.visible(group, engine.controllerConnected())
    local rows = {}
    for index, item in ipairs(optionItems) do
        local position = settingsItems.read(item, engine, store())
        local row = optionTraits(item, position)
        row.string = word(item.word)
        if item.values then
            row._template = 'lb_toggle_template'
        else
            row._template = 'lb_meter_template'
            row.user3 = item.count - 1
        end
        rows[index] = row
    end
    shown:batch(function()
        -- the file keeps where the centre of the values is for each submenu (the code of the game copies it to `_center_x`)
        shown:io('_center_x', shown.menu:getNumber(shown.menu:root(), group.center) or 360)
        lists.options.rows(rows)
        showScreen('options')
        lists.options.select(math.max(1, math.min(keepRow or 1, #rows)), true)
    end)
end

-- Put the item of a row at a position, as the game's code does when the row changes (the engine takes it at once)
local function setOption(row, position)
    local item = optionItems[row]
    if not item then return end
    position = settingsItems.write(item, position, engine, store())
    optionsChanged = true
    lists.options.update(row, optionTraits(item, position))
end

-- A step to the left or the right of the row that is lit (the arrows do not go past the first and the last position),
-- or for `wrap` from the last back to the first (a toggle that is clicked on its value)
local function stepOption(row, step, wrap)
    local item = optionItems[row]
    if not item then return end
    local position = settingsItems.read(item, engine, store()) + step
    if wrap and position >= item.count then position = 0 end
    setOption(row, position)
end

-- What was clicked in a row of the options: an arrow, the value of a toggle, or the bar of a meter (which is set to the
-- position the pointer is at)
local function optionClick(row, tile)
    local item = optionItems[row]
    if not item then return end
    local menu = shown.menu
    local target = tile
    while target do
        local name = menu:name(target)
        if name == 'lb_toggle_left' or name == 'lb_meter_left' then
            return stepOption(row, -1)
        elseif name == 'lb_toggle_right' or name == 'lb_meter_right' then
            return stepOption(row, 1)
        elseif name == 'lb_toggle_value' then
            return stepOption(row, 1, true)
        elseif name == 'lb_meter_hotrect' then
            local rect, pointer = shown:rect(target), shown:pointerAt()
            if rect and pointer and rect.width > 0 then
                local fraction = math.max(0, math.min(1, (pointer.x - rect.x) / rect.width))
                return setOption(row, math.floor(fraction * (item.count - 1) + 0.5))
            end
            return
        end
        target = menu:parent(target)
    end
end

-- Go back one step: from a question to the screen it came from, from a list of saves to the main list, from the options
-- to the submenus and from those to the main list
local function back()
    -- what was picked and not carried out yet (it is, in the next frame) is given up
    pending = nil
    if screen == 'confirm' then
        if confirmFrom == 'list' then showList() else showSaves(confirmFrom, lastSaveRow) end
    elseif screen == 'save' or screen == 'load' then
        showList()
    elseif screen == 'options' then
        showSettings()
    elseif screen == 'settings' then
        saveOptions()
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
    -- a submenu is shown at once; the rest is carried out in the next frame: the engine's calls that save and load are
    -- only allowed there
    if item.group then
        lastGroupRow = row
        showOptions(item.group)
        return
    end
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
        settings = opened:list('settings_container', 'lb_item_template'),
        options = opened:list('options_container', 'lb_toggle_template'),
    }
    if not (made and made.main and made.saves and made.confirm and made.settings and made.options) then
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
    lists.settings.onClick(pickItem)
    lists.options.onClick(optionClick)
    lists.saves.onClick(pickSave)
    shown:onClick('main_back_button', back)
    shown:batch(function()
        shown:set('main_back_button', 'string', word('sBack'))
        -- The Defaults button of the settings (the file shows it with the options) resets the bindings of the controls,
        -- which are not drawn yet, so it is left out: the file's own "additional visibility check" is what says it
        shown:set('main_defaults_button', 'user0', 0)
    end)
    showList()
    return true
end

-- A message box that is open (one that opened before the menu did) has the keys and the buttons of the pad, and so has the
-- console and a dialog of the engine's own windows (a failed save or load): the keys that are typed in them reach the
-- menu's script as well, and so does the key that closes one (the engine says who had the key or the button when it came)
local function inputTaken()
    if engine.inputTaken() then return true end
    local box = core.messageBox
    if box == nil then return false end
    local ok, current = pcall(box.current)
    return ok and current ~= nil
end

local function onKey(key)
    if not shown or inputTaken() then return end
    local code = key.code
    local KEY = input.KEY
    local list = screen == 'list' and lists.main or screen == 'confirm' and lists.confirm
        or screen == 'settings' and lists.settings or screen == 'options' and lists.options or lists.saves
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
    if screen == 'options' then
        -- the lit row changes with the arrows to the sides (a toggle goes round with Enter)
        local at = list.selected()
        if code == KEY.LeftArrow or code == KEY.RightArrow then
            if at > 0 then stepOption(at, code == KEY.LeftArrow and -1 or 1) end
        elseif code == KEY.Enter or code == KEY.Space then
            if at > 0 and optionItems[at] and optionItems[at].values then stepOption(at, 1, true) end
        end
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
    elseif button == PAD.DPadLeft then
        onKey({ code = input.KEY.LeftArrow })
    elseif button == PAD.DPadRight then
        onKey({ code = input.KEY.RightArrow })
    elseif button == PAD.LeftShoulder or button == PAD.RightShoulder then
        -- a meter jumps a fifth of its length (the file's jump value; a toggle has none)
        local at = screen == 'options' and lists.options.selected() or 0
        local item = at > 0 and optionItems[at] or nil
        if item and not item.values then
            local jump = math.max(1, math.floor((item.count - 1) / 5 + 0.5))
            stepOption(at, button == PAD.LeftShoulder and -jump or jump)
        end
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
