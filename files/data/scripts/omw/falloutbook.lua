-- The screen of a book or a scroll in Fallout 3 and New Vegas, drawn on the game's own menu file (book_menu.xml): the two
-- pages of a book with the arrows that turn them and the numbers of the pages, or the roll of a scroll with its scrollbar,
-- and the buttons that leave it and (for a book that lies in the world) take it. The file lays out the screen and turns
-- the pages itself (the page numbers count the clicks of the arrows); what the game's code puts in it is here, as the Mac
-- read it from the executable (round 12AM B4): the menu's own traits hold the words (`user2`), whether it is a book and
-- not a scroll (`user0`) and whether it can be taken (`user3`), the words are laid out as marked up text (the menu engine
-- does that: the marks, the pages, the number of pages) and the names of settings in them (`&sName;`) are the words of
-- the game's settings. It is a module of the screens that show a book (the Pip-Boy's items, and the interaction script
-- for a book that lies in the world), not a script.
--
--   book.show({ name, text, scroll, canTake }, { onLeave = function() end, onTake = function() end })
--   book.isOpen(); book.press(keyCode, typedLetter); book.update(); book.close()
--
-- Not there yet: the keys the game gives the screen on the PC are not known, so these are the arrows, the pages and the
-- wheel for the pages and the roll, Escape and Backspace to leave and T to take.
local core = require('openfallout.core')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')

local pipboy = core.pipboy

local module = {}

local FILE = 'book_menu.xml'

local shown = nil
-- A scroll is a roll with a scrollbar and not two pages
local isScroll = false
-- What is done when the player leaves the screen, and when the player takes the book
local onLeave, onTake = nil, nil

-- The words of the game's settings in the text (`&sName;`; no other name stands for anything)
local function filled(text)
    return (text:gsub('&([%w_]+);', function(name)
        local word = pipboy.text(name)
        if word ~= nil and word ~= '' then return word end
        return nil
    end))
end

-- The words of a text with the names of the game's settings filled in (the screens of the help manual use it too)
module.filled = filled

function module.isOpen()
    return shown ~= nil
end

-- The screen goes, and nothing is told (the window it was in is gone, or a screen is shown in its place)
function module.close()
    if shown then
        shown:close()
        shown = nil
    end
    onLeave, onTake = nil, nil
end

-- The player leaves the screen (a button, a key)
function module.leave()
    local told = onLeave
    module.close()
    if told then told() end
end

local function take()
    local told = onTake
    module.close()
    if told then told() end
end

-- Show a book or a scroll: `book` has what `core.pipboy.book` and `core.interact.book` give (the words, whether it is a
-- scroll, whether it can be taken). False when the menu file is not there or does not have what the screen needs.
function module.show(book, options)
    module.close()
    local opened = menuModule.open(FILE)
    if not opened then
        print('Book screen: the menu file did not open')
        return false
    end
    -- Setting a tile the file does not have is an error; a screen that cannot show the words is closed, not left open
    local ready, failure = pcall(opened.batch, opened, function()
        opened:io('user0', book.scroll and 0 or 1)
        opened:io('user2', filled(book.text or ''))
        opened:io('user3', book.canTake and 1 or 0)
    end)
    if not ready then
        print('Book screen: the words could not be set: ' .. tostring(failure))
        opened:close()
        return false
    end
    print('Book screen: shown as ' .. (book.scroll and 'a scroll' or 'a book'))
    shown, isScroll = opened, book.scroll == true
    onLeave = options and options.onLeave
    onTake = options and options.onTake
    opened:onClick('book_exit', module.leave)
    opened:onClick('book_scroll_exit', module.leave)
    opened:onClick('book_take', take)
    opened:onClick('book_scroll_take', take)
    -- The wheel turns the pages of a book (the code does, the file says so in the comment of `user8`); a scroll's roll
    -- takes the wheel itself
    opened:onWheel(function(notches, tile)
        if not tile and not isScroll then
            opened:clickNamed(notches > 0 and 'book_next' or 'book_prev')
        end
    end)
    return true
end

-- Each frame the screen is open: the window size
function module.update()
    if shown then
        shown:checkSize()
    end
end

-- The keys while the screen is there: the arrows and the pages turn the pages of a book and roll a scroll, Escape and
-- Backspace leave, T takes. True when the screen took the key.
function module.press(code)
    if not shown then
        return false
    end
    local KEY = input.KEY
    if code == KEY.Escape or code == KEY.Backspace then
        module.leave()
    elseif code == KEY.T then
        shown:clickNamed(isScroll and 'book_scroll_take' or 'book_take')
    elseif isScroll then
        if code == KEY.UpArrow or code == KEY.PageUp then
            shown:clickNamed('book_scroll_up')
        elseif code == KEY.DownArrow or code == KEY.PageDown then
            shown:clickNamed('book_scroll_down')
        end
    elseif code == KEY.LeftArrow or code == KEY.UpArrow or code == KEY.PageUp then
        shown:clickNamed('book_prev')
    elseif code == KEY.RightArrow or code == KEY.DownArrow or code == KEY.PageDown then
        shown:clickNamed('book_next')
    end
    return true
end

return module
