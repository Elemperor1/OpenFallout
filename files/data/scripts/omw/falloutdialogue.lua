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
-- About the height of a line of text, with some room (the layout is not asked, the lines are counted)
local lineHeight = textSize * 1.15

local element = nil
local visible = false
-- The topics are listed from this one, and as many as fit the screen: there is no scrolling widget, so the list is
-- moved by the wheel and the keys
local optionOffset, optionCount = 1, 0
-- The response that is said, the line of it that is being said, how long it has been (until the player has had the
-- time to read it, or the voice is over) and how long since its voice ended
local playedResponse = nil
local lineIndex, lineElapsed, voiceOverFor = 0, 0, 0
-- A voice was asked for the line
local lineVoiced = false
-- The time between a voice that is over and the next line
local pause = 0.25

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
-- The lines of the response do not all fit the screen
local responseWindowed = false

local function startLine(index)
    lineIndex, lineElapsed, voiceOverFor = index, 0, 0
    lineVoiced = conversation.say(index)
    -- A response that does not fit the screen shows the lines from the one that is said
    if responseWindowed and visible then
        render(conversation.get())
    end
end

-- A response that is new: its first line is said
local function startResponse(view)
    if view.speaking and view.response ~= playedResponse then
        playedResponse = view.response
        startLine(1)
    end
end

-- Picks the topic and shows the response to it
local function pick(topic)
    local view = conversation.choose(topic)
    playedResponse = nil
    optionOffset = 1
    render(view)
    startResponse(view)
end

-- The width of a character in the size of the text, as the font of the window draws it (measured on its letters: the
-- lower case ones are half as wide as the size, the capitals three quarters): the ideographs, kana and hangul of the
-- languages that are written without spaces are as wide as the size.
local function charWidth(code)
    if code < 128 then
        if code == 32 then
            return 0.43
        elseif code >= 97 and code <= 122 or code >= 48 and code <= 57 then
            return 0.5
        elseif code >= 65 and code <= 90 then
            return 0.74
        elseif code > 32 and code < 127 then
            return 0.32
        end
        return 0.5
    end
    return code >= 0x2E80 and 1 or 0.6
end

