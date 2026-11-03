-- The quest text of the head-up display of Fallout New Vegas, as the executable runs it (FalloutNV.exe UpdateQuestText
-- 0x77A650, the lines of objectives 0x77B430, the tile processor 0x77C170; Mac round 12AR C1). This file keeps the state of
-- the two parts and says what is on the screen; falloutmenuhud.lua draws it.
--
--   * QuestAdded: an event (a quest was added, completed or failed, a place was found) shows its heading (font 7) above the
--     name of the quest, which is written in upper case in font 8, one character to a tile: the character k starts to fade in
--     (2 s) at 0.1 s x (k + 1) after the event began, stays 4 s from then and fades out (2 s). A name of 32 characters or more
--     goes on a second line from the last space within the first 33; no more than 63 characters are shown.
--   * QuestStages: the lines of objectives. A batch of what has waited is taken when no line is left; the line that was queued
--     last is the first (at the top). The first four lines are looked after: the first fades in over 0.5 s; the second and the
--     third follow 1 s and 2 s later (when the first has been there 3 s); the first stays 3 s after that and fades out
--     (0.5 s), and the lines under it scroll up by its height (0.5 s). So the first line is shown 6.5 s, the others 3.5 s
--     after they were the first.
--   * Nothing is taken from the queues until 2 s have passed since the last thing was queued; an event is shown before the
--     lines (the lines wait while it is on the screen), and a quest that was completed takes the lines away and the ones that
--     wait with them.
local module = {}

