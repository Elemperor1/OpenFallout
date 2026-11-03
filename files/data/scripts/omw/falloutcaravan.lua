-- The card game Caravan of New Vegas. The rules, the cards, the opponent and the money are the engine's (core.casino.caravan);
-- this script draws the four screens of the game's CaravanMenu (caravan_menu.xml, which includes caravan_bet_menu.xml,
-- caravan_builddeck_menu.xml, caravan_game_menu.xml and caravan_results_menu.xml): the stake, the deck, the match and its result.
--
-- The files are the ones of the folder Menus\Prefabs (the executable's include handler reads them from there; the four files of
-- the same names next to caravan_menu.xml have no controls: Mac round 12AV S2). Every word of them is a game setting (the
-- executable writes the same settings itself), and the keys are the letters the files name (_PCButtonText): the stake has W
-- Auto-Match, A Raise, F Accept and R Exit, the deck W Add, A Remove and F Play Caravan (R leaves at once), the match W Select
-- Card, Q Discard Card, E Discard Caravan and R Cancel, and a key on the result closes it.
-- The stake is the game's own file: the opponent's ante, the player's ante and funds, and the buttons (Raise opens the quantity
-- prompt, as the game asks "how many"). The file of the deck gives the two counts, the button that randomizes the deck and the
-- label of the list of all the cards, Add, Remove and Play Caravan; its cards are 3D in the game and plain tiles here. The match
-- file gives the three counts of the corner (the pot of the match, the net total won and the matches won over lost, which are
-- the names' meaning, not read from the code) and the four buttons; the six caravans, their totals and the cards are 3D in the
-- game and are plain tiles of this script: the caravans as columns of rows (the opponent's above the player's, the player's n
-- under the opponent's 7 - n), the eight cards of the hand below. The result file has what the player has won and lost at
-- Caravan in all and the words of the result. The words on the tiles of this script (the line that tells what the opponent did
-- and the like) are not the game's: the executable has none that were read.
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
local RECTS = { bet = 'CBM_MainRect', deck = 'CDM_MainRect', game = 'CDG_MainRect', results = 'CRM_MainRect' }

local FONT = 7
local CARD_W, CARD_H = 72, 100
local ROWS, TRACKS = 7, 6
-- Seconds between the player's move and the opponent's
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

-- The deck screen: which list the cursor is in ('deck' or 'all'), where in each, and the first card of each that is shown
local activeList = 'all'
local cursor = { deck = 1, all = 1 }
local firstShown = { deck = 1, all = 1 }
local deckIndexes = { deck = {}, all = {} }

-- The match: the card of the hand that is picked up, whether a caravan is to be thrown away, the seconds the opponent
-- waits, and what it did last (to light it)
local pickedHand = nil
local discarding = false
local opponentWait = 0
local lastMove = nil
local markLeft = 0
local statusText = ''
-- The seconds the result has been shown
local resultAge = nil

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
    deckTiles, gameTiles = nil, nil
    activeList, cursor, firstShown = 'all', { deck = 1, all = 1 }, { deck = 1, all = 1 }
    pickedHand, discarding, lastMove, resultAge = nil, false, nil, nil
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

-- A picture over a tile: the texture of a card's record, which the game puts on the shapes of the card (the picture takes no
-- pointer, the tile under it does)
local function picture(parent, name)
    local m = shown.menu
    local tile = m:create(parent, 'image', name)
    if not tile then return nil end
    m:set(tile, 'target', 0)
    m:set(tile, 'visible', 0)
    return tile
end

-- A card slot: the edge, the face (which takes the pointer), the picture of the card over it when there is one and the words
-- on it when there is not
local function makeSlot(parent, prefix, index, onClick)
    local slot = {
        edge = fill(parent, prefix .. 'Edge' .. index, COLOR.edge),
        face = fill(parent, prefix .. 'Card' .. index, COLOR.face, 255, true),
        art = picture(parent, prefix .. 'Art' .. index),
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
    if slot.art then hide(slot.art) end
    hide(slot.text)
end

-- A card in a slot: where, how big, the words (and the ink of the suit), the ground and the edge; `face` is the texture of the
-- card's record, which is drawn instead of the words when there is one (the cards that are strips of a caravan have words only)
local function drawSlot(slot, x, y, width, height, text, ink, ground, edge, face)
    put(slot.edge, x - 2, y - 2, width + 4, height + 4)
    setColor(slot.edge, edge or COLOR.edge)
    put(slot.face, x, y, width, height)
    setColor(slot.face, ground or COLOR.face)
    if face and face ~= '' and slot.art then
        shown.menu:set(slot.art, 'filename', face)
        put(slot.art, x, y, width, height)
        hide(slot.text)
        return
    end
    if slot.art then hide(slot.art) end
    if text and text ~= '' then
        put(slot.text, x + 4, y + 2)
        setText(slot.text, text, ink or COLOR.black)
    else
        hide(slot.text)
    end
end

-- ---- Cards ----------------------------------------------------------------------------------------------------------------

local RANKS = { [1] = 'A', [10] = '10', [12] = 'J', [13] = 'Q', [14] = 'K', [15] = 'Jk' }
-- The suit numbers of the card records: 1 hearts, 2 spades, 3 diamonds, 4 clubs, 5 the Joker (Mac round 12AV S2)
local SUITS = { 'H', 'S', 'D', 'C', '*' }

local function rankOf(value)
    return RANKS[value] or tostring(value)
end

local function cardText(card)
    return rankOf(card.value) .. (SUITS[card.suit] or '?')
end

local function inkOf(card)
    return (card.suit == 1 or card.suit == 3) and COLOR.red or COLOR.black
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
                    drawSlot(tiles, left + (slot - 1) * (CARD_W + 8), y, CARD_W, CARD_H, cardText(card), inkOf(card), nil, edge,
                        card.face)
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
        -- The words of the deck screen that the executable writes itself (0x73D850): the title of the first count is the word of the
        -- deck, and the button on the right bumper says "Randomize" and the word of the deck
        setButton('CDM_CDM_CardsInDeckTitle', word('sCaravanDeckText', 'Caravan Deck'), true)
        setButton('CDM_CaravanDeckButton',
            word('sRSMRandomize', 'Randomize') .. ' ' .. word('sCaravanDeckText', 'Caravan Deck'), true)
        setButton('CDM_AllCardsButton', nil, true)
        setButton('CDM_NavigateButton', nil, false)
        local bar = shown:find('CDM_Scrollbar')
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
    local tiles = { rows = {}, hand = {}, opponentHand = {}, totals = {} }
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
    -- The right side is the file's (the counts of the corner on top, the four buttons at the bottom), the opponent under the counts
    local panel = 320
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
                    nil, edge, card.face)
            else
                hideSlot(slot)
            end
        end
        -- The opponent's hand, face down, in a row at the top of the panel
        for index = 1, 8 do
            local back = gameTiles.opponentHand[index]
            if index <= board.opponentHand then
                local x = width - panel + (index - 1) * 22
                put(back.edge, x - 1, 150 - 1, 20, 28)
                put(back.face, x, 150, 18, 26)
            else
                hide(back.edge)
                hide(back.face)
            end
        end
        put(gameTiles.opponent, width - panel, 186)
        setText(gameTiles.opponent, screen.opponent)
        put(gameTiles.piles, width - panel, 186 + lineH)
        setText(gameTiles.piles, string.format('Cards left: %d (you) %d', board.pile, board.opponentPile))
        -- The counts of the corner and the buttons are the file's: the pot of the match (both stakes: twice the stake of each
        -- side, 0x73EA90), what the player has won in all at Caravan (net) and the matches won over lost, and the four buttons
        -- (Cancel is the word the executable writes on the one the file calls Forfeit, 0x74087E). The file's tiles for the
        -- totals over the caravans and the numbers of cards are for the 3D scene and stay hidden
        local playing = board.playersTurn and not board.over
        setDisplay('CGM_AnteDisplay', nil, 2 * screen.pot)
        setDisplay('CGM_NetTotalDisplay', nil, screen.moneyWon - screen.moneyLost)
        setDisplay('CGM_WinLossDisplay', nil, string.format('%d/%d', screen.gamesWon, screen.gamesLost))
        for _, name in ipairs({ 'CGM_NPCTrackRect', 'CGM_PlayerTrackRect', 'CGM_CardNumRect' }) do
            setButton(name, nil, false)
        end
        setButton('CGM_SelectCardButton', nil, playing and #board.hand > 0)
        setButton('CGM_DiscardCardButton', nil, playing)
        setButton('CGM_DiscardTrackButton', nil, playing and board.phase ~= 'start')
        setButton('CGM_ForfeitGameButton', word('sCancel', 'Cancel'), not board.over)
        put(gameTiles.status, (width - panel) / 2, playerRows - lineH - 6)
        shown.menu:set(gameTiles.status, 'justify', 2)
        setText(gameTiles.status, statusText)
        shown.menu:set(gameTiles.status, 'visible', statusText ~= '' and 1 or 0)
    end)
