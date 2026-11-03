-- The lockpicking screen of Fallout 3 and New Vegas, as the games' own menu (LockPickMenu, lockpick_menu.xml) has it. The
-- file lays out the box with the skill, the bobby pins and the level of the lock, and the two buttons; the lock and the
-- bobby pin are two models that the games' code draws behind the tiles (Meshes\Terminals\LockInterface01.NIF and
-- BobbyPin01.NIF), the mouse moves the pin along the lock and the keys turn the cylinder. The rules (the sweet spot, how
-- far the cylinder turns with the pin where it is, the strain on the pin and its breaking, forcing) are the pure game of
-- core.lockpick, as the executables have them (Mac rounds 12AI and 12AJ); what a lock does when it opens, the
-- experience and the pins that are used up are the engine's (core.interact.lockOutcome). It is a module of the screen
-- of the locks (falloutinteraction.lua), not a script.
--
--   lockpick.start(lock, { leave = function() ... end })  -- false: the screen is not there, show the stand-in
--   lockpick.update(seconds); lockpick.press(code, symbol); lockpick.close(); lockpick.isOpen()
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')

local menuModule = require('scripts.omw.falloutmenu')
local sceneModule = require('scripts.omw.falloutscene')

local interact = core.interact
local pipboy = core.pipboy
local v3 = util.vector3

local module = {}

local FILE = 'lockpick_menu.xml'
local SCENE = 'lockpick'
local LOCK_MODEL = 'meshes/terminals/lockinterface01.nif'
local PIN_MODEL = 'meshes/terminals/bobbypin01.nif'

-- The scene of the games' code (FalloutNV.exe 0x78E1C0 to 0x78E900, round 12AJ A2): the scene root stands 125 units in
-- front of the camera and is turned so that the x of a model is to the right in the picture, its z up and its y away from
-- the camera; the camera is at the origin looking along +X, its frustum is +-tan(a)*0.75 wide at distance 1 with a = 75
-- degrees * 0.0174533 * 0.15 (fDefaultFOV:Display) and as high as the window is (fNearDistance:Display is 5.0)
local DEPTH = 125
local NEAR, FAR = 5.0, 2000.0
local SLOPE = math.tan(75 * 0.0174533 * 0.15) * 0.75
local SCENE_ROWS = { 0, 1, 0, 0, 0, 1, 1, 0, 0 }
-- The pin is turned about the lock's depth axis by the place it is at on the meter: M(A) about model Y, and the root
-- of the pin is moved so that its pivot stays where it is, 2.2 units above the root's origin (0x790DF0)
local PIN_RISE, PIN_LEVER = 0.6, 2.2
-- The lights are the two point lights and the ambient light of the lock's file (NifOsg does not read lights, so they are
-- made here with the numbers of the file: 'Omni02' is fixed in the lock's file, 'Omni01' moves with its frame, whose
-- dimmer of 1.4 is in the colour); they have no fall-off
local AMBIENT = v3(0.004, 0.031, 0.047)
local LIGHT_OMNI02 = v3(0.365, 0.471, 0.494)
local LIGHT_OMNI01 = v3(1.005, 1.274, 1.032)
local LIGHT_RADIUS = 400

