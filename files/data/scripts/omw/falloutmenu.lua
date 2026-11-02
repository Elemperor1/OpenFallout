-- Shows a menu of Fallout 3 or New Vegas from the game's own menu file (core.menus): the pictures the file lays out
-- are drawn as widgets, the pointer goes to the menu, and what the menu says in answer (a tile that was clicked) goes
-- to the script that opened it. What a screen shows and where is the file's; what the buttons do is the script's.
--
--   local menu = require('scripts.omw.falloutmenu')
--   local shown = menu.open('quantity_menu.xml')
--   shown:set('QM_AmountChosen', 'string', '5')
--   shown:onClick('QM_OKButton', function() ... end)
--   shown:close()
local async = require('openfallout.async')
local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')

local v2 = util.vector2

local module = {}

-- One texture resource for each part of a texture that was asked for
local textures = {}
local function textureOf(piece)
    local key = piece.texture .. '|' .. piece.sourceX .. '|' .. piece.sourceY .. '|' .. piece.sourceWidth .. '|'
        .. piece.sourceHeight .. (piece.mirrored and '|m' or '')
    local found = textures[key]
    if found == nil then
        if piece.mirrored then
            -- A mirrored picture reads the part of the texture from its right edge to its left edge
            found = ui.texture {
                path = piece.texture,
                offset = v2(piece.sourceX + piece.sourceWidth, piece.sourceY),
                size = v2(-piece.sourceWidth, piece.sourceHeight),
            }
        else
            found = ui.texture {
                path = piece.texture,
                offset = v2(piece.sourceX, piece.sourceY),
                size = v2(piece.sourceWidth, piece.sourceHeight),
            }
        end
        textures[key] = found
    end
    return found
end

local function pieceLayout(piece, dx, dy)
    return {
        type = ui.TYPE.Image,
        props = {
            position = v2(piece.x - dx, piece.y - dy),
            size = v2(piece.width, piece.height),
            resource = textureOf(piece),
            color = util.color.rgb(piece.red, piece.green, piece.blue),
            alpha = piece.alpha,
            ignorePointerEvents = true,
        },
    }
end

-- The widgets for the pieces, in their order. Pieces that are cut to a window are put in a widget the size of the
-- window, which cuts what is in it; pieces next to each other that have the same window share the widget.
local function contentOf(pieces)
    local content = ui.content {}
    local group, groupKey = nil, nil
    for _, piece in ipairs(pieces) do
        local clip = piece.clip
        if clip then
            local key = clip.x .. ',' .. clip.y .. ',' .. clip.width .. ',' .. clip.height
            if key ~= groupKey then
                group = {
                    props = {
                        position = v2(clip.x, clip.y),
                        size = v2(clip.width, clip.height),
                        ignorePointerEvents = true,
                    },
                    content = ui.content {},
                }
                content:add(group)
                groupKey = key
            end
            group.content:add(pieceLayout(piece, clip.x, clip.y))
        else
            groupKey = nil
            content:add(pieceLayout(piece, 0, 0))
        end
    end
    return content
end

local Shown = {}
Shown.__index = Shown

-- Draw the menu again if anything it depends on changed
function Shown:refresh()
    if not self.element or self.batching then return end
    if self.menu:version() == self.version then return end
    -- Making the pieces evaluates traits, so the version is read after it
    local pieces = self.menu:pieces()
    self.version = self.menu:version()
    self.element.layout.content = contentOf(pieces)
    self.element:update()
end

-- Change a number of things and draw the menu once, after the last of them
function Shown:batch(change)
    local outer = self.batching
    self.batching = true
    local ok, problem = pcall(change)
    self.batching = outer
    self:refresh()
    if not ok then error(problem, 0) end
end

-- Give the menu the size of the window again if the window changed size
function Shown:checkSize()
    local size = ui.screenSize()
    if size.x ~= self.size.x or size.y ~= self.size.y then
        self.size = size
        self.menu:resize(size.x, size.y)
        self.element.layout.props.size = size
        self.version = nil
        self:refresh()
    end
end

