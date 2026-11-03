-- The credits of Fallout 3 and New Vegas, drawn on the game's own menu file (options/credits_menu.xml, id 0x417), as the
-- executable reads and shows them (FalloutNV.exe 0x75FB90 loads and lays out, 0x75F9B8 scrolls; Mac round 12AO B5).
--
-- The text is Data\<LANGUAGE>\CREDITS.TXT (or Data\CREDITS.TXT), with a CREDITSPATCH.TXT next to it that replaces it when it
-- is there, and the Wild Wasteland variant CREDITSWACKY.TXT when the player has that trait. It is one buffer, not pages: a
-- line that has only a line end is a blank line, a line that begins with '*' is a comment, and every other run of
-- characters up to a line end, a '<', a '>' or a '*' is a piece of text that is a tile of its own, put under the one before
-- (the height of its font). The marks in angle brackets set the state: <Xn> <X+n> <X-n> the place across, <Yn> <Y+n> <Y-n> the
-- place down ('+' moves down), <JC> <JR> <JL> the alignment, <Fn> the font, <C r g b a> the colour (each 0 to 255), <I w h a
-- name> a picture (its size, a number that is not read, its file) and <B name> a background picture (a file under
-- Interface\Credits) that the screen shows in turn. The block starts a screen below the window and goes up by
-- fCreditsScrollSpeed (55) units a second, on the 1280 units wide menu; the screen ends when the last line has left it, or
-- with Escape. What it asks after it (sContinuePlayingAfterCredits) and what starts it (the global PlayCredits, 1 for the
-- credits and 2 for the Wild Wasteland ones) are the game's, not here: this module only shows the screen.
--
--   local credits = require('scripts.omw.falloutcredits')
--   credits.show({ onDone = function() end })  -- false when the data has no credits file or no menu file
--   credits.update(dt); credits.press(keyCode); credits.isOpen(); credits.close()
local input = require('openfallout.input')
local vfs = require('openfallout.vfs')

local menuModule = require('scripts.omw.falloutmenu')

local module = {}

local FILE = 'options/credits_menu.xml'
-- The start of the block below the window, and the setting's own default for the speed
local START_Y = 1020
local SPEED = 55
-- The width of the menu (the text is not wrapped inside it) and how far out of the window a line is made
local WIDTH = 1280
local MARGIN = 120
local WINDOW_HEIGHT = 960

-- The state a file starts with: font 2, left aligned, at the origin, white
local function initial()
    return { font = 2, align = 1, x = 0, y = 0, color = { 255, 255, 255, 255 } }
end

local function number(text)
    return tonumber(text:match('^%s*(%-?%d+)')) or 0
end

