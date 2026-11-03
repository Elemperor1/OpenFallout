-- The blackjack table of the casinos of New Vegas. The rules, the chips and the winnings are the engine's (core.casino); this
-- script draws the game's own menu file (black_jack_menu.xml: the bet, the chips, the casino, six buttons and a status text)
-- and the cards in the middle of it. The game's cards are 3D models on a table (a scene); here they are plain cards made of
-- tiles (a rank and a suit letter on a white ground, a blue back for the card that is face down), the dealer's above and the
-- player's hands below. The buttons are the game's: before the deal Deal, Increase Bet, Decrease Bet, Max Bet and Exit; in a
-- round Hit, Double Down, Split, Switch Hands, Surrender and Stay, each only when the rules allow it.
local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local padModule = require('scripts.omw.falloutpad')

local casino = core.casino
local pipboy = core.pipboy

if not casino.available() or not pipboy.available() then
    return {}
end

local FILE = 'black_jack_menu.xml'

-- Sizes in the units of the menu file
local CARD_W, CARD_H = 72, 100
local FONT = 7
local DEALER_Y, PLAYER_Y = 60, 290
local MAX_CARDS = 7

local COLOR = {
    face = { 238, 234, 220 },
    edge = { 30, 30, 30 },
    back = { 40, 70, 140 },
    black = { 20, 20, 20 },
    red = { 180, 30, 30 },
    text = { 255, 255, 255 },
    marker = { 235, 190, 60 },
}

local SOUND = {
    shuffle = 'GAMEBlackJackCardShuffle',
    deal = 'GAMEBlackJackCardDeal',
    flip = 'GAMEBlackJackCardFlip',
    collect = 'GAMEBlackJackCardCollect',
    win = 'GAMEBlackJackWin',
    lose = 'GAMEBlackJackLose',
    increase = 'GAMEBlackJackIncreaseBet',
    decrease = 'GAMEBlackJackDecreaseBet',
}

-- The buttons of the file, by name: what they are before the deal and in a round
local BUTTONS = {
    { name = 'BJM_Hit', round = 'hit' },
    { name = 'BJM_Double_Deal', round = 'double', bet = 'deal' },
    { name = 'BJM_Split_IncreaseBet', round = 'split', bet = 'increase' },
    { name = 'BJM_Switch_DecreaseBet', round = 'switch', bet = 'decrease' },
    { name = 'BJM_Surrender_MaxBet', round = 'surrender', bet = 'highest' },
    { name = 'BJM_Stay_Exit', round = 'stay', bet = 'exit' },
}

