-- The menu of a terminal in Fallout 3 and New Vegas, drawn on the game's own menu file (computers_menu.xml). The file lays
-- out the screen (a 4:3 screen of three quarters of the window's height, in the middle of the window, in the green of
-- the terminals); what the code of the game puts in it is here, as the Mac read it from the executable (rounds 12R, 12S,
-- 12U): a terminal whose record is not marked as unlocked types the introduction (four lines, the last of them stars as
-- many as the letters of a word of its game of hacking), waits two seconds and goes on to the menu; the menu has the two
-- header lines, the name of the terminal, the words of the menu with a rule under them, the lines of the menu as "> "
-- and their words in the order of the record (a line whose conditions fail is not there) and, in the menus below the
-- first, a "> " Back line at the end; all of it typed a character at a time (a game setting gives how fast). A line that
-- has a result puts it in the line under the menu for a few seconds; a line that has a note of text shows it in the zone
-- of the screen, a page at a time. What a terminal holds, and what a line does, is the engine's (core.interact): this
-- module shows the menu and gives the player's choices back to the screen that owns it (falloutinteraction.lua). A game
-- whose data lacks the file keeps the stand-in widgets of that script.
local core = require('openfallout.core')
local input = require('openfallout.input')
local util = require('openfallout.util')
local menuModule = require('scripts.omw.falloutmenu')

local pipboy = core.pipboy
local v2 = util.vector2

local FILE = 'computers_menu.xml'
-- The tiles this screen fills: a file that lacks one is not the game's
local TILES = { 'computers_header1', 'computers_header2', 'computers_server', 'computers_welcome',
    'computers_separator', 'computers_file_directory', 'computers_cursor', 'computers_intro_welcome',
    'computers_intro_logon', 'computers_intro_enterpw', 'computers_intro_pwdisplay', 'computers_result_prompt',
    'computers_result_text', 'computers_display_zone', 'computers_display_zone_text', 'computers_background' }
local INTRO_TILES = { 'computers_intro_welcome', 'computers_intro_logon', 'computers_intro_enterpw',
    'computers_intro_pwdisplay' }
-- The font of the file's texts
local FONT = 5
-- The characters a second the texts are typed at when the settings do not say (FalloutNV.exe 0x757ff0 and the settings of
-- the game): the menus, the notes, and what the game of hacking types
local DEFAULT_RATE = 150
local DEFAULT_NOTE_RATE = 150
local DEFAULT_INPUT_RATE = 20
-- The seconds a result stays under the menu, and how long the screen waits after the introduction
local DEFAULT_RESULT_SECONDS = 5
local INTRO_WAIT = 2
-- The cursor of the file blinks every _blink_interval milliseconds
local DEFAULT_BLINK = 0.4
-- The lines a page of a note has (the file's wraplines) and the width its lines are broken at (the file's wrapwidth)
local DEFAULT_PAGE_LINES = 16
local DEFAULT_PAGE_WIDTH = 600

local module = {}

local shown, list, hooks = nil, nil, nil
-- 'intro' (the introduction is typed), 'wait' (the two seconds after it), 'menu' or 'note'
local phase = nil
local reference = nil
-- The things typed one after the other: each has the tile it is typed in, its words, and what puts the first n characters
-- of it on the screen
local slots = {}
local slotIndex, typedChars, carry = 1, 0, 0
local rate, noteRate, inputRate = DEFAULT_RATE, DEFAULT_NOTE_RATE, DEFAULT_INPUT_RATE
local blink = DEFAULT_BLINK
local blinkClock = 0
local waitLeft = 0
-- The number of lines the engine's menu has (the Back line, when there is one, is the one after them)
local lineCount = 0
local lines = {}
local hasBack = false
-- What the menu that is shown was made from, so that it is made again when the engine's menu changes
local menuKey = nil
local headersTyped = false
-- The result under the menu: its number in the engine, and the seconds that are left to show it
local resultSerial, resultLeft = 0, 0
-- The note in the zone: its pages
local noteText, pages, pageIndex = nil, {}, 0

