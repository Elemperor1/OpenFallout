-- The slot machine of the casinos of New Vegas. The rules, the chips and the winnings are the engine's (core.casino.slots); this
-- script is the game's own menu (slot_machine_menu.xml: the bet, the chips, the casino, the six buttons and the status text)
-- over the game's own machine, which its code draws in 3D behind the tiles (FalloutNV.exe 0x7C1490 to 0x7C58F0, Mac round
-- 12AS E1): the model of the casino's record (its eighth model; the arm, the coins, the payout board and a sign) and three
-- reels of fourteen faces (Meshes\Terminals\NV_SlotMachine\NV_SlotMachine-Reel01 to 03.NIF).
--
-- The machine does not turn the reels to a stop that was drawn: every pull plays the same sequence of the reels' files (the arm
-- is pulled in 0.83 s, the reels start turning at 0.5 s and stop at 2.4, 3.3 and 4.2 s, each ending in the pose it began in), and
-- what the reels show is made by putting the pictures of the symbols on their faces: the six faces in front when the arm is
-- back, the eight others when the last reel has stopped. A pull that wins plays the machine's 'Backward' sequence (the coins
-- fall, 3.2 s); the payout list is the machine's 'Payout_In' and 'Payout_Out' sequences (the board slides in and out, 0.67 s).
-- The chips and the winnings are the engine's from the pull on and are shown when the reels have stopped.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local padModule = require('scripts.omw.falloutpad')
local sceneModule = require('scripts.omw.falloutscene')

local casino = core.casino
local pipboy = core.pipboy
local v3 = util.vector3

if not casino.available() or not pipboy.available() then
    return {}
end

