-- The player side of the scenario player (see roundworld.lua): the camera, the screenshots, the rays and the frame times belong
-- to the player script. It answers the questions of roundworld.lua (the event RoundAsk) with the event RoundAnswer and logs
-- each as a line that starts with ROUND: numbers, record ids and names given by the scenario, never a text of the game.
local core = require('openfallout.core')
local self = require('openfallout.self')
local camera = require('openfallout.camera')
local debug = require('openfallout.debug')
local nearby = require('openfallout.nearby')
local input = require('openfallout.input')
local util = require('openfallout.util')
local animation = require('openfallout.animation')
local types = require('openfallout.types')
local I = require('openfallout.interfaces')

local function log(...) print('ROUND', ...) end

local shots = 0
-- A message box of the game (a hint, a place, a tutorial) is modal and draws over the picture, and nobody clicks it in a round:
-- the player script answers it with its first button after BOX_DELAY real seconds (as autoplay does) and logs its number, its form
-- and its buttons, never its text. A scenario can leave the boxes alone (the question `boxes`).
local BOX_DELAY = 1.0
local answerBoxes = true
local boxSerial, boxSeenAt, boxAnswered = nil, 0, nil
-- the real time at which a box was last open or answered (the question `quiet` says how long ago)
local lastBoxAt = nil
-- the lengths of the last frames, in milliseconds, with the real time at which each one ended
local frames = {}
local MAX_FRAMES = 600
-- the real time of the last update: a frame is the difference between two of them (core.getRealFrameDuration is clamped to 200 ms
-- and scaled by the simulation speed, so a slow frame or r.speed() would give false numbers)
local lastFrameAt = nil

local function cellText()
    local cell = self.cell
    if cell == nil then return 'nocell' end
    if cell.isExterior then
        return string.format('%s:%d,%d', tostring(cell.worldSpaceId), cell.gridX or 0, cell.gridY or 0)
    end
    return tostring(cell.id)
end

local function eyePosition()
    -- the eye: the camera position in the first person view, else the head of the player
    local ok, pos = pcall(camera.getPosition)
    if ok and pos ~= nil then return pos end
    return self.position + util.vector3(0, 0, 128)
end

local function openBox()
    if core.messageBox == nil then return nil end
    local ok, box = pcall(core.messageBox.current)
    return ok and box or nil
end

local function answerBox()
    local box = openBox()
    if box == nil then
        boxSerial = nil
        return
    end
    -- an answered box stays in the list until the frame after, and the next one has another serial
    if box.serial == boxAnswered then return end
    local now = core.getRealTime()
    lastBoxAt = now
    if box.serial ~= boxSerial then
        boxSerial, boxSeenAt = box.serial, now
        log('box', 'serial=' .. tostring(box.serial), 'buttons=' .. #box.buttons, 'form=' .. tostring(box.form),
            answerBoxes and 'open' or 'left open')
    end
    if answerBoxes and now - boxSeenAt >= BOX_DELAY then
        local ok, pressed = pcall(core.messageBox.press, 1)
        log('box', 'serial=' .. tostring(box.serial), 'press 1', tostring(ok and pressed))
        boxAnswered, boxSerial, lastBoxAt = box.serial, nil, now
    end
end

-- The camera mode that the scenario asked for: the game's camera goes to its vanity mode (it circles the player) when nothing has
-- touched the controls for a while, and a round touches nothing, so the mode is put back (and logged, three times at most)
local wantedMode, modeForced = nil, 0

local handlers = {}

-- Whether a box is open and for how many seconds none has been (the add-ons of a save open their notices one after the other
-- when a place has loaded); boxes that are left open (`boxes` with answer = false) do not count
function handlers.quiet()
    if not answerBoxes then return { open = false, quiet = 1e9 } end
    return { open = openBox() ~= nil, quiet = lastBoxAt and (core.getRealTime() - lastBoxAt) or 1e9 }
end

function handlers.boxes(args)
    answerBoxes = args.answer ~= false
    log('boxes', answerBoxes and 'answered' or 'left open')
    return {}
end

function handlers.shot(args)
    shots = shots + 1
    -- a box that is open when the picture is taken is in it: the line says so
    local box = openBox()
    if box then
        log('shot', tostring(args.name or '-'), 'n=' .. shots, 'box=' .. tostring(box.serial))
    else
        log('shot', tostring(args.name or '-'), 'n=' .. shots)
    end
    debug.takeScreenshot()
    return { n = shots }
