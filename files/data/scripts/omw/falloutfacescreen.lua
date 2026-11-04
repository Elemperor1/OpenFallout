-- The screen of the sex, race and face (RaceSexMenu, menu 1036; FalloutNV.exe 0x7AC730, Mac rounds 12AW S7 and 12AY R2),
-- drawn from the menu file chargen/race_sex_menu.xml. The file lays out the panel, the two buttons and the two templates
-- of the rows (one for the rows that are clicked, one for the sliders); the executable makes the rows from the templates,
-- twenty lists of them, of which one is shown at a time (the menu's own `user0` is the number of that list). What each
-- list holds and what a click does is the code's, and is here.
--
--   local face = require('scripts.omw.falloutfacescreen')
--   face.start(page, state, { done = function() ... end })   -- true when the screen is up
--   face.update(seconds); face.press(code, symbol, shift); face.escape(); face.close()
--
-- The head that the player turns (the device, the lights and the head of the character) is not drawn here: the window
-- has the panel only.

local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')
local ui = require('openfallout.ui')
local menuModule = require('scripts.omw.falloutmenu')

local chargen = core.chargen
local pipboy = core.pipboy

local module = {}

local MENU_FILE = 'chargen/race_sex_menu.xml'
local BACKGROUND = 'RSM_Background'
local ITEM_TEMPLATE = 'RSM_list_item_template'
local SLIDER_TEMPLATE = 'RSM_slider_option_template'

-- The heights of the templates (the file's: 36 for a row, 64 for a slider) and the rows of a list start where the
-- panel's `_top_bound` is; the file hides every row that is not wholly between `_top_bound` and `_bot_bound` (80 and 336)
local ROW_HEIGHT = 36
local SLIDER_HEIGHT = 64
local DEFAULT_TOP, DEFAULT_BOTTOM = 80, 336
-- The title of a list is a row of its own at the top of the panel (the file's default y of a row)
local HEADER_Y = 20

-- Where the lists lead from: the list that Escape and the Back button go up to
local PARENT = { [4] = 2, [5] = 3, [6] = 3, [7] = 3, [8] = 4, [9] = 4, [19] = 4 }
for list = 10, 18 do PARENT[list] = 9 end
-- The lists that are the tabs of the screen, in order
local TABS = { 0, 1, 2, 3 }

-- Each list's title setting (and a word for when the data has none), by the number of the list
local TITLES = {
    [0] = { 'sRSMSex', 'Sex' }, [1] = { 'sRSMRace', 'Race' }, [2] = { 'sRSMFace', 'Face' }, [3] = { 'sRSMHair', 'Hair' },
    [4] = { 'sRSMCustomize', 'Customize' }, [5] = { 'sRSMHairStyle', 'Hair Style' }, [6] = { 'sRSMHairColor', 'Hair Color' },
    [7] = { 'sRSMFacialHair', 'Facial Hair' }, [8] = { 'sRSMEyeColor', 'Eye Color' }, [9] = { 'sShape', 'Shape' },
    [10] = { 'sRSMGeneral', 'General' }, [11] = { 'sForehead', 'Forehead' }, [12] = { 'sBrow', 'Brow' },
    [13] = { 'sEyes', 'Eyes' }, [14] = { 'sNose', 'Nose' }, [15] = { 'sMouth', 'Mouth' }, [16] = { 'sCheeks', 'Cheeks' },
    [17] = { 'sJaw', 'Jaw' }, [18] = { 'sChin', 'Chin' }, [19] = { 'sTone', 'Tone' },
}

local opened = nil
local callbacks = nil
local mode = 0
-- The list that is shown, the tab it is under, the row it starts at and the row that has the keys' focus
local active = 0
local tab = 0
local first = 1
local focus = nil
-- The rows of the list that is shown, and the tiles made for them
local rows = {}
local background = nil
local bounds = { top = DEFAULT_TOP, bottom = DEFAULT_BOTTOM }
-- Which tile is which slider, for the clicks on the parts of one
local sliderTiles = {}
local look = nil
local sliderValues = {}
-- What the sliders that the host does not keep show: the number of the preset and the age
local presetValue = 1
local ageValue = 5
-- The colour preset of the hair colour list, and whether a colour was set by hand since (the list then says custom)
local colourPreset = 0
local custom = false
-- The list a click asked for, which is shown at the next frame
local pending = nil

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function titleOf(list)
    local entry = TITLES[list]
    return word(entry[1], entry[2])
end

local function clamp(value, low, high)
    return math.max(low, math.min(high, value))
end

local function refreshLook()
    look = chargen.look()
    sliderValues = {}
    for _, slider in ipairs(chargen.sliders()) do
        sliderValues[slider.kind .. slider.control] = slider.value
    end
end

local function lastTab()
    return mode >= 2 and mode or 3
end

-- ---- The lists ----

local function choice(text, selected, click)
    return { kind = 'choice', text = text, selected = selected, click = click }
end

local function link(list)
    return { kind = 'link', text = titleOf(list) .. ' >', list = list }
end

local function action(text, click)
    return { kind = 'action', text = text, click = click }
end

-- A slider: the whole numbers from `low` to `high`; `jump` is what a click on the bar moves by (the executable's
-- ceil((high - low + 1) * 0.25, or 0.125 for the colours); `get` gives the value now and `set(value)` moves it; `display`
-- is the number the slider writes beside its label (nothing for the face's and the age)
local function slider(label, low, high, fraction, get, set, display, enabled)
    return {
        kind = 'slider', text = label, low = low, high = high, step = 1,
        jump = math.ceil((high - low + 1) * fraction), get = get, set = set, display = display,
        enabled = enabled ~= false,
    }
end

local function faceSliders(list)
    local made = {}
    for _, entry in ipairs(chargen.sliders()) do
        if entry.list == list then
            local key = entry.kind .. entry.control
            local kind, control = entry.kind, entry.control
            made[#made + 1] = slider(word(entry.setting, entry.setting), entry.minimum, entry.maximum, 0.25,
                function() return sliderValues[key] or 0 end,
                function(value) chargen.setSlider(kind, control, value) end)
        end
    end
    return made
end

local function colourOf(preset)
    local packed = pipboy.setting(string.format('iHairColor%02d', preset))
    if packed == nil then return nil end
    packed = math.floor(packed)
    return packed % 256, math.floor(packed / 256) % 256, math.floor(packed / 65536) % 256
end

local function channelSlider(setting, fallback, channel)
    local names = { 'red', 'green', 'blue' }
    return slider(word(setting, fallback), 0, 255, 0.125,
        function() return look.hairColour[names[channel]] end,
        function(value)
            local colour = { look.hairColour.red, look.hairColour.green, look.hairColour.blue }
            colour[channel] = value
            custom = true
            chargen.setHairColour(colour[1], colour[2], colour[3])
        end,
        function() return tostring(look.hairColour[names[channel]]) end)
end

local function listRows(list)
    local made = {}
    if list == 0 then
        made[#made + 1] = choice(word('sMale', 'Male'), function() return not look.female end,
            function() chargen.setFemale(false); presetValue = 1 end)
        made[#made + 1] = choice(word('sFemale', 'Female'), function() return look.female end,
            function() chargen.setFemale(true); presetValue = 1 end)
    elseif list == 1 then
        for _, race in ipairs(look.lists.races) do
            local id = race.id
            made[#made + 1] = choice(race.name, function() return look.race == id end,
                function() chargen.setRace(id); presetValue = 1 end)
        end
    elseif list == 2 then
        local count = #look.lists.presets
        made[#made + 1] = slider(word('sRSMPreset', 'Preset'), 1, math.max(1, count), 0.25,
            function() return presetValue end,
            function(value)
                presetValue = value
                chargen.setPreset(value)
            end,
            function() return tostring(presetValue) end, count > 0)
        made[#made + 1] = link(4)
        made[#made + 1] = action(word('sRSMRandomize', 'Randomize'), function(done)
            local ask = I.FalloutMessage and I.FalloutMessage.ask
            local function randomize()
                chargen.randomize()
                done()
            end
            if ask and ask(word('sRSMConfirmRandomize', 'Randomize the face?'), { word('sYes', 'Yes'), word('sNo', 'No') },
                function(button) if button == 1 then randomize() end end) then
                return
            end
            randomize()
        end)
        -- The age moves the whole face along the age directions of the face file (the fan controls); the host has not
        -- got them yet, so the slider is there and dim
        made[#made + 1] = slider(word('sRSMAge', 'Age'), 1, 10, 0.25, function() return ageValue end,
            function(value) ageValue = value end, nil, false)
    elseif list == 3 then
        made[#made + 1] = link(5)
        made[#made + 1] = link(6)
        if not look.female then made[#made + 1] = link(7) end
    elseif list == 4 then
        made[#made + 1] = link(9)
        made[#made + 1] = link(19)
        made[#made + 1] = link(8)
    elseif list == 5 then
        for _, hair in ipairs(look.lists.hairs) do
            local id = hair.id
            made[#made + 1] = choice(hair.name, function() return look.hair == id end, function() chargen.setHair(id) end)
        end
    elseif list == 6 then
        made[#made + 1] = slider(word('sRSMHairColor', 'Hair Color'), 0, 15, 0.25,
            function() return colourPreset end,
            function(value)
                colourPreset = value
                custom = false
                local red, green, blue = colourOf(value)
                if red then chargen.setHairColour(red, green, blue) end
            end,
            function()
                return custom and word('sRSMCustom', 'Custom') or word('sHairColor' .. colourPreset, tostring(colourPreset))
            end)
        made[#made + 1] = channelSlider('sRSMRedAbbrev', 'R', 1)
        made[#made + 1] = channelSlider('sRSMGreenAbbrev', 'G', 2)
        made[#made + 1] = channelSlider('sRSMBlueAbbrev', 'B', 3)
    elseif list == 7 then
        for _, part in ipairs(look.lists.facialHair) do
            local id = part.id
            made[#made + 1] = choice(part.name, function()
                for _, worn in ipairs(look.headParts) do
                    if worn == id then return true end
                end
                return false
            end, function() chargen.toggleFacialHair(id) end)
        end
    elseif list == 8 then
        for _, eyes in ipairs(look.lists.eyes) do
            local id = eyes.id
            made[#made + 1] = choice(eyes.name, function() return look.eyes == id end, function() chargen.setEyes(id) end)
        end
    elseif list == 9 then
        for index = 10, 18 do made[#made + 1] = link(index) end
    elseif list >= 10 and list <= 19 then
        made = faceSliders(list)
    end
    return made
end

-- ---- The tiles ----

local function heightOf(row)
    return row.kind == 'slider' and SLIDER_HEIGHT or ROW_HEIGHT
end

local function setTop()
    local top = tonumber(opened:get(background, '_top_bound')) or DEFAULT_TOP
    local bottom = tonumber(opened:get(background, '_bot_bound')) or DEFAULT_BOTTOM
    bounds = { top = top, bottom = bottom }
end

-- The rows between where the list's window starts and its end, as far as whole rows go
local function lastShown()
    local used, last = 0, first - 1
    for index = first, #rows do
        used = used + heightOf(rows[index])
        if bounds.top + used > bounds.bottom then break end
        last = index
    end
    return last
end

local function scrollTo(row)
    if row < first then
        first = row
    else
        while lastShown() < row and first < row do first = first + 1 end
    end
end

-- Put each row where it is for this scroll, and show the arrows that scroll further
local function place()
    local y = bounds.top
    local firstY = 0
    for index = 1, first - 1 do firstY = firstY + heightOf(rows[index]) end
    local cumulative = 0
    opened:batch(function()
        for index, row in ipairs(rows) do
            opened:set(row.tile, 'y', bounds.top + cumulative - firstY)
            cumulative = cumulative + heightOf(row)
        end
        opened:set('RSM_scroll_up_target', 'visible', first > 1)
        opened:set('RSM_scroll_down_target', 'visible', lastShown() < #rows)
    end)
end

local function valueOf(row)
    return clamp(row.get(), row.low, row.high)
end

-- Write what the rows show now: the lit one of a list of choices, the place and number of a slider
local function sync()
    opened:batch(function()
        for index, row in ipairs(rows) do
            if row.kind == 'choice' then
                opened:set(row.tile, '_selected', row.selected() and 2 or 1)
            elseif row.kind == 'slider' then
                opened:set(row.tile, 'user1', valueOf(row))
                opened:set(row.tile, 'user5', row.display and row.display() or ' ')
            end
            opened:set(row.tile, 'mouseover', index == focus and 1 or 0)
        end
    end)
end

-- The rows are children of the panel, which has its buttons and arrows as children too: only the ones made here go
local made = {}

local function clearTiles()
    for _, tile in ipairs(made) do opened.menu:remove(tile) end
    made = {}
    sliderTiles = {}
    rows = {}
end

local function titleText(list)
    local title = titleOf(list)
    if mode < 2 and list <= 3 then
        return string.format('%d. %s', list + 1, title)
    end
    return title
end

local function buildTiles()
    opened:batch(function()
        clearTiles()
        rows = listRows(active)
        local header = opened:instantiate(background, ITEM_TEMPLATE, 'RSM_header')
        made[#made + 1] = header
        opened:set(header, 'user0', active)
        opened:set(header, '_is_header', true)
        opened:set(header, '_selected', 0)
        opened:set(header, 'y', HEADER_Y)
        opened:set(header, 'string', titleText(active))
        opened:set(header, 'target', false)
        for index, row in ipairs(rows) do
            local name = 'RSM_row' .. index
            if row.kind == 'slider' then
                row.tile = opened:instantiate(background, SLIDER_TEMPLATE, name)
                sliderTiles[row.tile] = row
                opened:set(row.tile, 'user2', row.low)
                opened:set(row.tile, 'user3', row.high)
                opened:set(row.tile, 'user4', row.jump)
                opened:set(row.tile, 'user6', row.step)
                opened:set(row.tile, '_enabled', row.enabled)
                -- The marker is the part that the pointer drags
                for _, child in ipairs(opened.menu:children(row.tile)) do
                    if opened.menu:name(child) == 'RSM_slider_marker' then
                        opened:set(child, 'draggable', row.enabled and 1 or 0)
                    end
                end
            else
                row.tile = opened:instantiate(background, ITEM_TEMPLATE, name)
                opened:set(row.tile, '_selected', 0)
            end
            made[#made + 1] = row.tile
            opened:set(row.tile, 'user0', active)
            opened:set(row.tile, 'string', row.text)
            row.name = name
        end
    end)
    for index, row in ipairs(rows) do
        if row.kind ~= 'slider' then
            local owner = index
            opened:onClick(row.name, function() module.activate(owner) end)
        end
    end
end

local function showButtons()
    opened:batch(function()
        local atLast = tab == lastTab()
        opened:set('RSM_next_button', 'string', atLast and word('sDone', 'Done') or word('sNext', 'Next'))
        opened:set('RSM_back_button', 'string', word('sBack', 'Back'))
        -- Back goes up a list, or to the tab before; with nowhere to go it is not shown (guess: the executable's)
        local canBack = PARENT[active] ~= nil or (mode < 2 and tab > 0)
        opened:set('RSM_back_button', 'visible', canBack)
    end)
end

-- Show a list: its rows from the top, with the focus on none
local function show(list)
    active = list
    first = 1
    focus = nil
    local root = list
    while PARENT[root] do root = PARENT[root] end
    tab = root
    opened:io('user0', list)
    buildTiles()
    showButtons()
    place()
    sync()
end

-- ---- What the player does ----

local function changed()
    refreshLook()
    sync()
end

-- The row's click, by its place in the list
function module.activate(index)
    local row = rows[index]
    if not row or not opened then return end
    if row.kind == 'link' then
        -- The row that was clicked is still in the click's way: the list is changed on the next frame
        pending = row.list
    elseif row.kind == 'choice' then
        row.click()
        changed()
        -- A choice of sex or race changes what the other lists hold, which are made again when they are shown
    elseif row.kind == 'action' then
        row.click(changed)
        changed()
    end
end

local function moveSlider(row, value)
    if not row or not row.enabled then return end
    value = clamp(value, row.low, row.high)
    if value == valueOf(row) then return end
    row.set(value)
    changed()
end

local function sliderOf(tile)
    while tile do
        if sliderTiles[tile] then return sliderTiles[tile] end
        tile = opened.menu:parent(tile)
    end
    return nil
end

-- The next tab, or the box that asks whether the player is done on the last one
local function confirmDone()
    local done = callbacks and callbacks.done
    local ask = I.FalloutMessage and I.FalloutMessage.ask
    local text = titleOf(lastTab()) .. ' ' .. word('sRSMConfirmDone', 'Are you done?')
    if ask and ask(text, { word('sYes', 'Yes'), word('sNo', 'No') }, function(button)
        if button == 1 and done then done() end
    end) then
        return
    end
    if done then done() end
end

local function goTab(delta)
    if mode >= 2 then return end
    local target = tab + delta
    if target < 0 then return end
    if target > lastTab() then
        confirmDone()
        return
    end
    show(TABS[target + 1])
end

local function up()
    local parent = PARENT[active]
    if parent == nil then return false end
    show(parent)
    return true
end

local function scroll(delta)
    local before = first
    first = clamp(first + delta, 1, math.max(1, #rows))
    if first ~= before then place() end
end

-- Escape and the back button of the pad: up one level when there is one, and else nothing (the menu cannot be cancelled)
function module.escape()
    if not opened then return false end
    up()
    return true
end

function module.press(code, symbol, shift)
    if not opened then return false end
    local K = input.KEY
    local row = focus and rows[focus] or nil
    if code == K.UpArrow or code == K.DownArrow then
        local delta = code == K.UpArrow and -1 or 1
        focus = clamp((focus or (delta > 0 and 0 or #rows + 1)) + delta, 1, math.max(1, #rows))
        scrollTo(focus)
        place()
        sync()
    elseif code == K.LeftArrow or code == K.RightArrow then
        local delta = code == K.LeftArrow and -1 or 1
        if shift then
            goTab(delta)
        elseif row and row.kind == 'slider' then
            moveSlider(row, valueOf(row) + delta * row.step)
        end
    elseif code == K.PageUp or code == K.PageDown then
        if row and row.kind == 'slider' then
            moveSlider(row, valueOf(row) + (code == K.PageUp and -1 or 1) * row.jump)
        end
    elseif code == K.Enter or code == K.Space then
        if row and row.kind ~= 'slider' then module.activate(focus) end
    else
        return false
    end
    return true
end

-- The slider that is dragged follows the pointer along its bar
local function drag()
    local tile = opened.menu:dragged()
    local row = tile and sliderOf(tile)
    if not row then return end
    local bar
    for _, child in ipairs(opened.menu:children(row.tile)) do
        if opened.menu:name(child) == 'RSM_slider_bar' then bar = child end
    end
    local rect = bar and opened:rect(bar)
    local at = opened:pointerAt()
    if not rect or not at or rect.width <= 0 then return end
    local fraction = clamp((at.x - rect.x) / rect.width, 0, 1)
    moveSlider(row, math.floor(row.low + (row.high - row.low) * fraction + 0.5))
end

-- ---- Opening and closing ----

-- The file puts the panel where the executable's device has its screen (the 2D menu is drawn into the screen of the
-- reflectron model: x 930, y 550 of a texture that is larger than the window). Here the panel goes to the right edge of
-- the window and to its middle; the window is 960 design units high.
local PANEL_WIDTH, PANEL_HEIGHT, PANEL_MARGIN = 340, 500, 40
local placedFor = nil

local function placePanel()
    local size = ui.screenSize()
    if placedFor and placedFor.x == size.x and placedFor.y == size.y then return end
    placedFor = size
    local width = 960 * size.x / size.y
    opened:batch(function()
        opened:set(background, 'x', math.floor(width - PANEL_WIDTH - PANEL_MARGIN))
        opened:set(background, 'y', math.floor((960 - PANEL_HEIGHT) / 2))
    end)
end

function module.isOpen()
    return opened ~= nil
end

function module.close()
    if opened then
        opened:close()
        opened = nil
    end
    callbacks = nil
    pending = nil
    rows = {}
    sliderTiles = {}
    made = {}
end

-- Open the screen on the list the mode starts at (the tab of the sex for a new character; the hair and the face for the
-- barber and the plastic surgeon, whose race and sex are not changed). False, with nothing left open, when the data has
-- no such menu file.
function module.start(page, state, options)
    module.close()
    callbacks = options
    -- The mode is the one value the command gave: 2 for the barber and 3 for the plastic surgeon, none for a new character
    mode = page and page.values and page.values[1] or 0
    opened = menuModule.open(MENU_FILE)
    if not opened then return false end
    background = opened:find(BACKGROUND)
    if not background then
        module.close()
        return false
    end
    placedFor = nil
    placePanel()
    setTop()
    presetValue, colourPreset, custom = 1, 0, false
    -- The glyphs on the ends of a slider's bar (the menu's `user1` and `user2`: guess, the executable's are not read)
    opened:io('user1', '<')
    opened:io('user2', '>')
    refreshLook()
    local start = mode == 2 and 3 or mode == 3 and 2 or 0
    show(start)
    for _, name in ipairs { 'RSM_slider_arrow_left', 'RSM_slider_arrow_right', 'RSM_slider_bar_left',
        'RSM_slider_bar_right' } do
        local delta = name:find('arrow') and 1 or nil
        local sign = name:find('left') and -1 or 1
        opened:onClick(name, function(tile)
            local row = sliderOf(tile)
            if row then moveSlider(row, valueOf(row) + sign * (delta and row.step or row.jump)) end
        end)
    end
    opened:onDrag('RSM_slider_marker', function() return nil end, function() end, drag)
    opened:onClick('RSM_back_button', function()
        if not up() then goTab(-1) end
    end)
    opened:onClick('RSM_next_button', function() goTab(1) end)
    opened:onClick('RSM_scroll_up_target', function() scroll(-1) end)
    opened:onClick('RSM_scroll_down_target', function() scroll(1) end)
    opened:onWheel(function(notches, tile)
        -- The wheel over the head zooms it (the head is not drawn here); over the rest it scrolls the list
        if tile and opened.menu:name(tile) == 'RSM_Face_Grab' then return end
        scroll(notches)
    end)
    return true
end

function module.update()
    if not opened then return end
    opened:checkSize()
    placePanel()
    if pending ~= nil then
        local list = pending
        pending = nil
        show(list)
    end
end

return module
