-- Plays the New Game of Fallout 3 and New Vegas for a person who is not there: answers the message boxes, finishes the
-- screens of character creation, picks the topics of a conversation (the one it has picked least often with the
-- character, the first of them) and ends it when it has picked them all twice, and logs each of these in lines that
-- start with AUTOPLAY (counts and numbers, no text of the game). It is a developer tool, not part of the game: it is loaded with
--     openfallout --data scripts/openfallout/autoplay --content autoplay.omwscripts ...
-- The scripts of the data folder are of the player (this file) and of the world (autoplayworld.lua, which follows the quests).
local core = require('openfallout.core')
local self = require('openfallout.self')
local I = require('openfallout.interfaces')
local config = require('scripts.autoplaygates')

-- How long (real seconds) a box, a screen or a conversation is left alone before it is answered
local DELAY = 0.7
-- The button of a message box that is picked (counted from 1), unless the table of gates names the message
local BUTTON = 1
-- The name the character is given
local NAME = 'Autoplay'
-- How often a topic is picked with a character, over all the conversations, before the conversation is ended instead
local MAX_PICKS = 2
-- How long (real seconds) a conversation that has nothing to pick from, and says nothing, is left before it is ended
local CLOSE_AFTER = 3

local boxSerial, boxWaited, boxAnswered = nil, 0, nil
local pageSerial, pageWaited = nil, 0
local topicWaited, emptyWaited = 0, 0
-- How often each topic was picked, by the character and the topic
local picks = {}
local lastStatus = 0

local function log(...) print('AUTOPLAY', ...) end

-- The button to press: the one that autoplaygates.lua names for the message of the box (a negative number counts from the
-- last button: -1 is the last), else the first
local function buttonFor(box)
    for _, entry in ipairs(config.boxes or {}) do
        local ok, form = pcall(core.getFormId, entry.file, entry.ref)
        if ok and form == box.form then
            local button = entry.button < 0 and #box.buttons + 1 + entry.button or entry.button
            return math.max(1, math.min(button, #box.buttons))
        end
    end
    return math.min(BUTTON, #box.buttons)
end

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
        local button = buttonFor(box)
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
    if not state.active or state.speaking then
        topicWaited, emptyWaited = 0, 0
        return
    end
    if #state.options == 0 then
        -- nothing to pick from: the player would press Goodbye
        topicWaited = 0
        emptyWaited = emptyWaited + dt
        if emptyWaited >= CLOSE_AFTER then
            emptyWaited = 0
            log('conversation close', 'options=0')
            core.conversation.close()
        end
        return
    end
    emptyWaited = 0
    topicWaited = topicWaited + dt
    if topicWaited < DELAY then return end
    topicWaited = 0
    local best, bestCount, bestKey
    for index, option in ipairs(state.options) do
        local key = state.speakerId .. ' ' .. option.topic
        local count = picks[key] or 0
        if best == nil or count < bestCount then best, bestCount, bestKey = index, count, key end
    end
    if bestCount >= MAX_PICKS then
        log('conversation close', string.format('options=%d picked=%d', #state.options, bestCount))
        core.conversation.close()
        return
    end
    picks[bestKey] = bestCount + 1
    log('topic', string.format('options=%d pick=%d count=%d', #state.options, best, bestCount + 1))
    core.conversation.choose(state.options[best].topic)
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
                log('status', 'mode=' .. tostring(I.UI.getMode()), 'paused=' .. tostring(core.isWorldPaused()),
                    string.format('cell=%s pos=%.0f,%.0f,%.0f', cell and tostring(cell.id) or 'none', self.position.x,
                        self.position.y, self.position.z))
            end
        end,
    },
}