local shown = nil
local shownSerial = nil
local modeOpen = false
local cards = {}
local marker = nil
local betTiles = {}
local lastWidth = nil
local view = nil
-- Seconds until the table closes by itself (a round took the winnings to a new quarter of the casino's most), nil for none
local closeIn = nil
-- The result of the round stays on the screen this long before that happens (the cards are collected, then it closes)
local CLOSE_DELAY = 2.5

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
    shown, marker, view, lastWidth, closeIn = nil, nil, nil, nil, nil
    cards, betTiles = {}, {}
end

-- The player gets up from the table: a round that is not over is lost, and what it has more or less than when it sat down
-- goes into its pack
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

-- ---- Drawing --------------------------------------------------------------------------------------------------------

local function fill(parent, name, color, alpha)
    local m = shown.menu
    local tile = m:create(parent, 'hotrect', name)
    if not tile then return nil end
    m:set(tile, 'brightness', -1)
    m:set(tile, 'red', color[1])
    m:set(tile, 'green', color[2])
    m:set(tile, 'blue', color[3])
    m:set(tile, 'alpha', alpha or 255)
    m:set(tile, 'target', 0)
    m:set(tile, 'visible', 0)
    return tile
end

local function label(parent, name, color, justify)
    local m = shown.menu
    local tile = m:create(parent, 'text', name)
    if not tile then return nil end
    m:set(tile, 'font', FONT)
    m:set(tile, 'justify', justify or 2)
    m:set(tile, 'brightness', -1)
    m:set(tile, 'red', color[1])
    m:set(tile, 'green', color[2])
    m:set(tile, 'blue', color[3])
    m:set(tile, 'alpha', 255)
    m:set(tile, 'target', 0)
    m:set(tile, 'string', '')
    m:set(tile, 'visible', 0)
    return tile
end

local function lineHeight()
    local measured = shown.menu:measure(FONT, 'Ag', 400)
    return measured and measured.height or 30
end

local function drawTiles()
    local main = shown:find('BJM_MainRect')
    if not main then return false end
    -- The place of the active hand: a gold ground a little larger than its cards
    marker = fill(main, 'BlackjackMarker', COLOR.marker, 120)
    -- Seven cards for the dealer and for each of two hands
    for index = 1, 3 * MAX_CARDS do
        cards[index] = {
            edge = fill(main, 'BlackjackEdge' .. index, COLOR.edge),
            face = fill(main, 'BlackjackCard' .. index, COLOR.face),
            rank = label(main, 'BlackjackRank' .. index, COLOR.black, 1),
            suit = label(main, 'BlackjackSuit' .. index, COLOR.black, 1),
        }
    end
    for index = 1, 2 do
        betTiles[index] = label(main, 'BlackjackBet' .. index, COLOR.text)
    end
    return true
end

local RANKS = { '2', '3', '4', '5', '6', '7', '8', '9', '10', 'J', 'Q', 'K', 'A' }
local SUITS = { 'C', 'D', 'H', 'S' }

local function mainWidth()
    local main = shown:find('BJM_MainRect')
    local width = main and shown:get(main, 'width')
    return tonumber(width) or 1200
end

-- Puts a card in a slot of the pool, face up or down
local function placeCard(slot, x, y, card, down)
    local m = shown.menu
    local tiles = cards[slot]
    if not tiles then return end
    local color = down and COLOR.back or COLOR.face
    m:set(tiles.edge, 'x', x - 2)
    m:set(tiles.edge, 'y', y - 2)
    m:set(tiles.edge, 'width', CARD_W + 4)
    m:set(tiles.edge, 'height', CARD_H + 4)
    m:set(tiles.edge, 'visible', 1)
    m:set(tiles.face, 'x', x)
    m:set(tiles.face, 'y', y)
    m:set(tiles.face, 'width', CARD_W)
    m:set(tiles.face, 'height', CARD_H)
    m:set(tiles.face, 'red', color[1])
    m:set(tiles.face, 'green', color[2])
    m:set(tiles.face, 'blue', color[3])
    m:set(tiles.face, 'visible', 1)
    if down then
        m:set(tiles.rank, 'visible', 0)
        m:set(tiles.suit, 'visible', 0)
        return
    end
    local ink = (card.suit == 1 or card.suit == 2) and COLOR.red or COLOR.black
    for _, tile in ipairs({ tiles.rank, tiles.suit }) do
        m:set(tile, 'red', ink[1])
        m:set(tile, 'green', ink[2])
        m:set(tile, 'blue', ink[3])
        m:set(tile, 'x', x + 6)
        m:set(tile, 'visible', 1)
    end
    local height = lineHeight()
    m:set(tiles.rank, 'y', y + 6)
    m:set(tiles.rank, 'string', RANKS[card.rank + 1] or '?')
    m:set(tiles.suit, 'y', y + CARD_H - height - 6)
    m:set(tiles.suit, 'string', SUITS[card.suit + 1] or '?')
end

local function hideCards(from)
    local m = shown.menu
    for slot = from, #cards do
        m:set(cards[slot].edge, 'visible', 0)
        m:set(cards[slot].face, 'visible', 0)
        m:set(cards[slot].rank, 'visible', 0)
        m:set(cards[slot].suit, 'visible', 0)
    end
end

-- How far apart the cards of a hand are put: side by side while they fit in the room there is, then overlapping
local function stepFor(count, room)
    if count <= 1 then return CARD_W + 10 end
    return math.max(30, math.min(CARD_W + 10, math.floor((room - CARD_W) / (count - 1))))
end

-- Lays the dealer's cards and the player's hands on the table
local function drawTable(state)
    local m = shown.menu
    local centre = mainWidth() / 2
    local slot = 0
    local function widthOf(count, step) return (math.max(count, 1) - 1) * step + CARD_W end
    local function row(list, left, y, step, hideSecond)
        for index, card in ipairs(list) do
            slot = slot + 1
            placeCard(slot, left + (index - 1) * step, y, card, hideSecond and index == 2)
        end
    end
    local dealerStep = stepFor(#state.dealer, 520)
    row(state.dealer, centre - widthOf(#state.dealer, dealerStep) / 2, DEALER_Y, dealerStep, state.holeHidden)
    local split = #state.hands > 1
    local room = split and 330 or 520
    local lefts, steps = {}, {}
    for index, hand in ipairs(state.hands) do
        steps[index] = stepFor(#hand.cards, room)
        if split then
            lefts[index] = index == 1 and centre - 40 - room or centre + 40
        else
            lefts[index] = centre - widthOf(#hand.cards, steps[index]) / 2
        end
    end
    for index, hand in ipairs(state.hands) do
        row(hand.cards, lefts[index], PLAYER_Y, steps[index], false)
        local tile = betTiles[index]
        if tile then
            m:set(tile, 'x', lefts[index] + widthOf(#hand.cards, steps[index]) / 2)
            m:set(tile, 'y', PLAYER_Y + CARD_H + 8)
            m:set(tile, 'string', tostring(hand.bet))
            m:set(tile, 'visible', 1)
        end
    end
    for index = #state.hands + 1, 2 do
        m:set(betTiles[index], 'visible', 0)
    end
    hideCards(slot + 1)
    -- The hand that is played now, when there are two
    if state.state == 'playing' and split and marker then
        local width = widthOf(#state.hands[state.active].cards, steps[state.active])
        m:set(marker, 'x', lefts[state.active] - 8)
        m:set(marker, 'y', PLAYER_Y - 8)
        m:set(marker, 'width', width + 16)
        m:set(marker, 'height', CARD_H + 16)
        m:set(marker, 'visible', 1)
    else
        m:set(marker, 'visible', 0)
    end
end

-- ---- The numbers and the buttons of the menu ---------------------------------------------------------------------------

local function setDisplay(name, title, value)
    local tile = shown:find(name)
    if not tile then return end
    shown:set(tile, '_Value', title)
    shown:set(tile, '_X', tostring(value))
end

-- What a button says and whether it is there: before the deal and after a round the buttons of the bet, in a round those of
-- the play (a move the rules do not allow now has no button)
local function buttonState(state, button)
    local betting = state.state ~= 'playing'
    local action
    if betting then action = button.bet else action = button.round end
    if not action then return nil end
    if betting then
        if action == 'deal' then return word('sDealText', 'Deal'), state.canDeal and not state.broke end
        if action == 'increase' then return word('sIncreaseBetText', 'Increase Bet'), not state.broke end
        if action == 'decrease' then return word('sDecreaseBetText', 'Decrease Bet'), not state.broke end
        if action == 'highest' then return word('sBetMax', 'Max Bet'), not state.broke end
        return word('sExit', 'Exit'), true
    end
    local texts = {
        hit = { word('sHitText', 'Hit'), state.canHit },
        double = { word('sDoubleDownText', 'Double Down'), state.canDouble },
        split = { word('sSplitText', 'Split'), state.canSplit },
        switch = { word('sSwitchHandsText', 'Switch Hands'), state.canSwitch },
        surrender = { word('sSurrenderText', 'Surrender'), state.canSurrender },
        stay = { word('sStayText', 'Stay'), state.canStay },
    }
    local entry = texts[action]
    return entry[1], entry[2]
end

local function status(text)
    local tile = shown:find('BJM_StatusText')
    if not tile then return end
    shown:batch(function()
        shown:set(tile, 'string', text)
        shown:set(tile, 'visible', text ~= '' and 1 or 0)
    end)
end

-- What the round came to, in the words of the game: you win, you lose (or the lucky and unlucky ones when Luck helped) and
-- how many chips
local function resultText(state)
    if state.net > 0 then
        local text = state.playerHelped and word('sLuckyWinText', 'Lucky win!') or word('sYouWin', 'You win')
        return string.format('%s %s', text, chipsText(state.net))
    elseif state.net < 0 then
        local text = state.dealerHelped and word('sUnluckyLoseText', 'Unlucky loss.') or word('sYouLose', 'You lose')
        return string.format('%s %s', text, chipsText(-state.net))
    end
    return word('sYouBreakEvenText', 'You break even.')
end

local function refresh(state)
    view = state
    local screen = casino.screen()
    shown:batch(function()
        setDisplay('BJM_CurrentBetDisplay', word('sCurrentBetText', 'Current Bet:'), state.bet)
        setDisplay('BJM_ChipsDisplay', word('sChipCountText', 'Chips:'), state.chips)
        -- The earnings are named with the casino and the number itself is not shown (the executable hides it)
        setDisplay('BJM_CasinoNameDisplay',
            word('sCasinoEarningsText', 'Casino Earnings:') .. ' ' .. (screen and screen.name or ''), '')
        local earnings = shown:find('BJM_TotalEarningsValue')
        if earnings then shown:set(earnings, 'visible', 0) end
        for _, button in ipairs(BUTTONS) do
            local tile = shown:find(button.name)
            local text, available = buttonState(state, button)
            if tile then
                if text == nil then
                    shown:set(tile, 'visible', 0)
                else
                    shown:set(tile, 'string', text)
                    shown:set(tile, 'visible', available and 1 or 0)
                end
            end
        end
        drawTable(state)
    end)
    if state.closes and closeIn == nil then closeIn = CLOSE_DELAY end
    if state.state == 'finished' and #state.hands > 0 then
        status(state.broke and word('sGamblingBrokeText', 'You do not have enough chips.') or resultText(state))
    elseif state.state == 'betting' and state.broke then
        status(word('sGamblingBrokeText', 'You do not have enough chips.'))
    else
        status('')
    end
end

-- ---- What the player does ---------------------------------------------------------------------------------------------------

-- One thing at the table: the engine does it (or says it cannot be done), and the sounds and the screen follow
local function act(action)
    local before = casino.blackjack.state()
    if not before then return end
    -- The table is closing by itself: only leaving is allowed
    if closeIn ~= nil and action ~= 'exit' then return end
    if action == 'exit' then
        leave()
        return
    end
    local function stateOf() return casino.blackjack.state() end
    if action == 'deal' and before.state == 'finished' and #before.hands > 0 then
        sound(SOUND.collect)
    end
    local done = casino.blackjack[action]()
    local after = stateOf()
    if not after then return end
    if done then
        if action == 'increase' or action == 'highest' then
            sound(SOUND.increase)
        elseif action == 'decrease' then
            sound(SOUND.decrease)
        elseif action == 'deal' or action == 'hit' or action == 'double' or action == 'split' then
            sound(SOUND.deal)
        end
        -- The dealer's card is turned and the result comes when the round is over
        if after.state == 'finished' and before.state ~= 'finished' then
            sound(SOUND.flip)
            sound(after.net > 0 and SOUND.win or after.net < 0 and SOUND.lose or SOUND.flip)
        end
    end
    refresh(after)
end

local function openMenu()
    local opened = menuModule.open(FILE)
    if not opened then return false end
    shown = opened
    if not drawTiles() then
        destroyMenu()
        return false
    end
    for _, button in ipairs(BUTTONS) do
        shown:onClick(button.name, function()
            local state = casino.blackjack.state()
            if not state then return end
            local action
            if state.state == 'playing' then action = button.round else action = button.bet end
            if action then act(action) end
        end)
    end
    return true
end

local function sync()
    local screen = casino.screen()
    if not screen or screen.game ~= 'blackjack' then
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
        if not openMenu() then
            print('blackjack: the file ' .. FILE .. ' is not there; the table is not shown')
            casino.close()
            shownSerial = nil
            closeMode()
            return
        end
        sound(SOUND.shuffle)
        local state = casino.blackjack.state()
        if state then refresh(state) end
    end
    if shown then
        shown:checkSize()
        -- The window can change its size: the cards are laid again
        local state = casino.blackjack.state()
        if state and view and mainWidth() ~= lastWidth then
            lastWidth = mainWidth()
            drawTable(state)
        end
    end
end

local function press(code, symbol)
    if not shown or I.UI.getMode() ~= 'Interface' then return end
    if symbol ~= nil and symbol ~= '' then
        shown:pcButton(symbol)
    end
end

-- On the pad the shoulders and X and Y stand for the keys of the buttons; the engine clicks with A (where the pointer is) and
-- leaves with B
local PAD_KEYS = {
    [input.CONTROLLER_BUTTON.LeftShoulder] = 'q',
    [input.CONTROLLER_BUTTON.RightShoulder] = 'e',
    [input.CONTROLLER_BUTTON.X] = 's',
    [input.CONTROLLER_BUTTON.Y] = 'w',
}

local function onPad(button)
    if not shown or I.UI.getMode() ~= 'Interface' then return end
    local symbol = PAD_KEYS[button]
    if symbol then
        padModule.log('blackjack', button, symbol)
        press(nil, symbol)
    end
end

local function onFrame(dt)
    sync()
    if closeIn ~= nil and shown then
        closeIn = closeIn - (dt or 0)
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