-- Run what a script has asked for when a tile is clicked (by name, and by the name of the menu file's tile)
function Shown:dispatch(tile)
    local handler = self.clicks[self.menu:name(tile)]
    if handler then handler(tile) end
    -- A tile in a row of a list is the row's click
    local list, row = self.menu:rowOf(tile)
    if list and self.rowClicks[list] then self.rowClicks[list](row, tile) end
end

-- The hook of the tile that is being dragged, or of one it is in
function Shown:dragHook()
    local tile = self.menu:dragged()
    while tile do
        local hook = self.dragHooks[self.menu:name(tile)]
        if hook then return hook end
        tile = self.menu:parent(tile)
    end
    return nil
end

-- A tile that is dragged (a meter) shows its value from the drag until it is let go, and from what the code says then:
-- `read()` gives the value at the moment it is let go and `apply(value)` puts it where the code keeps it
function Shown:onDrag(name, read, apply)
    self.dragHooks[name] = { read = read, apply = apply }
end

-- Call `handler(tile)` when the tile of this name is clicked
function Shown:onClick(name, handler)
    self.clicks[name] = handler
end

function Shown:set(tile, trait, value)
    self.menu:set(tile, trait, value)
    self:refresh()
end

function Shown:get(tile, trait)
    return self.menu:get(tile, trait)
end

-- Set a trait of the menu itself: what the host tells a menu file (`_DialogVisible` and the like)
function Shown:io(trait, value)
    self.menu:set(self.menu:root(), trait, value)
    self:refresh()
end

-- Make a list of the menu: `listTile` is the list (a tile of `list_box.xml`) and `template` the tile that each row is.
-- Returns an object with `rows(tables of traits)`, `select(row)`, `selected()`, `count()` and `onClick(function(row))`;
-- the rows are from 1.
function Shown:list(listTile, template)
    local number = self.menu:addList(listTile, template)
    if not number then return nil end
    local shown = self
    local list = {}
    function list.rows(rows)
        shown.menu:setRows(number, rows)
        shown:refresh()
    end
    function list.select(row, reveal)
        shown.menu:selectRow(number, row, reveal)
        shown:refresh()
    end
    function list.selected() return shown.menu:selectedRow(number) end
    function list.count() return shown.menu:rowCount(number) end
    function list.onClick(handler) shown.rowClicks[number] = handler end
    return list
end

-- Fill a tabline (a tile that includes tabline.xml) with a button for each label, as the game's code does with the
-- template the menu file keeps for the buttons. The buttons are spread over the line with the same gap between them, as
-- long as the lines the file draws between them (`_LeftLineLength`); a button is as wide as its text and the room the
-- template leaves around it. `options`: prefix (the names of the buttons are prefix .. number, default 'Tab').
-- Returns an object with `pick(index)` (the tab that is shown as the current one), `current()` and `count()`;
-- `onPick(index)` is called when a button is clicked.
function Shown:tabs(line, template, labels, picked, onPick, options)
    local menu = self.menu
    local prefix = options and options.prefix or 'Tab'
    local tabs = { buttons = {}, index = picked }
    local shown = self
    self:batch(function()
        for i, label in ipairs(labels) do
            local tile = menu:instantiate(line, template, prefix .. i)
            if not tile then
                error('the menu ' .. self.file .. ' has no template ' .. template, 0)
            end
            menu:set(tile, 'string', label)
            menu:set(tile, 'listindex', i - 1)
            tabs.buttons[i] = tile
        end
        local total = 0
        for _, tile in ipairs(tabs.buttons) do
            total = total + menu:getNumber(tile, 'width')
        end
        local gap = math.max(0, (menu:getNumber(line, 'width') - total) / (#labels + 1))
        local x = gap
        for _, tile in ipairs(tabs.buttons) do
            menu:set(tile, '_x', x)
            x = x + menu:getNumber(tile, 'width') + gap
        end
        menu:set(line, '_LeftLineLength', gap)
        menu:set(line, '_ButtonCount', #labels)
        menu:set(line, '_CurrentTab', picked - 1)
    end)
    function tabs.pick(index)
        tabs.index = index
        shown.menu:set(line, '_CurrentTab', index - 1)
        shown:refresh()
    end
    function tabs.current() return tabs.index end
    function tabs.count() return #tabs.buttons end
    for i = 1, #labels do
        self.clicks[prefix .. i] = function()
            if onPick then onPick(i) end
        end
    end
    return tabs
end

-- Put a scrollbar of the menu at a value
function Shown:setScrollbar(bar, value)
    self.menu:setScrollbar(bar, value)
    self:refresh()
end

function Shown:find(name)
    return self.menu:find(name)
end

-- Press the button of the pad (or the key) that the menu file ties to a tile: what clicking the tile does
function Shown:click(tile)
    self.menu:click(tile)
    self:dispatch(type(tile) == 'number' and tile or self.menu:find(tile))
    self:refresh()
end

function Shown:close()
    if self.element then
        self.element:destroy()
        self.element = nil
    end
end

-- Open a menu file for the whole window. `options`: layer (default 'Windows'), pad (a pad is in use)
function module.open(file, options)
    options = options or {}
    local size = ui.screenSize()
    local menu, reason = core.menus.open(file, size.x, size.y, options.pad == true)
    if not menu then
        print('menu ' .. file .. ' is not shown: ' .. tostring(reason))
        return nil
    end
    local shown = setmetatable({ menu = menu, file = file, size = size, clicks = {}, rowClicks = {}, dragHooks = {} }, Shown)
    shown.version = menu:version()
    shown.element = ui.create {
        layer = options.layer or 'Windows',
        props = { position = v2(0, 0), size = size },
        content = contentOf(menu:pieces()),
        events = {
            mouseMove = async:callback(function(e)
                shown.menu:pointerMoved(e.offset.x, e.offset.y)
                shown:refresh()
            end),
            mousePress = async:callback(function(e)
                if e.button ~= 1 then return end
                shown.menu:pointerPressed(e.offset.x, e.offset.y)
                shown:refresh()
            end),
            mouseRelease = async:callback(function(e)
                if e.button ~= 1 then return end
                -- What a tile that was dragged shows is read before it is let go, as the file takes the drag away then
                local hook = shown:dragHook()
                local kept = hook and hook.read()
                local tile = shown.menu:pointerReleased(e.offset.x, e.offset.y)
                if hook then hook.apply(kept) end
                if tile then shown:dispatch(tile) end
                shown:refresh()
            end),
            mouseWheel = async:callback(function(e)
                -- The files count the wheel the other way round: up is -1 (the scrollbars add `wheelmoved` to the place)
                local notches = e.delta.y > 0 and -1 or (e.delta.y < 0 and 1 or 0)
                if notches ~= 0 then
                    shown.menu:wheel(e.position.x, e.position.y, notches)
                    shown:refresh()
                end
            end),
        },
    }
    return shown
end

-- What the install lacks that menus asked for, for the log
function module.missingTextures()
    return core.menus.missingTextures()
end

return module
