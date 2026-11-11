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
-- and the mode of the window, the shaders, the mods). Help opens the manual (falloutmanual.lua, on the game's own
-- tutorial_menu.xml) when the data has one; the item that goes to the title screen is not here.
--
-- The same file is the TITLE screen (the game's code makes the menu in the title mode when no game is loaded: a list of
-- Continue, New, Load, Settings, Credits and Quit over the picture of the main menu, with the game's logo): this script
-- draws it when no game is running, from the same tiles, with the words of the executable (the title list's words are
-- compiled into it, and the data of the port has been loaded by then). The picture behind the list turns as the game's does
-- (a new one every ten seconds, fading in over the one before, from the main menu's load screens that the engine picks:
-- every picture once, at most 20), and the music of the title begins three seconds after the screen comes. Not here: the
-- row of the downloadable content.
--
-- The script says every frame that it draws the menu (claimMainMenu), so that a script that fails gives it back to the
-- engine, and keeps Escape and the B button for itself in a screen of the menu below the list (they go back one step);
-- in the list the engine closes the menu with them, as the game's menu does.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local vfs = require('openfallout.vfs')
local ambient = require('openfallout.ambient')
local engine = require('openfallout.menu')
local storage = require('openfallout.storage')
local menuModule = require('scripts.omw.falloutmenu')
local settingsItems = require('scripts.omw.falloutsettingsitems')
local manual = require('scripts.omw.falloutmanual')
local credits = require('scripts.omw.falloutcredits')
local pad = require('scripts.omw.falloutpad')

local FILE = 'options/start_menu.xml'
-- The engine's own menu is in a layer of the window manager that the layer of the windows is below, so the menu is drawn
-- in the layer above it (the one the engine's settings window is in). The console's layer is above that one, so the
-- console is still seen over the menu (the layer of the message boxes is above the console's)
local LAYER = 'Settings'
-- The picture behind the title list: the first picture of the main menu (sMainMenuBackground, main_background)
local BACKDROP = 'textures/interface/main/main_background.dds'
-- The pictures turn behind the list: how long one stays before the next begins to come, and how long that takes (the
-- executable's 10 seconds, and 1 / 0.00045 ms)
local PICTURE_SECONDS, PICTURE_FADE = 10, 1 / 0.45
-- The music of the title (sTitleMusic), and the seconds after the screen comes that it begins
local TITLE_MUSIC, MUSIC_DELAY = 'music/special/maintitle.mp3', 3
local MAX_SAVES = 1000
-- The row of the port that opens the engine's own window of settings (the game has no such row)
local ENGINE_WINDOW = 'Engine'
-- The room that the file gives the question and its choices: three rows of the height of an item, at the list's side
local CONFIRM_ROWS = 3

local shown = nil
local backdrop = nil
-- The picture that fades in over the backdrop, the pictures that turn (paths, the backdrop's own first) and which of them
-- the backdrop shows, the real time that the one on show came at, and the real time the title screen came at
local fader = nil
local slides, slideIndex, slideAt, titleAt, fading = {}, 1, nil, nil, nil
-- The music of the title: nil before it begins, 'playing', or 'none' when the install has no such file
local music = nil
local lists = {}
-- What is on show: 'list' (the main menu), 'save', 'load', 'confirm', 'settings' (the submenus), 'options' (the items of
-- one submenu), 'help' (the manual) or 'credits'
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
-- The real time of the last frame of the credits
local creditsAt = nil
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

-- No game is running, or the game is over (the engine ends it when the player dies, and the death box's Main Menu opens
-- the menu then): the menu is the title screen, not the pause menu
local function title()
    local state = engine.getState()
    return state == engine.STATE.NoGame or state == engine.STATE.Ended
end

local function closeShown()
    pending = nil
    manual.close()
    credits.close()
    saveOptions()
    if shown then
        shown:close()
        shown = nil
    end
    if backdrop then
        backdrop:destroy()
        backdrop = nil
    end
    if fader then
        fader:destroy()
        fader = nil
    end
    slides, slideIndex, slideAt, titleAt, fading = {}, 1, nil, nil, nil
    lists = {}
    screen, items, saveRows, panelFor = 'list', {}, {}, nil
    optionGroup, optionItems = nil, {}
end

-- The music of the title stops when a game begins (or the title goes)
local function stopTitleMusic()
    if music == 'playing' then pcall(ambient.stopMusic) end
    -- (not nil: a frame of the title that comes before the game does must not begin the music again)
    music = 'none'
end

local function startTitleMusic()
    local ok, present = pcall(vfs.fileExists, TITLE_MUSIC)
    if ok and present and pcall(ambient.streamMusic, TITLE_MUSIC, { fadeOut = 1 }) then
        music = 'playing'
    else
        music = 'none'
    end
end

-- The pictures of the title screen: the backdrop, then the pictures of the main menu that the engine picks (those that
-- the install has), each file once
local function titlePictures()
    local list = { BACKDROP }
    local seen = { [BACKDROP] = true }
    local ok, picked = pcall(engine.mainMenuPictures)
    if not ok or type(picked) ~= 'table' then return list end
    for _, icon in ipairs(picked) do
        local path = ('textures/' .. tostring(icon)):gsub('\\', '/'):lower()
        local found, present = pcall(vfs.fileExists, path)
        if not seen[path] and found and present then
            seen[path] = true
            list[#list + 1] = path
        end
    end
    return list
end

-- The picture behind the list turns, by real time (nothing runs with no game): the next picture fades in over the
-- backdrop and, when it is all there, becomes the backdrop
local function turnPictures(now)
    if not backdrop or not fader or #slides < 2 then return end
    slideAt = slideAt or now
    local late = now - slideAt - PICTURE_SECONDS
    if late < 0 then return end
    local nextIndex = slideIndex % #slides + 1
    if late >= PICTURE_FADE then
        backdrop.layout.props.resource = ui.texture { path = slides[nextIndex] }
        backdrop:update()
        fader.layout.props.alpha = 0
        fader:update()
        slideIndex, fading = nextIndex, nil
        -- (the next one begins ten seconds after this one did; after a long frame the schedule is not left behind, which
        -- would make the pictures go by one after the other)
        slideAt = math.max(slideAt + PICTURE_SECONDS, now - PICTURE_FADE)
        return
    end
    if fading ~= nextIndex then
        fading = nextIndex
        fader.layout.props.resource = ui.texture { path = slides[nextIndex] }
    end
    fader.layout.props.alpha = late / PICTURE_FADE
    fader:update()
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

-- The time played as hours, minutes and seconds, "%03i.%02i.%02i" (the game writes the text into the save's header when
-- it saves: hours are the milliseconds over 3600000, minutes and seconds the rest; Mac round 12BN B2)
local function playTime(seconds)
    seconds = math.floor(seconds or 0)
    local hours = math.floor(seconds / 3600)
    local minutes = math.floor((seconds % 3600) / 60)
    return string.format('%03d.%02d.%02d', hours, minutes, seconds % 60)
end

local function hidePanel()
    panelFor = nil
    shown:batch(function()
        -- (the version of the save, which New Vegas has a tile for, and the picture of the save are not shown)
        for _, tile in ipairs({ 'saveload_playername', 'saveload_playerlevel', 'saveload_playtime',
            'saveload_screenshot' }) do
            shown:set(tile, 'visible', 0)
        end
        -- no save is lit, so the Delete button cannot be clicked (the code of the game sets its `_target` trait)
        shown:set('main_defaults_button', '_target', 0)
    end)
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
        -- the button that is Defaults with the options is Delete with the saves (the file shows it when its code says so)
        shown:set('main_defaults_button', 'user0', (name == 'save' or name == 'load') and 1 or 0)
    end)
    -- the panel of the save that is lit goes with the list of saves (the file shows it only while that is shown): the
    -- question that comes after a pick is not drawn over it
    if name ~= 'save' and name ~= 'load' then hidePanel() end
end

local showList, showSaves, ask, showSettings, showOptions, showManual, showCredits

-- The row that quits, which asks first
local function quitItem()
    return {
        label = word('sQuit'),
        action = function()
            ask(word('sConfirmQuit'), 'list', {
                { label = word('sExitGameAffirm'), action = function() engine.quit() end },
                { label = word('sCancel'), action = function() showList() end },
            })
        end,
    }
end

-- A question that asks to be sure of what a row of the title list does: Yes carries it out, No goes back to the list
local function sure(question, yes)
    return function()
        ask(question, 'list', {
            { label = word('sYes'), action = yes },
            { label = word('sNo'), action = function() showList() end },
        })
    end
end

-- Whether the data has the credits (the row is there only when it does, as the game's credits need their file)
local creditsFile = nil

-- The list of the title screen (the mask 1 of the game's items, in its order): Continue and Load only when there is a save
-- (the game scans its folder of saves for them), Continue asks first and loads the newest save, New asks and starts a game
local function titleItems()
    local saves = collectSaves(true)
    local list = {}
    if #saves > 0 then
        list[#list + 1] = {
            label = word('sContinue'),
            action = sure(word('sConfirmContinue'), function() stopTitleMusic(); engine.loadGame(saves[1].dir, saves[1].file) end),
        }
    end
    list[#list + 1] = { label = word('sNew'), action = sure(word('sConfirmNew'), function() stopTitleMusic(); engine.newGame() end) }
    if #saves > 0 then
        list[#list + 1] = { label = word('sLoad'), action = function() showSaves('load') end }
    end
    list[#list + 1] = { label = word('sSettings'), action = function() showSettings() end }
    if creditsFile then
        list[#list + 1] = { label = word('sCrew'), action = function() showCredits() end }
    end
    list[#list + 1] = quitItem()
    return list
end

-- The list of the pause menu: Continue (the same as Escape: there is no item for resuming), Save when the game can be
-- saved, Load when there is a save, Settings, Help when the data has a manual, Quit
local function pauseItems()
    local list = {}
    list[#list + 1] = { label = word('sContinue'), action = function() engine.resume() end }
    if engine.canSave() then
        list[#list + 1] = { label = word('sSave'), action = function() showSaves('save') end }
    end
    if #collectSaves(true) > 0 then
        list[#list + 1] = { label = word('sLoad'), action = function() showSaves('load') end }
    end
    list[#list + 1] = { label = word('sSettings'), action = function() showSettings() end }
    -- Help is there when the data has a manual (the game greys it when its tutorial menu does not exist)
    local hasManual, pages = pcall(core.pipboy.helpManual, false)
    if hasManual and type(pages) == 'table' and #pages > 0 then
        list[#list + 1] = { label = word('sHelp'), action = function() showManual() end }
    end
    list[#list + 1] = quitItem()
    return list
end

-- The main list, with the items that the state of the game allows
showList = function()
    items = title() and titleItems() or pauseItems()
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

-- Help: the manual (the game's tutorial menu) is shown over the menu; leaving it brings the list back
showManual = function()
    local function open(pad)
        return manual.show({
            layer = LAYER,
            pad = pad,
            onLeave = function() screen = 'list' end,
        })
    end
    -- The list shows Help when the PC manual exists; a pad whose manual is not in the data reads the PC one
    local pad = engine.controllerConnected()
    local opened = open(pad)
    if not opened and pad then opened = open(false) end
    if opened then screen = 'help' end
end

-- Credits: the credits of the game over the title screen; the end of them (or Escape, or a click) brings the list back
showCredits = function()
    if credits.show({ layer = LAYER, onDone = function() screen = 'list' end }) then screen = 'credits' end
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
    elseif screen == 'help' then
        manual.leave()
    elseif screen == 'credits' then
        credits.close()
        screen = 'list'
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
            { label = word('sYes'), action = function() stopTitleMusic(); engine.loadGame(entry.save.dir, entry.save.file) end },
            { label = word('sNo'), action = function() showSaves(kind, lastSaveRow) end },
        })
    end
end

-- The Delete button of the lists of saves (the game's StartMenu asks first, with the question of its setting sConfirmDelete,
-- and takes the save away when the answer is yes; the list shows what is left)
local function askDelete()
    local kind = screen
    if kind ~= 'save' and kind ~= 'load' then return end
    local row = lists.saves.selected()
    local entry = row and row > 0 and saveRows[row] or nil
    if not (entry and entry.save) then return end
    lastSaveRow = row
    ask(word('sConfirmDelete'), kind, {
        { label = word('sYes'), action = function()
            engine.deleteGame(entry.save.dir, entry.save.file)
            showSaves(kind, lastSaveRow)
        end },
        { label = word('sNo'), action = function() showSaves(kind, lastSaveRow) end },
    })
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
        shown:set('main_defaults_button', '_target', on)
    end)
end

-- Whether the data has the picture of the main menu (a texture of the games' archives)
local function hasBackdrop()
    local ok, present = pcall(vfs.fileExists, BACKDROP)
    return ok and present
end

local function open()
    -- The paused game shows through the menu, darkened a little: the game draws the pause menu over a picture of its own.
    -- The title screen has the picture of the main menu instead (the game draws it from its loading menu, over the whole
    -- screen). It is made first, so that the element of the menu is above it
    local atTitle = title()
    local picture = atTitle and hasBackdrop()
    local dark = ui.create {
        layer = LAYER,
        type = ui.TYPE.Image,
        props = {
            relativeSize = util.vector2(1, 1),
            resource = ui.texture { path = picture and BACKDROP or 'white' },
            color = picture and util.color.rgb(1, 1, 1) or util.color.rgb(0, 0, 0),
            alpha = (atTitle and 1) or 0.45,
            ignorePointerEvents = true,
        },
    }
    -- (the picture that fades in over it when the pictures turn is made before the menu too, so it is below it)
    local fadeIn = picture and ui.create {
        layer = LAYER,
        type = ui.TYPE.Image,
        props = {
            relativeSize = util.vector2(1, 1),
            resource = ui.texture { path = BACKDROP },
            alpha = 0,
            ignorePointerEvents = true,
        },
    }
    if atTitle then
        local ok, text = pcall(credits.load)
        creditsFile = ok and text ~= nil
    end
    -- With a pad connected the file is the one of the 360 controller (`&xbox;`): the glyphs of the buttons are on the
    -- tiles that they press, and the Delete button of a list of saves is there (the file shows it only to a pad)
    local opened = menuModule.open(FILE, { layer = LAYER, pad = engine.controllerConnected() })
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
        if fadeIn then fadeIn:destroy() end
        failed = true
        return false
    end
    shown, backdrop, fader, lists = opened, dark, fadeIn or nil, made
    slides, slideIndex, slideAt, fading = picture and titlePictures() or { BACKDROP }, 1, nil, nil
    lists.main.onClick(pickItem)
    lists.confirm.onClick(pickItem)
    lists.settings.onClick(pickItem)
    lists.options.onClick(optionClick)
    lists.saves.onClick(pickSave)
    shown:onClick('main_back_button', back)
    shown:onClick('main_defaults_button', askDelete)
    shown:batch(function()
        shown:set('main_back_button', 'string', word('sBack'))
        shown:set('main_defaults_button', 'string', word('sDelete'))
        -- The Defaults button of the settings (the file shows it with the options) resets the bindings of the controls,
        -- which are not drawn yet, so it is left out: the file's own "additional visibility check" is what says it
        shown:set('main_defaults_button', 'user0', 0)
    end)
    -- The logo of the game is the title screen's (the file keeps it at no alpha; the code of the game fades it in, and in the
    -- pause menu leaves it out: the file's picture of the pause screen shows only when the logo does not)
    if atTitle then shown:set('main_title', 'alpha', 255) end
    showList()
    -- The menu is used by the lit row (the D-pad, and A for Enter), not by the pointer: the circle that the stick left on
    -- the screen stays out of it until the stick moves the pointer again (M13: "the pointer left on the pause menu")
    pad.holdPick()
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
    -- The manual has its own keys
    if screen == 'help' then
        manual.press(code, key.symbol)
        return
    end
    -- the credits end with Escape (the game ends them with a click or the cancel button as well)
    if screen == 'credits' then
        credits.press(code)
        return
    end
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
    if screen == 'credits' then
        if button == PAD.B or button == PAD.A then credits.press(input.KEY.Escape) end
        return
    end
    if screen == 'help' then
        -- the pages turn with the sides of the pad, B leaves (the game puts them on the triggers and B)
        if button == PAD.DPadLeft then
            onKey({ code = input.KEY.LeftArrow })
        elseif button == PAD.DPadRight then
            onKey({ code = input.KEY.RightArrow })
        elseif button == PAD.B then
            manual.leave()
        end
        return
    end
    if button == PAD.DPadUp or button == PAD.DPadDown or button == PAD.DPadLeft or button == PAD.DPadRight then
        pad.holdPick()
    end
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
    elseif button == PAD.Y then
        -- the Delete button of a list of saves has the Y button of the pad on it (the file's `_xbox_button`)
        askDelete()
    elseif button == PAD.B then
        if screen ~= 'list' then back() end
    elseif button == PAD.Start then
        -- (the pause menu is left with it; the title screen has nothing to go back to)
        if not title() then
            pending = function()
                if not title() then engine.resume() end
            end
        end
    end
end

return {
    engineHandlers = {
        onFrame = function(dt)
            -- The engine's own window for the settings is the menu while it is open
            local function drawnNow()
                local state = engine.getState()
                return (state == engine.STATE.Running or state == engine.STATE.NoGame or state == engine.STATE.Ended)
                    and not failed
                    and not engine.settingsOpen()
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
                manual.update()
                local now = core.getRealTime()
                if title() then
                    -- the title's own pictures and music (by real time too: nothing runs with no game)
                    titleAt = titleAt or now
                    if music == nil and now - titleAt >= MUSIC_DELAY then startTitleMusic() end
                    turnPictures(now)
                end
                -- (the credits scroll by real time: the world is paused with no game and in the pause menu, and then the frame's
                -- time is zero)
                if credits.isOpen() then
                    credits.update(creditsAt and math.min(math.max(now - creditsAt, 0), 0.25) or 0)
                    creditsAt = now
                else
                    creditsAt = nil
                end
            end
        end,
        onKeyPress = onKey,
        onControllerButtonPress = onPad,
        -- a click ends the credits, as the game's code ends them on the left button
        onMouseButtonPress = function(button)
            if screen == 'credits' and button == 1 then credits.press(input.KEY.Escape) end
        end,
        onStateChanged = function()
            stopTitleMusic()
            music = nil
            closeShown()
            release()
            failed = false
        end,
    },
}