-- The sounds the events of the game make (editor ids of the games' sound records). The tension sound is a loop in the
-- games and the sound is played by name without a loop, so it is played once.
local SOUNDS = {
    enter = 'UILockpickingEnter',
    move = 'UILockpickingPickMovement',
    turn = 'UILockpickingCylinderTurn',
    squeakA = 'UILockpickingCylinderSqueakA',
    squeakB = 'UILockpickingCylinderSqueakB',
    stop = 'UILockpickingCylinderStop',
    tensionStart = 'UILockpickingPickTensionLPM',
    ['break'] = 'UILockpickingPickBreak',
    unlock = 'UILockpickingUnlock',
    forceFailed = 'UILockpickingForceFail',
}

-- The pad: the right stick moves the pin (axis 7 of the games, dead zone 7849 of 32767, the cube of the stick times 80
-- units a frame) and pushing it up turns the cylinder (axis 9, dead zone 8689 of 32767). X forces the lock and B leaves.
-- The axes are the Mac's reading of the games' code; no pad has tried them.
local PAD_MOVE_DEAD, PAD_TURN_DEAD = 7849 / 32767, 8689 / 32767
local PAD_UNITS_PER_FRAME = 80
-- The menu is laid out 960 units high at any window size
local MENU_HEIGHT = 960
-- How long the game waits for the picture before it runs without it
local SCENE_WAIT = 2.0

-- The screen has not been seen in the running game: what it does is written to the log, the events and the changes of
-- the state of the game, and the first seconds of what the mouse and the keys gave it
local traced, traceLimit = 0, 300
local function trace(text)
    if traced < traceLimit then
        traced = traced + 1
        print('Lockpick: ' .. text .. (traced == traceLimit and ' (further events are not logged)' or ''))
    end
end

local shown, scene, game, options = nil, nil, nil, nil
local serial = nil
-- The width of the meter, the pin that was drawn last, the cursor, the wait for the key that opened the lock
local meter = 0
local drawn = {}
local cursorHidden = false
local waitedForScene = 0
local waitRelease = true

function module.isOpen()
    return shown ~= nil
end

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function showCursor(visible)
    if cursorHidden == visible then
        cursorHidden = not visible
        ui.setCursorVisible(visible)
    end
end

function module.close()
    showCursor(true)
    if shown then
        shown:close()
    end
    if scene then
        scene:close()
    end
    shown, scene, game, serial = nil, nil, nil, nil
    drawn = {}
end

-- The player leaves: with what happened (the engine's outcome of the lock) or without
local function finish(kind)
    local leave = options and options.leave
    if kind then
        interact.lockOutcome(kind)
    end
    module.close()
    if leave then
        leave()
    end
end

local function pinPlace(angle)
    local a = math.rad(angle)
    local s, c = math.sin(a), math.cos(a)
    return {
        position = v3(PIN_LEVER * s, 0, PIN_RISE - PIN_LEVER * c),
        rows = { c, 0, -s, 0, 1, 0, s, 0, c },
    }
end

local function setUpScene(size)
    local menuScene = core.menuScene
    local halfX = SLOPE * NEAR
    local halfY = halfX * size.y / size.x
    menuScene.setFrustum(SCENE, -halfX, halfX, -halfY, halfY, NEAR, FAR)
    menuScene.setCamera(SCENE, v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0))
    menuScene.setLight(SCENE, AMBIENT, v3(0, 0, 0), v3(0, -1, 0))
    menuScene.setClearColour(SCENE, 0, 0, 0, 0)
    menuScene.setScenePlace(SCENE, { position = v3(DEPTH, 0, 0), rows = SCENE_ROWS })
    menuScene.addModel(SCENE, 'lock', LOCK_MODEL)
    menuScene.addModel(SCENE, 'pin', PIN_MODEL)
    menuScene.addLight(SCENE, 'lock', 'Omni02', v3(0, 0, 0), LIGHT_OMNI02, LIGHT_RADIUS)
    menuScene.addLight(SCENE, 'lock', 'Omni01', v3(0, 0, 0), LIGHT_OMNI01, LIGHT_RADIUS)
    menuScene.setSequenceTime(SCENE, 'lock', 'Forward', 0)
    menuScene.setSequenceTime(SCENE, 'pin', 'Forward', 0)
    menuScene.setPlace(SCENE, 'pin', pinPlace(0))
end

-- The lock and the pin where the game has them
local function draw(report)
    local menuScene = core.menuScene
    if drawn.lockSequence ~= report.lockSequence or drawn.lockTime ~= report.lockTime then
        drawn.lockSequence, drawn.lockTime = report.lockSequence, report.lockTime
        menuScene.setSequenceTime(SCENE, 'lock', report.lockSequence, report.lockTime)
    end
    if drawn.pinSequence ~= report.pinSequence or drawn.pinTime ~= report.pinTime then
        drawn.pinSequence, drawn.pinTime = report.pinSequence, report.pinTime
        menuScene.setSequenceTime(SCENE, 'pin', report.pinSequence, report.pinTime)
    end
    if drawn.pickAngle ~= report.pickAngle then
        drawn.pickAngle = report.pickAngle
        menuScene.setPlace(SCENE, 'pin', pinPlace(report.pickAngle))
    end
end

local function setPicks(count)
    shown:set('LPM_PickCountDisplay', '_Value', string.format('%d', count))
end

-- The sounds of the events, and what the events change outside the screen
local function handle(report)
    for _, event in ipairs(report.events) do
        if event ~= 'move' then
            trace(string.format('event %s (pick %.1f, angle %.1f, state %s, picks %d)', event, report.pick, report.angle,
                report.state, report.picks))
        end
        local sound = SOUNDS[event]
        if sound then
            shown:playSound(sound)
        end
        if event == 'break' then
            -- The engine takes one bobby pin from the player
            interact.lockOutcome('pickBroke')
            setPicks(report.picks)
        elseif event == 'outOfPicks' then
            interact.lockOutcome('outOfPicks')
        elseif event == 'unlock' then
            finish('opened')
            return
        elseif event == 'forceFailed' then
            finish('forceFailed')
            return
        end
    end
end

local function force()
    if game then
        local report = game:force()
        draw(report)
        handle(report)
    end
end

local TURN_KEYS = {
    input.KEY.W, input.KEY.A, input.KEY.S, input.KEY.D,
    input.KEY.UpArrow, input.KEY.DownArrow, input.KEY.LeftArrow, input.KEY.RightArrow,
}
local TURN_ACTIONS = input.ACTION
        and { input.ACTION.MoveForward, input.ACTION.MoveBackward, input.ACTION.MoveLeft, input.ACTION.MoveRight }
    or {}

-- How hard the player turns the lock: the four movement controls (any of them is all the way), or the stick
local function turnInput()
    for _, action in ipairs(TURN_ACTIONS) do
        if input.isActionPressed(action) then
            return 1
        end
    end
    for _, key in ipairs(TURN_KEYS) do
        if input.isKeyPressed(key) then
            return 1
        end
    end
    local stick = -input.getAxisValue(input.CONTROLLER_AXIS.RightY)
    if stick > PAD_TURN_DEAD then
        return math.min(stick, 1)
    end
    return 0
end

-- How far the pin goes along the meter: the mouse in the units of the menu (1:1 with the cursor), or the stick
local function moveInput(seconds)
    local move = input.getMouseMoveX() * MENU_HEIGHT / ui.screenSize().y
    local stick = input.getAxisValue(input.CONTROLLER_AXIS.RightX)
    if math.abs(stick) > PAD_MOVE_DEAD then
        move = move + stick * stick * stick * PAD_UNITS_PER_FRAME * seconds * 60
    end
    return move
end

-- The games hide the pointer over the area the pin moves in
local function updateCursor()
    local rect = shown:rect('LPM_DetectArea')
    local at = shown:pointerAt()
    local inside = rect ~= nil and at ~= nil and at.x >= rect.x and at.x <= rect.x + rect.width
        and at.y >= rect.y and at.y <= rect.y + rect.height
    showCursor(not inside)
end

local function activateHeld()
    return input.ACTION ~= nil and input.isActionPressed(input.ACTION.Activate)
end

local lastState, frames = nil, 0
local function frame(seconds)
    shown:checkSize()
    local pictured = scene:update()
    updateCursor()
    frames = frames + 1
    if pictured and not drawn.pictured then
        drawn.pictured = true
        trace(string.format('the picture is on the screen after %d frames', frames))
    end
    if not pictured and waitedForScene < SCENE_WAIT then
        -- The picture is not on the screen yet: the lock has not flown in
        waitedForScene = waitedForScene + seconds
        return
    end
    -- The key that opened the lock is let go before the screen takes any
    if waitRelease and not activateHeld() then
        waitRelease = false
    end
    local report = game:step {
        seconds = math.min(seconds, 0.1),
        move = waitRelease and 0 or moveInput(seconds),
        turn = waitRelease and 0 or turnInput(),
    }
    if report.state ~= lastState then
        lastState = report.state
        trace(string.format('state %s (pick %.1f of %.0f, mouse %.2f, turn %.1f, lock %s %.2f, pin %s %.2f)', report.state,
            report.pick, meter, waitRelease and 0 or moveInput(0), waitRelease and 0 or turnInput(), report.lockSequence,
            report.lockTime, report.pinSequence, report.pinTime))
    end
    draw(report)
    handle(report)
end

-- Each frame the screen is there
function module.update(seconds)
    if not shown then
        return
    end
    local ok, problem = pcall(frame, seconds)
    if not ok then
        -- A screen that fails is left, not kept: the lock would be shut in with a pointer that cannot be seen
        print('The lockpicking screen failed: ' .. tostring(problem))
        local leave = options and options.leave
        module.close()
        if leave then
            leave()
        end
    end
end

-- Shows the screen for the lock the engine has open (the row of interact.lock()). False when this install cannot (no menu
-- file, no scenes) and the stand-in is to be shown; true also while it is already shown for that lock.
function module.start(lock, opts)
    if shown then
        return serial == lock.serial
    end
    if lock.keyOnly or core.lockpick == nil or not sceneModule.available() then
        return false
    end
    local opened = menuModule.open(FILE)
    if not opened then
        return false
    end
    local ready, width = pcall(function()
        return tonumber(opened:get('LPM_DetectMeter', 'width'))
    end)
    if not ready or not width or width <= 0 then
        opened:close()
        return false
    end
    local made = sceneModule.open(SCENE)
    if not made then
        opened:close()
        return false
    end

    shown, scene, options, serial, meter = opened, made, opts or {}, lock.serial, width
    drawn, waitedForScene, waitRelease = {}, 0, true
    lastState, frames = nil, 0
    trace(string.format('opened for class %s, skill %.0f, %d pins, meter %.0f units, window %dx%d', tostring(lock.class),
        lock.skill, lock.pins, width, ui.screenSize().x, ui.screenSize().y))
    setUpScene(ui.screenSize())
    game = core.lockpick.new { class = lock.class, skill = lock.skill, picks = lock.pins, meter = meter }

    -- The rows of the box and the button that the file leaves to the code
    shown:batch(function()
        shown:set('LPM_SkillDisplay', '_Value', string.format('%d', math.floor(lock.skill + 0.5)))
        shown:set('LPM_LevelDisplay', '_Value', lock.word)
        shown:set('LPM_ForceLockButton', 'string',
            string.format('%s [%d%%]', word('sForceLock', 'Force Lock'), math.floor(lock.force + 0.5)))
        setPicks(lock.pins)
    end)
    shown:onClick('LPM_ForceLockButton', force)
    shown:onClick('LPM_ExitButton', function() finish(nil) end)
    return true
end

-- A key (or the button of the pad that stands for one): the letters the file ties to its buttons (F forces, E leaves)
-- and, from the pad, X forces. True when it did something.
function module.press(code, symbol)
    if not shown then
        return false
    end
    if symbol ~= nil and symbol ~= '' then
        return shown:pcButton(symbol)
    end
    if code == input.KEY.F then
        force()
        return true
    end
    return false
end

return module