end

local function afterMove()
    pickedHand, discarding = nil, false
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
        discarding = false
        drawGame()
    end
    -- Select Card (W): picks up the next card of the hand (the 3D scene of the game has a cursor over the hand)
    gameTiles.select = function()
        local board = caravan.board()
        if not board or not board.playersTurn or board.over or #board.hand == 0 then return end
        pickedHand = ((pickedHand or 0) % #board.hand) + 1
        discarding = false
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
        pickedHand = nil
        setStatus(discarding and 'Pick the caravan to throw away.' or 'Your move.')
        drawGame()
    end
    -- Cancel (R): a card that is picked up (or a caravan that is to be thrown away) is put back, and nothing is asked; with
    -- nothing picked the one question is asked ("quit Caravan", Yes or No), and Yes is a loss with the pot paid (0x7490E0, 0x74A270)
    gameTiles.forfeit = function()
        local board = caravan.board()
        if not board or board.over then return end
        if pickedHand or discarding then
            pickedHand, discarding = nil, false
            setStatus(board.playersTurn and 'Your move.' or statusText)
            drawGame()
            return
        end
        local function giveUp()
            caravan.forfeit()
            afterMove()
        end
        local asked = I.FalloutMessage ~= nil and I.FalloutMessage.ask(
            word('sQuitCaravanText', 'Quit Caravan?'), { word('sYes', 'Yes'), word('sNo', 'No') },
            function(index)
                if index == 1 then giveUp() end
            end)
        if not asked then
            giveUp()
        end
    end
end

-- ---- The result -------------------------------------------------------------------------------------------------------------

-- The result is the file's: the money won and lost and the matches won over lost in all, the biggest ante, and under them the
-- words of the result. The executable shows the screen at once when the match ends (0x740980) with "You win" or "You lose",
-- while the table plays a sequence of 2.0 s, and writes the words that ask for a key over them when it is over (0x7420B2).
-- Keys before that do nothing; from then on any key closes it (0x749360). A click on the screen does nothing, ever; the
-- pad's B closes it a second later (the busy flag of the menu), which the engine's own B does at once.
local PRESS_KEY_AFTER = 2.0

local function drawResults(screen)
    shown:batch(function()
        setDisplay('CRM_NPCBetDisplay', nil, screen.moneyWon)
        setDisplay('CRM_Losses', nil, screen.moneyLost)
        setDisplay('CRM_WinLoss', nil, string.format('%d/%d', screen.gamesWon, screen.gamesLost))
        setDisplay('CRM_LargestWinning', nil, screen.biggestAnte)
        local info = shown:find('CRM_Info')
        if info then
            local text
            if resultAge ~= nil and resultAge >= PRESS_KEY_AFTER then
                text = word('sCaravanPressAnyKeyText', 'Press any key to continue')
            elseif screen.won then
                text = word('sCaravanYouWinText', 'You win')
            else
                text = word('sCaravanYouLoseText', 'You lose')
            end
            shown:set(info, 'string', text)
        end
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
        pickedHand, discarding, lastMove, markLeft = nil, false, nil, 0
        opponentWait = 0
        afterMove()
    else
        resultAge = 0
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
    -- The button of the deck says "Randomize" and the word of the deck (the executable writes it, 0x73D850), so it makes a deck
    shown:onClick('CDM_CaravanDeckButton', function() randomize() end)
    shown:onClick('CDM_AllCardsButton', function() chooseList('all') end)
    shown:onClick('CGM_SelectCardButton', function() if gameTiles then gameTiles.select() end end)
    shown:onClick('CGM_DiscardCardButton', function() if gameTiles then gameTiles.discardCard() end end)
    shown:onClick('CGM_DiscardTrackButton', function() if gameTiles then gameTiles.discardTrack() end end)
    shown:onClick('CGM_ForfeitGameButton', function() if gameTiles then gameTiles.forfeit() end end)
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
        -- (the match is over: the result is shown at once, as the executable does)
        enterStage(screen)
    end
end

local function stepFrame(dt)
    if markLeft > 0 then
        markLeft = markLeft - dt
        if markLeft <= 0 and drawn == 'game' then drawGame() end
    end
    if resultAge ~= nil and drawn == 'results' then
        local before = resultAge
        resultAge = resultAge + dt
        if before < PRESS_KEY_AFTER and resultAge >= PRESS_KEY_AFTER then
            local screen = caravan.screen()
            if screen then drawResults(screen) end
        end
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

local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end

-- The letters of the keys (the executable's key handler, 0x749360, reads them in both cases) and the button of the file that each
-- clicks on the screen: the files name the same letters on their buttons (_PCButtonText)
local KEYS = {
    bet = { w = 'CBM_AutoMatchButton', a = 'CBM_RaiseButton', f = 'CBM_AcceptButton', r = 'CBM_ExitButton' },
    deck = { w = 'CDM_AddButton', a = 'CDM_RemoveButton', f = 'CDM_PlayButton' },
    game = { w = 'CGM_SelectCardButton', q = 'CGM_DiscardCardButton', e = 'CGM_DiscardTrackButton',
        r = 'CGM_ForfeitGameButton' },
}

local function pressLetter(letter)
    local name = KEYS[drawn] and KEYS[drawn][letter]
    if name then shown:clickNamed(name) end
end

-- R leaves at once on the deck (and is the Exit button of the stake and the Cancel button of the match), any key on the result
-- closes it once the words that ask for a key are up
local function press(code, symbol)
    if not shown or I.UI.getMode() ~= 'Interface' or messageOpen() then return end
    if quantity.isOpen() then
        quantity.press(code, symbol)
        return
    end
    local letter = type(symbol) == 'string' and symbol:lower() or ''
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
        elseif letter == 'r' then
            leave()
        elseif letter == 's' then
            -- (the key table of the executable has an S that does something on this screen that was not read; it
            -- makes a deck here, as the button does)
            randomize()
        else
            pressLetter(letter)
        end
    elseif drawn == 'bet' then
        if code == input.KEY.Enter then
            shown:clickNamed('CBM_AcceptButton')
        else
            pressLetter(letter)
        end
    elseif drawn == 'game' then
        pressLetter(letter)
    elseif drawn == 'results' then
        if resultAge ~= nil and resultAge >= PRESS_KEY_AFTER then leave() end
    end
end

-- On the pad the buttons are the ones the files name, but A, which the engine clicks with where the pointer is (a second
-- meaning would act twice): the stake has X Raise, Y Accept and B Exit; the deck has the right bumper for the button that
-- makes a deck, X Remove and Y Play Caravan (Add is A); the match has the bumpers for the triggers (left Discard Card, right
-- Discard Caravan). B is the engine's: it leaves the screen, which on the deck and the result is what the executable does
-- with it too, and on the match is a forfeit (the executable cancels the picked card, or asks sQuitCaravanText; the engine
-- has no way yet for a screen of a script to keep the button)
local function onPad(button)
    if not shown or I.UI.getMode() ~= 'Interface' or quantity.isOpen() or messageOpen() then return end
    local B = input.CONTROLLER_BUTTON
    local names
    if drawn == 'bet' then
        names = { [B.X] = 'CBM_RaiseButton', [B.Y] = 'CBM_AcceptButton', [B.B] = 'CBM_ExitButton' }
    elseif drawn == 'deck' then
        names = { [B.RightShoulder] = 'CDM_CaravanDeckButton', [B.X] = 'CDM_RemoveButton', [B.Y] = 'CDM_PlayButton' }
        if button == B.DPadLeft then moveCursor(-1) return end
        if button == B.DPadRight then moveCursor(1) return end
    elseif drawn == 'game' then
        names = { [B.LeftShoulder] = 'CGM_DiscardCardButton', [B.RightShoulder] = 'CGM_DiscardTrackButton' }
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
