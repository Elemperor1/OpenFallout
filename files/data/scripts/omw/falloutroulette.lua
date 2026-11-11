-- The roulette table of the casinos of New Vegas. The rules, the chips and the winnings are the engine's (core.casino); this
-- script draws the game's own menu file (roulette_menu.xml: what the player has bet, the chips, the casino and the buttons)
-- and a betting cloth in the middle of it, which stands in for the 3D table of the game (a scene that the pointer picks the
-- bets from: the executable's 159 places, which are laid out here by their numbers). A click on the cloth puts the amount of the
-- bet on the place under the pointer; Place Bet and Remove Bet (and the keys W and F) do the same to the place the pointer
-- is over. The wheel is not drawn: the pocket that came up is written beside the cloth.
local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local padModule = require('scripts.omw.falloutpad')
local board = require('scripts.omw.falloutroulettetable')

local casino = core.casino
local pipboy = core.pipboy

if not casino.available() or not pipboy.available() then
    return {}
end

local FILE = 'roulette_menu.xml'

-- Sizes in the units of the menu file. The cloth starts at (BOARD_X, BOARD_Y) with the first number; the strips of the
-- bets on the outside are on its left, the zeroes above it and the three columns bets below.
local CELL_W, CELL_H = 100, 40
local BOARD_X, BOARD_Y = 340, 110
local FONT = 7
local CHIP_SIZE = 30

local COLOR = {
    felt = { 12, 60, 32 },
    red = { 170, 30, 30 },
    black = { 22, 22, 22 },
    green = { 24, 120, 56 },
    outside = { 30, 84, 50 },
    chip = { 235, 190, 60 },
    chipText = { 20, 20, 20 },
    text = { 255, 255, 255 },
    hover = { 255, 255, 255 },
}

local SOUND = {
    valid = 'GAMERouletteValidBet',
    invalid = 'GAMERouletteInvalidBet',
    increase = 'GAMERouletteIncreaseBet',
    decrease = 'GAMERouletteDecreaseBet',
    spin = 'GAMERouletteSpin',
    win = 'GAMERouletteWin',
}

local shown = nil
local shownSerial = nil
local modeOpen = false
local areas, lookup = nil, nil
local boardTile = nil
local hoverTile = nil
local chipTiles = {}
local resultTiles = {}
local colorOf = {}
local history = {}
local hovered = nil
local placed = nil
local lastView = nil
-- Seconds until the table closes by itself after a spin (it took the winnings to a new quarter of the casino's most, or the
-- chips are fewer than the least bet), nil for none; the broke player is told so when that is half over
local closeIn = nil
local brokeNote = false
local CLOSE_DELAY = 3

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
    shown, boardTile, hoverTile, hovered, placed, lastView, closeIn, brokeNote = nil, nil, nil, nil, nil, nil, nil, false
    chipTiles, resultTiles = {}, {}
end

-- The player gets up from the table: what it has more or less than when it sat down goes into its pack
local function leave()
    destroyMenu()
    casino.close()
    shownSerial = nil
    closeMode()
end

local function pocketText(pocket)
    return pocket == 37 and '00' or tostring(pocket)
end

local function chipsText(count)
    local chip = word('sCasinoChipText', 'Chip')
    return string.format('%d %s%s', count, chip, count == 1 and '' or word('sPlural', 's'))
end

-- ---- Drawing --------------------------------------------------------------------------------------------------------

local function fill(parent, name, x, y, width, height, color, alpha)
    local m = shown.menu
    local tile = m:create(parent, 'hotrect', name)
    if not tile then return nil end
    m:set(tile, 'x', x)
    m:set(tile, 'y', y)
    m:set(tile, 'width', width)
    m:set(tile, 'height', height)
    m:set(tile, 'brightness', -1)
    m:set(tile, 'red', color[1])
    m:set(tile, 'green', color[2])
    m:set(tile, 'blue', color[3])
    m:set(tile, 'alpha', alpha or 255)
    -- Only the cloth takes the pointer
    m:set(tile, 'target', 0)
    return tile
end

local function label(parent, name, x, y, text, color, justify)
    local m = shown.menu
    local tile = m:create(parent, 'text', name)
    if not tile then return nil end
    m:set(tile, 'font', FONT)
    m:set(tile, 'justify', justify or 2)
    m:set(tile, 'x', x)
    m:set(tile, 'y', y)
    m:set(tile, 'brightness', -1)
    m:set(tile, 'red', color[1])
    m:set(tile, 'green', color[2])
    m:set(tile, 'blue', color[3])
    m:set(tile, 'alpha', 255)
    m:set(tile, 'target', 0)
    m:set(tile, 'string', text)
    return tile
end

-- The size of a line of the font, to put a text in the middle of a cell
local function lineHeight()
    local measured = shown.menu:measure(FONT, 'Ag', 400)
    return measured and measured.height or 30
end

local function cellX(x) return BOARD_X + x * CELL_W end
local function cellY(y) return BOARD_Y + y * CELL_H end

local function drawBoard()
    local main = shown:find('ROM_MainRect')
    if not main then return false end
    local rows = casino.roulette.wheel()
    for _, row in ipairs(rows) do
        colorOf[row.pocket] = row.red and COLOR.red or row.black and COLOR.black or COLOR.green
    end
    local height = lineHeight()
    -- The felt under the whole cloth
    fill(main, 'RouletteFelt', cellX(-2) - 6, cellY(-1) - 6, 5 * CELL_W + 12, 14 * CELL_H + 12, COLOR.felt)
    local function cell(name, x, y, w, h, color, text)
        fill(main, name, cellX(x) + 1, cellY(y) + 1, w * CELL_W - 2, h * CELL_H - 2, color)
        if text then
            label(main, name .. 'Text', cellX(x) + w * CELL_W / 2, cellY(y) + (h * CELL_H - height) / 2, text, COLOR.text)
        end
    end
    -- The zeroes, then the numbers
    cell('Pocket0', 0, -1, 1.5, 1, colorOf[0], '0')
    cell('Pocket37', 1.5, -1, 1.5, 1, colorOf[37], '00')
    for pocket = 1, 36 do
        local row, column = math.floor((pocket - 1) / 3), (pocket - 1) % 3
        cell('Pocket' .. pocket, column, row, 1, 1, colorOf[pocket], tostring(pocket))
    end
    -- The three columns bets, the dozens and the bets on even money
    local twoToOne = word('sTwoToOneText', '2 to 1')
    for index = 1, 3 do
        cell('Column' .. index, index - 1, 12, 1, 1, COLOR.outside, twoToOne)
    end
    local dozens = {
        word('sFirstDozenText', '1st 12'), word('sSecondDozenText', '2nd 12'), word('sThirdDozenText', '3rd 12'),
    }
    for index = 1, 3 do
        cell('Dozen' .. index, -1, (index - 1) * 4, 1, 4, COLOR.outside, dozens[index])
    end
    local evenMoney = {
        { text = '1-18' }, { text = word('sEvenText', 'Even') }, { text = word('sRedText', 'Red'), color = COLOR.red },
        { text = word('sBlackText', 'Black'), color = COLOR.black }, { text = word('sOddtext', 'Odd') },
        { text = '19-36' },
    }
    for index, bet in ipairs(evenMoney) do
        cell('EvenMoney' .. index, -2, (index - 1) * 2, 1, 2, bet.color or COLOR.outside, bet.text)
    end
    -- The light over the place under the pointer, over the cloth and the chips
    hoverTile = fill(main, 'RouletteHover', 0, 0, 0, 0, COLOR.hover, 70)
    for index = 1, 10 do
        local chip = fill(main, 'RouletteChip' .. index, 0, 0, CHIP_SIZE, CHIP_SIZE, COLOR.chip)
        local text = label(main, 'RouletteChipText' .. index, 0, 0, '', COLOR.chipText)
        shown.menu:set(chip, 'visible', 0)
        shown.menu:set(text, 'visible', 0)
        chipTiles[index] = { chip = chip, text = text }
    end
    -- What came up, beside the cloth
    resultTiles.title = label(main, 'RouletteResultTitle', cellX(4) + 40, cellY(0), '', COLOR.text, 1)
    resultTiles.pocket = label(main, 'RouletteResult', cellX(4) + 40, cellY(1.4), '', COLOR.text, 1)
    resultTiles.history = label(main, 'RouletteHistory', cellX(4) + 40, cellY(4), '', COLOR.text, 1)
    -- The cloth takes the pointer: it is on top of the cells, and what is under the pointer is worked out from where it is
    boardTile = shown.menu:create(main, 'hotrect', 'RouletteBoard')
    shown.menu:set(boardTile, 'x', cellX(-2))
    shown.menu:set(boardTile, 'y', cellY(-1))
    shown.menu:set(boardTile, 'width', 5 * CELL_W)
    shown.menu:set(boardTile, 'height', 14 * CELL_H)
    shown.menu:set(boardTile, 'alpha', 0)
    shown.menu:set(boardTile, 'target', 1)
    return true
end

-- ---- What is under the pointer ----------------------------------------------------------------------------------------

-- The place on the cloth (in cells) that the pointer is on, nil when it has not been in the window yet
local function pointerOnCloth()
    local at = shown:pointerAt()
    local rect = boardTile and shown:rect(boardTile)
    if not at or not rect or rect.width <= 0 or rect.height <= 0 then return nil end
    local x = -2 + (at.x - rect.x) / rect.width * 5
    local y = -1 + (at.y - rect.y) / rect.height * 14
    return x, y
end

-- The area (the place in the list of the engine) that the pointer is over
local function areaUnderPointer()
    local x, y = pointerOnCloth()
    if not x then return nil end
    return lookup.find(board.hit(x, y))
end

local function showHover(area)
    if area == hovered then return end
    hovered = area
    local m = shown.menu
    if not area then
        m:set(hoverTile, 'visible', 0)
        return
    end
    local x, y, w, h = board.rectOf(areas[area])
    m:set(hoverTile, 'x', cellX(x))
    m:set(hoverTile, 'y', cellY(y))
    m:set(hoverTile, 'width', w * CELL_W)
    m:set(hoverTile, 'height', h * CELL_H)
    m:set(hoverTile, 'visible', 1)
    shown:refresh()
end

-- ---- The numbers of the menu ---------------------------------------------------------------------------------------------

local function setDisplay(name, title, value)
    local tile = shown:find(name)
    if not tile then return end
    shown:set(tile, '_Value', title)
    shown:set(tile, '_X', tostring(value))
end

local function drawChips(view)
    local m = shown.menu
    local used = 0
    -- The stakes are a table from the place of an area (in the list the engine gives) to what is bet on it
    local places = {}
    for place in pairs(view.stakes) do places[#places + 1] = place end
    table.sort(places)
    for _, place in ipairs(places) do
        used = used + 1
        local tiles = chipTiles[used]
        if not tiles then break end
        local cx, cy = board.centerOf(areas[place])
        m:set(tiles.chip, 'x', cellX(cx) - CHIP_SIZE / 2)
        m:set(tiles.chip, 'y', cellY(cy) - CHIP_SIZE / 2)
        m:set(tiles.chip, 'visible', 1)
        m:set(tiles.text, 'x', cellX(cx))
        m:set(tiles.text, 'y', cellY(cy) - lineHeight() / 2)
        m:set(tiles.text, 'string', tostring(view.stakes[place]))
        m:set(tiles.text, 'visible', 1)
    end
    for index = used + 1, #chipTiles do
        m:set(chipTiles[index].chip, 'visible', 0)
        m:set(chipTiles[index].text, 'visible', 0)
    end
end

local function fillAll()
    local view = casino.roulette.state()
    local screen = casino.screen()
    if not view or not screen then return end
    lastView = view
    shown:batch(function()
        setDisplay('ROM_CurrentBetDisplay', word('sCurrentBetText', 'Current Bet:'), view.amount)
        setDisplay('ROM_TotalBetDisplay', word('sTotalBetText', 'Total Bet:'), view.total)
        setDisplay('ROM_ChipsDisplay', word('sChipCountText', 'Chips:'), view.chips)
        -- The earnings are named with the casino and the number itself is not shown (the executable hides it)
        setDisplay('ROM_CasinoNameDisplay', word('sCasinoEarningsText', 'Casino Earnings:') .. ' ' .. screen.name, '')
        local earnings = shown:find('ROM_TotalEarningsValue')
        if earnings then shown:set(earnings, 'visible', 0) end
        drawChips(view)
    end)
end

local function setButtons()
    local labels = {
        ROM_PlaceBet = word('sPlaceBetText', 'Place Bet'),
        ROM_RemoveBet = word('sRemoveBetText', 'Remove Bet'),
        ROM_FinishBet = word('sFinsihBetText', 'Finish Bets'),
        ROM_IncreaseBet = word('sIncreaseBetText', 'Increase Bet'),
        ROM_DecreaseBet = word('sDecreaseBetText', 'Decrease Bet'),
    }
    shown:batch(function()
        for name, text in pairs(labels) do
            local tile = shown:find(name)
            if tile then shown:set(tile, 'string', text) end
        end
    end)
end

-- ---- What the player does ---------------------------------------------------------------------------------------------------

local function status(text)
    local tile = shown:find('ROM_StatusText')
    if not tile then return end
    shown:batch(function()
        shown:set(tile, 'string', text)
        shown:set(tile, 'visible', text ~= '' and 1 or 0)
    end)
end

local function place(area)
    if closeIn ~= nil then return end
    -- A place that has a stake takes no more, and the game says nothing about it (0x7BED70)
    if area and lastView and lastView.stakes[area] then return end
    if area and casino.roulette.place(area) then
        sound(SOUND.valid)
        status('')
        fillAll()
    else
        sound(SOUND.invalid)
    end
end

local function remove(area)
    if closeIn ~= nil then return end
    -- Where nothing is bet there is nothing to take off, and no sound (0x7BF150)
    if area and lastView and not lastView.stakes[area] then return end
    if area and casino.roulette.remove(area) then
        sound(SOUND.valid)
        fillAll()
    else
        sound(SOUND.invalid)
    end
end

local function changeAmount(increase)
    if closeIn ~= nil then return end
    -- (not `increase and a() or b()`: a call that changes nothing would then be followed by the opposite one)
    local changed
    if increase then
        changed = casino.roulette.increase()
    else
        changed = casino.roulette.decrease()
    end
    if changed then sound(increase and SOUND.increase or SOUND.decrease) end
    fillAll()
end

-- What the spin was, in the words of the game: the pocket, and what the player won or lost
local function resultText(result)
    local text
    if result.net > 0 then
        text = result.lucky and word('sLuckyWinText', 'Lucky win!') or word('sYouWin', 'You win')
    elseif result.net < 0 then
        text = result.unlucky and word('sUnluckyLoseText', 'Unlucky loss.') or word('sYouLose', 'You lose')
    else
        return word('sYouBreakEvenText', 'You break even.')
    end
    return string.format('%s %s', text, chipsText(math.abs(result.net)))
end

local function spin()
    if closeIn ~= nil then return end
    local result = casino.roulette.spin()
    if not result then
        sound(SOUND.invalid)
        return
    end
    sound(SOUND.spin)
    table.insert(history, 1, result.pocket)
    while #history > 10 do table.remove(history) end
    local m = shown.menu
    local colorName = colorOf[result.pocket] == COLOR.red and word('sRedText', 'Red')
        or colorOf[result.pocket] == COLOR.black and word('sBlackText', 'Black') or ''
    local pocketColor = colorOf[result.pocket]
    local lines = {}
    for index, pocket in ipairs(history) do
        if index > 1 then lines[#lines + 1] = pocketText(pocket) end
    end
    shown:batch(function()
        m:set(resultTiles.title, 'string', colorName)
        m:set(resultTiles.pocket, 'string', pocketText(result.pocket))
        m:set(resultTiles.pocket, 'red', pocketColor == COLOR.black and 200 or pocketColor[1] + 60)
        m:set(resultTiles.pocket, 'green', pocketColor == COLOR.black and 200 or pocketColor[2] + 60)
        m:set(resultTiles.pocket, 'blue', pocketColor == COLOR.black and 200 or pocketColor[3] + 60)
        m:set(resultTiles.history, 'string', table.concat(lines, ' '))
        status(resultText(result))
    end)
    if result.net > 0 then sound(SOUND.win) end
    fillAll()
    -- The game closes the table after a spin that took the winnings to a new quarter, and when the chips that are left are
    -- fewer than the least bet (and says so)
    local view = casino.roulette.state()
    if view and view.closes then
        closeIn = CLOSE_DELAY
        brokeNote = view.broke == true
    end
end

local function openMenu()
    local opened = menuModule.open(FILE)
    if not opened then return false end
    shown = opened
    -- No row here is picked by the D-pad: A is the click at the pointer, whatever an earlier screen held
    padModule.releasePick()
    areas = casino.roulette.areas()
    lookup = board.index(areas)
    if not drawBoard() then
        destroyMenu()
        return false
    end
    shown:onClick('RouletteBoard', function()
        place(areaUnderPointer())
    end)
    shown:onClick('ROM_PlaceBet', function() place(hovered or areaUnderPointer()) end)
    shown:onClick('ROM_RemoveBet', function() remove(hovered or areaUnderPointer()) end)
    shown:onClick('ROM_FinishBet', spin)
    shown:onClick('ROM_IncreaseBet', function() changeAmount(true) end)
    shown:onClick('ROM_DecreaseBet', function() changeAmount(false) end)
    shown:onClick('ROM_ExitButton', leave)
    setButtons()
    status('')
    return true
end

local function sync()
    local screen = casino.screen()
    if not screen or screen.game ~= 'roulette' then
        if shown or modeOpen then
            destroyMenu()
            closeMode()
            shownSerial = nil
        end
        return
    end
    if screen.serial ~= shownSerial then
        shownSerial = screen.serial
        history = {}
        destroyMenu()
        openMode()
        if not openMenu() then
            print('roulette: the file ' .. FILE .. ' is not there; the table is not shown')
            casino.close()
            shownSerial = nil
            closeMode()
            return
        end
        fillAll()
    end
    if shown then
        shown:checkSize()
        showHover(areaUnderPointer())
    end
end

local function press(code, symbol)
    if not shown or I.UI.getMode() ~= 'Interface' then return end
    if symbol ~= nil and symbol ~= '' then
        shown:pcButton(symbol)
    end
end

-- On the pad the shoulders stand for the triggers (down and up with the bet), X finishes the bets and Y takes a bet off;
-- the engine clicks with A (where the pointer is) and leaves with B
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
        padModule.log('roulette', button, symbol)
        press(nil, symbol)
    end
end

-- The game stands still while this screen is up and the engine hands onFrame a step of 0 then: the time of the frame is the real one
local function onFrame()
    sync()
    if closeIn ~= nil and shown then
        local before = closeIn
        closeIn = closeIn - core.getRealFrameDuration()
        if brokeNote and before > CLOSE_DELAY / 2 and closeIn <= CLOSE_DELAY / 2 then
            brokeNote = false
            status(word('sGamblingBrokeText', 'You do not have enough chips.'))
        end
        if closeIn <= 0 then leave() end
    end
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
        -- However the screen is left (the button, Escape, a script), the player gets up from the table
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
