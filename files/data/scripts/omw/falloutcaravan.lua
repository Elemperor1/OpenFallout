-- The card game Caravan of New Vegas. The rules, the cards, the opponent and the money are the engine's (core.casino.caravan);
-- this script draws the four screens of the game's CaravanMenu (caravan_menu.xml, which includes caravan_bet_menu.xml,
-- caravan_builddeck_menu.xml, caravan_game_menu.xml and caravan_results_menu.xml): the stake, the deck, the match and its result.
--
-- The stake is the game's own file: the opponent's ante, the player's ante and funds, and the buttons Auto-Match, Raise (the
-- quantity prompt, as the game asks "how many") and Accept. The file of the deck gives the two counts, the two buttons that pick
-- the list (the caravan deck above, all the cards below) and Add, Remove and Play Caravan; its cards are 3D in the game and plain
-- tiles here. The files of the match and of the result hold nothing ("all controls will be handled in code"), so their controls
-- are the tiles of this script: the six caravans as columns of rows (the opponent's above the player's, the player's n under
-- the opponent's 7 - n), the eight cards of the hand below, the totals, and the buttons that throw a card or a caravan away and
-- give the match up. The words on the tiles of the match are not the game's: the executable's text for them was not read.
local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local padModule = require('scripts.omw.falloutpad')
local quantity = require('scripts.omw.falloutquantity')

local casino = core.casino
local pipboy = core.pipboy

if not casino.available() or not pipboy.available() then
    return {}
end

local caravan = casino.caravan
local FILE = 'caravan_menu.xml'
local RECTS = { bet = 'CBM_MainRect', deck = 'CDM_MainRect', game = 'CDG_MainRect', results = 'CDR_MainRect' }

local FONT = 7
local CARD_W, CARD_H = 72, 100
local ROWS, TRACKS = 7, 6
-- Seconds between the end of the match and its result, and between the player's move and the opponent's
local RESULT_DELAY = 2.2
local OPPONENT_DELAY = 1.0
local MARK_TIME = 1.6

local COLOR = {
    face = { 238, 234, 220 },
    edge = { 30, 30, 30 },
    back = { 40, 70, 140 },
    black = { 20, 20, 20 },
    red = { 180, 30, 30 },
    text = { 255, 255, 255 },
    picked = { 235, 190, 60 },
    inDeck = { 70, 160, 90 },
    empty = { 90, 90, 90 },
    sold = { 120, 220, 120 },
    over = { 230, 90, 80 },
    button = { 60, 60, 60 },
}

local shown = nil
local shownSerial = nil
local modeOpen = false
local drawn = nil
local lastWidth = nil

-- The tiles that are made once, in the files' rects: the cards of the deck screen and of the match, the labels and the buttons
local deckTiles = nil
local gameTiles = nil
local resultTiles = nil

-- The deck screen: which list the cursor is in ('deck' or 'all'), where in each, and the first card of each that is shown
local activeList = 'all'
local cursor = { deck = 1, all = 1 }
local firstShown = { deck = 1, all = 1 }
local deckIndexes = { deck = {}, all = {} }

-- The match: the card of the hand that is picked up, whether a caravan is to be thrown away, whether giving up is asked
-- for once already, the seconds the opponent waits, and what it did last (to light it)
local pickedHand = nil
local discarding = false
local forfeitAsked = false
local opponentWait = 0
local lastMove = nil
local markLeft = 0
local statusText = ''
-- The seconds left before the result is shown
local resultIn = nil

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
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
    quantity.close()
    if shown then
        shown:close()
    end
    shown, drawn, lastWidth = nil, nil, nil
    deckTiles, gameTiles, resultTiles = nil, nil, nil
    activeList, cursor, firstShown = 'all', { deck = 1, all = 1 }, { deck = 1, all = 1 }
    pickedHand, discarding, forfeitAsked, lastMove, resultIn = nil, false, false, nil, nil
    opponentWait, markLeft, statusText = 0, 0, ''
end

-- The player gets up: a match that is not over is lost, and the money has moved by then
local function leave()
    destroyMenu()
    caravan.close()
    shownSerial = nil
    closeMode()
end

-- ---- Tiles ----------------------------------------------------------------------------------------------------------------

local function fill(parent, name, color, alpha, clickable)
    local m = shown.menu
    local tile = m:create(parent, 'hotrect', name)
    if not tile then return nil end
    m:set(tile, 'brightness', -1)
    m:set(tile, 'red', color[1])
    m:set(tile, 'green', color[2])
    m:set(tile, 'blue', color[3])
    m:set(tile, 'alpha', alpha or 255)
    m:set(tile, 'target', clickable and 1 or 0)
    m:set(tile, 'visible', 0)
    return tile
end

local function label(parent, name, color, justify)
    local m = shown.menu
    local tile = m:create(parent, 'text', name)
    if not tile then return nil end
    m:set(tile, 'font', FONT)
    m:set(tile, 'justify', justify or 1)
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

local function rectSize(name)
    local rect = shown:find(name)
    local width = rect and tonumber(shown:get(rect, 'width'))
    local height = rect and tonumber(shown:get(rect, 'height'))
    return width or 1200, height or 700
end

local function put(tile, x, y, width, height)
    local m = shown.menu
    m:set(tile, 'x', x)
    m:set(tile, 'y', y)
    if width then m:set(tile, 'width', width) end
    if height then m:set(tile, 'height', height) end
    m:set(tile, 'visible', 1)
end

local function hide(tile)
    shown.menu:set(tile, 'visible', 0)
end

local function setText(tile, text, color)
    local m = shown.menu
    m:set(tile, 'string', text)
    if color then
        m:set(tile, 'red', color[1])
        m:set(tile, 'green', color[2])
        m:set(tile, 'blue', color[3])
    end
end

local function setColor(tile, color)
    local m = shown.menu
    m:set(tile, 'red', color[1])
    m:set(tile, 'green', color[2])
    m:set(tile, 'blue', color[3])
end

-- A card slot: the edge, the face (which takes the pointer) and the words on it
local function makeSlot(parent, prefix, index, onClick)
    local slot = {
        edge = fill(parent, prefix .. 'Edge' .. index, COLOR.edge),
        face = fill(parent, prefix .. 'Card' .. index, COLOR.face, 255, true),
        text = label(parent, prefix .. 'Text' .. index, COLOR.black, 1),
    }
    if onClick then
        shown:onClick(prefix .. 'Card' .. index, function() onClick(index) end)
    end
    return slot
end

local function hideSlot(slot)
    hide(slot.edge)
    hide(slot.face)
    hide(slot.text)
end

-- A card in a slot: where, how big, the words (and the ink of the suit), the ground and the edge
local function drawSlot(slot, x, y, width, height, text, ink, ground, edge)
    put(slot.edge, x - 2, y - 2, width + 4, height + 4)
    setColor(slot.edge, edge or COLOR.edge)
    put(slot.face, x, y, width, height)
    setColor(slot.face, ground or COLOR.face)
    if text and text ~= '' then
        put(slot.text, x + 4, y + 2)
        setText(slot.text, text, ink or COLOR.black)
    else
        hide(slot.text)
    end
end

local function button(parent, name, text, onClick)
    local tile = {
        edge = fill(parent, name .. 'Edge', COLOR.edge),
        face = fill(parent, name, COLOR.button, 255, true),
        text = label(parent, name .. 'Text', COLOR.text, 2),
    }
    shown:onClick(name, onClick)
    tile.string = text
    return tile
end

local function drawButton(tile, x, y, width, height, text)
    put(tile.edge, x - 2, y - 2, width + 4, height + 4)
    put(tile.face, x, y, width, height)
    put(tile.text, x + width / 2, y + (height - lineHeight()) / 2)
    setText(tile.text, text or tile.string)
end

local function hideButton(tile)
    hide(tile.edge)
    hide(tile.face)
    hide(tile.text)
end

-- ---- Cards ----------------------------------------------------------------------------------------------------------------

local RANKS = { [1] = 'A', [10] = '10', [12] = 'J', [13] = 'Q', [14] = 'K', [15] = 'Jk' }
local SUITS = { 'C', 'D', 'H', 'S', '*' }

local function rankOf(value)
    return RANKS[value] or tostring(value)
end

local function cardText(card)
    return rankOf(card.value) .. (SUITS[card.suit] or '?')
end

local function inkOf(card)
    return (card.suit == 2 or card.suit == 3) and COLOR.red or COLOR.black
end

-- ---- The stake --------------------------------------------------------------------------------------------------------------

local function setDisplay(name, title, value)
    local tile = shown:find(name)
    if not tile then return end
    if title then shown:set(tile, 'string', title) end
    shown:set(tile, '_Value', tostring(value))
end

local function setButton(name, text, visible)
    local tile = shown:find(name)
    if not tile then return end
    if text then shown:set(tile, 'string', text) end
    shown:set(tile, 'visible', visible and 1 or 0)
end

local function drawBet(screen)
    shown:batch(function()
        setDisplay('CBM_NPCBetDisplay', 'Opponent\'s Current Ante: ', screen.ante)
        setDisplay('CBM_PlayerBetDisplay', nil, screen.bet)
        setDisplay('CBM_TotalFundsDisplay', nil, screen.funds)
        setButton('CBM_AutoMatchButton', nil, screen.canMatch)
        setButton('CBM_RaiseButton', nil, screen.mostRaise > 0)
        setButton('CBM_AcceptButton', nil, screen.canAccept)
        setButton('CBM_ExitButton', nil, true)
    end)
end

local function raise()
    local screen = caravan.screen()
    if not screen or screen.stage ~= 'bet' or screen.mostRaise <= 0 then return end
    quantity.ask(screen.mostRaise, function(count)
        if caravan.raise(count) then
            local after = caravan.screen()
            if after and after.stage == 'bet' then drawBet(after) end
        end
    end)
end

-- ---- The deck ---------------------------------------------------------------------------------------------------------------

local function visibleCount(width)
    return math.max(3, math.min(14, math.floor((width - 80) / (CARD_W + 8))))
end

local function onDeckCard(slotIndex)
    -- Slots 1 to count are the deck strip's, those after them the strip of all the cards
    local count = deckTiles.count
    local list = slotIndex <= count and 'deck' or 'all'
    local position = (list == 'deck' and slotIndex or slotIndex - count) + firstShown[list] - 1
    if position > #deckIndexes[list] then return end
    activeList = list
    cursor[list] = position
    deckTiles.redraw()
end

local function makeDeckTiles()
    local main = shown:find(RECTS.deck)
    if not main then return nil end
    local width = rectSize(RECTS.deck)
    local count = visibleCount(width)
    local tiles = { count = count, slots = {} }
    for index = 1, 2 * count do
        tiles.slots[index] = makeSlot(main, 'CaravanDeck', index, onDeckCard)
    end
    return tiles
end

local function currentCard()
    local cards = caravan.cards()
    local list = deckIndexes[activeList]
    local index = list[cursor[activeList]]
    return index, index and cards[index], cards
end

local function drawDeck()
    local screen = caravan.screen()
    if not screen or screen.stage ~= 'deck' then return end
    local cards = caravan.cards()
    deckIndexes = { deck = {}, all = {} }
    for index, card in ipairs(cards) do
        deckIndexes.all[#deckIndexes.all + 1] = index
        if card.inDeck then deckIndexes.deck[#deckIndexes.deck + 1] = index end
    end
    -- The cursor stays on a card of its list
    for _, list in ipairs({ 'deck', 'all' }) do
        cursor[list] = math.max(1, math.min(cursor[list], math.max(#deckIndexes[list], 1)))
    end
    if #deckIndexes.deck == 0 then activeList = 'all' end
    local width, height = rectSize(RECTS.deck)
    -- The slots that were made, and how many of them the window has room for now (a narrower window hides the rest)
    local made = deckTiles.count
    local count = math.min(made, visibleCount(width))
    local middle = height / 2
    local lineH = lineHeight()
    local left = (width - (count * (CARD_W + 8) - 8)) / 2
    local deckY = math.max(10, middle - 25 - 75 - 20 - CARD_H)
    local allY = middle - 25 + 75 + lineH + 20
    shown:batch(function()
        for _, list in ipairs({ 'deck', 'all' }) do
            local indexes = deckIndexes[list]
            -- The strip follows the cursor
            if cursor[list] < firstShown[list] then firstShown[list] = cursor[list] end
            if cursor[list] > firstShown[list] + count - 1 then firstShown[list] = cursor[list] - count + 1 end
            firstShown[list] = math.max(1, math.min(firstShown[list], math.max(#indexes - count + 1, 1)))
            local y = list == 'deck' and deckY or allY
            local base = list == 'deck' and 0 or made
            for slot = 1, made do
                local tiles = deckTiles.slots[base + slot]
                local position = firstShown[list] + slot - 1
                local cardIndex = indexes[position]
                if slot > count then
                    hideSlot(tiles)
                elseif cardIndex then
                    local card = cards[cardIndex]
                    local edge = COLOR.edge
                    if card.inDeck then edge = COLOR.inDeck end
                    if list == activeList and position == cursor[list] then edge = COLOR.picked end
                    drawSlot(tiles, left + (slot - 1) * (CARD_W + 8), y, CARD_W, CARD_H, cardText(card), inkOf(card), nil, edge)
                else
                    hideSlot(tiles)
                end
            end
        end
        setDisplay('CDM_CardsInDeckDisplay', nil, screen.inDeck)
        setDisplay('CDM_TotalCardsDisplay', nil, screen.owned)
        local _, card = currentCard()
        setButton('CDM_AddButton', nil, card ~= nil and not card.inDeck)
        setButton('CDM_RemoveButton', nil, card ~= nil and card.inDeck)
        setButton('CDM_PlayButton', nil, screen.canPlay)
        setButton('CDM_CaravanDeckButton', nil, true)
        setButton('CDM_AllCardsButton', nil, true)
        setButton('CDM_NavigateButton', nil, false)
        local bar = shown:find('SWM_Scrollbar')
        if bar then shown:set(bar, 'visible', 0) end
    end)
end

local function addCard(add)
    local index, card = currentCard()
    if not index or card.inDeck == add then return end
    caravan.setInDeck(index, add)
    drawDeck()
end

local function chooseList(list)
    if list == 'deck' and #deckIndexes.deck == 0 then return end
    activeList = list
    drawDeck()
end

local function moveCursor(step)
    local list = deckIndexes[activeList]
    cursor[activeList] = math.max(1, math.min(cursor[activeList] + step, math.max(#list, 1)))
    drawDeck()
end

local function randomize()
    caravan.randomize()
    cursor = { deck = 1, all = 1 }
    firstShown = { deck = 1, all = 1 }
    drawDeck()
end

-- ---- The match --------------------------------------------------------------------------------------------------------------

local function columnOf(track)
    if track <= 3 then return track end
    return 7 - track
end

local function onRow(slotIndex)
    -- The slots of the caravans come first: track by track, row by row
    local track = math.floor((slotIndex - 1) / ROWS) + 1
    local row = (slotIndex - 1) % ROWS + 1
    gameTiles.pick(track, row)
end

local function makeGameTiles()
    local main = shown:find(RECTS.game)
    if not main then return nil end
    local tiles = { rows = {}, hand = {}, opponentHand = {}, totals = {}, buttons = {} }
    for index = 1, TRACKS * ROWS do
        tiles.rows[index] = makeSlot(main, 'CaravanRow', index, onRow)
    end
    for index = 1, 8 do
        tiles.hand[index] = makeSlot(main, 'CaravanHand', index, function(slot) gameTiles.hold(slot) end)
        tiles.opponentHand[index] = {
            edge = fill(main, 'CaravanBackEdge' .. index, COLOR.edge),
            face = fill(main, 'CaravanBack' .. index, COLOR.back),
        }
    end
    for track = 1, TRACKS do
        tiles.totals[track] = label(main, 'CaravanTotal' .. track, COLOR.text, 1)
    end
    tiles.status = label(main, 'CaravanStatus', COLOR.text, 2)
    tiles.opponent = label(main, 'CaravanOpponent', COLOR.text, 1)
    tiles.piles = label(main, 'CaravanPiles', COLOR.text, 1)
    tiles.buttons.discardCard = button(main, 'CaravanDiscardCard', 'Discard Card', function() gameTiles.discardCard() end)
    tiles.buttons.discardTrack = button(main, 'CaravanDiscardTrack', 'Discard Caravan', function() gameTiles.discardTrack() end)
    tiles.buttons.forfeit = button(main, 'CaravanForfeit', 'Forfeit', function() gameTiles.forfeit() end)
    return tiles
end

local function totalColor(total)
    if total >= 21 and total <= 26 then return COLOR.sold end
    if total > 26 then return COLOR.over end
    return COLOR.text
end

local function setStatus(text)
    statusText = text
    if gameTiles and drawn == 'game' then
        shown:batch(function()
            setText(gameTiles.status, text)
            shown.menu:set(gameTiles.status, 'visible', text ~= '' and 1 or 0)
        end)
    end
end

local function drawGame()
    local screen = caravan.screen()
    local board = caravan.board()
    if not screen or not board then return end
    local width, height = rectSize(RECTS.game)
    local lineH = lineHeight()
    local colW, gap = 150, 36
    local panel = 210
    local boardLeft = (width - panel - (3 * colW + 2 * gap)) / 2
    -- How far apart the rows of a caravan are: as far as the room goes (two caravans of seven rows, four lines of totals and
    -- words, the hand and the gaps), a line at most
    local step = math.max(14, math.min(lineH + 4, math.floor((height - CARD_H - 66 - 4 * lineH) / (2 * ROWS))))
    local rowH = step - 2
    local opponentTop = lineH + 16
    local opponentRows = opponentTop + lineH + 4
    local playerRows = opponentRows + ROWS * step + lineH + 12
    local handY = height - CARD_H - 10
    shown:batch(function()
        -- The six caravans
        for track = 1, TRACKS do
            local column = columnOf(track)
            local x = boardLeft + (column - 1) * (colW + gap)
            local own = track <= 3
            local top = own and playerRows or opponentRows
            local data = board.tracks[track]
            for row = 1, ROWS do
                local slot = gameTiles.rows[(track - 1) * ROWS + row]
                local cards = data.rows[row]
                local y = top + (row - 1) * step
                if cards then
                    local head = cards[1]
                    local text = cardText(head)
                    for index = 2, #cards do
                        text = text .. ' ' .. rankOf(cards[index].value)
                    end
                    local edge = COLOR.edge
                    if lastMove and lastMove.track == track and lastMove.row == row and markLeft > 0 then
                        edge = COLOR.picked
                    end
                    drawSlot(slot, x, y, colW, rowH, text, inkOf(head), nil, edge)
                elseif own and row == #data.rows + 1 and board.playersTurn and pickedHand then
                    -- The row a number card of the hand would go on
                    local picked = board.hand[pickedHand]
                    if picked and picked.value <= 10 and caravan.canPlace(track, row, pickedHand) then
                        drawSlot(slot, x, y, colW, rowH, '', nil, COLOR.empty, COLOR.picked)
                    else
                        hideSlot(slot)
                    end
                elseif own and row == 1 and #data.rows == 0 then
                    drawSlot(slot, x, y, colW, rowH, '', nil, COLOR.empty, COLOR.edge)
                else
                    hideSlot(slot)
                end
            end
            -- The total, over the opponent's caravan and under the player's
            local total = gameTiles.totals[track]
            local totalY = own and (playerRows + ROWS * step + 4) or (opponentTop)
            local arrow = data.direction > 0 and ' ^' or data.direction < 0 and ' v' or ''
            put(total, x, totalY)
            setText(total, tostring(data.total) .. arrow .. (data.suit <= 4 and (' ' .. SUITS[data.suit]) or ''),
                totalColor(data.total))
        end
        -- The player's hand, with the card that is picked up lifted
        for index = 1, 8 do
            local slot = gameTiles.hand[index]
            local card = board.hand[index]
            if card then
                local edge = index == pickedHand and COLOR.picked or COLOR.edge
                local y = index == pickedHand and handY - 12 or handY
                drawSlot(slot, boardLeft - 40 + (index - 1) * (CARD_W + 8), y, CARD_W, CARD_H, cardText(card), inkOf(card),
                    nil, edge)
            else
                hideSlot(slot)
            end
        end
        -- The opponent's hand, face down, in a row at the top of the panel
        for index = 1, 8 do
            local back = gameTiles.opponentHand[index]
            if index <= board.opponentHand then
                local x = width - panel + (index - 1) * 22
                put(back.edge, x - 1, 10 - 1, 20, 28)
                put(back.face, x, 10, 18, 26)
            else
                hide(back.edge)
                hide(back.face)
            end
        end
        put(gameTiles.opponent, width - panel, 48)
        setText(gameTiles.opponent, screen.opponent)
        put(gameTiles.piles, width - panel, 48 + lineH)
        setText(gameTiles.piles, string.format('Cards left: %d (you) %d', board.pile, board.opponentPile))
        -- The buttons
        local buttonY = handY - 60
        local playing = board.playersTurn and not board.over
        drawButton(gameTiles.buttons.discardCard, width - panel, buttonY - 2 * (lineH + 20), panel - 30, lineH + 8)
        drawButton(gameTiles.buttons.discardTrack, width - panel, buttonY - (lineH + 20), panel - 30, lineH + 8,
            discarding and 'Pick a Caravan' or 'Discard Caravan')
        drawButton(gameTiles.buttons.forfeit, width - panel, buttonY, panel - 30, lineH + 8,
            forfeitAsked and 'Sure?' or 'Forfeit')
        if board.over then
            hideButton(gameTiles.buttons.discardCard)
            hideButton(gameTiles.buttons.discardTrack)
            hideButton(gameTiles.buttons.forfeit)
        else
            if not playing then
                hideButton(gameTiles.buttons.discardCard)
                hideButton(gameTiles.buttons.discardTrack)
            end
            if board.phase == 'start' then hideButton(gameTiles.buttons.discardTrack) end
        end
        put(gameTiles.status, width / 2 - panel / 2, playerRows - lineH - 6)
        shown.menu:set(gameTiles.status, 'justify', 2)
        setText(gameTiles.status, statusText)
        shown.menu:set(gameTiles.status, 'visible', statusText ~= '' and 1 or 0)
    end)
end

local function afterMove()
    pickedHand, discarding, forfeitAsked = nil, false, false
    opponentWait = 0
    local board = caravan.board()
    if board and board.over then
        setStatus(board.won and 'You won the match.' or 'You lost the match.')
    elseif board and board.playersTurn then
        setStatus(board.phase == 'start' and 'Put a number card on each of your caravans.' or 'Your move.')
    else
        setStatus('Opponent\'s move...')
    end
    drawGame()
end

local function describeMove(move)
    if move.type == 5 then return 'The opponent threw a caravan away.' end
    if move.type == 6 then return 'The opponent threw a card away.' end
    return 'The opponent played ' .. cardText(move) .. '.'
end

local function opponentMove()
    local move = caravan.opponentMove()
    if not move then return end
    lastMove = nil
    markLeft = 0
    if move.type ~= 5 and move.type ~= 6 then
        lastMove = { track = move.track, row = move.row }
        markLeft = MARK_TIME
    end
    afterMove()
    -- The words of the move stay where afterMove put those of the turn
    local board = caravan.board()
    if board and not board.over then
        setStatus(describeMove(move))
        drawGame()
    end
end

local function bumpTile(text)
    setStatus(text)
    drawGame()
end

local function onPlaced(done, text)
    if done then
        lastMove = nil
        markLeft = 0
        afterMove()
    else
        bumpTile(text)
    end
end

local function installGameActions()
    gameTiles.hold = function(slot)
        local board = caravan.board()
        if not board or not board.playersTurn or board.over then return end
        if not board.hand[slot] then return end
        pickedHand = pickedHand ~= slot and slot or nil
        discarding, forfeitAsked = false, false
        drawGame()
    end
    gameTiles.pick = function(track, row)
        local board = caravan.board()
        if not board or not board.playersTurn or board.over then return end
        if discarding then
            if track <= 3 and #board.tracks[track].rows > 0 then
                onPlaced(caravan.discardTrack(track), 'You cannot do that now.')
            else
                bumpTile('Pick one of your caravans.')
            end
            return
        end
        if not pickedHand or not board.hand[pickedHand] then
            bumpTile('Pick a card of your hand first.')
            return
        end
        local card = board.hand[pickedHand]
        -- A number card goes on the next row of a caravan; a face card on the row that is clicked
        local target = card.value <= 10 and #board.tracks[track].rows + 1 or row
        if target > ROWS or not caravan.canPlace(track, target, pickedHand) then
            bumpTile('The card cannot go there.')
            return
        end
        onPlaced(caravan.place(track, target, pickedHand), 'The card cannot go there.')
    end
    gameTiles.discardCard = function()
        local board = caravan.board()
        if not board or not board.playersTurn or board.over then return end
        if not pickedHand or not board.hand[pickedHand] then
            bumpTile('Pick the card to throw away first.')
            return
        end
        onPlaced(caravan.discardCard(pickedHand), 'You cannot throw that away now.')
    end
    gameTiles.discardTrack = function()
        local board = caravan.board()
        if not board or not board.playersTurn or board.over or board.phase == 'start' then return end
        discarding = not discarding
        forfeitAsked, pickedHand = false, nil
        setStatus(discarding and 'Pick the caravan to throw away.' or 'Your move.')
        drawGame()
    end
    gameTiles.forfeit = function()
        local board = caravan.board()
        if not board or board.over then return end
        if not forfeitAsked then
            forfeitAsked, discarding, pickedHand = true, false, nil
            setStatus('Press Forfeit again to give the match up.')
            drawGame()
            return
        end
        caravan.forfeit()
        afterMove()
    end
end

-- ---- The result -------------------------------------------------------------------------------------------------------------

local function makeResultTiles()
    local main = shown:find(RECTS.results)
    if not main then return nil end
    return {
        title = label(main, 'CaravanResultTitle', COLOR.text, 2),
        pot = label(main, 'CaravanResultPot', COLOR.text, 2),
        exit = button(main, 'CaravanResultExit', word('sExit', 'Exit'), function() leave() end),
    }
end

local function drawResults(screen)
    local width, height = rectSize(RECTS.results)
    local lineH = lineHeight()
    shown:batch(function()
        put(resultTiles.title, width / 2, height / 2 - lineH * 2)
        setText(resultTiles.title, screen.won and word('sYouWin', 'You win') or word('sYouLose', 'You lose'))
        put(resultTiles.pot, width / 2, height / 2 - lineH * 0.5)
        local text = screen.pot > 0 and ((screen.won and 'You won ' or 'You lost ') .. screen.pot) or ''
        if screen.forfeited then text = (text ~= '' and (text .. ' - ') or '') .. 'you gave the match up' end
        setText(resultTiles.pot, text)
        drawButton(resultTiles.exit, width / 2 - 70, height / 2 + lineH, 140, lineH + 8)
    end)
end

-- ---- The screens ------------------------------------------------------------------------------------------------------------

local function showStage(name)
    shown:batch(function()
        for stage, rect in pairs(RECTS) do
            local tile = shown:find(rect)
            if tile then shown:set(tile, 'visible', stage == name and 1 or 0) end
        end
    end)
    drawn = name
end

local function enterStage(screen)
    quantity.close()
    showStage(screen.stage)
    if screen.stage == 'bet' then
        drawBet(screen)
    elseif screen.stage == 'deck' then
        deckTiles = deckTiles or makeDeckTiles()
        deckTiles.redraw = drawDeck
        drawDeck()
    elseif screen.stage == 'game' then
        if not gameTiles then
            gameTiles = makeGameTiles()
            installGameActions()
        end
        pickedHand, discarding, forfeitAsked, lastMove, markLeft = nil, false, false, nil, 0
        opponentWait = 0
        afterMove()
    else
        resultTiles = resultTiles or makeResultTiles()
        drawResults(screen)
    end
end

local function openMenu()
    local opened = menuModule.open(FILE)
    if not opened then return false end
    shown = opened
    shown:onClick('CBM_AutoMatchButton', function()
        if caravan.match() then
            local screen = caravan.screen()
            if screen then drawBet(screen) end
        end
    end)
    shown:onClick('CBM_RaiseButton', raise)
    shown:onClick('CBM_AcceptButton', function()
        if caravan.accept() then
            local screen = caravan.screen()
            if screen then enterStage(screen) end
        end
    end)
    shown:onClick('CBM_ExitButton', leave)
    shown:onClick('CDM_AddButton', function() addCard(true) end)
    shown:onClick('CDM_RemoveButton', function() addCard(false) end)
    shown:onClick('CDM_CaravanDeckButton', function() chooseList('deck') end)
    shown:onClick('CDM_AllCardsButton', function() chooseList('all') end)
    shown:onClick('CDM_PlayButton', function()
        if caravan.play() then
            local screen = caravan.screen()
            if screen then enterStage(screen) end
        end
    end)
    shown:onWheel(function(notches)
        if drawn == 'deck' and notches ~= 0 then moveCursor(notches > 0 and 1 or -1) end
    end)
    return true
end

local function sync()
    local screen = caravan.screen()
    if not screen then
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
            print('caravan: the file ' .. FILE .. ' is not there; the game is not shown')
            caravan.close()
            shownSerial = nil
            closeMode()
            return
        end
        enterStage(screen)
        return
    end
    if not shown then return end
    shown:checkSize()
    local width = rectSize(RECTS[screen.stage])
    if lastWidth and width ~= lastWidth and drawn == screen.stage then
        -- The window changed: the tiles of the stage on the screen are laid out again (they are not made twice)
        if screen.stage == 'deck' then
            drawDeck()
        elseif screen.stage == 'game' then
            drawGame()
        elseif screen.stage == 'results' then
            drawResults(screen)
        end
    end
    lastWidth = width
    if screen.stage ~= drawn then
        -- The match is over: the board stays a moment, then the result
        if screen.stage == 'results' and drawn == 'game' then
            resultIn = resultIn or RESULT_DELAY
        else
            enterStage(screen)
        end
    end
end

local function stepFrame(dt)
    if markLeft > 0 then
        markLeft = markLeft - dt
        if markLeft <= 0 and drawn == 'game' then drawGame() end
    end
    if resultIn then
        resultIn = resultIn - dt
        if resultIn <= 0 then
            resultIn = nil
            local screen = caravan.screen()
            if screen then enterStage(screen) end
        end
        return
    end
    if drawn == 'game' then
        local board = caravan.board()
        if board and not board.over and not board.playersTurn then
            opponentWait = opponentWait + dt
            if opponentWait >= OPPONENT_DELAY then
                opponentWait = 0
                opponentMove()
            end
        end
    end
end

-- ---- Keys and the pad ---------------------------------------------------------------------------------------------------------

local function press(code, symbol)
    if not shown or I.UI.getMode() ~= 'Interface' then return end
    if quantity.isOpen() then
        quantity.press(code, symbol)
        return
    end
    if drawn == 'deck' then
        if code == input.KEY.LeftArrow then
            moveCursor(-1)
        elseif code == input.KEY.RightArrow then
            moveCursor(1)
        elseif code == input.KEY.UpArrow then
            chooseList('deck')
        elseif code == input.KEY.DownArrow then
            chooseList('all')
        elseif code == input.KEY.Enter then
            local _, card = currentCard()
            if card then addCard(not card.inDeck) end
        elseif symbol == 'r' or symbol == 'R' then
            randomize()
        end
    elseif drawn == 'bet' then
        if code == input.KEY.Enter then shown:clickNamed('CBM_AcceptButton') end
    elseif drawn == 'results' then
        if code == input.KEY.Enter then leave() end
    end
end

-- On the pad the buttons are the ones the files name: the stake has A Auto-Match, X Raise, Y Accept and B Exit; the deck has
-- the shoulders for the two lists, Y Add, X Remove and A Play Caravan; the rest is the pointer's
local function onPad(button)
    if not shown or I.UI.getMode() ~= 'Interface' or quantity.isOpen() then return end
    local B = input.CONTROLLER_BUTTON
    local names
    if drawn == 'bet' then
        names = { [B.A] = 'CBM_AutoMatchButton', [B.X] = 'CBM_RaiseButton', [B.Y] = 'CBM_AcceptButton', [B.B] = 'CBM_ExitButton' }
    elseif drawn == 'deck' then
        names = {
            [B.RightShoulder] = 'CDM_CaravanDeckButton',
            [B.LeftShoulder] = 'CDM_AllCardsButton',
            [B.Y] = 'CDM_AddButton',
            [B.X] = 'CDM_RemoveButton',
            [B.A] = 'CDM_PlayButton',
        }
        if button == B.DPadLeft then moveCursor(-1) return end
        if button == B.DPadRight then moveCursor(1) return end
    elseif drawn == 'results' and button == B.A then
        leave()
        return
    end
    local name = names and names[button]
    if name then
        padModule.log('caravan', button, name)
        shown:clickNamed(name)
    end
end

local function onFrame(dt)
    sync()
    if shown then
        quantity.update()
        stepFrame(dt or 0)
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
        -- However the screen is left (a button, Escape, a script), the player gets up: a match that is on is lost
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyMenu()
                caravan.close()
                shownSerial = nil
            end
        end,
    },
}
