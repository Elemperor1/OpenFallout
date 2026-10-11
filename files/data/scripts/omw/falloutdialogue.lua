-- The conversation of the player with a character of Fallout 3 or New Vegas: what the character says and the topics the
-- player picks from. The rules and the state of the conversation are the engine's (core.conversation); this script shows
-- them in the window "Dialogue", which it takes over from the one of Morrowind for content that has dialogue of Fallout.
local core = require('openfallout.core')
local input = require('openfallout.input')
local async = require('openfallout.async')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

local conversation = core.conversation
local v2 = util.vector2

local normalColor = util.color.rgb(0.45, 1, 0.55)
local focusColor = util.color.rgb(1, 1, 1)
local speakerColor = util.color.rgb(1, 0.85, 0.4)
local hintColor = util.color.rgb(0.6, 0.7, 0.6)

local textSize = ui._getDefaultFontSize() + 4
local gap = v2(0, 6)

local element = nil
local visible = false
-- How long the response has been on the screen, until the player has had the time to read it
local elapsed = 0

local function destroyElement()
    if element then
        element:destroy()
        element = nil
    end
end

local function closeWindow()
    I.UI.removeMode('Dialogue')
end

local advance, render

-- Picks the topic and shows the response to it
local function pick(topic)
    local view = conversation.choose(topic)
    elapsed = 0
    render(view)
end

-- The words of a text in lines that fit the width. Text widgets do not wrap to a width and then grow in height (they
-- grow in width, past the edge of the screen), and a text edit can wrap but takes the clicks, so the lines are made here
-- with a width for a letter that is on the large side, and each is a widget of its own.
local function wrap(text, width)
    local limit = math.max(8, math.floor(width / (textSize * 0.55)))
    local lines, current = {}, ''
    for word in text:gmatch('%S+') do
        if current == '' then
            current = word
        elseif #current + 1 + #word <= limit then
            current = current .. ' ' .. word
        else
            lines[#lines + 1] = current
            current = word
        end
    end
    lines[#lines + 1] = current
    return lines
end

-- The text as a column of lines, which takes the events of the widget it is in (those that no line handles go to it)
local function paragraph(text, color, width, events)
    local lines = {}
    -- the first of the content gives the whole width, so that the empty part of a line takes the clicks too
    local content = { { props = { size = v2(width, 0) } } }
    for _, line in ipairs(wrap(text, width)) do
        lines[#lines + 1] = {
            type = ui.TYPE.Text,
            props = { text = line, textSize = textSize, textColor = color },
        }
        content[#content + 1] = lines[#lines]
    end
    return {
        type = ui.TYPE.Flex,
        props = { autoSize = true },
        events = events,
        content = ui.content(content),
        lines = lines,
    }
end

local function optionRow(text, onPick, width)
    local row = paragraph(text, normalColor, width)
    local function paint(color)
        for _, line in ipairs(row.lines) do
            line.props.textColor = color
        end
        if element then element:update() end
    end
    row.events = {
        mouseClick = async:callback(onPick),
        focusGain = async:callback(function() paint(focusColor) end),
        focusLoss = async:callback(function() paint(normalColor) end),
    }
    return row
end

function render(view)
    destroyElement()
    if not view.active and not view.ended then
        return
    end
    local width = math.max(360, math.floor(ui.screenSize().x * 0.6))
    local inner = width - 48

    -- the window has the same width whatever the lines are
    local rows = { { props = { size = v2(inner, 0) } } }
    local function add(layout)
        if #rows > 1 then
            rows[#rows + 1] = { props = { size = gap } }
        end
        rows[#rows + 1] = layout
    end

    if view.speakerName ~= '' then
        add {
            type = ui.TYPE.Text,
            props = { text = view.speakerName, textSize = textSize + 2, textColor = speakerColor },
        }
    end
    for _, line in ipairs(view.lines) do
        add(paragraph(line.text, focusColor, inner))
    end

    local panelEvents = nil
    if view.speaking then
        add {
            type = ui.TYPE.Text,
            props = { text = 'Click or press Space to continue', textSize = textSize - 4, textColor = hintColor },
        }
        panelEvents = { mouseClick = async:callback(function() advance() end) }
    else
        for _, option in ipairs(view.options) do
            local topic = option.topic
            add(optionRow(option.text, function() pick(topic) end, inner))
        end
        if #view.options == 0 then
            add(optionRow('Goodbye', function()
                conversation.close()
                closeWindow()
            end, inner))
        end
    end

    element = ui.create {
        layer = 'Windows',
        template = I.MWUI.templates.boxTransparent,
        events = panelEvents,
        props = {
            relativePosition = v2(0.5, 0.94),
            anchor = v2(0.5, 1),
        },
        content = ui.content {
            {
                template = I.MWUI.templates.padding,
                content = ui.content {
                    {
                        type = ui.TYPE.Flex,
                        props = { autoSize = true },
                        content = ui.content(rows),
                    },
                },
            },
        },
    }
end

-- The response has been read: what it does is done and the options appear, or the conversation is over
function advance()
    local view = conversation.finish()
    elapsed = 0
    if view.ended then
        destroyElement()
        closeWindow()
        return
    end
    render(view)
end

local function show()
    visible = true
    elapsed = 0
    render(conversation.get())
end

local function hide()
    visible = false
    destroyElement()
end

-- Windows are replaced only when the loaded content has the dialogue of Fallout: the dialogue of Morrowind keeps its own
if conversation.available() then
    I.UI.registerWindow('Dialogue', show, hide)
end

return {
    engineHandlers = {
        -- The game is paused while the window is there, so the time that passes is the real one
        onFrame = function()
            if not visible then
                return
            end
            local view = conversation.get()
            if not view.speaking then
                return
            end
            elapsed = elapsed + core.getRealFrameDuration()
            if elapsed >= view.duration then
                advance()
            end
        end,
        onKeyPress = function(key)
            if not visible then
                return
            end
            local view = conversation.get()
            if view.speaking then
                if key.code == input.KEY.Space or key.code == input.KEY.Enter then
                    advance()
                end
            elseif key.code >= input.KEY._1 and key.code <= input.KEY._9 then
                local option = view.options[key.code - input.KEY._1 + 1]
                if option then
                    pick(option.topic)
                end
            end
        end,
    },
    eventHandlers = {
        -- However the window is left (Escape, the main menu, a script), the conversation is over
        UiModeChanged = function(data)
            if data.oldMode == 'Dialogue' and data.newMode ~= 'Dialogue' then
                conversation.close()
            end
        end,
    },
}
