-- The message boxes of Fallout 3 and New Vegas (a script's ShowMessage with its buttons, the box that names a reputation
-- and the like), drawn on the game's own menu file (message_menu.xml). The file lays out the box; what the game's code
-- puts in it is here, as the Mac read it from the executable (round 12Q): the title and the words of the message, the
-- buttons in the order of the record, the width of the box from the widest of the title, the words and the buttons (kept
-- between the file's least and most), the words and the title both centred when the words are one line and both to the
-- left when they are more, and a message with no button that closes itself once its display time is over. Which message
-- is open and which button was picked is the engine's (core.messagebox); this script shows the box and presses the
-- button. A game whose data lacks the file keeps the window of the engine's own: the script gives the boxes back to it.
local core = require('openfallout.core')
local I = require('openfallout.interfaces')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')

local messagebox = core.messagebox
if not (core.pipboy and core.pipboy.available()) or not messagebox or not messagebox.claim then
    return {}
end

local FILE = 'message_menu.xml'
-- Font 2 is the text of the box and the buttons, font 6 its title
local TEXT_FONT, TITLE_FONT = 2, 6
-- What the code adds to the width of the widest title and to that of the widest button
local TITLE_EXTRA, BUTTON_EXTRA = 100, 80
-- The room the file's own button text leaves in the wrap width (its wrapwidth is the most the box is wide, less the
-- room on both sides, less 60)
local BUTTON_ROOM = 60
-- The most the file lets the list of buttons be high (<min> 420 on its height): the rows beyond that scroll
local LIST_MOST = 420
-- The icon of a message is not shown yet: the records that hold the pictures (MICN) are not read by the engine

local shown = nil
-- The box that is shown: its serial, the time it has been open, what it asks for (the buttons, or the time), and
-- whether its answer has been given
local serial, openFor, current, answered = nil, 0, nil, false
local list = nil
local ownsMode = false
-- The game's own window is back in charge of the boxes (the file is not there, or does not have what the box needs)
local given = false

local function closeMode()
    if ownsMode then
        ownsMode = false
        I.UI.removeMode('Interface')
    end
end

local function close()
    if shown then
        shown:close()
        shown = nil
    end
    list = nil
    serial, current, answered = nil, nil, false
    closeMode()
end

local function giveBack()
    given = true
    close()
    messagebox.claim(false)
end

-- The button `index` (from 1) is picked: said once, the engine closes the box in the next frame
local function press(index)
    if answered or not current then return end
    answered = true
    messagebox.press(index)
end

local function numberOf(tile, trait, default)
    local value = shown.menu:getNumber(tile, trait)
    return value or default
end

-- Put the message in the file's tiles and size the box as the code of the game does
local function fill(box)
    local menu = shown.menu
    local root = menu:root()
    local minWidth = numberOf(root, '_MinMenuWidth', 200)
    local maxWidth = numberOf(root, '_MaxMenuWidth', 700)
    local border = numberOf(root, '_horbuf', 40)
    local title = box.title or ''
    local body = box.body or ''
    if title == '' and body == '' then body = box.text or '' end
    local labels = {}
    if not box.timed then
        for index, label in ipairs(box.buttons) do labels[index] = label end
    end

    -- The words wrap at what is left of the most the box is wide when the sides are kept free (no icon is shown)
    local wrap = maxWidth - 2 * border
    local words = menu:measure(TEXT_FONT, body, wrap)
    -- The words are centred when they are one line and to the left when they are more, and the title goes with them
    local justify = (words.lines or 1) > 1 and 1 or 2

    local widestButton = 0
    local buttonWrap = wrap - BUTTON_ROOM
    for _, label in ipairs(labels) do
        widestButton = math.max(widestButton, menu:measure(TEXT_FONT, label, buttonWrap).width or 0)
    end
    local buttonsWidth = widestButton + BUTTON_EXTRA
    local titleWidth = (menu:measure(TITLE_FONT, title, 0).width or 0) + TITLE_EXTRA
    local textWidth = (words.width or 0) + 2 * border
    -- The widest of the three, never more than the most and never less than the least (the least is applied last)
    local width = math.max(math.max(titleWidth, textWidth), buttonsWidth)
    width = math.max(math.min(width, maxWidth), minWidth)

    shown:batch(function()
        shown:set('MM_Title', 'string', title)
        shown:set('MM_Title', 'visible', title ~= '' and 1 or 0)
        shown:set('MM_Title', 'justify', justify)
        shown:set('MM_MessageText', 'string', body)
        shown:set('MM_MessageText', 'justify', justify)
        shown:set('MM_MessageIcon', 'visible', 0)
        shown:set('MM_MainRect', 'width', width)
        local rows = {}
        for index, label in ipairs(labels) do rows[index] = { string = label } end
        list.rows(rows)
        -- The list is as high as its rows are together, and as wide as the widest button needs
        local height = 0
        for _, row in ipairs(menu:children(menu:find('MM_ButtonList'))) do
            if menu:name(row) == 'MM_Button' then
                height = height + (numberOf(row, 'height', 0) or 0)
            end
        end
        shown:set('MM_ButtonList', 'height', math.min(height, LIST_MOST))
        shown:set('MM_ButtonList', 'width', buttonsWidth)
        -- The file's own count of what the list holds takes every row as high as a step of the scrollbar, which says
        -- more than the list has room for when the rows are taller than a step: the bar is shown when the rows do not
        -- fit in the list (the file says it is shown when "list items' height exceed the list height")
        shown:set('MM_ButtonList', '_scrollbar_vis', height > LIST_MOST and 1 or 0)
    end)
    -- The rows are the buttons: a click on one picks it
    list.onClick(function(row) press(row) end)