module.GATE = 2.0
module.CHAR_FADE_IN = 2.0
module.CHAR_FADE_OUT = 2.0
module.CHAR_REMAIN = 4.0
-- int(fQuestCinematicCharacterFadeInDelay x 1000) ms: the delay between two characters
module.CHAR_DELAY = 0.1
module.LINE_FIRST_FADE = 0.5
module.LINE_FADE_IN = 0.5
module.LINE_FADE_OUT = 0.5
-- fQuestCinematicObjectiveFadeInDelay: the first line is paused 3 x this before its own pause begins, the second 1 x
-- and the third 2 x before they fade in
module.LINE_DELAY = 1.0
module.LINE_PAUSE = 3.0
module.LINE_SCROLL = 0.5
-- Tiles under QuestAdded: the heading and 63 characters; the lines of QuestStages
module.MAX_CHARS = 63
module.MAX_LINES = 32
-- How far the heading is above the name (its height and this), the line break of a long name, the second line's distance
module.HEADING_GAP = 15
module.BREAK_AT = 32
module.SECOND_LINE_GAP = 5
-- The alpha that the display fades to (the opacity of the interface is 1 here)
module.ALPHA = 255
-- The lines of one batch are in the list with the last one queued first, so that one is the top line (as the walk of the
-- executable's list reads); false lays the first queued line at the top
module.NEWEST_FIRST = true

local Quests = {}
Quests.__index = Quests

function module.new()
    return setmetatable({
        now = 0,
        gateUntil = 0,
        events = {},
        objectives = {},
        sounds = {},
        added = { active = false, heading = nil, chars = {} },
        stages = { active = false, lines = {} },
    }, Quests)
end

-- ---- Animations of a trait ----------------------------------------------------------------------------------------------

-- A trait that moves in a straight line to a value: `value`, `from`, `to`, `left` seconds of `seconds`
local function trait(value)
    return { value = value or 0 }
end

local function animate(tr, from, to, seconds)
    tr.value = from
    if seconds <= 0 then
        tr.value, tr.anim = to, nil
        return
    end
    tr.anim = { from = from, to = to, seconds = seconds, age = 0 }
end

local function advance(tr, dt)
    local a = tr.anim
    if not a then return end
    a.age = a.age + dt
    if a.age >= a.seconds then
        tr.value, tr.anim = a.to, nil
    else
        tr.value = a.from + (a.to - a.from) * a.age / a.seconds
    end
end

-- What the trait is going to be: the end of its animation, or its value
local function finalValue(tr)
    return tr.anim and tr.anim.to or tr.value
end

local function stop(tr, value)
    tr.value, tr.anim = value, nil
end

local function newTile(text, font)
    return {
        text = text, font = font, x = 0, y = 0, visible = false, alpha = trait(0), user0 = nil, user1 = nil,
    }
end

-- ---- What the game tells ---------------------------------------------------------------------------------------------------

-- An event: `kind` (0 a quest was added, 1 completed, 2 failed, 3 a place was found), `heading`, `name` and `sound`
function Quests:addEvent(event)
    self.events[#self.events + 1] = event
    self.gateUntil = self.now + module.GATE
end

-- A line of an objective: `text` (a completed one has its word in front already), `completed`, `reminder`
function Quests:addObjective(line)
    self.objectives[#self.objectives + 1] = line
    self.gateUntil = self.now + module.GATE
end

-- Everything on the screen is gone (the game was loaded)
function Quests:clear()
    self.events, self.objectives, self.sounds = {}, {}, {}
    self.added = { active = false, heading = nil, chars = {} }
    self.stages = { active = false, lines = {} }
    self.gateUntil = 0
end

-- The names of the sounds that the last steps began, once
function Quests:takeSounds()
    local sounds = self.sounds
    self.sounds = {}
    return sounds
end

-- ---- QuestAdded ------------------------------------------------------------------------------------------------------------

local function characters(text)
    local list = {}
    for _, code in utf8.codes(text) do list[#list + 1] = utf8.char(code) end
    return list
end

-- The tile processor (0x77C170) for one tile: the fade in begins when its time has come, the fade out when its time is over,
-- and a tile that has faded out is hidden
local function processTile(tile, now)
    if tile.user0 and tile.user0 < now then
        animate(tile.alpha, 0, module.ALPHA, module.CHAR_FADE_IN)
        tile.user0 = nil
    elseif tile.user1 and tile.user1 < now then
        animate(tile.alpha, tile.alpha.value > 175 and tile.alpha.value or 175, 0, module.CHAR_FADE_OUT)
        tile.user1 = nil
    elseif finalValue(tile.alpha) == 0 and tile.alpha.value == 0 and tile.user1 == nil then
        tile.visible = false
    end
end

local function anyVisible(tiles)
    for _, tile in ipairs(tiles) do
        if tile.visible then return true end
    end
    return false
end

-- The tiles of QuestAdded: the heading and the characters
local function addedTiles(added)
    local tiles = { added.heading }
    for _, tile in ipairs(added.chars) do tiles[#tiles + 1] = tile end
    return tiles
end

function Quests:addedBusy()
    local added = self.added
    if not added.active then return false end
    if added.heading and anyVisible(addedTiles(added)) then return true end
    added.active = false
    return false
end

function Quests:stagesBusy()
    local stages = self.stages
    if not stages.active then return false end
    if anyVisible(stages.lines) then return true end
    stages.active = false
    return false
end

-- Takes the first event and writes it on the tiles. `env.measure(font, text, wrap)` gives { width, height }
function Quests:startEvent(env)
    local event = table.remove(self.events, 1)
    local now = self.now
    local added = { active = true, chars = {} }
    self.added = added
    local heading = newTile(event.heading or '', 7)
    local size = env.measure(7, heading.text, 2048) or {}
    heading.y = -((size.height or 0) + module.HEADING_GAP)
    heading.visible = true
    heading.user0, heading.user1 = now, now + module.CHAR_REMAIN
    added.heading = heading
    if event.sound and event.sound ~= '' then self.sounds[#self.sounds + 1] = event.sound end
    if event.kind == 1 then
        -- A quest that is completed takes the lines that are shown away, and those that wait (0x77F5E0)
        for _, line in ipairs(self.stages.lines) do
            line.visible = false
            stop(line.alpha, 0)
            stop(line.wc, 0)
            stop(line.ht, 0)
        end
        self.objectives = {}
    end
    -- The name is in upper case, a character to a tile
    local name = string.upper(event.name or '')
    local list = characters(name)
    while #list > module.MAX_CHARS do list[#list] = nil end
    local breakAt = module.BREAK_AT
    if #list >= module.BREAK_AT then
        for i = module.BREAK_AT, 0, -1 do
            if list[i + 1] == ' ' then
                breakAt = i
                break
            end
        end
    end
    local x = 0
    local time = now
    for i, char in ipairs(list) do
        local index = i - 1
        local tile = newTile(char, 8)
        local measured = env.measure(8, char, 0) or {}
        if index == breakAt + 1 then x = 0 end
        tile.x = x
        tile.y = index <= breakAt and 0 or ((measured.height or 0) + module.SECOND_LINE_GAP)
        time = time + module.CHAR_DELAY
        tile.user0, tile.user1 = time, time + module.CHAR_REMAIN
        tile.visible = true
        x = x + math.floor(measured.width or 0)
        added.chars[#added.chars + 1] = tile
    end
end

-- ---- QuestStages ---------------------------------------------------------------------------------------------------------

function Quests:takeLines(env)
    local stages = self.stages
    local taken = {}
    while #self.objectives > 0 and #taken < module.MAX_LINES do
        taken[#taken + 1] = table.remove(self.objectives, 1)
    end
    stages.active = true
    local lines = {}
    for _, entry in ipairs(taken) do
        local line = {
            text = entry.text, completed = entry.completed == true, reminder = entry.reminder == true,
            x = 0, y = 0, visible = true, alpha = trait(0), wc = trait(0), ht = trait(0), yTrait = trait(0),
        }
        self.sounds[#self.sounds + 1] = 'UIQuestUpdate'
        if module.NEWEST_FIRST then
            table.insert(lines, 1, line)
        else
            lines[#lines + 1] = line
        end
    end
    stages.lines = lines
    -- The lines go one under the other in the order of the list, each as high as its text
    local wrap = math.floor((env.width or 1280) / 3)
    local top = 0
    for _, line in ipairs(lines) do
        line.wrap = wrap
        line.yTrait.value = top
        local size = env.measure(7, line.text, wrap) or {}
        top = top + math.floor(size.height or 0)
    end
end

local function visibleLines(stages)
    local list = {}
    for _, line in ipairs(stages.lines) do
        if line.visible then list[#list + 1] = line end
    end
    return list
end

-- The first four lines that are shown go through the sequence of the executable's 0x77B430
function Quests:driveLines()
    local stages = self.stages
    local list = visibleLines(stages)
    local a, b, c, d = list[1], list[2], list[3], list[4]
    if not a then return end
    local alpha = module.ALPHA
    if a.wc.value == 0 and not a.wc.anim then
        animate(a.alpha, 0, alpha, module.LINE_FIRST_FADE)
        animate(a.wc, 0, 1, 3 * module.LINE_DELAY)
        if b then animate(b.wc, 0, 1, 1 * module.LINE_DELAY) end
        if c then animate(c.wc, 0, 1, 2 * module.LINE_DELAY) end
    end
    for _, line in ipairs({ b, c }) do
        if line and line.wc.value == 1 and not line.wc.anim and line.alpha.value == 0 and not line.alpha.anim then
            animate(line.alpha, 0, alpha, module.LINE_FADE_IN)
        end
    end
    if a.wc.value == 1 and not a.wc.anim then
        if a.ht.value == 0 and not a.ht.anim then
            animate(a.ht, 0, 1, module.LINE_PAUSE)
        elseif a.ht.value == 1 and not a.ht.anim and not a.fading then
            animate(a.alpha, a.alpha.value, 0, module.LINE_FADE_OUT)
            a.wc.value = 2
            a.fading = true
        end
    end
    if a.wc.value == 2 and not a.alpha.anim and a.alpha.value == 0 then
        if b then
            local dy = math.floor(b.yTrait.value - a.yTrait.value)
            for _, line in ipairs(stages.lines) do
                if line ~= a then animate(line.yTrait, line.yTrait.value, line.yTrait.value - dy, module.LINE_SCROLL) end
            end
        end
        if d then
            d.wc.value, d.wc.anim = 1, nil
        end
        a.visible = false
        a.wc.value = 0
    end
end

-- ---- A frame ---------------------------------------------------------------------------------------------------------------

-- `dt` seconds have passed. `env`: `active` (the display is in a mode of the game), `width` (of the menu, in its units) and
-- `measure(font, text, wrap)`. The animations run whatever the mode is; what is taken from the queues and what begins is
-- looked at only in a mode of the game and when the gate of 2 s since the last thing was queued is open.
function Quests:step(dt, env)
    self.now = self.now + dt
    local added, stages = self.added, self.stages
    for _, tile in ipairs(added.heading and addedTiles(added) or {}) do advance(tile.alpha, dt) end
    for _, line in ipairs(stages.lines) do
        advance(line.alpha, dt)
        advance(line.wc, dt)
        advance(line.ht, dt)
        advance(line.yTrait, dt)
    end
    if not env.active or self.now < self.gateUntil then return end
    if added.heading then
        for _, tile in ipairs(addedTiles(added)) do processTile(tile, self.now) end
    end
    if self:addedBusy() then
        -- The lines wait while an event is on the screen, and begin again when it is over
        for _, line in ipairs(stages.lines) do
            stop(line.alpha, 0)
            stop(line.wc, 0)
            line.fading = nil
        end
        return
    end
    if #self.events > 0 then
        local top = stages.lines[1]
        if not self:stagesBusy() or (top and top.alpha.value == 0) then
            self:startEvent(env)
            return
        end
    end
    if #self.objectives > 0 and not self:stagesBusy() then self:takeLines(env) end
    if self:stagesBusy() then self:driveLines() end
end

-- What is on the screen: the heading, the characters and the lines, each with its place and alpha (0 to 255)
function Quests:view()
    local view = { chars = {}, lines = {} }
    local heading = self.added.heading
    if heading then
        view.heading = {
            text = heading.text, font = heading.font, x = heading.x, y = heading.y, visible = heading.visible,
            alpha = math.floor(heading.alpha.value + 0.5),
        }
        for _, tile in ipairs(self.added.chars) do
            view.chars[#view.chars + 1] = {
                text = tile.text, font = tile.font, x = tile.x, y = tile.y, visible = tile.visible,
                alpha = math.floor(tile.alpha.value + 0.5),
            }
        end
    end
    for _, line in ipairs(self.stages.lines) do
        view.lines[#view.lines + 1] = {
            text = line.text, completed = line.completed, y = math.floor(line.yTrait.value + 0.5), visible = line.visible,
            alpha = math.floor(line.alpha.value + 0.5), wrap = line.wrap,
        }
    end
    return view
end

return module
