-- The help manual of Fallout 3 and New Vegas on the game's own menu file (tutorial_menu.xml): the pages of the manual
-- (the messages of the form list `HelpManual`, or `HelpManualXBox` for a pad), the title and the words of the page that is
-- shown, the number of the page (`1/12`), Previous and Next, and the scrollbar of a page that is longer than the room.
-- The file lays the screen out; what the game's code puts in it (Mac round 12AN B4, FalloutNV.exe 0x7E8890) is here: the
-- title is the name of the message, the words are its text (marked up text when it begins with `<`, laid out by the menu
-- engine like the words of a book), `page/count` is the number of the page, Previous is clickable when there is a page
-- before and Next when there is one after (a button that cannot be clicked stays, it is not hidden), and each page starts
-- at the top of its text. A message that is not a page of the manual (a hint that the game asks for by name) is shown
-- alone, without the number of a page. The screens of a hint that comes by itself while the player plays (the game asks
-- for them from a queue when the help is on) are not here. It is a module of the pause menu (the Help item), not a script.
--
--   manual.show({ start = 'HelpPipBoyItems', pad = false, layer = 'Settings', onLeave = function() end })
--   manual.isOpen(); manual.press(keyCode, typedLetter); manual.update(); manual.leave(); manual.close()
--
-- Not there yet: the keys of the screen on the PC are the file's hints (A next, S previous, E close) and the arrows and
-- Escape here; the wheel scrolls the text (the file does that).
local core = require('openfallout.core')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')
local bookModule = require('scripts.omw.falloutbook')

local pipboy = core.pipboy

local module = {}

local FILE = 'tutorial_menu.xml'

local shown = nil
local pages = {}
local index = 1
-- The message is no page of the manual: no number of a page, and no other page to go to
local alone = false
local onLeave = nil

function module.isOpen()
    return shown ~= nil
end

-- The screen goes, and nothing is told
function module.close()
    if shown then
        shown:close()
        shown = nil
    end
    pages, index, alone, onLeave = {}, 1, false, nil
end

-- The player leaves the screen (the button, a key)
function module.leave()
    local told = onLeave
    module.close()
    if told then told() end
end

-- The page that is shown goes into the tiles: its title, its words, `page/count`, which buttons can be clicked
local function showPage()
    local page = pages[index]
    local text = bookModule.filled(page.text or '')
    shown:batch(function()
        shown:set('TM_title', 'string', page.title or '')
        shown:set('TM_text', 'ishtml', text:sub(1, 1) == '<' and 1 or 0)
        shown:set('TM_text', 'string', text)
        if alone then
            shown:set('TM_page', 'visible', 0)
        else
            shown:set('TM_page', 'string', string.format('%d/%d', index, #pages))
        end
        shown:set('TM_prev_button', 'target', (not alone and index > 1) and 1 or 0)
        shown:set('TM_next_button', 'target', (not alone and index < #pages) and 1 or 0)
    end)
    -- each page starts at the top of its text
    shown:setScrollbar('TM_scrollbar', 0)
end

local function move(step)
    local to = index + step
    if alone or to < 1 or to > #pages then
        return
    end
    index = to
    showPage()
end

-- Show the manual at a page: `start` is the editor id of the message the screen opens at (the first page when there is
-- none). False when there is nothing to show, or the menu file is not there or lacks what the screen needs.
function module.show(options)
    module.close()
    options = options or {}
    local manual = pipboy.helpManual(options.pad == true)
    pages, index, alone = {}, 1, false
    for _, page in ipairs(manual) do pages[#pages + 1] = page end
    local start = options.start
    if start and start ~= '' then
        local found = nil
        for number, page in ipairs(pages) do
            if page.id:lower() == start:lower() then found = number break end
        end
        if found then
            index = found
        else
            local message = pipboy.helpMessage(start)
            if not message then
                pages = {}
            else
                pages, index, alone = { message }, 1, true
            end
        end
    end
    if #pages == 0 then
        return false
    end
    local opened = menuModule.open(FILE, { layer = options.layer })
    if not opened then
        pages = {}
        return false
    end
    shown = opened
    onLeave = options.onLeave
    -- A tile the file does not have is an error; a screen that cannot show the words is closed, not left open
    local ready = pcall(showPage)
    if not ready then
        module.close()
        return false
    end
    opened:onClick('TM_next_button', function() move(1) end)
    opened:onClick('TM_prev_button', function() move(-1) end)
    opened:onClick('TM_close_button', module.leave)
    return true
end

-- Each frame the screen is open: the window size
function module.update()
    if shown then
        shown:checkSize()
    end
end

-- The keys while the screen is there: the letters the file gives (A next, S previous, E close), the arrows turn the pages
-- and Escape leaves. True when the screen took the key.
function module.press(code, symbol)
    if not shown then
        return false
    end
    local KEY = input.KEY
    if code == KEY.Escape then
        module.leave()
    elseif code == KEY.RightArrow or code == KEY.PageDown then
        shown:clickNamed('TM_next_button')
    elseif code == KEY.LeftArrow or code == KEY.PageUp then
        shown:clickNamed('TM_prev_button')
    else
        shown:pcButton(symbol)
    end
    return true
end

return module
