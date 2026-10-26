-- Plays the New Game of Fallout 3 and New Vegas for a person who is not there: answers the message boxes, finishes the
-- screens of character creation, picks the topics of a conversation (the one it has picked least often with the
-- character, the first of them) and ends it when it has picked them all twice, shoots at what the world script tells it to
-- (the weapon is the player's), picks locks, hacks and walks through terminals, waits when a bed or the Wait key asks how
-- long, spends the points of a level up, and logs each of these in lines that
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
-- How long (real seconds) the player shoots at one target before giving up (the weapon is ready again only after its time
-- between two shots has passed in the game, which a screen that stops the game does not let pass)
local FIRE_LIMIT = 90
-- How many bobby pins are tried on one lock before the screen is left, and how many steps (a menu line, a guess) are made at
-- one terminal before it is closed
local LOCK_TRIES = 40
local TERMINAL_STEPS = 60
-- How long (game hours) the player waits when a screen asks for it (a bed or the Wait key)
local REST_HOURS = 1

local boxSerial, boxWaited, boxAnswered = nil, 0, nil
local pageSerial, pageWaited = nil, 0
local topicWaited, emptyWaited = 0, 0
-- How often each topic was picked, by the character and the topic
local picks = {}
local lastStatus = 0
local lockSerial, lockWaited, lockTries = nil, 0, 0
local terminalWaited, terminalSteps = 0, 0
-- How often each line of each menu was picked at the terminal that is open, by the menu (its name and its lines) and the number
-- of the line; forgotten when the terminal is left
local linePicks = {}
local restWaited = 0
-- What the player is shooting at: the object, its form id (as core.getFormId writes it), how many shots it may take, whether
-- it goes on until the target is dead, the shots fired so far and the real seconds spent
local firing = nil

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

-- A lock the player has in front of it (a door or container that was activated): bobby pins are tried until it opens, the pins
-- are gone, or only a key opens it, and then the screen is left. The screen closes by itself when the lock opens.
local function answerLock(dt)
    local lock = core.interact.lock()
    if not lock then
        lockSerial = nil
        return
    end
    if lock.serial ~= lockSerial then
        lockSerial, lockWaited, lockTries = lock.serial, 0, 0
        log('lock', string.format('level=%d keyOnly=%s pins=%d chance=%.2f', lock.level, tostring(lock.keyOnly), lock.pins,
            lock.chance))
    end
    lockWaited = lockWaited + dt
    if lockWaited < DELAY then return end
    lockWaited = 0
    lockTries = lockTries + 1
    if lock.keyOnly or lock.pins <= 0 or lockTries > LOCK_TRIES then
        log('lock close', lock.keyOnly and 'keyonly' or (lock.pins <= 0 and 'nopins' or 'tries'))
        core.interact.closeLock()
        return
    end
    log('lock pick', core.interact.pick())
end

local function likeness(a, b)
    local same = 0
    for i = 1, math.min(#a, #b) do
        if a:sub(i, i) == b:sub(i, i) then same = same + 1 end
    end
    return same
end

-- The place of the word, in the words of a closed terminal, that agrees with every likeness the guesses so far were given, as a
-- player works it out; nil when there is none
local function hackGuess(terminal)
    for index, word in ipairs(terminal.words) do
        local fits = true
        for _, guess in ipairs(terminal.guesses) do
            if guess.word == word or likeness(word, guess.word) ~= guess.likeness then
                fits = false
                break
            end
        end
        if fits then return index end
    end
    return nil
end

-- A terminal the player has open: a closed one is hacked, a menu is walked through (a line is picked twice at most, then Back,
-- and then the terminal is left), and one that goes on for too many steps is closed.
local function answerTerminal(dt)
    if core.interact.terminalSerial() == 0 then
        terminalWaited, terminalSteps = 0, 0
        linePicks = {}
        return
    end
    terminalWaited = terminalWaited + dt
    if terminalWaited < DELAY then return end
    terminalWaited = 0
    local terminal = core.interact.terminal()
    if not terminal then return end
    terminalSteps = terminalSteps + 1
    if terminalSteps > TERMINAL_STEPS then
        log('terminal close', 'steps')
        core.interact.closeTerminal()
        return
    end
    if terminal.hacking then
        -- Locked out for a while, or no attempts left: the game of hacking goes on when the terminal lets it
        if terminal.lockout > 0 or terminal.attempts <= 0 then return end
        local word = hackGuess(terminal)
        if not word then
            log('hack close', string.format('words=%d guesses=%d', #terminal.words, #terminal.guesses))
            core.interact.closeTerminal()
            return
        end
        log('hack', string.format('word=%d of %d attempts=%d likeness=%d', word, #terminal.words, terminal.attempts,
            core.interact.guess(word)))
        return
    end
    -- Two menus can have the same name, so the lines are part of what tells them apart
    local texts = { terminal.name }
    for _, line in ipairs(terminal.lines) do table.insert(texts, line.text) end
    local menu = table.concat(texts, '\n')
    local best, bestCount
    for _, line in ipairs(terminal.lines) do
        local count = linePicks[menu .. '\n' .. line.index] or 0
        if bestCount == nil or count < bestCount then best, bestCount = line, count end
    end
    if best == nil or bestCount >= MAX_PICKS then
        if terminal.canGoBack then
            log('terminal back', string.format('lines=%d', #terminal.lines))
            core.interact.back()
        else
            log('terminal close', string.format('lines=%d', #terminal.lines))
            core.interact.closeTerminal()
        end
        return
    end
    linePicks[menu .. '\n' .. best.index] = bestCount + 1
    log('terminal line', string.format('lines=%d pick=%d count=%d', #terminal.lines, best.index, bestCount + 1))
    core.interact.choose(best.index)
end

-- The screen that asks how long to wait or sleep (a bed, or the Wait key): REST_HOURS hours, or the screen is left when the
-- game says no (for a reason it gives in words, which the log does not repeat)
local function answerRest(dt)
    local request = core.interact.rest()
    if not request or core.interact.restProgress().active then
        restWaited = 0
        return
    end
    restWaited = restWaited + dt
    if restWaited < DELAY then return end
    restWaited = 0
    local refused = core.interact.startRest(REST_HOURS) ~= ''
    log('rest', 'sleeping=' .. tostring(request.sleeping), refused and 'refused' or 'started')
    if refused then core.interact.cancelRest() end
end

-- The screen of a level up (falloutpipboy.lua opens it, and the world stops until it is left): the points of the level go to the
-- skills in the order of the list, as many as each can take, a perk is picked if one is offered (the first the player qualifies
-- for), and the screen is left; the Pip-Boy script then hands out the next level, if the experience reached one. A level whose
-- points nothing can take (every skill at 100, no perk) is logged once and the screen is left alone.
local levelWaited, levelStuck = 0, false
local function answerLevelUp(dt)
    local pipboy = core.pipboy
    if levelStuck or not pipboy.available() or I.UI.getMode() ~= 'Interface' then
        levelWaited = 0
        return
    end
    local stats = pipboy.stats()
    local offered = stats.perkPoints > 0 and pipboy.offeredPerks() or {}
    if stats.skillPoints <= 0 and #offered == 0 then
        levelWaited = 0
        return
    end
    levelWaited = levelWaited + dt
    if levelWaited < DELAY then return end
    levelWaited = 0
    local left, perk = stats.skillPoints, nil
    for _, skill in ipairs(pipboy.skills()) do
        if left <= 0 then break end
        left = left - pipboy.spendSkillPoints(skill.number, left)
    end
    for _, offer in ipairs(offered) do
        if offer.available ~= false and pipboy.choosePerk(offer.id) then
            perk = offer.id
            break
        end
    end
    log('level', string.format('playerLevel=%d skillPoints=%d spent=%d perk=%s', stats.level, stats.skillPoints,
        stats.skillPoints - left, tostring(perk)))
    if left == stats.skillPoints and not perk then
        levelStuck = true
        log('level stuck')
        return
    end
    I.UI.removeMode('Interface')
end

-- The screens above need the content of Fallout 3 or New Vegas; an error in one of them is logged once and they are left
local screensBroken = false
local function answerScreens(dt)
    if screensBroken then return end
    local ok, err = pcall(function()
        answerLock(dt)
        answerTerminal(dt)
        answerRest(dt)
        answerLevelUp(dt)
    end)
    if not ok then
        screensBroken = true
        log('screens error', tostring(err))
    end
end

-- The player takes the weapon in hand and shoots at the target, once each time the weapon is ready, until the shots are
-- fired or (when the gate says so) the target is dead. A shot at an object tells its script about the blow; one at a character
-- or creature takes health from it.
local function shoot(dt)
    if not firing then return end
    firing.waited = firing.waited + dt
    local why
    if firing.untilDead and core.combat.isDead(firing.form) then
        why = 'dead'
    elseif firing.fired >= firing.shots then
        why = 'shots'
    elseif firing.waited >= FIRE_LIMIT then
        why = 'time'
    else
        local ok, row = pcall(core.combat.fire, firing.target)
        if not ok then
            log('fire error', tostring(row))
            why = 'error'
        elseif row.outOfAmmo then
            why = 'ammo'
        elseif row.fired then
            firing.fired = firing.fired + 1
            log('fire', string.format('shot=%d hit=%s damage=%.1f killed=%s', firing.fired, tostring(row.hit),
                row.damage, tostring(row.killed)))
        end
    end
    if why then
        log('fire done', why, string.format('shots=%d real=%.1f', firing.fired, firing.waited))
        firing = nil
    end
end

return {
    eventHandlers = {
        AutoplayFire = function(data)
            local weapon = core.combat.weapon()
            log('fire start', string.format('shots=%d untilDead=%s rounds=%d range=%.0f', data.shots,
                tostring(data.untilDead), weapon.rounds, weapon.range))
            core.combat.setDrawn(true)
            firing = { target = data.target, form = data.form, shots = data.shots, untilDead = data.untilDead,
                fired = 0, waited = 0 }
        end,
    },
    engineHandlers = {
        -- every frame, also while a screen or a box stops the game (the time of the game does not pass then)
        onFrame = function()
            local dt = core.getRealFrameDuration()
            answerBox(dt)
            finishPage(dt)
            pickTopic(dt)
            answerScreens(dt)
            shoot(dt)
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