-- One mark: the state it changes (and, for a picture or a background, what it adds)
local function applyTag(tag, state, result, lineHeight)
    local letter = tag:sub(1, 1):upper()
    local rest = tag:sub(3)
    if letter == 'X' then
        local sign = tag:sub(3, 3)
        if sign == '+' then state.x = state.x + number(tag:sub(4))
        elseif sign == '-' then state.x = state.x - number(tag:sub(4))
        else state.x = number(rest) end
    elseif letter == 'Y' then
        -- The executable's Y runs down the page as a smaller number, so '+' moves down
        local sign = tag:sub(3, 3)
        if sign == '+' then state.y = state.y + number(tag:sub(4))
        elseif sign == '-' then state.y = state.y - number(tag:sub(4))
        else state.y = number(rest) end
    elseif letter == 'J' then
        local kind = tag:sub(3, 3):upper()
        state.align = kind == 'C' and 2 or kind == 'R' and 4 or 1
    elseif letter == 'F' then
        state.font = number(tag:sub(3, 3)) >= 1 and number(tag:sub(3, 3)) or state.font
    elseif letter == 'C' then
        -- Four numbers at fixed offsets; each is read as far as its digits go, as the executable's atoi does
        state.color = { number(tag:sub(3)), number(tag:sub(7)), number(tag:sub(11)), number(tag:sub(15)) }
    elseif letter == 'I' then
        -- Three numbers at offsets 2, 7 and 12 (each read as far as its digits go), then the name at offset 16
        local width, height = number(tag:sub(3)), number(tag:sub(8))
        local name = tag:sub(17):match('^%s*(.-)%s*$')
        local x = state.x
        if state.align == 2 then x = x - width / 2 elseif state.align == 4 then x = x - width end
        result.items[#result.items + 1] = { kind = 'image', name = name, x = x, y = state.y, width = width, height = height }
        result.extent = math.max(result.extent, state.y + height)
        state.y = state.y + height
        result.height = result.height + height
    elseif letter == 'B' then
        local name = tag:sub(3):match('^%s*(.-)%s*$')
        if name ~= '' then result.backgrounds[#result.backgrounds + 1] = 'Interface\\Credits\\' .. name end
    end
end

-- The pieces of a credits file: `items` (texts and pictures, in order, each with its place in the block: x across from the
-- middle, y down from the top of the block), `backgrounds` (the files to show in turn), `height` (what the lines and the
-- pictures add up to) and `extent` (how far down the block reaches: a <Y> mark can put a piece below the sum).
-- `lineHeight(font)` is the height of a line of the font.
function module.parse(text, lineHeight)
    local result = { items = {}, backgrounds = {}, height = 0, extent = 0 }
    local state = initial()
    local i, size = 1, #text
    local atLineStart = true
    while i <= size do
        local char = text:sub(i, i)
        if char == '\r' then
            -- A line that has nothing but its end is a blank line
            if atLineStart then
                local height = lineHeight(state.font)
                state.y = state.y + height
                result.height = result.height + height
            end
            i = i + 1
        elseif char == '\n' then
            atLineStart = true
            i = i + 1
        elseif char == '*' then
            -- A comment, up to the end of the line
            local finish = text:find('\n', i, true)
            i = finish or (size + 1)
            atLineStart = false
        elseif char:byte() < 32 then
            i = i + 1
        elseif char == '<' then
            local finish = text:find('>', i, true)
            if not finish then break end
            applyTag(text:sub(i + 1, finish - 1), state, result, lineHeight)
            i = finish + 1
            atLineStart = false
        elseif char == '>' then
            i = i + 1
            atLineStart = false
        else
            local piece = text:match('^[^\r\n<>*]+', i)
            local used = #piece
            if used > 255 then piece = piece:sub(1, 255) end
            result.items[#result.items + 1] = { kind = 'text', text = piece, x = state.x, y = state.y, font = state.font,
                align = state.align, color = { state.color[1], state.color[2], state.color[3], state.color[4] } }
            local height = lineHeight(state.font)
            result.extent = math.max(result.extent, state.y + height)
            state.y = state.y + height
            result.height = result.height + height
            i = i + used
            atLineStart = false
        end
    end
    return result
end

-- The names of the files to try, in the order the executable decides between them: the language folder's credits (and its
-- patch, which replaces it) else the plain folder's. The executable's paths start with Data\ ; the virtual file system of the
-- engine is rooted at the data folder, so they are given from there.
function module.candidates(language, wacky)
    local name = wacky and 'CREDITSWACKY' or 'CREDITS'
    local folder = language:upper() .. '\\'
    return {
        language = folder .. name .. '.TXT', languagePatch = folder .. name .. 'PATCH.TXT',
        plain = name .. '.TXT', plainPatch = name .. 'PATCH.TXT',
    }
end

local function slurp(path)
    local ok, exists = pcall(vfs.fileExists, path)
    if not ok or not exists then return nil end
    local opened = vfs.open(path)
    if not opened then return nil end
    local text = opened:read('*a')
    opened:close()
    return text
end

-- The text of the credits file the executable would use, nil when there is none
function module.load(language, wacky)
    local names = module.candidates(language or 'ENGLISH', wacky)
    local base = slurp(names.language)
    if base ~= nil then return slurp(names.languagePatch) or base end
    return slurp(names.plainPatch) or slurp(names.plain)
end

local shown = nil
local state = nil

function module.isOpen()
    return shown ~= nil
end

function module.close()
    if shown then shown:close() end
    shown, state = nil, nil
end

local function finish()
    local done = state and state.onDone
    module.close()
    if done then done() end
end

-- Whether the piece is near enough to the window to be made
local function near(item, y)
    local top = item.y + y
    local bottom = top + (item.height or 40)
    return bottom > -MARGIN and top < WINDOW_HEIGHT + MARGIN
end

local function make(index)
    local item = state.parsed.items[index]
    local m = shown.menu
    local name = 'credits_piece' .. index
    if item.kind == 'text' then
        local tile = m:create(state.rect, 'text', name)
        if not tile then return nil end
        m:set(tile, 'font', item.font)
        m:set(tile, 'justify', item.align)
        m:set(tile, 'wrapwidth', WIDTH)
        m:set(tile, 'x', item.x)
        m:set(tile, 'y', item.y)
        m:set(tile, 'red', item.color[1])
        m:set(tile, 'green', item.color[2])
        m:set(tile, 'blue', item.color[3])
        m:set(tile, 'alpha', item.color[4])
        m:set(tile, 'string', item.text)
        return tile
    end
    local tile = m:create(state.rect, 'image', name)
    if not tile then return nil end
    m:set(tile, 'filename', item.name)
    m:set(tile, 'x', item.x)
    m:set(tile, 'y', item.y)
    m:set(tile, 'width', item.width)
    m:set(tile, 'height', item.height)
    return tile
end

-- The pieces that are near the window exist, the ones that are far from it do not (a file has a thousand of them)
local function updatePieces()
    local m = shown.menu
    for index, item in ipairs(state.parsed.items) do
        local wanted = near(item, state.y)
        local tile = state.tiles[index]
        if wanted and not tile then
            state.tiles[index] = make(index) or false
        elseif not wanted and tile then
            m:remove(tile)
            state.tiles[index] = nil
        end
    end
end

-- The height of a line of a font in menu units (what the font says, not what the window shows of it)
local function lineHeightOf(m)
    local heights = {}
    return function(font)
        if heights[font] then return heights[font] end
        local measured = m:measure(font, 'Ag', WIDTH)
        local height = measured and measured.height or 0
        heights[font] = height > 0 and height or 20
        return heights[font]
    end
end

-- Show the screen. `options`: onDone (called when it is over), language ('ENGLISH'), wacky (the Wild Wasteland credits)
function module.show(options)
    options = options or {}
    if shown then return true end
    local text = module.load(options.language, options.wacky)
    if text == nil then return false end
    local opened = menuModule.open(FILE, { layer = options.layer or 'Settings' })
    if not opened then return false end
    local m = opened.menu
    local rect = m:find('credits_rect')
    if not rect then
        opened:close()
        return false
    end
    shown = opened
    state = { onDone = options.onDone, rect = rect, tiles = {}, y = START_Y, seconds = 0, slide = nil }
    state.parsed = module.parse(text, lineHeightOf(m))
    m:set(m:root(), 'alpha', 255)
    updatePieces()
    shown:refresh()
    return true
end

-- A frame: the block goes up, the pieces that come near are made, the background changes in turn
function module.update(dt)
    if not shown then return end
    shown:checkSize()
    state.seconds = state.seconds + dt
    state.y = START_Y - SPEED * state.seconds
    local m = shown.menu
    m:set(state.rect, 'y', state.y)
    updatePieces()
    local backgrounds = state.parsed.backgrounds
    if #backgrounds > 0 then
        -- Each background stays for the time the block takes to go its share of the way
        local slide = math.max(1, (math.max(state.parsed.height, state.parsed.extent) / SPEED) / #backgrounds)
        local index = math.min(#backgrounds, math.floor(state.seconds / slide) + 1)
        if index ~= state.slide then
            state.slide = index
            m:set('background', 'filename', backgrounds[index])
        end
    end
    shown:refresh()
    if state.y + math.max(state.parsed.height, state.parsed.extent) < 0 then finish() end
end

function module.press(code)
    if not shown then return false end
    if code == input.KEY.Escape then
        finish()
        return true
    end
    return false
end

return module
