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
-- the lengths of the last frames, in milliseconds, with the real time at which each one ended
local frames = {}
local MAX_FRAMES = 600

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

local handlers = {}

function handlers.shot(args)
    shots = shots + 1
    log('shot', tostring(args.name or '-'), 'n=' .. shots)
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
    camera.setMode(mode)
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
            local ms = core.getRealFrameDuration() * 1000
            if ms > 0 then
                table.insert(frames, { ms = ms, at = core.getRealTime() })
                if #frames > MAX_FRAMES then table.remove(frames, 1) end
            end
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