end

function handlers.look(args)
    if args.yaw ~= nil then camera.setYaw(args.yaw) end
    if args.pitch ~= nil then camera.setPitch(args.pitch) end
    log('look', string.format('yaw=%.2f pitch=%.2f', camera.getYaw(), camera.getPitch()))
    return {}
end

function handlers.camera(args)
    local mode = camera.MODE[args.mode]
    if mode == nil then
        log('camera', 'unknown mode', tostring(args.mode))
        return {}
    end
    camera.setMode(mode, true)
    wantedMode = mode
    log('camera', 'mode', tostring(args.mode))
    return {}
end

function handlers.eye(args)
    local pos = self.position
    local eye = eyePosition()
    log('eye', tostring(args.label or '-'), 'cell=' .. cellText(),
        string.format('pos=%.0f,%.0f,%.0f eye=%.0f,%.0f,%.0f yaw=%.2f pitch=%.2f mode=%s', pos.x, pos.y, pos.z, eye.x, eye.y, eye.z,
            camera.getYaw(), camera.getPitch(), tostring(camera.getMode())))
    return { x = pos.x, y = pos.y, z = pos.z }
end

function handlers.ground(args)
    local eye = eyePosition()
    local points = { { 0, 0 } }
    for _, offset in ipairs(args.offsets or {}) do table.insert(points, offset) end
    local heights = {}
    for _, offset in ipairs(points) do
        local from = util.vector3(eye.x + offset[1], eye.y + offset[2], eye.z + 4000)
        local to = util.vector3(eye.x + offset[1], eye.y + offset[2], eye.z - 40000)
        local result = nearby.castRay(from, to, { ignore = self, collisionType = nearby.COLLISION_TYPE.HeightMap })
        local height = result.hit and result.hitPos.z or nil
        table.insert(heights, height)
        log('ground', tostring(args.label or '-'), string.format('dx=%.0f dy=%.0f', offset[1], offset[2]),
            height and string.format('z=%.0f', height) or 'no hit')
    end
    return { heights = heights }
end

