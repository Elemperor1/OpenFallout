-- The scenario player of the rounds (global script). A round is a check of a build on the real game: instead of a person (or a
-- session that presses keys and moves the pointer) driving the game, the scenario file `scripts/roundscenario.lua` says what
-- to do, in order, and this script does it from inside the engine, so that the window can stay hidden (`--headless`) and
-- nobody's keyboard, pointer or screen is touched. The scenario is a Lua function that is given `r` (below) and runs as a
-- coroutine: `r.wait(5)` lets five real seconds go by, `r.shot('name')` takes an engine screenshot, `r.teleport` moves the
-- player, and so on. Every line this script and the player script log starts with ROUND, and holds counts, numbers and record
-- ids, never a text of the game. `scripts/openfallout/round/round.py` runs it and reads the lines.
--
-- The file returns either the function, or a table `{ run = function(r) ... end, limit = <real seconds>, delay = <real seconds
-- before the first step> }`. When the function ends (or fails: `ROUND error`), the game is quit once the screenshots are written.
local core = require('openfallout.core')
local util = require('openfallout.util')
local world = require('openfallout.world')
local types = require('openfallout.types')

local function log(...) print('ROUND', ...) end

-- A step that fails logs a line that starts with `error`: round.py fails the round on it (the scenario goes on, so that the
-- report shows everything that went wrong, but a round that asked for a place it did not get is not a pass)
local function outcome(step, ok, err, ...)
    if ok then log(step, 'ok', ...) else log('error', step, tostring(err), ...) end
    return ok
end

local loaded, scenario = pcall(require, 'scripts.roundscenario')
if not loaded then
    log('error', 'no scenario', tostring(scenario))
    scenario = nil
end
local run = type(scenario) == 'function' and scenario or (type(scenario) == 'table' and scenario.run or nil)
local LIMIT = type(scenario) == 'table' and scenario.limit or 600
local DELAY = type(scenario) == 'table' and scenario.delay or 3

local started, co, waiting = core.getRealTime(), nil, nil
-- the answers of the player script, by the number of the question
local answers, asked = {}, 0
local steps = 0
local finished = false
-- how many pictures were asked for: the engine writes each one in the background, in the work queue that also loads the land, and
-- on a busy Mac the second picture of a run was not written within seven seconds (rounds R5 and R6)
local shotsTaken = 0

local function now() return core.getRealTime() end

local function player() return world.players[1] end

local function describe(object)
    if object == nil then return 'none' end
    local cell = object.cell
    local where = cell and (cell.isExterior and string.format('%s:%d,%d', tostring(cell.worldSpaceId), cell.gridX or 0, cell.gridY or 0)
        or tostring(cell.id)) or 'nocell'
    local p = object.position
    return string.format('%s record=%s cell=%s pos=%.0f,%.0f,%.0f enabled=%s', tostring(object.id), tostring(object.recordId),
        where, p.x, p.y, p.z, tostring(object.enabled))
end

local function lookup(form)
    -- a form id as core.getFormId writes it ("FormId:0x1104c0c"), or { 'FalloutNV.esm', 0x104c0c }
    if type(form) == 'table' then form = core.getFormId(form[1], form[2]) end
    return world.getObjectByFormId(form)
end

-- The steps the scenario can take. Those that wait (`wait`, `waitFor`, `shot` and everything that asks the player script)
-- must be called from the scenario function itself, not from inside a pcall.
local r = {}

function r.log(...) log('note', ...) end

-- A step of a scenario that failed: a line that starts with `error`, which fails the round (r.log is only a note)
function r.error(...) log('error', ...) end

function r.wait(seconds)
    coroutine.yield({ at = now() + seconds })
end

-- Waits until the function gives something true, at most `timeout` real seconds (60 by default); gives whether it did
function r.waitFor(predicate, timeout, label)
    local deadline = now() + (timeout or 60)
    local ok = coroutine.yield({ predicate = predicate, deadline = deadline })
    log('waitFor', tostring(label or '-'), ok and 'ok' or 'timeout', string.format('real=%.1f', now() - started))
    return ok
end

-- Asks the player script (which owns the camera, the screenshots and the rays) and waits for its answer
local function ask(what, args)
    asked = asked + 1
    local id = asked
    local data = { id = id, what = what, args = args or {} }
    player():sendEvent('RoundAsk', data)
    local reply = coroutine.yield({ answer = id, deadline = now() + 20 })
    if reply == nil then
        log('error', what, 'the player script did not answer')
        return nil
    end
    return reply
