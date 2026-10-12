-- Plays the New Game of Fallout 3 and New Vegas for a person who is not there: answers the message boxes, finishes the
-- screens of character creation, picks the first topic of a conversation, and logs each of these in lines that start with
-- AUTOPLAY (counts and numbers, no text of the game). It is a developer tool, not part of the game: it is loaded with
--     openfallout --data scripts/openfallout/autoplay --content autoplay.omwscripts ...
-- The scripts of the data folder are of the player (this file) and of the world (autoplayworld.lua, which follows the quests).
local core = require('openfallout.core')
local self = require('openfallout.self')
local I = require('openfallout.interfaces')

-- How long (real seconds) a box, a screen or a conversation is left alone before it is answered
local DELAY = 0.7
-- The button of a message box that is picked (counted from 1)
local BUTTON = 1
-- The name the character is given
local NAME = 'Autoplay'

local boxSerial, boxWaited, boxAnswered = nil, 0, nil
local pageSerial, pageWaited = nil, 0
local topicWaited = 0
local lastStatus = 0

local function log(...) print('AUTOPLAY', ...) end

local function answerBox(dt)
    local box = core.messageBox.current()
    if not box then
        boxSerial = nil
        return
    end
    -- a box that was answered stays in the list until the frame after, and the next one is another serial
    if box.serial == boxAnswered then return end
    if box.serial ~= boxSerial then
        boxSerial, boxWaited = box.serial, 0
        log('box', string.format('serial=%d buttons=%d', box.serial, #box.buttons))
    end
    boxWaited = boxWaited + dt
    if boxWaited >= DELAY and boxSerial ~= nil then
        local button = math.min(BUTTON, #box.buttons)
        log('box press', button, tostring(core.messageBox.press(button)))
        boxAnswered, boxSerial = box.serial, nil
    end
end

local function finishPage(dt)
    local page = core.chargen.page()
    if not page then
        pageSerial = nil
        return
    end
    if page.serial ~= pageSerial then
        pageSerial, pageWaited = page.serial, 0
        log('chargen page', page.page, 'serial=' .. page.serial)
    end
    pageWaited = pageWaited + dt
    if pageWaited >= DELAY then
        if page.page == 'name' then core.chargen.setName(NAME) end
        local state = core.chargen.state()
        log('chargen finish', page.page, string.format('name=%s points=%d', state.name, state.pointsLeft))
        pageSerial = nil
        core.chargen.finish()
    end
end

local function pickTopic(dt)
    if not core.conversation.available() then return end
    local state = core.conversation.get()
    if not state.active or state.speaking or #state.options == 0 then
        topicWaited = 0
        return
    end
    topicWaited = topicWaited + dt
    if topicWaited >= DELAY then
        topicWaited = 0
        log('topic', string.format('options=%d', #state.options))
        core.conversation.choose(state.options[1].topic)
    end
end

return {
    engineHandlers = {
        -- every frame, also while a screen or a box stops the game (the time of the game does not pass then)
        onFrame = function()
            local dt = core.getRealFrameDuration()
            answerBox(dt)
            finishPage(dt)
            pickTopic(dt)
            lastStatus = lastStatus + dt
            if lastStatus >= 5 then
                lastStatus = 0
                local cell = self.cell
                log('status', 'mode=' .. tostring(I.UI.getMode()), string.format('cell=%s pos=%.0f,%.0f,%.0f',
                    cell and tostring(cell.id) or 'none', self.position.x, self.position.y, self.position.z))
            end
        end,
    },
}