-- The characters of a word with their widths. Text that is not UTF-8 is taken a byte at a time.
local function glyphs(word)
    local list = {}
    local ok = pcall(function()
        for _, code in utf8.codes(word) do
            list[#list + 1] = { utf8.char(code), charWidth(code) }
        end
    end)
    if not ok then
        list = {}
        for i = 1, #word do
            list[i] = { word:sub(i, i), charWidth(word:byte(i)) }
        end
    end
    return list
end

-- The text of a response or a topic as the window shows it. The font has no glyph for a control character (it is drawn as
-- a substitute), a tag of the markup of the game (<Global=...>, <Alias=...>) is not a word, and a note in braces (15% of
-- the answers of New Vegas start with one, a word or two glued to the first word of the sentence, and no game text or
-- script of this engine reads them; taken to be a note of the author, not text for the player), so all three are taken
-- out, as a line that starts with them shows noise. What was taken out is logged by its kind and count, never as text.
local reportedMarkup = 0
local function clean(text)
    text = tostring(text or '')
    local tags, controls = {}, 0
    text = text:gsub('<(%a+)=[^<>\r\n]*>', function(kind)
        tags[#tags + 1] = kind
        return ' '
    end)
    text = text:gsub('{[^{}\r\n]*}', function()
        tags[#tags + 1] = 'note'
        return ' '
    end)
    text = text:gsub('%c', function()
        controls = controls + 1
        return ' '
    end)
    if (#tags > 0 or controls > 0) and reportedMarkup < 5 then
        reportedMarkup = reportedMarkup + 1
        print(string.format('Dialogue text: %d tag(s) [%s] and %d control character(s) taken out', #tags,
            table.concat(tags, ','), controls))
    end
    return text
end

-- The words of a text in lines that fit the width. Text widgets do not wrap to a width and then grow in height (they
-- grow in width, past the edge of the screen), and a text edit can wrap but takes the clicks, so the lines are made here
-- from the widths of the characters, with some room left over for the letters that are wider than the mean of their
-- kind, and each is a widget of its own. A word that is longer than a line (text of a language written without spaces)
-- is cut between its characters.
local function wrap(text, width)
    local limit = math.max(4, width / textSize * 0.92)
    local space = charWidth(32)
    local lines, current, used = {}, '', 0
    for word in clean(text):gmatch('%S+') do
        local list = glyphs(word)
        local wordWidth = 0
        for _, glyph in ipairs(list) do
            wordWidth = wordWidth + glyph[2]
        end
        if wordWidth <= limit then
            if current == '' then
                current, used = word, wordWidth
            elseif used + space + wordWidth <= limit then
                current, used = current .. ' ' .. word, used + space + wordWidth
            else
                lines[#lines + 1] = current
                current, used = word, wordWidth
            end
        else
            if current ~= '' then
                lines[#lines + 1] = current
            end
            current, used = '', 0
            for _, glyph in ipairs(list) do
                if used + glyph[2] > limit and current ~= '' then
                    lines[#lines + 1] = current
                    current, used = '', 0
                end
                current, used = current .. glyph[1], used + glyph[2]
            end
        end
    end
    lines[#lines + 1] = current
    return lines
end

-- The text as a column of lines, which takes the events of the widget it is in (those that no line handles go to it).
-- A text with more lines than `maxLines` (a response longer than the screen) is cut: it keeps the first lines and ends
-- with an ellipsis, or with `fromEnd` the last ones and starts with one.
local function paragraph(text, color, width, events, maxLines, fromEnd)
    local lines = {}
    -- the first of the content gives the whole width, so that the empty part of a line takes the clicks too
    local content = { { props = { size = v2(width, 0) } } }
    local wrapped = wrap(text, width)
    if maxLines and #wrapped > maxLines then
        local kept = {}
        if fromEnd then
            for i = #wrapped - maxLines + 1, #wrapped do
                kept[#kept + 1] = wrapped[i]
            end
            kept[1] = '... ' .. kept[1]
        else
            for i = 1, maxLines do
                kept[i] = wrapped[i]
            end
            kept[#kept] = kept[#kept] .. ' ...'
        end
        wrapped = kept
    end
    for _, line in ipairs(wrapped) do
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
    -- the panel ends at the bottom of the screen with some room, and has padding
    local available = ui.screenSize().y * 0.94 - 48

    -- the window has the same width whatever the lines are
    local rows = { { props = { size = v2(inner, 0) } } }
    local used = 0
    local function add(layout, lineCount)
        if #rows > 1 then
            rows[#rows + 1] = { props = { size = gap } }
            used = used + gap.y
        end
        rows[#rows + 1] = layout
        used = used + (lineCount or 1) * lineHeight
    end

    if view.speakerName ~= '' then
        add({
            type = ui.TYPE.Text,
            props = { text = view.speakerName, textSize = textSize + 2, textColor = speakerColor },
        }, 1)
    end
    -- The lines of the response that fit what is left of the screen (with room for a row of topics or the hint): from
    -- the line that is said while it is said, the last ones when it is over
    local layouts, total = {}, 0
    local function heightOf(layout)
        return #layout.lines * lineHeight + gap.y
    end
    for _, line in ipairs(view.lines) do
        layouts[#layouts + 1] = paragraph(line.text, focusColor, inner)
        total = total + heightOf(layouts[#layouts])
    end
    local first, last = 1, #layouts
    local room = available - used - lineHeight - gap.y
    responseWindowed = total > room and #layouts > 1
    if responseWindowed then
        local taken
        if view.speaking then
            first = math.max(1, math.min(lineIndex, #layouts))
            last, taken = first, heightOf(layouts[first])
            while last < #layouts and taken + heightOf(layouts[last + 1]) <= room do
                last = last + 1
                taken = taken + heightOf(layouts[last])
            end
        else
            first, taken = #layouts, heightOf(layouts[#layouts])
            while first > 1 and taken + heightOf(layouts[first - 1]) <= room do
                first = first - 1
                taken = taken + heightOf(layouts[first])
            end
        end
    end
    -- One line of the response that is taller than what is left of the screen is cut
    if first == last and heightOf(layouts[first]) > room then
        local maxLines = math.max(1, math.floor((room - gap.y) / lineHeight))
        layouts[first] = paragraph(view.lines[first].text, focusColor, inner, nil, maxLines, not view.speaking)
        responseWindowed = true
    end
    for index = first, last do
        add(layouts[index], #layouts[index].lines)
    end

    local panelEvents = nil
    optionCount = 0
    if view.speaking then
        add({
            type = ui.TYPE.Text,
            props = { text = 'Click or press Space to continue', textSize = textSize - 4, textColor = hintColor },
        }, 1)
        panelEvents = { mouseClick = async:callback(function() advance() end) }
    else
        local options = view.options
        optionOffset = math.max(1, math.min(optionOffset, #options))
        -- The topics from the first one shown, as many as fit what is left of the screen (at least one). When they
        -- do not all fit, a line says which are shown.
        local function fit(room)
            local shown, taken = {}, 0
            for index = optionOffset, #options do
                local topic = options[index].topic
                local row = optionRow(options[index].text, function() pick(topic) end, inner)
                local height = #row.lines * lineHeight + gap.y
                if #shown > 0 and taken + height > room then
                    break
                end
                shown[#shown + 1] = row
                taken = taken + height
            end
            return shown
        end
        local left = available - used
        local shown = fit(left)
        local first, last = optionOffset, optionOffset + #shown - 1
        local partial = first > 1 or last < #options
        if partial then
            shown = fit(left - lineHeight - gap.y)
            last = optionOffset + #shown - 1
        end
        optionCount = #shown
        for _, row in ipairs(shown) do
            add(row, #row.lines)
        end
        if partial then
            add({
                type = ui.TYPE.Text,
                props = {
                    text = string.format('%d-%d of %d (wheel, Page Up/Down)', first, last, #options),
                    textSize = textSize - 4,
                    textColor = hintColor,
                },
            }, 1)
        end
        if #options == 0 then
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

-- Moves the list of topics (by a number of topics, down when it is positive) when it does not all fit the screen
local function scroll(by)
    local view = conversation.get()
    if not visible or view.speaking or not view.options or #view.options == 0 then
        return
    end
    local moved
    if by > 0 then
        -- the last page is not left with one topic: a page ends where the last topic does
        if optionOffset + optionCount > #view.options then
            return
        end
        moved = math.min(optionOffset + by, math.max(optionOffset + 1, #view.options - optionCount + 1))
    else
        moved = math.max(1, optionOffset + by)
    end
    if moved ~= optionOffset then
        optionOffset = moved
        render(view)
    end
end

-- The response has been read: what it does is done and the options appear, or the conversation is over
function advance()
    conversation.stopVoice()
    local view = conversation.finish()
    playedResponse = nil
    optionOffset = 1
    if view.ended then
        destroyElement()
        closeWindow()
        return
    end
    render(view)
end

local function show()
    visible = true
    optionOffset = 1
    local view = conversation.get()
    render(view)
    startResponse(view)
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
            -- A conversation that something else ended (a script, core.conversation.close) takes the window with it:
            -- the game is paused while the mode is there
            if not view.active then
                destroyElement()
                closeWindow()
                return
            end
            if not view.speaking then
                return
            end
            startResponse(view)
            local dt = core.getRealFrameDuration()
            lineElapsed = lineElapsed + dt
            local line = view.lines[lineIndex]
            if not line then
                advance()
                return
            end
            -- A voice is waited for (a line of the data that has no file, or a game with no sound to play it on, stays
            -- as long as it takes to read)
            local voice = lineVoiced and conversation.voice() or 'none'
            local over
            if voice == 'pending' or voice == 'playing' then
                over = lineElapsed > line.duration * 4 + 10
            elseif voice == 'done' then
                voiceOverFor = voiceOverFor + dt
                over = voiceOverFor >= pause
            else
                over = lineElapsed >= line.duration
            end
            if over then
                if lineIndex >= #view.lines then
                    advance()
                else
                    startLine(lineIndex + 1)
                end
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
                -- the numbers are those of the topics that are shown
                local number = key.code - input.KEY._1 + 1
                local option = number <= optionCount and view.options[optionOffset + number - 1] or nil
                if option then
                    pick(option.topic)
                end
            elseif key.code == input.KEY.DownArrow then
                scroll(1)
            elseif key.code == input.KEY.UpArrow then
                scroll(-1)
            elseif key.code == input.KEY.PageDown then
                scroll(math.max(1, optionCount))
            elseif key.code == input.KEY.PageUp then
                scroll(-math.max(1, optionCount))
            end
        end,
        onMouseWheel = function(vertical)
            if vertical > 0 then
                scroll(-1)
            elseif vertical < 0 then
                scroll(1)
            end
        end,
    },
    eventHandlers = {
        -- However the window is left (Escape, the main menu, a script), the conversation is over. A screen that opens on
        -- top of it (the making of a character, from the end script of a response) leaves it where it is.
        UiModeChanged = function(data)
            if data.oldMode == 'Dialogue' and data.newMode ~= 'Dialogue' then
                for _, mode in ipairs(ui._getUiModeStack()) do
                    if mode == 'Dialogue' then
                        return
                    end
                end
                conversation.close()
                -- Nothing of the conversation stays for the next one (the same response may be its first)
                conversation.stopVoice()
                playedResponse = nil
                lineIndex, lineElapsed, voiceOverFor, lineVoiced = 0, 0, 0, false
            end
        end,
    },
}