end

-- Waits until no message box has been open for `quiet` real seconds (6 by default), at most `timeout` seconds (40): the boxes that
-- the game opens are answered by the player script one second after they open, the next one can follow at once, and a run of
-- them can pause for five seconds between two. It gives at once when no box has been seen yet (a notice that has not opened by
-- then is not waited for) and after `r.boxes(false)`, even if a box is open. Gives whether it came to rest; a timeout is a note
-- (the `shot` line then says which box was open), not a failed round.
function r.settle(quiet, timeout)
    quiet, timeout = quiet or 6, timeout or 40
    local started_at = now()
    local deadline = started_at + timeout
    while true do
        local reply = ask('quiet', {})
        if reply == nil then return false end
        if not reply.open and (reply.quiet or math.huge) >= quiet then
            if now() - started_at >= 0.5 then log('settle', 'ok', string.format('waited=%.1f', now() - started_at)) end
            return true
        end
        if now() >= deadline then
            log('settle', 'timeout', string.format('waited=%.1f', now() - started_at))
            return false
        end
        r.wait(0.25)
    end
end

-- Takes a screenshot of what the engine draws. The file is `screenshotNNN` in the screenshots folder of the user data; the
-- line `ROUND shot <name> n=<number>` says which is which, and round.py renames them
function r.shot(name, settle)
    -- a picture with a message box in it shows nothing: the notices of add-ons come one after the other when a place has loaded
    if settle ~= false then r.settle() end
    local reply = ask('shot', { name = name })
    shotsTaken = shotsTaken + 1
    -- the engine writes the picture on the frame after, in the background
    r.wait(0.6)
    return reply
end

-- Turns the camera (radians). The pitch is down positive, as everywhere in the Lua API (`pitchChange`, `camera.setPitch`): 0.3
-- looks down, -0.3 looks up.
function r.look(yaw, pitch) return ask('look', { yaw = yaw, pitch = pitch }) end

function r.camera(mode) return ask('camera', { mode = mode }) end

-- A message box that the game opens covers the picture and nobody clicks it in a round, so it is answered with its first button
-- after a second (`ROUND box` lines name it, never its text). `r.boxes(false)` leaves the boxes open, `r.boxes(true)` answers again.
function r.boxes(answer) return ask('boxes', { answer = answer ~= false }) end

-- Logs where the player's eye is and what the camera does; the answer is a table
function r.eye(label) return ask('eye', { label = label }) end

-- The height of the ground (rays down from above the eye) at the eye and at the given offsets ({ { dx, dy }, ... })
function r.ground(label, offsets) return ask('ground', { label = label, offsets = offsets }) end

-- The characters near the player: record id, distance, whether dead, up to `count` of them
function r.actors(label, radius, count) return ask('actors', { label = label, radius = radius, count = count }) end

-- Frame times of the last `seconds` seconds
function r.fps(label, seconds) return ask('fps', { label = label, seconds = seconds }) end

-- Takes the player to a place. `pos` is { x, y, z } (z can be left out with `onGround = true`); in the open the cell is the one
-- of the position, in the worldspace of the cell that the player is in or `worldspace` (a cell or a cell name of it); `cell` is
-- the name of a cell to go into, `cellId` its id. `yaw` turns the player (radians).
function r.teleport(o)
    local p = player()
    local ok, err = pcall(function()
        local pos = o.pos and util.vector3(o.pos[1], o.pos[2], o.pos[3] or p.position.z) or p.position
        local cell
        if o.cellId then
            cell = world.getCellById(o.cellId)
        elseif o.cell then
            cell = world.getCellByName(o.cell)
        else
            cell = world.getExteriorCell(math.floor(pos.x / 4096), math.floor(pos.y / 4096), o.worldspace or p.cell)
        end
        local options = { onGround = o.onGround == true }
        if o.yaw then options.rotation = util.transform.rotateZ(o.yaw) end
        p:teleport(cell, pos, options)
    end)
    return outcome('teleport', ok, err, o.label or '-',
        o.pos and string.format('pos=%.0f,%.0f,%s', o.pos[1], o.pos[2], o.pos[3] and string.format('%.0f', o.pos[3]) or '-') or '-')
end

