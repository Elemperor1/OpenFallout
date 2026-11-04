-- The two screens on which the player sets SPECIAL by turning the pages of a model: the Vitomatic Vigor Tester of New Vegas
-- (LoveTesterMenu, love_tester_menu.xml, menu 1074) and the SPECIAL book of Fallout 3 (SPECIALBookMenu, specialbookmenu.xml,
-- menu 1060). The menu files hold nothing to see: a full-window area that takes the click and the pad's buttons. Everything
-- on the screen is the model (Meshes\architecture\Goodsprings\NV_VitoMaticVigorTester_Activate.NIF and its cabinet, or
-- Meshes\Terminals\Babybook02.NIF), which the games' code turns page by page with its sequences and whose digits and button
-- faces it changes by the shape's name. The rules (the pages, the clips, the points, the sounds) are core.specialScreen
-- (Mac rounds 12AW S5 and S6 and 12AY R1); what the player sets is written into the character's attributes of core.chargen as
-- it is set, and the screen leaves when the points are all given. It is a module of falloutchargen.lua, not a script.
--
--   special.start(page, state)  -- false: this install cannot show it, the stand-in is to be drawn
--   special.update(seconds); special.press(code, symbol); special.pad(button); special.close(); special.isOpen()
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')

local menuModule = require('scripts.omw.falloutmenu')
local sceneModule = require('scripts.omw.falloutscene')

local chargen = core.chargen
local v3 = util.vector3

local module = {}

local SCENE = 'special'
local MENU_TESTER, MENU_BOOK = 1074, 1060

-- The scene of the games' code (FalloutNV.exe 0x7928E0 and 0x7945F0 for the tester, 0x7C6C80 and 0x7C9C30 for the book):
-- a camera 'Surgery3DCamera' at (0, 0, 1) that looks along -Z with +Y up, a frustum whose half width on the near plane
-- (1) is 1.810 for the tester and 0.2925 for the book (a narrow lens) and whose height follows the window's, and the
-- models placed in front of it
local CAMERA_EYE, CAMERA_FORWARD, CAMERA_UP = v3(0, 0, 1), v3(0, 0, -1), v3(0, 1, 0)
local NEAR, FAR = 1.0, 5000.0

local KINDS = {
    tester = {
        file = 'chargen/love_tester_menu.xml',
        halfWidth = 1.810,
        -- Rx(1.5708) * Rz(3.14159) * Rx(0.125664) as the rows of a matrix that turns a column vector; both models alike
        rows = { -1, 0, 0, 0, -0.12533, 0.99212, 0, 0.99212, 0.12533 },
        scale = 1,
        models = {
            { name = 'tester', path = 'meshes/architecture/goodsprings/nv_vitomaticvigortester_activate.nif' },
            { name = 'cabinet', path = 'meshes/architecture/goodsprings/nv_vitomaticvigortester_cabinet.nif' },
        },
        -- (guess) neither file has a light and the games' scene has a point light at its origin: a light from the camera
        -- stands for it, with a little ambient light
        ambient = v3(0.3, 0.3, 0.3),
        diffuse = v3(0.85, 0.85, 0.85),
        sequenced = 'tester',
    },
    book = {
        file = 'chargen/specialbookmenu.xml',
        halfWidth = 0.2925,
        -- +90 degrees about X (the sign is assumed, it is to be seen in a render)
        rows = { 1, 0, 0, 0, 0, -1, 0, 1, 0 },
        scale = 0.05,
        models = { { name = 'book', path = 'meshes/terminals/babybook02.nif' } },
        ambient = v3(0.25, 0.25, 0.25),
        diffuse = v3(0, 0, 0),
        sequenced = 'book',
    },
}

-- The screen has not been seen in the running game: what it does is written to the log, the first events and what
-- the pointer found
local traced, traceLimit = 0, 200
local function trace(text)
    if traced < traceLimit then
        traced = traced + 1
        print('SpecialScreen: ' .. text .. (traced == traceLimit and ' (further events are not logged)' or ''))
    end
end

local open = nil

function module.isOpen()
    return open ~= nil
end

function module.close()
    if not open then return end
    local current = open
    open = nil
    if current.shown then current.shown:close() end
    if current.scene then current.scene:close() end
end

-- The model that a shape belongs to: the lamps of the cabinet are in its file, the rest in the one that turns
local function modelOf(kind, shape)
    if kind == 'tester' and shape:lower():find('^p1_pointval_') then
        return 'cabinet'
    end
    return KINDS[kind].sequenced
end