local function word(setting)
    return pipboy.text(setting)
end

-- The part of a string of `count` characters (the characters of UTF-8 text are not cut in two)
local function prefix(text, count)
    if count >= #text then return text end
    local cut = count
    while cut > 0 do
        local byte = text:byte(cut + 1)
        if not byte or byte < 0x80 or byte >= 0xC0 then break end
        cut = cut - 1
    end
    return text:sub(1, cut)
end

function module.isOpen()
    return shown ~= nil
end

function module.close()
    if shown then
        shown:close()
    end
    shown, list, hooks, phase, reference = nil, nil, nil, nil, nil
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    lines, lineCount, hasBack, menuKey, headersTyped = {}, 0, false, nil, false
    resultSerial, resultLeft = 0, 0
    noteText, pages, pageIndex = nil, {}, 0
end

local function typing()
    return slotIndex <= #slots
end

-- Put the cursor of the file at the end of what is typed
local function placeCursor()
    if not shown then return end
    if not typing() then
        shown:set('computers_cursor', 'visible', 0)
        return
    end
    local slot = slots[slotIndex]
    local x, y = slot.cursorAt(typedChars)
    if not x then
        shown:set('computers_cursor', 'visible', 0)
        return
    end
    local visible = (blinkClock % (2 * blink)) < blink
    shown:set('computers_cursor', 'x', x)
    shown:set('computers_cursor', 'y', y)
    shown:set('computers_cursor', 'visible', visible and 1 or 0)
end

-- A text of one tile (the tile is as wide as what is typed in it when it is not wrapped)
local function textSlot(tile, text, wrapped, speed)
    slots[#slots + 1] = {
        text = text,
        rate = speed or rate,
        apply = function(part) shown:set(tile, 'string', part) end,
        cursorAt = function(count)
            local rect = shown:rect(tile)
            if not rect then return nil end
            local part = prefix(text, count)
            if wrapped then
                local measure = shown.menu:measure(FONT, part, wrapped)
                local rowsTyped = math.max(measure.lines or 1, 1)
                local x = rect.x + (rowsTyped == 1 and (measure.width or 0) or 0)
                return x, rect.y + (measure.height or 0) * (rowsTyped - 1) / rowsTyped
            end
            return rect.x + (shown.menu:measure(FONT, part, 0).width or 0), rect.y
        end,
    }
end

-- Everything that is still to be typed appears at once
function module.skip()
    if not shown or not typing() then return end
    shown:batch(function()
        while typing() do
            local slot = slots[slotIndex]
            slot.apply(slot.text)
            slotIndex = slotIndex + 1
        end
        typedChars, carry = 0, 0
        placeCursor()
    end)
end

local function pick(row)
    if not hooks or phase ~= 'menu' then return end
    if row >= 1 and row <= lineCount then
        hooks.choose(lines[row].index)
    elseif hasBack and row == lineCount + 1 then
        hooks.back()
    end
end

local function highlight(step)
    if not list or phase ~= 'menu' then return end
    local count = lineCount + (hasBack and 1 or 0)
    if count == 0 then return end
    local at = list.selected()
    if at == 0 then at = step > 0 and 0 or count + 1 end
    list.select(math.max(1, math.min(count, at + step)), true)
end

