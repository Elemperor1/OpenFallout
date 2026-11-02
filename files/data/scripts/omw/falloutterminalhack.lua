-- The game of hacking a terminal in Fallout 3 and New Vegas, drawn on the game's own menu file (hacking_menu.xml). The file
-- lays out the screen (the introduction, the header with the blocks of the attempts, the two columns of the memory
-- dump, the log of what was chosen); what the code of the game puts in it is here, as the Mac read it from the
-- executable (rounds 12R, 12S, 12T, 12U): the introduction is typed and, when the ten seconds since the last screen of
-- the game are over, the board is laid out: the header lines and then the 34 rows of the dump, typed at the rate of
-- the dump; the pointer on a word, a pair of brackets or a character lights it (its text in inverse video, and typed in
-- the line of the log) and a click chooses it; what choosing does, and the rules of the game, are the engine's
-- (core.interact: the dump, the words, the likeness, the attempts); the lines of the log are what the game's words say.
-- The screen of a terminal that locked the player out on an earlier visit shows the locked message until the player
-- leaves. A game whose data lacks the file keeps the stand-in widgets of falloutinteraction.lua.
local core = require('openfallout.core')
local input = require('openfallout.input')
local menuModule = require('scripts.omw.falloutmenu')

local pipboy = core.pipboy
local interact = core.interact

local FILE = 'hacking_menu.xml'
-- The tiles this screen fills: a file that lacks one is not the game's
local TILES = { 'hacking_depth_rect', 'hacking_cursor', 'hacking_intro_rect', 'hacking_header_rect',
    'hacking_password_file_rect', 'hacking_password_log_rect', 'hacking_password_prompt', 'hacking_password_entry',
    'hacking_locked_message_rect', 'hacking_background' }
local TEMPLATES = { ['hacking_intro_rect'] = 'hacking_intro_template', ['hacking_header_rect'] = 'hacking_intro_template',
    ['hacking_password_file_rect'] = 'hacking_password_file_template',
    ['hacking_password_log_rect'] = 'hacking_password_log_template' }
-- The font of the file's texts
local FONT = 5
-- The dump: 34 rows of 12 characters in two columns of 17, each row with its address ("0x" and four digits, a space)
local ROWS, ROW_SIZE, COLUMN_ROWS, ADDRESS_CHARS = 34, 12, 17, 7
-- The width of a letter and the height of a row of the file (FalloutNV.exe 0x769700: 17), the line of the header the
-- blocks of the attempts are in, and the room of the depth rect that the file centers on the window
local CELL = 17
local ATTEMPTS_LINE_Y = 90
local DEPTH_WIDTH = 920
-- The rates the game types at when its settings do not say (characters a second): what the game says, what it echoes of
-- the player, the dump
local DEFAULT_OUTPUT_RATE, DEFAULT_INPUT_RATE, DEFAULT_DUMP_RATE = 67, 20, 500
-- The seconds after a right guess before the screen hands over to the menu of the terminal, and the steps of a screen that
-- slides out after the lock-out: 30 units each 50 ms (0x76ab40)
local SOLVED_WAIT = 0.5
local SLIDE_STEP, SLIDE_UNITS, SLIDE_END = 0.05, 30, 700
local DEFAULT_BLINK = 0.4

local module = {}

local shown, hooks = nil, nil
-- 'intro' (typed), 'gate' (waits for the seconds since the last screen of the game), 'board' (typed), 'play', 'solved',
-- 'lockout' (the screen slides out), 'locked' (a terminal that locked the player out before)
local phase = nil
local boardKey = nil
local outputRate, inputRate, dumpRate = DEFAULT_OUTPUT_RATE, DEFAULT_INPUT_RATE, DEFAULT_DUMP_RATE
local blink, blinkClock = DEFAULT_BLINK, 0
local wordLength = 0
-- The things typed one after the other (the tile, the words, the rate and how the words get to the tile)
local slots = {}
local slotIndex, typedChars, carry = 1, 0, 0
-- The tiles of the board: the header lines, the blocks of the attempts, the rows of the dump (by number from 0 and by
-- tile), the lines of the log
local headerTiles, blocks, rowTiles, rowOfTile, logTiles = {}, {}, {}, {}, {}
local attemptsTile = nil
local dump, address = '', 0
local attempts, maxAttempts = 0, 0
local rowCell = CELL
-- What the pointer is on: the first character of the unit, and the text typed in the line of the log for it
local hoverFirst, hoverText = nil, ''
local echoText, echoTyped, echoCarry = '', 0, 0
local waitLeft, slideLeft, slideAt = 0, 0, 0
local width = 0