-- The screen shows the model at the page it is at, with every shape as the rules have it
local function drawShapes(force)
    local menuScene = core.menuScene
    for _, row in ipairs(open.screen:shapes()) do
        local before = open.applied[row.shape]
        if before == nil then
            before = { visible = true }
            open.applied[row.shape] = before
        end
        local model = modelOf(open.kind, row.shape)
        if force or before.visible ~= row.visible then
            before.visible = row.visible
            menuScene.setShapeVisible(SCENE, model, row.shape, row.visible)
        end
        if row.texture ~= '' and (force or before.texture ~= row.texture) then
            before.texture = row.texture
            menuScene.setTexture(SCENE, model, row.shape, row.texture)
        end
    end
end

local function drawSequence()
    local view = open.screen:view()
    if open.drawnSequence ~= view.sequence or open.drawnTime ~= view.time then
        open.drawnSequence, open.drawnTime = view.sequence, view.time
        core.menuScene.setSequenceTime(SCENE, KINDS[open.kind].sequenced, view.sequence, view.time)
    end
end

-- The sound of an action, and what it changed: the attribute it set goes to the character
local function handle(outcome)
    if outcome.sound then
        open.shown:playSound(outcome.sound)
    end
    if outcome.attribute then
        local wanted = outcome.value
        local before = open.values[outcome.attribute]
        if before ~= wanted then
            if not chargen.adjust(4 + outcome.attribute, wanted - before) then
                trace(string.format('the character would not take attribute %d from %d to %d', outcome.attribute, before,
                    wanted))
            end
            open.values[outcome.attribute] = wanted
        end
        trace(string.format('attribute %d is %d (points left %d)', outcome.attribute, wanted, open.screen:view().left))
    end
    drawShapes(false)
    if outcome.close then
        trace('done')
        module.close()
        chargen.finish()
    end
end

local function setUpScene(kind, size)
    local menuScene = core.menuScene
    local config = KINDS[kind]
    local halfX = config.halfWidth * NEAR
    local halfY = halfX * size.y / size.x
    menuScene.setFrustum(SCENE, -halfX, halfX, -halfY, halfY, NEAR, FAR)
    menuScene.setCamera(SCENE, CAMERA_EYE, CAMERA_FORWARD, CAMERA_UP)
    menuScene.setLight(SCENE, config.ambient, config.diffuse, CAMERA_FORWARD)
    menuScene.setClearColour(SCENE, 0, 0, 0, 0)
    local position
    if kind == 'tester' then
        -- (0, -85, -75) when the window is 640 wide or less, else (0, -85, -85)
        position = v3(0, -85, size.x <= 640 and -75 or -85)
    else
        position = v3(0, 0, -60)
    end
    for _, model in ipairs(config.models) do
        -- keepShapes: each shape of the files stays a node of its own, for the digits and buttons to be named
        menuScene.addModel(SCENE, model.name, model.path, true)
        menuScene.setPlace(SCENE, model.name, { position = position, rows = config.rows, scale = config.scale })
    end
    if kind == 'book' then
        -- The file's own light, 'Omni01', a point light with no fall-off at (-7.7, -139.9, 92.1) in the book's file,
        -- dimmer 1.5, colour (0.796, 0.933, 0.824); the book is scaled by 0.05, so the light's reach is big
        menuScene.addLight(SCENE, 'book', '', v3(-7.7, -139.9, 92.1), v3(0.796, 0.933, 0.824) * 1.5, 20000)
    end
end

-- The click of the player: which button of the model is under the pointer is asked of the scene, and the answer comes in
-- a later frame
-- The device the player used last, told by the script that sees every key and button (also while no screen is open)
local lastPad = false

function module.noteDevice(pad)
    lastPad = pad
end

-- The device in use: a button of the pad puts the cursor on the index page, a key or the pointer takes it away again
local function usePad(on)
    if open and open.padMode ~= on then
        open.padMode = on
        open.screen:setPad(on)
        -- The index page shows the buttons of the cursor's row only while the pad is in use: the shapes come back for the
        -- pointer (or go for the pad) at once, before a pick is asked of the scene
        if open.started then drawShapes(false) end
    end
end

local function click()
    if not open or open.asking then return end
    usePad(false)
    local at = open.shown:pointerAt()
    local size = ui.screenSize()
    if not at or size.x <= 0 or size.y <= 0 then return end
    local x, y = at.x / size.x, at.y / size.y
    local _, answered = core.menuScene.picked(SCENE)
    open.asking = answered
    open.askedAt = { x = x, y = y }
    core.menuScene.pick(SCENE, x, y, open.screen:buttons())
end

local function answer()
    if not open.asking then return end
    local shape, answered = core.menuScene.picked(SCENE)
    if answered == open.asking then return end
    open.asking = nil
    trace(string.format('the pointer at (%.3f, %.3f) found %s', open.askedAt.x, open.askedAt.y,
        shape ~= '' and shape or 'nothing'))
    if shape ~= '' then
        handle(open.screen:press(shape))
    end
end