local FILE = 'slot_machine_menu.xml'
local SCENE = 'slots'
local FALLBACK_MACHINE = 'meshes/terminals/nv_slotmachine/nv_slotmachine-minigame.nif'
local REEL_MODELS = {
    'meshes/terminals/nv_slotmachine/nv_slotmachine-reel01.nif',
    'meshes/terminals/nv_slotmachine/nv_slotmachine-reel02.nif',
    'meshes/terminals/nv_slotmachine/nv_slotmachine-reel03.nif',
}
-- The picture on a face between two symbols (the game's code loads it as the eighth picture)
local BLANK = 'textures/terminals/nv_slotmachine/nv_slotmachine-symbols.dds'

-- The scene of the games' code (0x7C1AF0 and the camera 0x7C2180, round 12AS E1): the root stands 175 units in front of the
-- camera and 18 below its axis, turned so that the x of a model is to the right in the picture, its z up and its y away from the
-- camera; the camera is at the origin looking along +X with a frustum +-tan(a)*0.75 wide at distance 1, a = 75 degrees * 0.0174533
-- * 0.15 (fDefaultFOV:Display), as high as the window is (fNearDistance:Display is 5.0). Nothing is behind the machine: the world
-- shows round it.
local ROOT = v3(175, -18, 0)
local SCENE_ROWS = { 0, 1, 0, 0, 0, 1, 1, 0, 0 }
local NEAR, FAR = 5.0, 4000.0
local SLOPE = math.tan(75 * 0.0174533 * 0.15) * 0.75
-- The four point lights of the machine's file (NifOsg does not read lights): where each is in the file's root, and its colour with
-- its dimmer in it. They have no fall-off, and no ambient light is in the file.
local LIGHTS = {
    { place = v3(-37.2262, 13.4226, 6.7873), colour = v3(0.5927, 0.5604, 0.5060) },
    { place = v3(-7.1447, -45.9107, 59.7387), colour = v3(0.2285, 0.2245, 0.2060) },
    { place = v3(37.2361, -16.9895, 27.698), colour = v3(0.925, 0.922, 0.898) },
    { place = v3(-47.3805, -27.9602, 24.1924), colour = v3(0.792, 0.78, 0.71) },
}
local LIGHT_RADIUS = 400

-- Seconds of the sequences of the files
local ARM_END = 0.8333
local REEL_END = 4.3667
local COINS_END = 3.1667
local BOARD_TIME = 0.6667
-- The status text stays this long after the state it was made in began
local STATUS_SECONDS = 1.0
-- A machine that closes by itself (the player has no chips left) is open this long after the notice
local CLOSE_DELAY = 3

local SOUND = {
    activate = 'GAMESlotsActivate',
    increase = 'GAMESlotsIncreaseBet0%d',
    decrease = 'GAMESlotsDecreaseBet',
    pull = 'GameSlotsPullLever',
    small = 'GAMESlotsWinSmall',
    medium = 'GAMESlotsWinMed',
    jackpot = 'GAMESlotsWinJackpot',
    notice = 'UIPopUpMessageGeneral',
}

-- What the screen is doing: 'idle' (waits for a click), 'spin' (the arm and the reels), 'win' (the coins fall), 'boardIn' (the
-- payout list comes in and waits for a button), 'boardOut'
local shown, scene = nil, nil
local shownSerial = nil
local modeOpen = false
local state = 'idle'
local stateTime = 0
local pending = nil
local frontDone = false
local statusShown = false
local previous = { 14, 14, 14 }
local symbols = {}
local pitch = 1
local closeIn = nil
local sizeSet = nil

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function sound(name)
    if shown then shown:playSound(name) end
end

local function openMode()
    if not modeOpen then
        modeOpen = true
        I.UI.addMode('Interface', { windows = {} })
    end
end

local function closeMode()
    if modeOpen then
        modeOpen = false
        I.UI.removeMode('Interface')
    end
end

local function destroyMenu()
    if shown then
        shown:close()
    end
    if scene then
        scene:close()
    end
    shown, scene, pending, closeIn, sizeSet = nil, nil, nil, nil, nil
    state, stateTime, frontDone, statusShown = 'idle', 0, false, false
    previous = { 14, 14, 14 }
end

-- The player gets up from the machine: what it has more or less than when it sat down goes into its pack
local function leave()
    destroyMenu()
    casino.close()
    shownSerial = nil
    closeMode()
end

local function chipsText(count)
    local chip = word('sCasinoChipText', 'Chip')
    return string.format('%d %s%s', count, chip, count == 1 and '' or word('sPlural', 's'))
end

-- ---- The reels ---------------------------------------------------------------------------------------------------------------

-- The face is the picture of one of the seven symbols (0 to 6, in the order of the casino's record) or the blank between them (7)
local function setFace(reel, face, picture)
    local path = picture == 7 and BLANK or symbols[picture + 1]
    if path and path ~= '' then
        core.menuScene.setTexture(SCENE, 'reel' .. (reel + 1), string.format('Cylinder0%d:%d', 6 - reel, face % 14), path)
    end
end

-- The six faces that are in front when the reels are at rest, for the stop of a reel (0 to 13: an even stop is the symbol
-- stop / 2 in the middle of the window with the next ones above and below, an odd one is between two symbols)
local function frontFaces(reel, stop)
    local base = 2 * reel - 2
    if stop % 2 == 0 then
        local c = math.floor(stop / 2)
        setFace(reel, base, (c - 1) % 7)
        setFace(reel, base + 1, 7)
        setFace(reel, base + 2, c % 7)
        setFace(reel, base + 3, 7)
        setFace(reel, base + 4, (c + 1) % 7)
        setFace(reel, base + 5, 7)
    else
        setFace(reel, base, 7)
        setFace(reel, base + 1, math.floor((stop - 1) / 2) % 7)
        setFace(reel, base + 2, 7)
        setFace(reel, base + 3, math.floor((stop + 1) / 2) % 7)
        setFace(reel, base + 4, 7)
        setFace(reel, base + 5, math.floor((stop + 3) / 2) % 7)
    end
end

-- The eight faces behind them
local function backFaces(reel, stop)
    local base = 2 * reel + 4
    if stop % 2 == 0 then
        local c = math.floor(stop / 2)
        for k = 0, 3 do
            setFace(reel, base + 2 * k, (c + 3 + k) % 7)
            setFace(reel, base + 2 * k + 1, 7)
        end
    else
        for k = 0, 3 do
            setFace(reel, base + 2 * k, 7)
            setFace(reel, base + 2 * k + 1, math.floor((stop + 3 + k) / 2) % 7)
        end
    end
end

-- A reel is given its faces only when its stop is not the one it had
local function showStops(stops, which)
    for reel = 0, 2 do
        local stop = stops[reel + 1]
        if stop ~= previous[reel + 1] then
            if which ~= 'back' then frontFaces(reel, stop) end
            if which ~= 'front' then backFaces(reel, stop) end
        end
    end
    if which ~= 'front' then
        for reel = 1, 3 do previous[reel] = stops[reel] end
    end
end

local function setUpScene(view)
    local menuScene = core.menuScene
    local size = ui.screenSize()
    sizeSet = size
    local halfX = SLOPE * NEAR
    local halfY = halfX * size.y / size.x
    menuScene.setFrustum(SCENE, -halfX, halfX, -halfY, halfY, NEAR, FAR)
    menuScene.setCamera(SCENE, v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0))
    menuScene.setLight(SCENE, v3(0, 0, 0), v3(0, 0, 0), v3(0, -1, 0))
    menuScene.setClearColour(SCENE, 0, 0, 0, 0)
    menuScene.setScenePlace(SCENE, { position = ROOT, rows = SCENE_ROWS })
    -- The machine is added first: its lights are found before the reels are drawn
    menuScene.addModel(SCENE, 'machine', view.scene ~= '' and view.scene or FALLBACK_MACHINE)
    for index, path in ipairs(REEL_MODELS) do
        menuScene.addModel(SCENE, 'reel' .. index, path)
    end
    for _, light in ipairs(LIGHTS) do
        menuScene.addLight(SCENE, 'machine', '', light.place, light.colour, LIGHT_RADIUS)
    end
    menuScene.setSequenceTime(SCENE, 'machine', 'Forward', 0)
    for index = 1, 3 do
        menuScene.setSequenceTime(SCENE, 'reel' .. index, 'Forward', 0)
    end
    -- The reels show the symbols of the stops they are at (the game builds them as if they came from the stop 14)
    previous = { 14, 14, 14 }
    showStops(view.stops, 'both')
end

-- ---- The numbers of the menu -----------------------------------------------------------------------------------------------

local function setDisplay(name, title, value)
    local tile = shown:find(name)
    if not tile then return end
    shown:set(tile, '_Value', title)
    shown:set(tile, '_X', tostring(value))
end

local function fillAll()
    local view = casino.slots.state()
    local screen = casino.screen()
    if not view or not screen then return end
    shown:batch(function()
        setDisplay('SMM_CurrentBetDisplay', word('sCurrentBetText', 'Current Bet:'), view.bet)
        setDisplay('SMM_ChipsDisplay', word('sChipCountText', 'Chips:'), view.chips)
        -- The earnings are named with the casino and the number itself is not shown (as on the other tables)
        setDisplay('SMM_CasinoNameDisplay', word('sCasinoEarningsText', 'Casino Earnings:') .. ' ' .. screen.name, '')
        local earnings = shown:find('SMM_TotalEarningsValue')
        if earnings then shown:set(earnings, 'visible', 0) end
    end)
end

local function setButtons()
    local labels = {
        SMM_Spin = word('sSpinText', 'Spin'),
        SMM_IncreaseBet = word('sIncreaseBetText', 'Increase Bet'),
        SMM_DecreaseBet = word('sDecreaseBetText', 'Decrease Bet'),
        SMM_PayoutList = word('sPayoutListText', 'Payout List'),
        SMM_BetMax = word('sBetMax', 'Max Bet'),
    }
    shown:batch(function()
        for name, text in pairs(labels) do
            local tile = shown:find(name)
            if tile then shown:set(tile, 'string', text) end
        end
    end)
end

local function status(text)
    local tile = shown:find('SMM_StatusText')
    if not tile then return end
    statusShown = text ~= ''
    shown:batch(function()
        shown:set(tile, 'string', text)
        shown:set(tile, 'visible', text ~= '' and 1 or 0)
    end)
end

-- ---- What the player does ----------------------------------------------------------------------------------------------------

local function setState(name)
    state = name
    stateTime = 0
end

local function busy()
    return state ~= 'idle' or closeIn ~= nil
end

-- The three sounds of a bet that goes up come one after the other
local function raiseSound()
    sound(string.format(SOUND.increase, pitch))
    pitch = pitch % 3 + 1
end

local function spin()
    if busy() then return end
    local view = casino.slots.state()
    if not view or not view.canSpin then return end
    pending = casino.slots.spin()
    if not pending then return end
    pitch = 1
    frontDone = false
    sound(SOUND.pull)
    setState('spin')
end

local function increase()
    if busy() then return end
    casino.slots.increase()
    raiseSound()
    fillAll()
end

local function decrease()
    if busy() then return end
    casino.slots.decrease()
    pitch = 1
    sound(SOUND.decrease)
    fillAll()
end

local function highest()
    if busy() then return end
    casino.slots.highest()
    raiseSound()
    fillAll()
end

local function inBoard()
    return state == 'boardIn'
end

-- Any button while the payout list is up sends it away again
local function endBoard()
    status('')
    setState('boardOut')
end

local function showBoard()
    if busy() then return end
    status(word('sSlotPressAnyButtonText', 'Press any button'))
    setState('boardIn')
end

-- A button of the screen is clicked
local function onButton(action)
    return function()
        if inBoard() then
            endBoard()
        else
            action()
        end
    end
end

-- The pull is over: the other faces, the numbers, and the words of the result (a win goes on with the coins falling, the
-- rest is over)
local function finishSpin()
    local result = pending
    pending = nil
    showStops(result.stops, 'back')
    fillAll()
    local view = casino.slots.state()
    if result.class > 3 then
        local label = result.lucky and word('sLuckyWinText', 'Lucky win!') or word('sYouWin', 'You win')
        status(string.format('%s %s', label, chipsText(math.abs(result.net))))
        sound(result.class == 4 and SOUND.small or result.class == 5 and SOUND.medium or SOUND.jackpot)
        setState('win')
    else
        status(string.format('%s %s', word('sYouLose', 'You lose'), chipsText(math.abs(result.net))))
        setState('idle')
        -- A player with no chips left is told so, and the machine closes
        if view and view.broke then
            closeIn = CLOSE_DELAY
            status(word('sGamblingBrokeText', 'You do not have enough chips.'))
            sound(SOUND.notice)
        end
    end
end

local function stepFrame(dt)
    stateTime = stateTime + dt
    local menuScene = core.menuScene
    if state == 'spin' then
        if scene then
            menuScene.setSequenceTime(SCENE, 'machine', 'Forward', stateTime)
            for index = 1, 3 do
                menuScene.setSequenceTime(SCENE, 'reel' .. index, 'Forward', stateTime)
            end
        end
        if statusShown and stateTime > STATUS_SECONDS then status('') end
        if not frontDone and stateTime >= ARM_END then
            frontDone = true
            showStops(pending.stops, 'front')
        end
        if stateTime >= REEL_END then finishSpin() end
    elseif state == 'win' then
        if scene then menuScene.setSequenceTime(SCENE, 'machine', 'Backward', stateTime) end
        if statusShown and stateTime > STATUS_SECONDS then status('') end
        if stateTime >= COINS_END then
            if scene then menuScene.setSequenceTime(SCENE, 'machine', 'Forward', 0) end
            local view = casino.slots.state()
            setState('idle')
            -- The winnings reached a new quarter of the casino's most: the machine closes
            if view and view.closes then leave() end
        end
    elseif state == 'boardIn' then
        if scene then menuScene.setSequenceTime(SCENE, 'machine', 'Payout_In', math.min(stateTime, BOARD_TIME)) end
    elseif state == 'boardOut' then
        if scene then menuScene.setSequenceTime(SCENE, 'machine', 'Payout_Out', math.min(stateTime, BOARD_TIME)) end
        if stateTime >= BOARD_TIME then
            if scene then menuScene.setSequenceTime(SCENE, 'machine', 'Forward', 0) end
            setState('idle')
        end
    elseif statusShown and stateTime > STATUS_SECONDS then
        status('')
    end
    if closeIn ~= nil then
        closeIn = closeIn - dt
        if closeIn <= 0 then leave() end
    end
end

local function openMenu(view)
    local opened = menuModule.open(FILE)
    if not opened then return false end
    shown = opened
    symbols = view.symbols
    shown:onClick('SMM_Spin', onButton(spin))
    shown:onClick('SMM_IncreaseBet', onButton(increase))
    shown:onClick('SMM_DecreaseBet', onButton(decrease))
    shown:onClick('SMM_PayoutList', onButton(showBoard))
    shown:onClick('SMM_BetMax', onButton(highest))
    shown:onClick('SMM_ExitButton', function()
        if inBoard() then
            endBoard()
        elseif not busy() then
            leave()
        end
    end)
    setButtons()
    status('')
    if sceneModule.available() then
        scene = sceneModule.open(SCENE)
        if scene then setUpScene(view) end
    end
    sound(SOUND.activate)
    return true
end

local function sync()
    local screen = casino.screen()
    if not screen or screen.game ~= 'slots' then
        if shown or modeOpen then
            destroyMenu()
            closeMode()
            shownSerial = nil
        end
        return
    end
    if screen.serial ~= shownSerial then
        shownSerial = screen.serial
        destroyMenu()
        openMode()
        local view = casino.slots.state()
        if not view or not openMenu(view) then
            print('slots: the file ' .. FILE .. ' is not there; the machine is not shown')
            casino.close()
            shownSerial = nil
            closeMode()
            return
        end
        pitch = 1
        fillAll()
    end
    if shown then
        shown:checkSize()
        if scene then
            scene:update()
            -- The picture follows the window: the frustum is as high as the window is
            local size = ui.screenSize()
            if sizeSet and (size.x ~= sizeSet.x or size.y ~= sizeSet.y) then
                sizeSet = size
                local halfX = SLOPE * NEAR
                core.menuScene.setFrustum(SCENE, -halfX, halfX, -halfX * size.y / size.x, halfX * size.y / size.x, NEAR, FAR)
            end
        end
    end
end

local function press(code, symbol)
    if not shown or I.UI.getMode() ~= 'Interface' then return end
    if inBoard() then
        endBoard()
        return
    end
    if symbol ~= nil and symbol ~= '' then
        shown:pcButton(symbol)
    end
end

-- On the pad the shoulders stand for the triggers (down and up with the bet), X is the greatest bet and Y the payout list; A is
-- left to the engine, which clicks with it where the pointer is (so the arm is pulled by clicking Spin, as in the other casino
-- screens: a second binding on A would spin on top of whatever else the click hit), and B leaves
local PAD_KEYS = {
    [input.CONTROLLER_BUTTON.LeftShoulder] = 'q',
    [input.CONTROLLER_BUTTON.RightShoulder] = 'e',
    [input.CONTROLLER_BUTTON.X] = 's',
    [input.CONTROLLER_BUTTON.Y] = 'f',
}

local function onPad(button)
    if not shown or I.UI.getMode() ~= 'Interface' then return end
    local symbol = PAD_KEYS[button]
    if symbol then
        padModule.log('slots', button, symbol)
        press(nil, symbol)
    end
end

-- The game stands still while this screen is up and the engine hands onFrame a step of 0 then: the time of the frame is the real one
local function onFrame()
    sync()
    if shown then stepFrame(core.getRealFrameDuration()) end
end

return {
    engineHandlers = {
        onFrame = onFrame,
        onKeyPress = function(key)
            press(key.code, key.symbol)
        end,
        onControllerButtonPress = onPad,
    },
    eventHandlers = {
        PadButton = function(data)
            onPad(data.button)
        end,
        -- However the screen is left (the button, Escape, a script), the player gets up from the machine
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyMenu()
                casino.close()
                shownSerial = nil
            end
        end,
    },
}
