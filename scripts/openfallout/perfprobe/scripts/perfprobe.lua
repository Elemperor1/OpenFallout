-- The probe of the frame times (M9): a player script that lets the player stand, turn round and run for a fixed number of real
-- seconds and logs, in lines that start with PERF, how long the frames took in each phase (the number of frames, the mean, the
-- median, the 95th and 99th percentile, the longest, how many took more than 33 ms and more than 100 ms), the same for every
-- ten seconds of a phase, each frame of more than 100 ms (the first thirty), and where the player was. It takes a screenshot at
-- the end of the settling and at each quarter of the turn (the engine writes it to the screenshots folder of the user data) so
-- that what is drawn can be looked at, and quits when the last phase is over. Counts, numbers and form ids only, no text of the
-- game. It is a developer tool, not part of the game: it is loaded with
--     openfallout --skip-menu --start <cell> --content <the game> --data scripts/openfallout/perfprobe --content perfprobe.omwscripts
-- The player runs straight ahead and turns a quarter to the right when it has made less than a quarter of the way a second of
-- running should give, so it keeps loading new cells in the open and does not stand against a wall.
local core = require('openfallout.core')
local self = require('openfallout.self')
local debug = require('openfallout.debug')
local I = require('openfallout.interfaces')

-- The phases, in order. `secs` is real time. `shot` takes a screenshot when the phase ends, `turn` makes a full turn to the
-- right with a screenshot at each quarter, `run` makes the player run.
local PHASES = {
    { name = 'settle', secs = 25, shot = true },
    { name = 'stand', secs = 20 },
    { name = 'turn', secs = 40, turn = true },
    { name = 'run', secs = 90, run = true },
    { name = 'end', secs = 6, shot = true },
}
-- A data folder that is loaded before this one can replace the phases and the numbers below with a module
-- `scripts.perfprobeconfig` (a table with any of `phases`, `window`, `stall`), as the smoke test does to keep the run short
local ok, config = pcall(require, 'scripts.perfprobeconfig')
if ok and type(config) == 'table' and config.phases then PHASES = config.phases end

-- Real seconds in a window of the log, and the length of a frame (ms) that is logged as a hitch
local WINDOW = ok and type(config) == 'table' and config.window or 10
local HITCH = 100
local MAX_HITCHES = 30
-- A second of running that covers less than this many units counts as standing against something
local STALL_UNITS = ok and type(config) == 'table' and config.stall or 80

local started, phase, phaseStart, windowStart = nil, 0, 0, 0
local frames, window = {}, {}
local hitches, hitchLog = 0, 0
local shots = 0
local lastPos, lastCheck = nil, 0
local overridden = false

local function log(...) print('PERF', ...) end

local function where()
    local cell = self.cell
    local pos = self.position
    local grid = cell.isExterior and string.format('%d,%d', cell.gridX or 0, cell.gridY or 0) or '-'
    return string.format('cell=%s world=%s grid=%s pos=%.0f,%.0f,%.0f', tostring(cell.id), tostring(cell.worldSpaceId), grid,
        pos.x, pos.y, pos.z)
end

local function percentile(sorted, fraction)
    if #sorted == 0 then return 0 end
    return sorted[math.max(1, math.min(#sorted, math.ceil(#sorted * fraction)))]
end

local function summary(list)
    local sorted = {}
    local sum, over33, over100 = 0, 0, 0
    for i, ms in ipairs(list) do
        sorted[i] = ms
        sum = sum + ms
        if ms > 33.4 then over33 = over33 + 1 end
        if ms > HITCH then over100 = over100 + 1 end
    end
    table.sort(sorted)
    local n = #sorted
    return string.format('frames=%d mean=%.1f p50=%.1f p95=%.1f p99=%.1f max=%.1f over33=%d over100=%d', n,
        n > 0 and sum / n or 0, percentile(sorted, 0.5), percentile(sorted, 0.95), percentile(sorted, 0.99),
        sorted[n] or 0, over33, over100), sum
end

local function endWindow(now)
    if #window > 0 then
        local text, sum = summary(window)
        log('window', PHASES[phase].name, string.format('t=%.0f', now - phaseStart), text,
            string.format('fps=%.1f', #window / (sum / 1000)), where())
    end
    window = {}
    windowStart = now
end

local function endPhase(now)
    endWindow(now)
    local text, sum = summary(frames)
    log('phase', PHASES[phase].name, string.format('secs=%.1f', now - phaseStart), text,
        string.format('fps=%.1f', #frames / math.max(sum / 1000, 0.001)), where())
    frames = {}
end

local function startPhase(now)
    phase = phase + 1
    phaseStart, windowStart = now, now
    lastPos, lastCheck = self.position, now
    if phase > #PHASES then
        log('done', string.format('hitches=%d', hitches), where())
        core.quit()
        return false
    end
    log('start', PHASES[phase].name, where())
    return true
end

local function shot(name)
    shots = shots + 1
    log('shot', shots, name)
    debug.takeScreenshot()
end

local function onUpdate()
    local now = core.getRealTime()
    if not started then
        started = now
        log('begin', string.format('phases=%d', #PHASES))
        if not startPhase(now) then return end
    end
    if phase > #PHASES then return end
    local spec = PHASES[phase]
    local dt = core.getRealFrameDuration()
    local ms = dt * 1000
    if ms > 0 then
        table.insert(frames, ms)
        table.insert(window, ms)
        if ms > HITCH then
            hitches = hitches + 1
            if hitchLog < MAX_HITCHES then
                hitchLog = hitchLog + 1
                log('hitch', spec.name, string.format('ms=%.0f t=%.1f', ms, now - phaseStart), where())
            end
        end
    end

    local controls = self.controls
    local inPhase = now - phaseStart
    if spec.turn then
        -- a full turn to the right over the phase, a screenshot at the start of each quarter
        controls.yawChange = dt * 2 * math.pi / spec.secs
        local quarter = math.floor(inPhase / (spec.secs / 4))
        if quarter ~= spec.lastQuarter and quarter < 4 then
            spec.lastQuarter = quarter
            shot('turn' .. quarter)
        end
    end
    -- the default controls script writes the movement of the player from the keyboard every frame unless it is told not to
    local wantOverride = spec.run or spec.turn
    if wantOverride ~= overridden then
        overridden = wantOverride
        I.Controls.overrideMovementControls(overridden and true or false)
    end
    if spec.run then
        controls.movement = 1
        controls.run = true
        if now - lastCheck >= 1 then
            local pos = self.position
            if lastPos and (pos - lastPos):length() < STALL_UNITS then
                controls.yawChange = math.pi / 2
                log('turn', 'stalled', where())
            end
            lastPos, lastCheck = pos, now
        end
    end

    if now - windowStart >= WINDOW then endWindow(now) end
    if inPhase >= spec.secs then
        controls.movement = 0
        controls.run = false
        if spec.shot then shot(spec.name) end
        endPhase(now)
        startPhase(now)
    end
end

return {
    engineHandlers = {
        onUpdate = onUpdate,
    },
}