local function word(setting)
    return pipboy.text(setting)
end

local function rateOf(setting, fallback)
    local rate = pipboy.setting(setting) or fallback
    return rate > 0 and rate or 1
end

local function prefix(text, count)
    if count >= #text then return text end
    return text:sub(1, count)
end

function module.isOpen()
    return shown ~= nil
end

function module.phase() return phase end

function module.close()
    if shown then
        shown:close()
    end
    shown, hooks, phase, boardKey = nil, nil, nil, nil
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    headerTiles, blocks, rowTiles, rowOfTile, logTiles, attemptsTile = {}, {}, {}, {}, {}, nil
    dump, address, attempts, maxAttempts = '', 0, 0, 0
    hoverFirst, hoverText, echoText, echoTyped, echoCarry = nil, '', '', 0, 0
end

local function typing()
    return slotIndex <= #slots
end

local function rowText(index)
    local first = index * ROW_SIZE
    return string.format('0x%04X %s', (address + first) % 65536, dump:sub(first + 1, first + ROW_SIZE))
end

local function textSlot(tile, text, rate, onFirst)
    slots[#slots + 1] = {
        tile = tile,
        text = text,
        rate = rate,
        first = onFirst,
        apply = function(part) shown:set(tile, 'string', part) end,
    }
end

local function typeAll()
    while typing() do
        local slot = slots[slotIndex]
        if slot.first then slot.first() end
        slot.apply(slot.text)
        slotIndex = slotIndex + 1
    end
    typedChars, carry = 0, 0
end

-- Units: the traits of the file and what `measure` gives are in design units (960 from the top of the window to its bottom),
-- the rectangles of the tiles and the pointer in pixels
local function design(pixels)
    return pixels * 960 / shown.size.y
end

-- The block cursor of the file stands after the text that is typed
local function placeCursor()
    if not shown then return end
    local visible = false
    local x, y = 0, 0
    if typing() then
        local slot = slots[slotIndex]
        local rect = shown:rect(slot.tile)
        local depth = shown:rect('hacking_depth_rect')
        if rect and depth then
            local part = prefix(slot.text, typedChars)
            x = design(rect.x - depth.x) + (shown.menu:measure(FONT, part, 0).width or 0)
            y = design(rect.y - depth.y)
            visible = true
        end
    elseif phase == 'play' then
        local entry = shown:rect('hacking_password_entry')
        local depth = shown:rect('hacking_depth_rect')
        if entry and depth then
            x = design(entry.x - depth.x) + (shown.menu:measure(FONT, echoText:sub(1, echoTyped), 0).width or 0)
            y = design(entry.y - depth.y)
            visible = true
        end
    end
    visible = visible and (blinkClock % (2 * blink)) < blink
    shown:set('hacking_cursor', 'x', x)
    shown:set('hacking_cursor', 'y', y)
    shown:set('hacking_cursor', 'visible', visible and 1 or 0)
end

-- The depth rect (and the locked message that is as wide as it) is in the middle of the window
local function center()
    -- (the menu is laid out in 960 units from the top of the window to its bottom)
    local x = math.max(0, math.floor((shown.size.x * 960 / shown.size.y - DEPTH_WIDTH) / 2))
    shown:set('hacking_depth_rect', 'x', x)
    shown:set('hacking_locked_message_rect', 'x', x)
    shown:set('hacking_locked_message_rect', 'y', 48)
end