end

-- The box that the engine has open is drawn, on top of whatever is open
local function show(box)
    close()
    local opened = menuModule.open(FILE, { layer = 'Modal' })
    if not opened then
        giveBack()
        return
    end
    shown = opened
    list = opened:list('MM_ButtonList', 'MM_ButtonTemplate')
    local ready = list ~= nil and pcall(fill, box)
    if not ready then
        print('message box: the file ' .. FILE .. ' does not have what the box needs; the game\'s own window is used')
        giveBack()
        return
    end
    serial, current, openFor, answered = box.serial, box, 0, false
    -- With nothing open the world stops and the pointer shows; over a screen that is open the box is just on top
    if I.UI.getMode() == nil then
        I.UI.addMode('Interface', { windows = {} })
        ownsMode = true
    end
end

local function labelStartsWith(label, symbol)
    return label ~= '' and symbol ~= '' and label:sub(1, 1):upper() == symbol:upper()
end

local function onKey(key)
    if not shown or not current or answered then return end
    local count = #current.buttons
    local code = key.code
    local KEY = input.KEY
    if current.timed or count == 0 then return end
    if code == KEY.Escape then
        -- The last button is the way out, as in the game
        press(count)
        return
    end
    if list then
        if code == KEY.UpArrow or code == KEY.DownArrow then
            local at = list.selected()
            local step = code == KEY.UpArrow and -1 or 1
            if at == 0 then at = step > 0 and 0 or count + 1 end
            list.select(math.max(1, math.min(count, at + step)), true)
            return
        end
        if code == KEY.Enter or code == KEY.Space then
            -- The lit button, and the first one when none is lit
            local at = list.selected()
            press(at > 0 and at or 1)
            return
        end
    end
    -- Another key picks the first button whose words start with it
    local symbol = key.symbol or ''
    for index, label in ipairs(current.buttons) do
        if labelStartsWith(label, symbol) then
            press(index)
            return
        end
    end
end

return {
    engineHandlers = {
        onFrame = function()
            if given then return end
            -- The script says every frame that it draws the boxes: a script that is gone stops being asked
            messagebox.claim(true)
            local box = messagebox.current()
            if not box then
                if shown then close() end
                return
            end
            if not shown or box.serial ~= serial then
                show(box)
                return
            end
            shown:checkSize()
            -- The mode of the box is taken back when it was left (Escape) and the box is still open
            if ownsMode and I.UI.getMode() == nil then
                I.UI.addMode('Interface', { windows = {} })
            end
            -- A message with no button closes itself when its time is over
            if current.timed and not answered then
                openFor = openFor + core.getRealFrameDuration()
                if openFor >= (current.time or 0) then press(1) end
            end
        end,
        onKeyPress = onKey,
    },
}
