-- The plain widgets that the screens of Fallout 3 and New Vegas are drawn with (the Pip-Boy, the level up, the loot of
-- a container, the death of the player): coloured lines of text, rows, cells of a fixed width, lines that do something
-- when they are clicked, and a panel to put them in. Each screen makes its own set with `widgets.new(update)`, where
-- `update` redraws the element of the screen after a line has changed its colour.
local async = require('openfallout.async')
local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

local v2 = util.vector2

local module = {}

-- The colours: the green of Fallout 3 and the amber of New Vegas (the system colour of its menus, 255 182 66), as the
-- games' own screens have them. Which game it is the engine says (the stats of the player); until it has said, the
-- green is given and not kept
local palettes = {
    green = {
        normalColor = util.color.rgb(0.45, 1, 0.55),
        focusColor = util.color.rgb(1, 1, 1),
        titleColor = util.color.rgb(1, 0.85, 0.4),
        pickedColor = util.color.rgb(1, 0.85, 0.4),
        hintColor = util.color.rgb(0.6, 0.7, 0.6),
        dimColor = util.color.rgb(0.3, 0.45, 0.35),
        badColor = util.color.rgb(1, 0.45, 0.4),
    },
    amber = {
        normalColor = util.color.rgb(1, 182 / 255, 66 / 255),
        focusColor = util.color.rgb(1, 0.96, 0.82),
        titleColor = util.color.rgb(1, 0.93, 0.7),
        pickedColor = util.color.rgb(1, 0.93, 0.7),
        hintColor = util.color.rgb(0.72, 0.52, 0.22),
        dimColor = util.color.rgb(0.4, 0.29, 0.12),
        badColor = util.color.rgb(1, 0.42, 0.3),
    },
}
local palette = nil
setmetatable(module, {
    __index = function(_, key)
        if not palette then
            local ok, stats = pcall(function() return core.pipboy.stats() end)
            if ok and type(stats) == 'table' then
                palette = stats.newVegas and palettes.amber or palettes.green
            else
                return palettes.green[key]
            end
        end
        return palette[key]
    end,
})

module.textSize = ui._getDefaultFontSize() + 4
module.gap = v2(0, 6)

-- Whether the screens are the amber ones of New Vegas (the palette is known by then: the engine has said which game it is)
local function amber()
    local _ = module.normalColor
    return palette == palettes.amber
end

-- The solid box of the amber screens: a black ground in plain lines of the amber of the menus (the box of the engine's
-- template has the green frame pieces of Fallout 3, which no colour of a line turns amber). It is made like the
-- template's `boxSolid`: the pieces of the frame around a slot, the ground under them
local flatBoxTemplate = nil
local function flatBox()
    if flatBoxTemplate then return flatBoxTemplate end
    local white = ui.texture { path = 'white' }
    local border = 2
    local color = palettes.amber.normalColor
    local box = {
        type = ui.TYPE.Container,
        content = ui.content {
            {
                type = ui.TYPE.Image,
                props = {
                    resource = white,
                    color = util.color.rgb(0, 0, 0),
                    relativeSize = v2(1, 1),
                    size = v2(border * 2, border * 2),
                },
            },
        },
    }
    local function piece(props)
        props.resource = white
        props.color = color
        box.content:add { type = ui.TYPE.Image, props = props }
    end
    local sides = { left = { 0, 0 }, right = { 1, 0 }, top = { 0, 0 }, bottom = { 0, 1 } }
    for name, at in pairs(sides) do
        local horizontal = name == 'top' or name == 'bottom'
        local dx, dy = horizontal and 1 or 0, horizontal and 0 or 1
        piece {
            position = v2((dx + at[1]) * border, (dy + at[2]) * border),
            relativePosition = v2(at[1], at[2]),
            size = v2((1 - dx) * border, (1 - dy) * border),
            relativeSize = v2(dx, dy),
        }
    end
    for _, at in ipairs { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } } do
        piece { position = v2(at[1] * border, at[2] * border), relativePosition = v2(at[1], at[2]), size = v2(border, border) }
    end
    box.content:add {
        external = { slot = true },
        props = { position = v2(border, border), relativeSize = v2(1, 1) },
    }
    flatBoxTemplate = box
    return box
end

-- The words of a text in lines that fit the width (text widgets do not wrap)
function module.wrap(text, width)
    local limit = math.max(8, math.floor(width / (module.textSize * 0.5)))
    local lines, current = {}, ''
    for word in tostring(text or ''):gmatch('%S+') do
        if current == '' then
            current = word
        elseif #current + 1 + #word <= limit then
            current = current .. ' ' .. word
        else
            lines[#lines + 1] = current
            current = word
        end
    end
    if current ~= '' then lines[#lines + 1] = current end
    return lines
end

-- The widgets of one screen. `update` is called to draw the element again after a change of colour.
function module.new(update)
    local textSize = module.textSize
    local widgets = {}

    function widgets.label(text, color, size)
        return {
            type = ui.TYPE.Text,
            props = { text = text, textSize = size or textSize, textColor = color or module.focusColor },
        }
    end

    function widgets.spacer(width, height)
        return { props = { size = v2(width, height) } }
    end

    -- A widget of a fixed width, so that what is in a column starts at the same place in every row
    function widgets.cell(layout, width)
        local container = {
            props = { size = v2(width, textSize + 6) },
            content = ui.content { layout },
        }
        if layout.events then
            container.events = layout.events
            layout.events = nil
        end
        return container
    end

    function widgets.row(...)
        return {
            type = ui.TYPE.Flex,
            props = { horizontal = true, autoSize = true },
            content = ui.content { ... },
        }
    end

    function widgets.column(rows)
        return {
            type = ui.TYPE.Flex,
            props = { autoSize = true },
            content = ui.content(rows),
        }
    end

    -- A line of text that does something when it is clicked. `options`: color (when not under the pointer), onFocus
    -- (called when the pointer comes), enabled (false: a dim line that does nothing), size
    function widgets.clickable(text, onClick, options)
        options = options or {}
        local color = options.color or module.normalColor
        local layout = {
            type = ui.TYPE.Text,
            props = { text = text, textSize = options.size or textSize, textColor = color },
        }
        if options.enabled == false then
            layout.props.textColor = module.dimColor
            return layout
        end
        layout.events = {
            mouseClick = async:callback(onClick),
            focusGain = async:callback(function()
                layout.props.textColor = module.focusColor
                if options.onFocus then options.onFocus() end
                update()
            end),
            focusLoss = async:callback(function()
                layout.props.textColor = color
                update()
            end),
        }
        return layout
    end

    -- The rows in a solid box, placed on the screen. `place`: where (relative position and anchor), default the middle.
    function widgets.panel(rows, place)
        place = place or {}
        return {
            layer = 'Windows',
            template = amber() and flatBox() or I.MWUI.templates.boxSolid,
            props = {
                relativePosition = place.position or v2(0.5, 0.5),
                anchor = place.anchor or v2(0.5, 0.5),
            },
            content = ui.content {
                {
                    template = I.MWUI.templates.padding,
                    content = ui.content {
                        widgets.column(rows),
                    },
                },
            },
        }
    end

    return widgets
end

return module
