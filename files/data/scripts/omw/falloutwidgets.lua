-- The plain widgets that the screens of Fallout 3 and New Vegas are drawn with (the Pip-Boy, the level up, the loot of
-- a container, the death of the player): coloured lines of text, rows, cells of a fixed width, lines that do something
-- when they are clicked, and a panel to put them in. Each screen makes its own set with `widgets.new(update)`, where
-- `update` redraws the element of the screen after a line has changed its colour.
local async = require('openfallout.async')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

local v2 = util.vector2

local module = {}

module.normalColor = util.color.rgb(0.45, 1, 0.55)
module.focusColor = util.color.rgb(1, 1, 1)
module.titleColor = util.color.rgb(1, 0.85, 0.4)
module.pickedColor = util.color.rgb(1, 0.85, 0.4)
module.hintColor = util.color.rgb(0.6, 0.7, 0.6)
module.dimColor = util.color.rgb(0.3, 0.45, 0.35)
module.badColor = util.color.rgb(1, 0.45, 0.4)

module.textSize = ui._getDefaultFontSize() + 4
module.gap = v2(0, 6)

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
            template = I.MWUI.templates.boxSolid,
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
