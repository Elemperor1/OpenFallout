-- The player side of the scenario player (see roundworld.lua): the camera, the screenshots, the rays and the frame times belong
-- to the player script. It answers the questions of roundworld.lua (the event RoundAsk) with the event RoundAnswer and logs
-- each as a line that starts with ROUND: numbers, record ids and names given by the scenario, never a text of the game.
local core = require('openfallout.core')
local self = require('openfallout.self')
local camera = require('openfallout.camera')
local debug = require('openfallout.debug')
local nearby = require('openfallout.nearby')
local util = require('openfallout.util')

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

return {
    engineHandlers = {
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