local function frame(seconds)
    open.shown:checkSize()
    local pictured = open.scene:update()
    if not pictured then
        -- The picture is not on the screen yet: the model has not flown in
        return
    end
    if not open.started then
        open.started = true
        trace(string.format('the picture is on the screen (window %dx%d)', ui.screenSize().x, ui.screenSize().y))
        drawShapes(true)
        drawSequence()
        local sound = open.screen:opening()
        if sound ~= '' then open.shown:playSound(sound) end
    end
    open.screen:update(math.min(seconds, 0.1))
    drawSequence()
    answer()
end

-- A frame of the screen. True when the screen failed and was closed: the page is then to be shown again by the stand-in
-- (ending the character here would leave the points unspent)
function module.update(seconds)
    if not open then return false end
    local ok, problem = pcall(frame, seconds)
    if not ok then
        print('The SPECIAL screen failed: ' .. tostring(problem))
        module.close()
        module.failed = true
        return true
    end
    return false
end

-- Shows the screen for the page of core.chargen (`page.menu` says which of the two) with the character as it is now
-- (`state`). False when this install cannot: the menu file or the scenes are not there, and the stand-in is to be shown.
function module.start(page, state)
    if open then return true end
    if module.failed then return false end
    local kind = page.menu == MENU_TESTER and 'tester' or (page.menu == MENU_BOOK and 'book' or nil)
    if not kind or core.specialScreen == nil or not sceneModule.available() then
        return false
    end
    local config = KINDS[kind]
    local shown = menuModule.open(config.file)
    if not shown then
        return false
    end
    local scene = sceneModule.open(SCENE)
    if not scene then
        shown:close()
        return false
    end
    local values = {}
    for i, attribute in ipairs(state.attributes) do
        values[i] = attribute.value
    end
    -- The pictures of the buttons are chosen once, when the screen opens, by the device the player used last (the pad's
    -- or the keyboard's)
    open = {
        kind = kind, shown = shown, scene = scene, values = values, applied = {}, padMode = lastPad,
        screen = core.specialScreen.new { kind = kind, limit = page.limit, values = values, pad = lastPad },
    }
    trace(string.format('%s opened with %d points and the attributes %s, window %dx%d', kind, page.limit,
        table.concat(values, ' '), ui.screenSize().x, ui.screenSize().y))
    setUpScene(kind, ui.screenSize())
    -- The file's hot rectangle that covers the window (id 5) takes every click in the executable (a click there casts a ray
    -- into the model); the file does not say it is a target, so it is made one
    pcall(function() shown:set('clickable', 'target', 1) end)
    shown:onClick('clickable', click)
    -- The ids of the file that the pad's buttons stand for, and the exit
    shown:onClick('next_page', function() handle(open.screen:next()) end)
    shown:onClick('previous_page', function() handle(open.screen:previous()) end)
    shown:onClick('increase_value', function() handle(open.screen:increase()) end)
    shown:onClick('decrease_value', function() handle(open.screen:decrease()) end)
    shown:onClick('index_up', function() handle(open.screen:moveCursor(-1)) end)
    shown:onClick('index_down', function() handle(open.screen:moveCursor(1)) end)
    shown:onClick('exit_menu', function() handle(open.screen:exit()) end)
    return true
end

-- A key: Left and Right turn the pages, Up and Down raise and lower the attribute of the page ('+', '=' and '-' too on the
-- tester), A is the exit (the executable reads it as a key of its own). True when the key was the screen's.
function module.press(code, symbol)
    if not open then return false end
    usePad(false)
    local K = input.KEY
    local tester = open.kind == 'tester'
    if code == K.LeftArrow then
        handle(open.screen:previous())
    elseif code == K.RightArrow then
        handle(open.screen:next())
    elseif code == K.UpArrow or (tester and (symbol == '+' or symbol == '=')) then
        handle(open.screen:increase(true))
    elseif code == K.DownArrow or (tester and (symbol == '-' or symbol == '_')) then
        handle(open.screen:decrease(true))
    elseif code == K.A then
        handle(open.screen:exit())
    else
        return false
    end
    return true
end

-- A button of the pad, as the file ties them: the right and left of the pad raise and lower the attribute, up and down
-- move the cursor of the index, the shoulders turn the pages, X leaves
function module.pad(button)
    if not open then return false end
    usePad(true)
    local B = input.CONTROLLER_BUTTON
    if button == B.DPadRight then
        handle(open.screen:increase())
    elseif button == B.DPadLeft then
        handle(open.screen:decrease())
    elseif button == B.DPadUp then
        handle(open.screen:moveCursor(-1))
    elseif button == B.DPadDown then
        handle(open.screen:moveCursor(1))
    elseif button == B.RightShoulder then
        handle(open.screen:next())
    elseif button == B.LeftShoulder then
        handle(open.screen:previous())
    elseif button == B.X then
        handle(open.screen:exit())
    else
        return false
    end
    return true
end

return module