-- The hour of the day (the game's global GameHour)
function r.hour(hour)
    local ok, err = pcall(function() world.globals.GameHour = hour end)
    return outcome('hour', ok, err, string.format('%.2f', hour))
end

-- Reads a global variable, or sets it too when a value is given
function r.global(name, value)
    local ok, result = pcall(function()
        if value ~= nil then world.globals[name] = value end
        return world.globals[name]
    end)
    if ok then log('global', name, tostring(result)) else log('error', 'global', name, tostring(result)) end
    return ok and result or nil
end

-- Puts a quest at a stage (when `stage` is given) and logs the stage it is at
function r.quest(name, stage)
    local ok, result = pcall(function()
        local quest = world.quests.find(name)
        if quest == nil then
            -- asking for a quest that is not there is an answer; asking to put it at a stage is a failed step
            if stage ~= nil then error('no such quest') end
            return 'missing'
        end
        if stage ~= nil then
            -- setStage answers false (it does not throw) when the quest has no such stage, or the stage was done already and
            -- may not repeat: the stage the quest is at then says whether the setup the scenario asked for is there
            if quest:setStage(stage) == false and quest.stage ~= stage then
                error(string.format('the quest could not be set to stage %s (it is at stage %s)', tostring(stage), tostring(quest.stage)))
            end
        end
        return string.format('stage=%d running=%s', quest.stage, tostring(quest.running))
    end)
    if ok then log('quest', name, result) else log('error', 'quest', name, tostring(result)) end
    return ok and result or nil
end

-- Logs what the world says about an object ("FormId:0x..." or { file, id })
function r.object(form, label)
    local ok, result = pcall(function() return describe(lookup(form)) end)
    local what = tostring(label or (type(form) == 'string' and form or form[2]))
    if ok then log('object', what, result) else log('error', 'object', what, tostring(result)) end
    return ok and lookup(form) or nil
end

function r.activate(form, label)
    local ok, err = pcall(function() lookup(form):activateBy(player()) end)
    local what = tostring(label or (type(form) == 'string' and form or form[2]))
    if ok then log('activate', what, 'ok') else log('error', 'activate', what, tostring(err)) end
    return ok
end

-- How fast the game goes relative to the clock (1 is normal; more than about 5 makes the physics step too long)
function r.speed(scale)
    local ok, err = pcall(function() world.setSimulationTimeScale(scale) end)
    return outcome('speed', ok, err, string.format('%.2f', scale))
end

-- ---- M14 additions: characters, weapons, the fight -------------------------------------------------------------------------
-- Records are given as "FormId:0x..." (core.getFormId) or { file, id }; objects are the ones these steps return.

local function recordOf(record)
    if type(record) == 'table' then return core.getFormId(record[1], record[2]) end
    return record
end

local function form(object)
    if object == nil then return '' end
    local id = tostring(object.id)
    -- a placed reference's id is "0x..." and the combat calls want it as core.getFormId writes it
    if id:sub(1, 2) == '0x' then id = 'FormId:' .. id end
    -- the player's own reference has no form id of the content; the games call it 0x14
    if object == player() then id = 'FormId:0x14' end
    return id
end

-- Makes a character (or any object) of the given record and puts it in the world: `pos` is a position, or `dist` and `yaw` put it
-- that far from the player along the direction `yaw` (radians, 0 north, as the Lua API turns things); the cell is the player's.
-- Gives the object (or nil), logs the record, the form id and the place.
function r.spawn(o)
    local label = o.label or '-'
    local object
    local ok, err = pcall(function()
        local p = player()
        local pos = o.pos and util.vector3(o.pos[1], o.pos[2], o.pos[3] or p.position.z)
            or (p.position + util.vector3(math.sin(o.bearing or 0), math.cos(o.bearing or 0), 0) * (o.dist or 1000))
        object = world.createObject(recordOf(o.record), o.count)
        local options = {}
        if o.yaw then options.rotation = util.transform.rotateZ(o.yaw) end
        object:teleport(p.cell, pos, options)
    end)
    if ok then log('spawn', label, 'ok', 'id=' .. tostring(object.id), describe(object)) else log('error', 'spawn', label, tostring(err)) end
    return ok and object or nil
end

-- Puts an object (a character that stands in the world already, or one that r.spawn made) at `dist` units from the player along
-- the direction `bearing` (radians, 0 north), in the player's cell; `yaw` turns it (default: facing the player)
function r.place(object, o)
    local ok, err = pcall(function()
        local p = player()
        local pos = o.pos and util.vector3(o.pos[1], o.pos[2], o.pos[3] or p.position.z)
            or (p.position + util.vector3(math.sin(o.bearing or 0), math.cos(o.bearing or 0), 0) * (o.dist or 1000))
        local yaw = o.yaw
        if yaw == nil then
            local d = p.position - pos
            yaw = math.atan2(d.x, d.y)
        end
        object:teleport(p.cell, pos, { rotation = util.transform.rotateZ(yaw), onGround = true })
    end)
    return outcome('place', ok, err, tostring(o.label or '-'), o.dist and string.format('dist=%.0f bearing=%.2f', o.dist, o.bearing or 0) or '-')
end

-- Puts an object (a character from some other place of the world, say) at a position `pos` = { x, y, z } of the same worldspace as
-- the player's, whatever cell that is; `yaw` turns it (0 faces north)
function r.putAt(object, o)
    local ok, err = pcall(function()
        local p = player()
        local cell = world.getExteriorCell(math.floor(o.pos[1] / 4096), math.floor(o.pos[2] / 4096), p.cell)
        object:teleport(cell, util.vector3(o.pos[1], o.pos[2], o.pos[3] or p.position.z),
            { rotation = util.transform.rotateZ(o.yaw or 0), onGround = o.onGround ~= false })
    end)
    return outcome('putAt', ok, err, tostring(o.label or '-'), string.format('pos=%.0f,%.0f', o.pos[1], o.pos[2]))
end

-- Puts `count` items of a record into the inventory of an actor (the player by default)
function r.give(record, count, actor, label)
    local ok, err = pcall(function()
        world.createObject(recordOf(record), count or 1):moveInto(types.Actor.inventory(actor or player()))
    end)
    return outcome('give', ok, err, tostring(label or record), tostring(count or 1))
end

-- A command of the script language of Fallout by its name, on a reference ("player" or a form id as text; none for a command that
-- needs none) with numbers or form ids as text: it runs on the next frame and the value it gives is in the engine log
function r.command(name, reference, arguments)
    local reply = ask('command', { name = name, reference = reference, arguments = arguments })
    r.wait(0.4)
    return reply
end
-- Uses an item of the player (the Pip-Boy's own use: an aid item is eaten or drunk), by its form id as text
function r.pipboyUse(form, label) return ask('pipboyUse', { form = form, label = label }) end
-- The quests the Pip-Boy lists: how many, and the one that is followed (the Pip-Boy's own list of quests)
function r.pipboyQuests(label) return ask('pipboyQuests', { label = label }) end
-- The player's weapon, ammunition and what is in the hand; `weapon{label}` logs it, `equip(record)` puts a weapon of the inventory in the hand
function r.weapon(label) return ask('weapon', { label = label }) end
-- `equip(record)`: the weapon with that record in the hand; without one, the first ranged weapon that has rounds (`pick`: the nth)
function r.equip(record, pick) return ask('equip', { form = record and recordOf(record) or nil, pick = pick }) end
-- What an actor carries as the Pip-Boy lists it (the player when `actor` is left out): one line for each weapon and ammunition
function r.inventory(label, actor) return ask('inventory', { label = label, holder = actor and form(actor) or nil }) end
function r.god(on) return ask('god', { on = on }) end
function r.drawn(on) return ask('drawn', { on = on }) end
function r.fire(label, target) return ask('fire', { label = label, target = target }) end
function r.aimAt(target, label, up) return ask('aimAt', { target = target, label = label, up = up }) end
function r.move(o) return ask('move', o) end
function r.crouch(on) return ask('crouch', { on = on }) end
function r.use(on) return ask('use', { on = on }) end
function r.status(label) return ask('status', { label = label }) end
function r.mode(label) return ask('mode', { label = label }) end
function r.actorState(label, actor) return ask('actorState', { label = label, actor = actor }) end
function r.vatsInfo(label, actor) return ask('vatsInfo', { label = label, actor = actor }) end
function r.messages(label) return ask('messages', { label = label }) end
-- The player's health as the player script reads it (nil when the stats are not there)
function r.hp() local reply = ask('status', { label = 'hp' }) return reply and reply.hp end
function r.vats(button) return ask('vats', { button = button }) end
-- Animation sampling: `list` is { { label = 'player', actor = obj }, ... }; one `ROUND sample` line each `every` seconds
function r.track(list, every) return ask('track', { list = list, every = every }) end
function r.untrack() return ask('untrack', {}) end

-- The fight of the engine (core.combat): the actor fights the target (objects)
function r.fight(actor, target, label)
    local ok, err = pcall(function() core.combat.start(form(actor), form(target)) end)
    local state = ''
    if ok then
        local good, text = pcall(function()
            return string.format('inCombat=%s target=%s', tostring(core.combat.inCombat(form(actor))), core.combat.target(form(actor)))
        end)
        state = good and text or tostring(text)
        -- the call does not say whether the fight began (a dead actor, or one that is out, is not started): it is a failed step when the
        -- actor is not in a fight a moment after, so that a round does not pass without the fight that it asked for
        local fighting, inCombat = pcall(core.combat.inCombat, form(actor))
        if fighting and not inCombat then
            ok, err = false, 'the fight did not begin (the actor is not in combat)'
        end
    end
    return outcome('fight', ok, err, tostring(label or '-'), state)
end

-- Two factions (form ids) are enemies, both ways, as SetEnemy says; for a round with a character that attacks the player on sight
function r.enemy(first, second, label)
    local ok, err = pcall(function() core.combat.setEnemy(first, second) end)
    return outcome('enemy', ok, err, tostring(label or '-'), tostring(first), tostring(second))
end

function r.peace(actor, label)
    local ok, err = pcall(function() core.combat.stop(form(actor)) end)
    return outcome('peace', ok, err, tostring(label or '-'))
end

-- The fight state of an actor, once, in a line: the engine's own words for whom it fights, whether it is dead, its health
function r.state(label, actor) return ask('actorState', { label = label, actor = actor }) end

-- Time spent on the shots of the player: `shots` shots, one each `interval` seconds, each at what the camera ray hits (or `target`),
-- a picture each `shotEvery` shots when `shotName` is given; stops at once when the target is dead. Gives how many were fired.
function r.shoot(o)
    local fired = 0
    for i = 1, o.shots or 1 do
        local row = r.fire(o.label or 'shot', o.target)
        if row and row.fired then fired = fired + 1 end
        if o.shotName and (i % (o.shotEvery or 1) == 0) then r.shot(o.shotName .. i, false) end
        if o.target then
            local good, dead = pcall(core.combat.isDead, form(o.target))
            if good and dead then break end
        end
        r.wait(o.interval or 1)
    end
    log('shoot', tostring(o.label or '-'), 'fired=' .. fired)
    return fired
end

function r.inspect(label, actor) return r.inventory(label, actor) end

r.form = form
r.types = types

r.world = world
r.core = core
r.util = util
r.player = player
r.lookup = lookup
r.describe = describe

local function finish(reason)
    if finished then return end
    finished = true
    log('done', reason, string.format('real=%.1f steps=%d', now() - started, steps))
    -- a screenshot that is still being written is lost when the game quits: with pictures to write the game waits long, round.py
    -- stops it as soon as they are saved (it reads the log), and this delay only matters where nothing else does
    waiting = { at = now() + (shotsTaken > 0 and 30 or 2), quit = true }
end

local function resume(value)
    steps = steps + 1
    local ok, request = coroutine.resume(co, value)
    if not ok then
        log('error', 'scenario', tostring(request))
        finish('error')
    elseif coroutine.status(co) == 'dead' then
        finish('end')
    else
        waiting = request
    end
end

return {
    engineHandlers = {
        onUpdate = function()
            local t = now()
            if t - started >= LIMIT and not finished then
                log('error', 'time limit', string.format('%.0f', LIMIT))
                finish('limit')
                return
            end
            if waiting and waiting.quit then
                if t >= waiting.at then core.quit() end
                return
            end
            if co == nil then
                if run == nil then
                    finish('no scenario')
                    return
                end
                -- the world is up when the player exists and nothing holds the world (a load)
                if player() == nil or core.isWorldPaused() or t - started < DELAY then return end
                co = coroutine.create(function() run(r) end)
                log('begin', string.format('real=%.1f', t - started))
                resume(nil)
                return
            end
            if waiting == nil then return end
            if waiting.at and t < waiting.at then return end
            if waiting.predicate then
                local ok, result = pcall(waiting.predicate)
                if ok and result then
                    waiting = nil
                    resume(true)
                elseif t >= waiting.deadline then
                    waiting = nil
                    resume(false)
                end
                return
            end
            if waiting.answer then
                local reply = answers[waiting.answer]
                if reply ~= nil then
                    answers[waiting.answer] = nil
                    waiting = nil
                    resume(reply)
                elseif t >= waiting.deadline then
                    waiting = nil
                    resume(nil)
                end
                return
            end
            waiting = nil
            resume(nil)
        end,
    },
    eventHandlers = {
        RoundAnswer = function(data) answers[data.id] = data.result or {} end,
    },
}