-- A line of the header or of the introduction: its tile, a text typed in it
local function addLine(parent, y)
    local tile = shown:instantiate(parent, TEMPLATES[parent], 'Line' .. #headerTiles .. parent)
    shown:set(tile, 'y', y)
    shown:set(tile, 'string', '')
    return tile
end

-- ---- The log ----

-- Lines are put in the log whole, the last of them at the bottom; the ones before move up a line each
local function log(texts)
    local count = #texts
    shown:batch(function()
        for _, tile in ipairs(logTiles) do
            tile.index = tile.index + count
            shown:set(tile.id, 'listindex', tile.index)
        end
        for i, text in ipairs(texts) do
            local id = shown:instantiate('hacking_password_log_rect', 'hacking_password_log_template',
                'Log' .. (#logTiles + 1))
            local index = count - i
            shown:set(id, 'listindex', index)
            shown:set(id, 'string', text)
            logTiles[#logTiles + 1] = { id = id, index = index }
        end
    end)
end

-- ---- The board ----

local function attemptsText()
    return string.format('%d %s', attempts, word('sHackingHeader3'))
end

-- The line with the attempts left and a block for each of them; the warning in place of the second line when one is left
local function showAttempts()
    shown:batch(function()
        if attemptsTile then
            local text = attemptsText()
            shown:set(attemptsTile, 'string', text)
            shown:set('hacking_header_rect', 'user1', rowCell * #text)
        end
        for i, block in ipairs(blocks) do
            shown:set(block, 'visible', i <= attempts and 1 or 0)
        end
        if headerTiles[2] then
            shown:set(headerTiles[2], 'string', attempts == 1 and word('sHackingWarning') or word('sHackingHeader2'))
        end
    end)
end

local function showRows()
    shown:batch(function()
        for i = 0, ROWS - 1 do
            shown:set(rowTiles[i], 'string', rowText(i))
        end
    end)
end

local function layBoard(terminal)
    dump, address = terminal.dump, terminal.address
    attempts, maxAttempts = terminal.attempts, terminal.maxAttempts
    wordLength = terminal.words[1] and #terminal.words[1] or 0
    shown:batch(function()
        shown:set('hacking_intro_rect', 'visible', 0)
        headerTiles = {}
        for i, y in ipairs({ 0, 30, ATTEMPTS_LINE_Y }) do
            local tile = shown:instantiate('hacking_header_rect', 'hacking_intro_template', 'Header' .. i)
            shown:set(tile, 'y', y)
            shown:set(tile, 'string', '')
            shown:set(tile, 'visible', 1)
            headerTiles[i] = tile
        end
        attemptsTile = headerTiles[3]
        shown:set('hacking_header_rect', 'user0', 0)
        shown:set('hacking_header_rect', 'user2', ATTEMPTS_LINE_Y)
        for i = 1, maxAttempts do
            local block = shown:instantiate('hacking_header_rect', 'hacking_guess_template', 'Block' .. i)
            shown:set(block, 'listindex', i - 1)
            blocks[i] = block
        end
        rowTiles, rowOfTile = {}, {}
        for i = 0, ROWS - 1 do
            local tile = shown:instantiate('hacking_password_file_rect', 'hacking_password_file_template')
            shown:set(tile, 'user0', math.floor(i / COLUMN_ROWS))
            shown:set(tile, 'listindex', i % COLUMN_ROWS)
            shown:set(tile, 'string', '')
            rowTiles[i] = tile
            rowOfTile[tile] = i
        end
    end)
    -- The width of a letter, from the first row of the file as it is laid out
    shown:set(rowTiles[0], 'string', rowText(0))
    local cell = (shown:get(rowTiles[0], 'width') or 0) / (ADDRESS_CHARS + ROW_SIZE)
    if cell > 0 then rowCell = cell end
    shown:set(rowTiles[0], 'string', '')
    -- What is typed: the header at the rate the game says things, then the rows of the dump, the first column and the second
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    textSlot(headerTiles[1], word('sHackingHeader'), outputRate)
    textSlot(headerTiles[2], word('sHackingHeader2'), outputRate)
    textSlot(headerTiles[3], attemptsText(), outputRate, function()
        shown:set('hacking_header_rect', 'user1', rowCell * #attemptsText())
        shown:set('hacking_header_rect', 'user0', 1)
    end)
    for i = 0, ROWS - 1 do
        textSlot(rowTiles[i], rowText(i), dumpRate, function() shown:set(rowTiles[i], 'visible', 1) end)
    end
    phase = 'board'
    shown:set('hacking_header_rect', 'user1', 0)
end

-- ---- What the pointer is on ----

local function clearHover()
    hoverFirst, hoverText = nil, ''
    shown:batch(function()
        shown:set('hacking_password_file_rect', 'user1', -1)
        shown:set('hacking_password_file_rect', 'user4', -1)
        shown:set('hacking_password_file_rect', 'user0', '')
        shown:set('hacking_password_file_rect', 'user3', '')
    end)
    echoText, echoTyped, echoCarry = '', 0, 0
    shown:set('hacking_password_entry', 'string', '')
end

-- Where the character of the dump (from 1) is on the screen: its row and the place in the row
local function placeOf(position)
    local index = position - 1
    return math.floor(index / ROW_SIZE), index % ROW_SIZE
end

-- The box that lights a part of a unit that lies in one row: the text, and its place in the file rect (the traits of the
-- file rect that stand for the box)
local function lightPart(first, length, boxText, boxX, boxY)
    local row, column = placeOf(first)
    local tile = rowTiles[row]
    if not tile then return end
    shown:set('hacking_password_file_rect', boxText, dump:sub(first, first + length - 1))
    shown:set('hacking_password_file_rect', boxX, (shown:get(tile, 'x') or 0) + (ADDRESS_CHARS + column) * rowCell)
    shown:set('hacking_password_file_rect', boxY, shown:get(tile, 'y') or 0)
end

-- The unit that the character at `position` belongs to is lit (and its text typed after the prompt of the log)
local function light(unit)
    if hoverFirst == unit.first then return end
    hoverFirst = unit.first
    local _, column = placeOf(unit.first)
    local inFirst = math.min(unit.length, ROW_SIZE - column)
    shown:batch(function()
        lightPart(unit.first, inFirst, 'user0', 'user1', 'user2')
        if inFirst < unit.length then
            lightPart(unit.first + inFirst, unit.length - inFirst, 'user3', 'user4', 'user5')
        else
            shown:set('hacking_password_file_rect', 'user4', -1)
        end
    end)
    hoverText = dump:sub(unit.first, unit.first + unit.length - 1)
    echoText, echoTyped, echoCarry = hoverText, 0, 0
    shown:set('hacking_password_entry', 'string', '')
end

-- The place of the dump (from 1) the pointer is on, nil when it is on no character of it
local function positionAt(tile)
    local row = rowOfTile[tile]
    local pointer = shown:pointerAt()
    local rect = shown:rect(tile)
    if not row or not pointer or not rect then return nil end
    local at = math.floor((pointer.x - rect.x) / (rect.width / (ADDRESS_CHARS + ROW_SIZE)))
    if at < ADDRESS_CHARS or at >= ADDRESS_CHARS + ROW_SIZE then return nil end
    return row * ROW_SIZE + (at - ADDRESS_CHARS) + 1
end

local function hover()
    local tile = shown.menu:hovered()
    local position = tile and positionAt(tile)
    if not position then
        if hoverFirst then clearHover() end
        return
    end
    local unit = interact.hackUnit(position)
    if not unit then
        if hoverFirst then clearHover() end
        return
    end
    light(unit)
end

-- ---- A choice ----

local function startSlide()
    phase = 'lockout'
    slideLeft, slideAt = SLIDE_STEP, 0
end

local function solved()
    phase = 'solved'
    waitLeft = SOLVED_WAIT
end

local function choose(position)
    if phase ~= 'play' then return end
    local result = interact.hackPick(position)
    if not result then return end
    local prompt = '>'
    local texts = { prompt .. result.text }
    if result.kind == 'brackets' then
        if result.effect == 'denied' then
            texts[#texts + 1] = prompt .. word('sHackingDenied')
        elseif result.effect == 'removed' then
            texts[#texts + 1] = prompt .. word('sHackingDudRemoved')
        elseif result.effect == 'replenished' then
            texts[#texts + 1] = prompt .. word('sHackingToleranceReset1')
            texts[#texts + 1] = prompt .. word('sHackingToleranceReset2')
        end
    elseif result.correct then
        texts[#texts + 1] = prompt .. word('sHackingGranted')
        texts[#texts + 1] = prompt .. word('sHackingAccessing1')
        texts[#texts + 1] = prompt .. word('sHackingAccessing2')
        texts[#texts + 1] = prompt .. word('sHackingAccessing3')
    else
        texts[#texts + 1] = prompt .. word('sHackingDenied')
        texts[#texts + 1] = string.format('%s%d/%d %s.', prompt, result.likeness, wordLength, word('sHackingCorrect'))
        if result.lockedOut then
            texts[#texts + 1] = prompt .. word('sHackingLockout1')
            texts[#texts + 1] = word('sHackingLockout2')
        end
    end
    log(texts)
    if result.kind == 'brackets' then
        local terminal = interact.terminal()
        if terminal then
            dump, attempts = terminal.dump, terminal.attempts
            showRows()
            showAttempts()
        end
    else
        attempts = result.attempts
        showAttempts()
    end
    clearHover()
    if result.correct then
        solved()
    elseif result.lockedOut then
        startSlide()
    end
end

-- ---- The introduction ----

local function startIntro(terminal)
    phase = 'intro'
    slots, slotIndex, typedChars, carry = {}, 1, 0, 0
    local lines = {
        { 'sHackingSecurityReset', 0, outputRate },
        { 'sHackingIntro01', 60, outputRate },
        { 'sHackingIntro02', 120, inputRate },
        { 'sHackingIntro03', 180, outputRate },
        { 'sHackingIntro04', 240, inputRate },
    }
    shown:batch(function()
        for i, line in ipairs(lines) do
            local tile = shown:instantiate('hacking_intro_rect', 'hacking_intro_template', 'Intro' .. i)
            shown:set(tile, 'y', line[2])
            shown:set(tile, 'string', '')
            shown:set(tile, 'visible', 1)
            textSlot(tile, word(line[1]), line[3])
        end
    end)
end

-- The board comes: at once from the introduction when the seconds since the last screen of the game are over
local function endIntro()
    local terminal = interact.terminal()
    if not terminal or not terminal.hacking or terminal.dump == '' then return end
    if interact.hackGate() > 0 then
        phase = 'gate'
        return
    end
    typeAll()
    layBoard(terminal)
end

function module.skipIntro()
    if phase == 'intro' or phase == 'gate' then
        typeAll()
        endIntro()
    end
end

-- A key (or a button of the pad that stands for one); true when the screen took it
function module.press(code)
    if not shown then return false end
    local KEY = input.KEY
    if code == KEY.Enter or code == KEY.Space then
        if phase == 'intro' or phase == 'gate' then
            module.skipIntro()
        elseif phase == 'locked' and hooks then
            hooks.leave()
        end
        return true
    elseif code == KEY.Backspace then
        -- The game's key that cancels leaves the screen (it costs no attempt)
        if hooks and phase ~= 'lockout' and phase ~= 'solved' then hooks.leave() end
        return true
    end
    return false
end

-- The screen of the game of hacking for the terminal that the engine says is open (core.interact.terminal()). `handlers`:
-- leave(). Called again for the same board while the engine's screen changes, nothing is done then. Nothing is drawn,
-- and false is given, when the file is not in the install.
function module.start(terminal, handlers)
    local key = terminal.dump ~= '' and (tostring(terminal.address) .. ':' .. table.concat(terminal.words, ',')) or 'locked'
    if shown and boardKey == key then
        hooks = handlers
        return true
    end
    module.close()
    local opened = menuModule.open(FILE, {})
    if not opened then return false end
    for _, name in ipairs(TILES) do
        if not opened:find(name) then
            opened:close()
            return false
        end
    end
    shown, hooks, boardKey = opened, handlers, key
    outputRate = rateOf('iHackingOutputRate', DEFAULT_OUTPUT_RATE)
    inputRate = rateOf('iHackingInputRate', DEFAULT_INPUT_RATE)
    dumpRate = rateOf('iHackingDumpRate', DEFAULT_DUMP_RATE)
    blink = (shown:get('hacking_cursor', '_blink_interval') or 400) / 1000
    blinkClock = 0
    shown:onClick('hacking_background', function()
        if phase == 'intro' or phase == 'gate' then module.skipIntro() end
    end)
    shown:onClick('hacking_password_file_target', function(tile)
        local position = positionAt(tile)
        if position then choose(position) end
    end)
    shown:batch(function()
        shown:io('user0', '>')
        shown:io('user3', 0)
        center()
        shown:set('hacking_cursor', 'visible', 0)
        shown:set('hacking_password_entry', 'string', '')
        if key == 'locked' then
            shown:set('hacking_locked_message_text_1', 'string', word('sHackingLockout3'))
            shown:set('hacking_locked_message_text_2', 'string', word('sHackingLockout4'))
            shown:io('user2', 1)
            shown:set('hacking_intro_rect', 'visible', 0)
            shown:set('hacking_depth_rect', 'visible', 0)
        end
    end)
    if key == 'locked' then
        phase = 'locked'
    else
        startIntro(terminal)
        -- The words of the log's own prompt, the dump, the attempts: what the engine says now
        dump, address = terminal.dump, terminal.address
        attempts, maxAttempts = terminal.attempts, terminal.maxAttempts
        wordLength = terminal.words[1] and #terminal.words[1] or 0
    end
    return true
end

-- Frame by frame: the typing goes on, the cursor blinks, the pointer lights what it is on
function module.update(dt)
    if not shown then return end
    local before = shown.size
    shown:checkSize()
    if shown.size ~= before then center() end
    blinkClock = blinkClock + dt
    if phase == 'solved' then
        waitLeft = waitLeft - dt
        if waitLeft <= 0 then
            phase = 'finished'
            interact.hackFinish()
        end
        return
    elseif phase == 'lockout' then
        slideLeft = slideLeft - dt
        while slideLeft <= 0 and phase == 'lockout' do
            slideLeft = slideLeft + SLIDE_STEP
            slideAt = slideAt + SLIDE_UNITS
            shown:io('user1', slideAt)
            if slideAt >= SLIDE_END then
                phase = 'finished'
                if hooks then hooks.leave() end
            end
        end
        return
    elseif phase == 'gate' then
        endIntro()
    end
    if not shown then return end
    if typing() then
        local slot = slots[slotIndex]
        carry = carry + dt * slot.rate
        local count = math.floor(carry)
        if count > 0 then
            carry = carry - count
            shown:batch(function()
                while count > 0 and typing() do
                    slot = slots[slotIndex]
                    if typedChars == 0 and slot.first then slot.first() end
                    local step = math.min(count, #slot.text - typedChars)
                    typedChars = typedChars + step
                    count = count - step
                    slot.apply(prefix(slot.text, typedChars))
                    if typedChars >= #slot.text then
                        slotIndex = slotIndex + 1
                        typedChars = 0
                    end
                end
                placeCursor()
            end)
        else
            shown:batch(placeCursor)
        end
        if not typing() then
            if phase == 'intro' then
                endIntro()
            elseif phase == 'board' then
                phase = 'play'
                showAttempts()
            end
        end
        return
    end
    if phase == 'intro' then
        endIntro()
    elseif phase == 'play' then
        shown:batch(function()
            hover()
            -- The line of the log types what is lit, as the player would
            if echoTyped < #echoText then
                echoCarry = echoCarry + dt * inputRate
                local count = math.floor(echoCarry)
                if count > 0 then
                    echoCarry = echoCarry - count
                    echoTyped = math.min(#echoText, echoTyped + count)
                    shown:set('hacking_password_entry', 'string', echoText:sub(1, echoTyped))
                end
            end
            placeCursor()
        end)
    elseif phase == 'locked' then
        shown:batch(placeCursor)
    end
end

return module