function handlers.actors(args)
    local radius = args.radius or 2000
    local list = {}
    for _, actor in ipairs(nearby.actors) do
        if actor ~= self.object then
            local distance = (actor.position - self.position):length()
            if distance <= radius then table.insert(list, { actor = actor, distance = distance }) end
        end
    end
    table.sort(list, function(a, b) return a.distance < b.distance end)
    for i = 1, math.min(#list, args.count or 10) do
        local actor, p = list[i].actor, list[i].actor.position
        log('actor', tostring(args.label or '-'), string.format('record=%s distance=%.0f pos=%.0f,%.0f,%.0f', tostring(actor.recordId),
            list[i].distance, p.x, p.y, p.z))
    end
    log('actors', tostring(args.label or '-'), 'count=' .. #list)
    return { count = #list }
end

function handlers.fps(args)
    local seconds = args.seconds or 2
    local now = core.getRealTime()
    local sum, n, worst = 0, 0, 0
    for _, frame in ipairs(frames) do
        if now - frame.at <= seconds then
            sum, n, worst = sum + frame.ms, n + 1, math.max(worst, frame.ms)
        end
    end
    local mean = n > 0 and sum / n or 0
    log('fps', tostring(args.label or '-'), string.format('frames=%d mean=%.1fms max=%.1fms fps=%.1f', n, mean, worst,
        mean > 0 and 1000 / mean or 0))
    return { frames = n, mean = mean }
end


-- ---- M14 additions: the fight, the weapon, the animation groups ------------------------------------------------------------
-- Each handler logs lines that start with ROUND and holds numbers, record ids and the names of animation groups, never a text.

local Actor = types.Actor

local function formOf(object)
    -- the form id as core.combat wants it ("" when the object has none)
    if object == nil then return '' end
    local id = tostring(object.id)
    -- a placed reference's id is "0x..." and the combat calls want it as core.getFormId writes it
    if id:sub(1, 2) == '0x' then id = 'FormId:' .. id end
    -- the player's own reference has no form id of the content; the games call it 0x14
    if object == self.object then id = 'FormId:0x14' end
    return id
end

local function number(value, digits)
    if value == nil then return 'nil' end
    return string.format('%.' .. (digits or 1) .. 'f', value)
end

-- The health of the player is the Pip-Boy's (Actor.stats does not follow the engine's health of Fallout); a character's is the one the
-- crosshair's bar shows
local function health(actor)
    local ok, value = pcall(function()
        if actor == self.object then return core.pipboy.stats().health end
        return core.hud.target(actor).health
    end)
    return ok and value or nil
end

-- The player's godmode: on, so that a round is not cut short (the state is logged)
function handlers.god(args)
    local want = args.on ~= false
    if debug.isGodMode() ~= want then debug.toggleGodMode() end
    log('god', tostring(debug.isGodMode()))
    return { on = debug.isGodMode() }
end

-- A command of the script language of Fallout, called by its name (the tool debug.scriptCommand): on the next frame, in the main
-- thread; the value it gives is written to the engine log ("Command: Name on 0x14 (1, 2) = 3"), not given back here
function handlers.command(args)
    debug.scriptCommand(args.name, args.reference, args.arguments)
    log('command', tostring(args.name), tostring(args.reference or '-'))
    return { name = args.name }
end

-- The quests the Pip-Boy lists, with the one that is followed: numbers and ids only
function handlers.pipboyQuests(args)
    local rows = core.pipboy.quests()
    local active, count = nil, 0
    for index, row in ipairs(rows) do
        count = count + 1
        if row.active then active = row.id end
    end
    log('pipboyQuests', tostring(args.label or '-'), 'count=' .. count, 'active=' .. tostring(active))
    local ids = {}
    for index, row in ipairs(rows) do ids[index] = { id = row.id, active = row.active, completed = row.completed } end
    return { count = count, active = active, rows = ids }
end

-- Uses an item the player carries as the Pip-Boy does (an aid item is eaten or drunk, a book read): the item with that form; what
-- happened is logged ("used", "read", "equipped", or nothing)
function handlers.pipboyUse(args)
    for _, row in ipairs(core.pipboy.items()) do
        if row.form == args.form then
            local result = core.pipboy.use(row.id)
            log('pipboyUse', tostring(args.label or '-'), 'form=' .. tostring(args.form), 'result=' .. tostring(result))
            return { result = result }
        end
    end
    log('error', 'pipboyUse', 'the player does not carry ' .. tostring(args.form))
    return { result = '' }
end

-- The weapon of the player: the form, what the games' rules allow, the rounds
function handlers.weapon(args)
    local stats = core.combat.weapon()
    log('weapon', tostring(args.label or '-'), 'form=' .. tostring(stats.weapon), 'melee=' .. tostring(stats.melee),
        'auto=' .. tostring(stats.automatic), 'range=' .. number(stats.range, 0), 'interval=' .. number(stats.interval, 2),
        'ammoUse=' .. tostring(stats.ammoUse), 'rounds=' .. tostring(stats.rounds), 'stance=' .. tostring(Actor.getStance(self)))
    return { weapon = stats.weapon, rounds = stats.rounds, melee = stats.melee }
end

-- What the player carries as the Pip-Boy lists it (the engine's inventory of the games is not the one of Actor.inventory):
-- one line for each weapon and kind of ammunition, with forms and numbers only
local function pipboyWeapons(holder)
    local ok, rows = pcall(core.pipboy.items, holder)
    if not ok then
        log('error', 'inventory', tostring(rows))
        return {}
    end
    return rows
end

function handlers.inventory(args)
    local rows = pipboyWeapons(args.holder)
    local weapons, ammo, other = 0, 0, 0
    for _, item in ipairs(rows) do
        if item.category == 'weapons' then
            weapons = weapons + 1
            log('item', tostring(args.label or '-'), 'weapon', 'form=' .. tostring(item.form), 'count=' .. tostring(item.count),
                'equipped=' .. tostring(item.equipped), 'hasAmmo=' .. tostring(item.hasAmmo), 'clip=' .. tostring(item.clip),
                'loaded=' .. tostring(item.loaded), 'reserve=' .. tostring(item.reserve), 'damage=' .. number(item.damage))
        elseif item.category == 'ammo' then
            ammo = ammo + 1
            log('item', tostring(args.label or '-'), 'ammo', 'form=' .. tostring(item.form), 'count=' .. tostring(item.count),
                'usable=' .. tostring(item.usable))
        else
            other = other + 1
        end
    end
    log('inventory', tostring(args.label or '-'), 'weapons=' .. weapons, 'ammoKinds=' .. ammo, 'others=' .. other)
    return { weapons = weapons }
end

-- Puts a weapon of the Pip-Boy's list in the hand: the one with this form, or (no form given) the first ranged one that has
-- rounds; `pick` is the 1st, 2nd... of those. Uses the Pip-Boy's own `use`, as the player's click on the row does.
function handlers.equip(args)
    local chosen, n = nil, 0
    for _, item in ipairs(pipboyWeapons()) do
        if item.category == 'weapons' then
            local match
            if args.form then
                match = tostring(item.form) == tostring(args.form)
            else
                match = item.hasAmmo and (item.clip or 0) > 0 and ((item.loaded or 0) + (item.reserve or 0)) > 0
            end
            if match then
                n = n + 1
                if n == (args.pick or 1) then chosen = item end
            end
        end
    end
    if chosen == nil then
        log('error', 'equip', 'no such weapon', tostring(args.form or 'ranged #' .. tostring(args.pick or 1)))
        return { ok = false }
    end
    local result = chosen.equipped and 'already' or core.pipboy.use(chosen.id)
    local stats = core.combat.weapon()
    log('equip', 'form=' .. tostring(chosen.form), 'result=' .. tostring(result), 'inHand=' .. tostring(stats.weapon),
        'rounds=' .. tostring(stats.rounds), 'range=' .. number(stats.range, 0))
    -- the use of an item says what happened, and nothing (an empty answer) when nothing did: the weapon is not in the hand then
    if result == nil or result == '' then
        log('error', 'equip', 'nothing happened when the weapon was used', tostring(chosen.form))
        return { ok = false }
    end
    return { ok = true, form = tostring(chosen.form) }
end

-- The weapon out or away: the stance of the actor and the engine's own flag (what falloutcombat.lua does on the key)
function handlers.drawn(args)
    local on = args.on ~= false
    Actor.setStance(self, on and Actor.STANCE.Weapon or Actor.STANCE.Nothing)
    core.combat.setDrawn(on)
    log('drawn', tostring(on), 'stance=' .. tostring(Actor.getStance(self)))
    return {}
end

-- What the camera's ray hits (as falloutcombat.lua aims): the object, or nil
local function aimTarget(stats)
    local from = camera.getPosition()
    local origin = self.position
    local to = from + camera.viewportToWorldVector(util.vector2(0.5, 0.5)) * (stats.range + (from - origin):length())
    local result = nearby.castRay(from, to, { ignore = self, collisionType = nearby.COLLISION_TYPE.Default })
    return result.hitObject, result.hitPos
end

-- The player shoots once with core.combat.fire at `target` (an object), or at what the camera's ray hits when none is given
function handlers.fire(args)
    local stats = core.combat.weapon()
    local target, hitPos = args.target, nil
    local how = 'given'
    if target == nil then target, hitPos = aimTarget(stats) how = 'ray' end
    local ok, row = pcall(core.combat.fire, target)
    if not ok then
        log('error', 'fire', tostring(row))
        return { fired = false }
    end
    log('fire', tostring(args.label or '-'), 'aim=' .. how, 'target=' .. (target and tostring(target.recordId) or 'none'),
        'fired=' .. tostring(row.fired), 'ammoOut=' .. tostring(row.outOfAmmo), 'hit=' .. tostring(row.hit),
        'damage=' .. number(row.damage), 'crit=' .. tostring(row.critical), 'killed=' .. tostring(row.killed),
        'downed=' .. tostring(row.downed), 'rounds=' .. tostring(core.combat.weapon().rounds))
    return { fired = row.fired, hit = row.hit, killed = row.killed, damage = row.damage }
end

-- Turns the camera and the player to look at an object (its middle), the pitch positive down
function handlers.aimAt(args)
    local target = args.target
    local eye = eyePosition()
    local to = target.position + util.vector3(0, 0, args.up or 80)
    local d = to - eye
    local yaw = math.atan2(d.x, d.y)
    local pitch = math.atan2(-d.z, math.sqrt(d.x * d.x + d.y * d.y))
    camera.setYaw(yaw)
    camera.setPitch(pitch)
    log('aimAt', tostring(args.label or '-'), string.format('yaw=%.2f pitch=%.2f distance=%.0f', yaw, pitch, d:length()))
    return {}
end

-- Walks, runs or sneaks for some real seconds: the controls are set each frame (the movement controls of the game are left
-- alone while it lasts); `sneak` alone with no movement crouches in place
local moving = nil
function handlers.move(args)
    I.Controls.overrideMovementControls(true)
    moving = { forward = args.forward or 0, side = args.side or 0, run = args.run == true, sneak = args.sneak == true,
        untilAt = core.getRealTime() + (args.seconds or 2) }
    log('move', string.format('forward=%.0f side=%.0f run=%s sneak=%s seconds=%.1f', moving.forward, moving.side,
        tostring(moving.run), tostring(moving.sneak), args.seconds or 2))
    return {}
end

-- Keeps crouching (or stands again) without moving
local crouch = nil
function handlers.crouch(args)
    I.Controls.overrideMovementControls(true)
    crouch = args.on ~= false
    self.controls.sneak = crouch
    log('crouch', tostring(crouch))
    return {}
end

-- The attack control of the actor (the engine's own attack of a weapon in hand, apart from core.combat.fire)
function handlers.use(args)
    self.controls.use = args.on == false and self.ATTACK_TYPE.NoAttack or self.ATTACK_TYPE.Any
    log('use', tostring(args.on ~= false))
    return {}
end

-- What the characters around make of the player: the sneak meter's word, and the health of the player
function handlers.status(args)
    local ok, state = pcall(core.hud.status)
    if not ok then
        log('error', 'status', tostring(state))
        return {}
    end
    log('status', tostring(args.label or '-'), 'sneaking=' .. tostring(state.sneaking), 'sneak=' .. tostring(state.sneak),
        'weaponDrawn=' .. tostring(state.weaponDrawn), 'hp=' .. number(health(self.object)))
    return { sneak = state.sneak, sneaking = state.sneaking, hp = health(self.object) }
end

-- The camera and the stance of the player in numbers
function handlers.mode(args)
    log('mode', tostring(args.label or '-'), 'camera=' .. tostring(camera.getMode()), 'stance=' .. tostring(Actor.getStance(self)),
        'sneaking=' .. tostring(self.controls.sneak), 'onGround=' .. tostring(Actor.isOnGround(self)),
        'speed=' .. number(Actor.getCurrentSpeed(self)), 'ui=' .. tostring(I.UI.getMode()))
    return {}
end

-- Sampling of animation state: every `every` real seconds (0.25) the groups that play on each tracked actor, one line each.
-- tracked: list of { label, actor }. The bone groups: lower body, torso, left arm, right arm (openfallout.animation.BONE_GROUP).
local tracked, trackEvery, lastTrack = {}, 0.25, 0
local trackStarted = 0
local function groups(actor)
    local out = {}
    for _, pair in ipairs({ { 'L', animation.BONE_GROUP.LowerBody }, { 'T', animation.BONE_GROUP.Torso },
        { 'LA', animation.BONE_GROUP.LeftArm }, { 'RA', animation.BONE_GROUP.RightArm } }) do
        local ok, group = pcall(animation.getActiveGroup, actor, pair[2])
        table.insert(out, pair[1] .. '=' .. (ok and (group == '' and '-' or tostring(group)) or 'error'))
    end
    return table.concat(out, ' ')
end

local sampleErrorLogged = {}
-- the animation groups that the skeleton's folder has (file names without the folder and the ending), asked of an actor each sample
-- to see which play: the folder of the skeleton and its locomotion folders, and the first person folder
local candidates = nil
local function candidateGroups()
    if candidates ~= nil then return candidates end
    candidates = {}
    local vfs = require('openfallout.vfs')
    local seen = {}
    for _, prefix in ipairs({ 'meshes/characters/_male/', 'meshes/characters/_1stperson/' }) do
        local ok, iterator = pcall(vfs.pathsWithPrefix, prefix)
        if ok then
            for path in iterator do
                path = tostring(path):lower()
                if path:match('%.kf$') and not path:find('/idleanims/', 1, true) then
                    local stem = path:match('([^/]+)%.kf$')
                    if stem and not seen[stem] then seen[stem] = true table.insert(candidates, stem) end
                end
            end
        end
    end
    table.sort(candidates)
    log('candidates', #candidates .. ' group names from the folders of the skeleton and the first person')
    return candidates
end

local function playingGroups(actor)
    local out = {}
    for _, name in ipairs(candidateGroups()) do
        local ok, playing = pcall(animation.isPlaying, actor, name)
        if ok and playing then table.insert(out, name) end
    end
    return #out > 0 and table.concat(out, '+') or '-'
end

local function sample(entry)
    local actor = entry.actor
    if not actor:isValid() then
        log('sample', entry.label, 'invalid')
        return
    end
    local p = actor.position
    local distance = (p - self.position):length()
    local parts = { 'sample', entry.label, string.format('t=%.2f', core.getRealTime() - trackStarted), groups(actor) }
    table.insert(parts, 'playing=' .. playingGroups(actor))
    table.insert(parts, string.format('pos=%.0f,%.0f,%.0f d=%.0f', p.x, p.y, p.z, distance))
    local function try(text, fn)
        local ok, value = pcall(fn)
        table.insert(parts, text .. '=' .. (ok and tostring(value) or 'n/a'))
    end
    try('yaw', function() return string.format('%.2f', actor.rotation:getYaw()) end)
    try('stance', function() return Actor.getStance(actor) end)
    try('hp', function()
        if actor == self.object then return number(health(actor)) end
        local t = core.hud.target(actor)
        return string.format('%.0f/%.0f', t.health, t.maxHealth)
    end)
    local id = formOf(actor)
    if actor ~= self.object then
        try('dead', function() return core.combat.isDead(id) end)
        try('inCombat', function() return core.combat.inCombat(id) end)
        try('target', function()
            local t = core.combat.target(id)
            return t == formOf(self.object) and 'player' or (t == '' and '-' or 'other')
        end)
    end
    log(table.unpack(parts))
end

function handlers.track(args)
    tracked = args.list or {}
    trackEvery = args.every or 0.25
    lastTrack = 0
    trackStarted = core.getRealTime()
    log('track', #tracked .. ' actors', 'every=' .. trackEvery)
    return {}
end

function handlers.untrack()
    tracked = {}
    log('untrack')
    return {}
end

-- A blank of the whole player-side state of the fight, for one actor (state of its fight and its animation, once)
function handlers.actorState(args)
    sample({ label = args.label or 'actor', actor = args.actor })
    return {}
end

-- What the screen of V.A.T.S. would see: the state of the game's own V.A.T.S., and for one actor whether it can be a target, the angle
-- to it from the camera and what the ray to its head hits
function handlers.vatsInfo(args)
    local ok, state = pcall(core.vats.state)
    if ok then
        log('vatsInfo', tostring(args.label or '-'), 'usable=' .. tostring(state.usable), 'open=' .. tostring(state.open),
            'ap=' .. number(state.actionPoints) .. '/' .. number(state.maxActionPoints), 'cost=' .. number(state.cost),
            'range=' .. tostring(state.range), 'ui=' .. tostring(I.UI.getMode()))
    else
        log('error', 'vatsInfo', tostring(state))
    end
    local actor = args.actor
    local answer = {}
    if actor then
        local good, can = pcall(core.vats.canTarget, actor)
        local from = camera.getPosition()
        local head = actor.position + util.vector3(0, 0, 130)
        local direction = (head - from):normalize()
        local cosine = direction:dot(camera.viewportToWorldVector(util.vector2(0.5, 0.5)))
        local ray = nearby.castRay(from, head, { ignore = self, collisionType = nearby.COLLISION_TYPE.Default })
        log('vatsInfo', 'target', 'canTarget=' .. tostring(good and can or ('error ' .. tostring(can))), 'cosine=' .. string.format('%.3f', cosine),
            'distance=' .. number((head - from):length(), 0), 'ray=' .. (ray.hitObject and tostring(ray.hitObject.recordId) or 'clear'))
        -- whether the ray to the head hits nothing, or the character itself: a scenario that wants a target that V.A.T.S. can offer
        -- tries another place when this is false (round 14D: the first place was inside a building)
        answer.clear = ray.hitObject == nil or ray.hitObject == actor
        answer.canTarget = good and can == true
    end
    return answer
end

-- The HUD's messages since the last call (the number, and the port's own words only when they are the one this round knows)
function handlers.messages(args)
    local ok, list = pcall(core.hud.takeMessages)
    local n = ok and #list or -1
    local known = false
    if ok then for _, m in ipairs(list) do if m.text == 'No target in sight' then known = true end end end
    log('messages', tostring(args.label or '-'), 'count=' .. n, 'noTargetInSight=' .. tostring(known))
    return {}
end

-- ---- M16 needs: waiting, sleeping, travelling and the needs of the hardcore mode ---------------------------------------------------
-- The game's clock in hours since the start of its count
function handlers.clock()
    return { hours = core.getGameTime() / 3600 }
end

-- The player asks to wait (the Wait key) or has just activated a bed (`activated`); when the screen of waiting opens the player
-- asks for `hours` of it. Logs the numbers only: whether a screen opened, whether the rest began, the length of the refusal
-- (its reason is in the engine's log as "Rest refused: ...") and how many messages the HUD was given
function handlers.rest(args)
    local interact = core.interact
    if not args.activated then interact.requestWait() end
    local screen = interact.rest()
    local okMessages, list = pcall(core.hud.takeMessages)
    local messages = okMessages and #list or -1
    local started, chars = false, 0
    if screen ~= nil then
        local text = interact.startRest(args.hours or 1)
        started = text == ''
        chars = #text
        if not started then interact.cancelRest() end
    end
    log('rest', tostring(args.label or '-'), 'asked=' .. (args.activated and 'bed' or 'wait'), 'screen=' .. tostring(screen ~= nil),
        'sleeping=' .. tostring(screen ~= nil and screen.sleeping), 'started=' .. tostring(started), 'refusalChars=' .. chars,
        'messages=' .. messages, string.format('gameHours=%.2f', core.getGameTime() / 3600))
    return { screen = screen ~= nil, started = started, refusalChars = chars, messages = messages }
end

function handlers.restProgress()
    local p = core.interact.restProgress()
    return { active = p.active, sleeping = p.sleeping, done = p.done, total = p.total, ended = p.ended, stoppedChars = #(p.stopped or '') }
end

-- The hardcore mode: asked about (no argument) or turned on or off, and the levels of the needs and the tiles the display draws
function handlers.hardcore(args)
    local interact = core.interact
    if args.on ~= nil then interact.setHardcore(args.on == true) end
    log('hardcore', tostring(args.label or '-'), 'on=' .. tostring(interact.hardcore()))
    return { on = interact.hardcore() }
end

function handlers.needs(args)
    local ok, state = pcall(core.hud.status)
    if not ok or state.needs == nil then
        log('error', 'needs', tostring(state))
        return {}
    end
    local n = state.needs
    local function tile(name)
        local t = n.tiles[name]
        return string.format('%s=%s/%s', name, tostring(t.color), tostring(t.alpha))
    end
    log('needs', tostring(args.label or '-'), 'hardcore=' .. tostring(n.hardcore), string.format('dehydration=%.0f', n.dehydration),
        string.format('hunger=%.0f', n.hunger), string.format('sleep=%.0f', n.sleep), 'crippledParts=' .. tostring(n.crippledParts),
        string.format('most=%.0f/%.0f/%.0f', n.dehydrationMost, n.hungerMost, n.sleepMost), 'tiles',
        tile('dehydration'), tile('hunger'), tile('sleep'), tile('rads'), tile('limbs'), 'hp=' .. number(health(self.object)))
    return { dehydration = n.dehydration, hunger = n.hunger, sleep = n.sleep, hardcore = n.hardcore }
end

-- Fast travel to the nearest marker the player may travel to (the map's rows say which)
function handlers.travel(args)
    local interact = core.interact
    local ok, rows = pcall(interact.map)
    if not ok then
        log('error', 'travel', tostring(rows))
        return {}
    end
    local usable, best = 0, nil
    for _, row in ipairs(rows) do
        if row.travel then
            usable = usable + 1
            if row.distance >= 0 and (best == nil or best.distance < 0 or row.distance < best.distance) then best = row
            elseif best == nil then best = row end
        end
    end
    log('travel', tostring(args.label or '-'), 'markers=' .. #rows, 'usable=' .. usable, best and ('nearest=' .. best.id) or 'nearest=none',
        best and string.format('distance=%.0f', best.distance) or '-')
    if best == nil then return { found = false } end
    local text = interact.travel(best.id)
    log('travel', tostring(args.label or '-'), 'went=' .. tostring(text == ''), 'refusalChars=' .. #text, string.format('gameHours=%.2f', core.getGameTime() / 3600))
    return { found = true, went = text == '', refusalChars = #text }
end

-- The Pip-Boy opened at a page, as a key of the page would (the event is the one the Pip-Boy script answers)
function handlers.pipboyOpen(args)
    self:sendEvent('PipboyOpen', { page = args.page, stats = args.stats, genrep = args.genrep, row = args.row })
    log('pipboyOpen', tostring(args.label or '-'), 'page=' .. tostring(args.page), 'stats=' .. tostring(args.stats),
        'genrep=' .. tostring(args.genrep), 'row=' .. tostring(args.row))
    return {}
end

function handlers.pipboyClose(args)
    self:sendEvent('PipboyClose', {})
    log('pipboyClose', tostring(args.label or '-'))
    return {}
end

-- What the general page shows: the title and the picture of the karma and the numbers of the counts and reputations
function handlers.pipboyGeneral(args)
    local general = core.pipboy.general()
    local rows = {}
    for _, count in ipairs(general.counts) do rows[#rows + 1] = tostring(count.value) end
    log('pipboyGeneral', tostring(args.label or '-'), 'karmaPictureChars=' .. #general.karma.picture,
        'karmaWordChars=' .. #general.karma.word, 'karmaTitleChars=' .. #general.karma.title,
        'counts=' .. #general.counts, 'values=' .. table.concat(rows, ','), 'reputations=' .. #general.reputations)
    for _, reputation in ipairs(general.reputations) do
        log('pipboyGeneral', 'reputation', tostring(reputation.id), string.format('fame=%.1f infamy=%.1f levels=%d/%d',
            reputation.fame, reputation.infamy, reputation.fameLevel, reputation.infamyLevel),
            'nameChars=' .. #reputation.name, 'titleChars=' .. #reputation.title, 'pictureChars=' .. #reputation.picture)
    end
    return { counts = #general.counts, reputations = #general.reputations }
end

-- V.A.T.S.: the same event the pad's bumper sends the screen
function handlers.vats(args)
    if args.button then
        self:sendEvent('PadButton', { button = input.CONTROLLER_BUTTON[args.button] })
        log('vats', 'pad', tostring(args.button))
    else
        self:sendEvent('OpenVats', { targets = args.targets })
        log('vats', 'open', 'ui=' .. tostring(I.UI.getMode()))
    end
    return {}
end

return {
    engineHandlers = {
        onFrame = function()
            local now = core.getRealTime()
            if wantedMode ~= nil and camera.getMode() ~= wantedMode then
                modeForced = modeForced + 1
                if modeForced <= 3 then log('camera', 'mode forced back', 'from=' .. tostring(camera.getMode()), 'to=' .. tostring(wantedMode)) end
                camera.setMode(wantedMode, true)
            end
            if moving then
                if now >= moving.untilAt then
                    self.controls.movement, self.controls.sideMovement, self.controls.run, self.controls.sneak = 0, 0, false, crouch == true
                    moving = nil
                    log('move', 'done')
                else
                    self.controls.movement, self.controls.sideMovement = moving.forward, moving.side
                    self.controls.run, self.controls.sneak = moving.run, moving.sneak or crouch == true
                end
            elseif crouch ~= nil then
                self.controls.sneak = crouch
            end
            if #tracked > 0 and now - lastTrack >= trackEvery then
                lastTrack = now
                for _, entry in ipairs(tracked) do sample(entry) end
            end
        end,
        onUpdate = function()
            answerBox()
            local now = core.getRealTime()
            if lastFrameAt ~= nil and now > lastFrameAt then
                table.insert(frames, { ms = (now - lastFrameAt) * 1000, at = now })
                if #frames > MAX_FRAMES then table.remove(frames, 1) end
            end
            lastFrameAt = now
        end,
    },
    eventHandlers = {
        RoundAsk = function(data)
            local handler = handlers[data.what]
            local ok, result
            if handler == nil then
                ok, result = false, 'unknown question ' .. tostring(data.what)
            else
                ok, result = pcall(handler, data.args or {})
            end
            if not ok then log('error', tostring(data.what), tostring(result)) end
            core.sendGlobalEvent('RoundAnswer', { id = data.id, result = ok and result or { error = tostring(result) } })
        end,
    },
}