-- The part of the menu that is its list: the lines of the record, then the Back line of a menu below the first
local function rowTextsOf(terminal)
    lines = {}
    for _, line in ipairs(terminal.lines or {}) do lines[#lines + 1] = line end
    lineCount = #lines
    hasBack = terminal.canGoBack == true
    local rowTexts = {}
    for index, line in ipairs(lines) do rowTexts[index] = '> ' .. line.text end
    if hasBack then rowTexts[lineCount + 1] = '> ' .. word('sComputersBack') end
    return rowTexts
end

local function menuKeyOf(terminal)
    local parts = { terminal.body or '', tostring(terminal.canGoBack == true) }
    for _, line in ipairs(terminal.lines or {}) do parts[#parts + 1] = line.text end
    return table.concat(parts, '\n')
end

-- The introduction goes and the menu comes: the header lines, the name of the terminal, the welcome words and the lines
local function showMenu(terminal, onlyList)
    phase = 'menu'
    local rowTexts = rowTextsOf(terminal)
    menuKey = menuKeyOf(terminal)
    local body = terminal.body or ''
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    if not onlyList and not headersTyped then
        headersTyped = true
        textSlot('computers_header1', word('sComputersHeader1'))
        textSlot('computers_header2', word('sComputersHeader2'))
        textSlot('computers_server', terminal.name or '')
    end
    if body ~= '' then textSlot('computers_welcome', body, DEFAULT_PAGE_WIDTH) end
    -- The lines are typed one after the other, the ones above in full and the ones below not there yet
    local function rowsShowing(upTo, chars)
        local shownRows = {}
        for index, text in ipairs(rowTexts) do
            if index < upTo then
                shownRows[index] = { user0 = text }
            elseif index == upTo then
                shownRows[index] = { user0 = prefix(text, chars) }
            else
                shownRows[index] = { user0 = '' }
            end
        end
        list.rows(shownRows)
    end
    local visibleRows = shown:get('computers_file_directory', '_number_of_visible_items') or 12
    for index, text in ipairs(rowTexts) do
        slots[#slots + 1] = {
            text = text,
            rate = rate,
            apply = function(part) rowsShowing(index, #part) end,
            cursorAt = function(count)
                local rect = shown:rect('computers_file_directory')
                if not rect then return nil end
                local height = rect.height / math.max(visibleRows, 1)
                return rect.x + (shown.menu:measure(FONT, prefix(text, count), 0).width or 0),
                    rect.y + (index - 1) * height
            end,
        }
    end
    shown:batch(function()
        for _, tile in ipairs(INTRO_TILES) do shown:set(tile, 'string', '') end
        shown:set('computers_file_directory', 'visible', 1)
        shown:set('computers_display_zone', 'visible', 0)
        shown:set('computers_display_zone_text', 'visible', 0)
        if not onlyList then
            shown:set('computers_header1', 'string', '')
            shown:set('computers_header2', 'string', '')
            shown:set('computers_server', 'string', '')
        end
        shown:set('computers_welcome', 'visible', body ~= '' and 1 or 0)
        shown:set('computers_welcome', 'string', '')
        shown:set('computers_separator', 'visible', body ~= '' and 1 or 0)
        shown:set('computers_cursor', 'visible', 0)
        rowsShowing(0, 0)
    end)
end

-- The line under the menu: "> " and the words the line that was chosen has; they go after a few seconds
local function hideResult()
    resultLeft = 0
    if shown then
        shown:set('computers_result_prompt', 'visible', 0)
        shown:set('computers_result_text', 'visible', 0)
    end
end

local function showResult(text)
    resultLeft = pipboy.setting('iComputersResultDisplayTimeout') or DEFAULT_RESULT_SECONDS
    shown:batch(function()
        shown:set('computers_result_prompt', 'string', '> ')
        shown:set('computers_result_prompt', 'visible', 1)
        shown:set('computers_result_text', 'string', '')
        shown:set('computers_result_text', 'visible', 1)
    end)
    textSlot('computers_result_text', text, nil, rate)
end

-- The lines of a text, broken where the zone's width ends (a new line of the text is one too)
local function wrapLines(text, width)
    local result = {}
    for paragraph in (text:gsub('\r', '') .. '\n'):gmatch('(.-)\n') do
        local line = ''
        for piece in paragraph:gmatch('%S+') do
            local candidate = line == '' and piece or (line .. ' ' .. piece)
            if line ~= '' and (shown.menu:measure(FONT, candidate, 0).width or 0) > width then
                result[#result + 1] = line
                line = piece
            else
                line = candidate
            end
        end
        result[#result + 1] = line
    end
    return result
end

-- A page of the note is typed in the zone; false when there is none left
local function showPage(index)
    if index > #pages then return false end
    pageIndex = index
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    shown:set('computers_display_zone_text', 'string', '')
    textSlot('computers_display_zone_text', pages[index], DEFAULT_PAGE_WIDTH, noteRate)
    return true
end

local function openNote(text)
    phase = 'note'
    noteText = text
    local width = shown:get('computers_display_zone_text', 'wrapwidth') or DEFAULT_PAGE_WIDTH
    local height = shown:get('computers_display_zone_text', 'wraplines') or DEFAULT_PAGE_LINES
    if width <= 0 then width = DEFAULT_PAGE_WIDTH end
    if height <= 0 then height = DEFAULT_PAGE_LINES end
    local all = wrapLines(text, width)
    pages = {}
    for first = 1, math.max(#all, 1), height do
        pages[#pages + 1] = table.concat(all, '\n', first, math.min(first + height - 1, #all))
    end
    shown:batch(function()
        shown:set('computers_file_directory', 'visible', 0)
        shown:set('computers_welcome', 'visible', 0)
        shown:set('computers_separator', 'visible', 0)
        shown:set('computers_display_zone', 'visible', 1)
        shown:set('computers_display_zone_text', 'visible', 1)
    end)
    hideResult()
    showPage(1)
end

-- A click on the zone, or the key that cancels, while a note is shown: the next page, or the end of the note
local function turnPage()
    if typing() then
        module.skip()
    elseif not showPage(pageIndex + 1) then
        noteText = nil
        if hooks then hooks.back() end
    end
end

-- The key that cancels (the game's code 10): the note is turned or closed, or the menu goes back (the first leaves)
local function cancel()
    if phase == 'note' then
        turnPage()
    elseif hooks then
        hooks.back()
    end
end

-- A key (or a button of the pad that stands for one); true when the menu took it
function module.press(code)
    if not shown then return false end
    local KEY = input.KEY
    if code == KEY.Enter or code == KEY.Space then
        if phase == 'intro' or phase == 'wait' then
            module.skipIntro()
        elseif phase == 'note' then
            turnPage()
        elseif typing() then
            module.skip()
        else
            local at = list and list.selected() or 0
            pick(at > 0 and at or 1)
        end
        return true
    elseif code == KEY.UpArrow then
        highlight(-1)
        return true
    elseif code == KEY.DownArrow then
        highlight(1)
        return true
    elseif code == KEY.Backspace then
        -- (Escape is the engine's: it closes the screen)
        cancel()
        return true
    end
    return false
end

-- The introduction is typed: four lines, one after the other, at the rate the game of hacking types its own
local function startIntro(terminal)
    phase = 'intro'
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    textSlot('computers_intro_welcome', word('sHackingIntro01'), nil, inputRate)
    textSlot('computers_intro_logon', '> ' .. word('sComputersLogon'), nil, inputRate)
    textSlot('computers_intro_enterpw', word('sHackingHeader2'), nil, inputRate)
    textSlot('computers_intro_pwdisplay', '> ' .. string.rep('*', math.max(terminal.introStars or 0, 0)), nil,
        inputRate)
    shown:batch(function()
        for _, tile in ipairs(INTRO_TILES) do shown:set(tile, 'string', '') end
        shown:set('computers_header1', 'string', '')
        shown:set('computers_header2', 'string', '')
        shown:set('computers_server', 'string', '')
        shown:set('computers_welcome', 'visible', 0)
        shown:set('computers_separator', 'visible', 0)
        shown:set('computers_file_directory', 'visible', 0)
        shown:set('computers_cursor', 'visible', 0)
    end)
end

local pendingTerminal = nil

-- A click on the picture of the screen (or the key that accepts) before the menu came: the menu comes at once
function module.skipIntro()
    if shown and (phase == 'intro' or phase == 'wait') and pendingTerminal then
        showMenu(pendingTerminal)
    end
end

-- What the engine says changed: the menu below this one, a result, a note
local function refresh(terminal)
    if phase == 'intro' or phase == 'wait' then
        pendingTerminal = terminal
        return
    end
    pendingTerminal = terminal
    if (terminal.note or '') ~= '' then
        if noteText ~= terminal.note then openNote(terminal.note) end
        return
    end
    local key = menuKeyOf(terminal)
    if phase == 'note' or key ~= menuKey then
        noteText = nil
        showMenu(terminal, true)
    end
    if (terminal.result or '') == '' then
        if resultLeft > 0 then hideResult() end
        resultSerial = terminal.resultSerial or 0
    elseif terminal.resultSerial ~= resultSerial then
        resultSerial = terminal.resultSerial
        showResult(terminal.result)
    end
end

-- The menu of the terminal that the engine says is open (core.interact.terminal()). `handlers`: choose(index) for a line
-- of the menu and back(), which leaves from the first menu. Called again with the same terminal when the engine's screen
-- changed, the screen is then changed where it stands. Nothing is drawn, and false is given, when the file is not in the
-- install.
function module.start(terminal, handlers)
    if shown and reference == terminal.reference then
        hooks = handlers
        refresh(terminal)
        return true
    end
    module.close()
    local opened = menuModule.open(FILE, {
        region = function(size) return v2(size.y, size.y * 0.75) end,
    })
    if not opened then return false end
    for _, name in ipairs(TILES) do
        if not opened:find(name) then
            opened:close()
            return false
        end
    end
    local rows = opened:list('computers_file_directory', 'computers_file_template')
    if not rows then
        opened:close()
        return false
    end
    shown, list, hooks, reference = opened, rows, handlers, terminal.reference
    rate = pipboy.setting('iComputersDisplayRateMenus') or DEFAULT_RATE
    if rate <= 0 then rate = 1 end
    noteRate = pipboy.setting('iComputersDisplayRateNotes') or DEFAULT_NOTE_RATE
    if noteRate <= 0 then noteRate = 1 end
    inputRate = pipboy.setting('iHackingInputRate') or DEFAULT_INPUT_RATE
    if inputRate <= 0 then inputRate = 1 end
    blink = (shown:get('computers_cursor', '_blink_interval') or 400) / 1000
    blinkClock = 0
    resultSerial = terminal.resultSerial or 0
    pendingTerminal = terminal

    list.onClick(function(row) pick(row) end)
    -- A click on the screen's picture that is not on a line brings the menu at once, from the introduction
    shown:onClick('computers_background', function()
        if phase == 'intro' or phase == 'wait' then
            module.skipIntro()
        elseif phase == 'menu' then
            module.skip()
        end
    end)
    shown:onClick('computers_display_zone', function()
        if phase == 'note' then turnPage() end
    end)
    -- Whatever a screen starts with, nothing but the file's own picture is shown until it is typed
    if terminal.intro then
        startIntro(terminal)
    else
        showMenu(terminal)
    end
    if (terminal.note or '') ~= '' then openNote(terminal.note) end
    return true
end

-- Frame by frame: the typing goes on, the cursor blinks, the result goes after its time
function module.update(dt)
    if not shown then return end
    shown:checkSize()
    blinkClock = blinkClock + dt
    if resultLeft > 0 then
        resultLeft = resultLeft - dt
        if resultLeft <= 0 then hideResult() end
    end
    if not typing() then
        if phase == 'intro' then
            phase, waitLeft = 'wait', INTRO_WAIT
        elseif phase == 'wait' then
            waitLeft = waitLeft - dt
            if waitLeft <= 0 then module.skipIntro() end
        end
        shown:batch(placeCursor)
        return
    end
    local slot = slots[slotIndex]
    carry = carry + dt * slot.rate
    local count = math.floor(carry)
    if count > 0 then
        carry = carry - count
        shown:batch(function()
            while count > 0 and typing() do
                slot = slots[slotIndex]
                local left = #slot.text - typedChars
                local step = math.min(count, left)
                typedChars = typedChars + step
                count = count - step
                slot.apply(prefix(slot.text, typedChars))
                if typedChars >= #slot.text then
                    slot.apply(slot.text)
                    slotIndex = slotIndex + 1
                    typedChars = 0
                end
            end
            placeCursor()
        end)
    else
        shown:batch(placeCursor)
    end
end

return module
