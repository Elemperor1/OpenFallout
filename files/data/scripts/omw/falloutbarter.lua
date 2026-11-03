-- Trading with a character, in Fallout 3 and New Vegas: the menu a conversation opens with ShowBarterMenu, drawn on the
-- game's own file (barter_menu.xml). The player's things are on the left, the vendor's on the right. A thing that is
-- clicked goes into the pile of the deal and shows in the list of the other side with a square, where a click takes it
-- out of the pile again; nothing changes hands until Accept. What a deal comes to, whether the Accept button works and the
-- deal itself are the engine's (core.pipboy.barter*, the prices and the checks as the executable has them, FalloutNV.exe
-- 0x72d250 to 0x72fd10, Mac round 12AG); this script lists the things, keeps the piles and gives the choices back.
local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local quantity = require('scripts.omw.falloutquantity')
local cards = require('scripts.omw.falloutitemcards')
local pad = require('scripts.omw.falloutpad')

local pipboy = core.pipboy

if not (pipboy and pipboy.available() and pipboy.barter) then
    return {}
end

local FILE = 'barter_menu.xml'
-- The sounds the rows of a list make (the template of the file's lists names them: the pointer comes to a row, a row is clicked);
-- the pointer's own are played by the menu module, a key plays them here
local ROW_FOCUS, ROW_CLICK = 'UIMenuFocus', 'UIMenuOK'

-- A message box drawn over the screen (falloutmessage.lua) has the keys, the pad and the wheel
local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end

-- The tabs the arrows over a list go through: everything, then one kind of thing at a time
local FILTERS = {
    { setting = 'sInventoryItems', fallback = 'Items' },
    { key = 'weapons', setting = 'sInventoryWeapons', fallback = 'Weapons' },
    { key = 'apparel', setting = 'sInventoryApparel', fallback = 'Apparel' },
    { key = 'aid', setting = 'sInventoryAid', fallback = 'Aid' },
    { key = 'misc', setting = 'sInventoryMisc', fallback = 'Misc' },
    { key = 'ammo', setting = 'sInventoryAmmo', fallback = 'Ammo' },
}

-- The arrow of the caps that move in the middle of the screen: the file keeps two pictures, numbered
local ARROW_BUY, ARROW_SELL = 1, 2
-- How faint the amount is while the deal cannot be made, as the code of the game sets it
local DIM_ALPHA, FULL_ALPHA = 64, 255

local shown, sides = nil, nil
-- The list that has the focus, 'player' or 'vendor' (see setListFocus)
local listFocus = nil
local modeOpen = false
local shownSerial = nil
local active = 'player'
-- The things of the two holders as the engine lists them (with the price of one), and what is in the piles: the id of
-- a thing to how many of it
local stock = { player = {}, vendor = {} }
local piles = { buy = {}, sell = {} }
-- Whether a deal was made while the menu was open
local traded = false
-- The thing whose cards are shown: the side and the row
local cardSide, cardRow = nil, nil
local lastRows = { player = 0, vendor = 0 }

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

-- The price of one in the value column, as the code of the game writes it: two dashes for nothing, tenths below 1,
-- whole numbers above
local function priceText(value)
    if value == 0 then return '--' end
    if value < 1 then return string.format('%.1f', value) end
    return number(value)
end

local function rowText(entry)
    return entry.item.name .. (entry.count > 1 and (' (' .. entry.count .. ')') or '')
end

local function pileList(pile)
    local list = {}
    for id, count in pairs(pile) do
        if count > 0 then list[#list + 1] = { id = id, count = count } end
    end
    return list
end

local function pilesEmpty()
    return next(piles.buy) == nil and next(piles.sell) == nil
end

local function clearPiles()
    piles.buy, piles.sell = {}, {}
end

local function isOpen()
    return shown ~= nil
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
    listFocus = nil
    if shown then
        shown:close()
        shown, sides, cardSide, cardRow = nil, nil, nil, nil
    end
end

-- The player leaves the screen: what was open is closed here, because the mode is no longer ours when the event that
-- tells of it comes
local function leave()
    destroyMenu()
    pipboy.closeBarter()
    shownSerial = nil
    clearPiles()
    closeMode()
end

local function refreshStock()
    stock.player = pipboy.barterRows('player')
    stock.vendor = pipboy.barterRows('vendor')
    table.sort(stock.player, function(a, b)
        local x, y = a.name:lower(), b.name:lower()
        if x ~= y then return x < y end
        return a.id < b.id
    end)
    table.sort(stock.vendor, function(a, b)
        local x, y = a.name:lower(), b.name:lower()
        if x ~= y then return x < y end
        return a.id < b.id
    end)
end

local function byName(a, b)
    local x, y = a.item.name:lower(), b.item.name:lower()
    if x ~= y then return x < y end
    return a.item.id < b.item.id
end

-- What a list shows: the things of its holder that are not in a pile (the rest of a stack that is partly in one), and
-- after them, added last as the code of the game does, the things of the other holder that are in a pile, with a
-- square. The tab above the list says which kinds are listed (`filterIndex`, the side's own when none is given).
local function listedEntries(side, filterIndex)
    local entries = {}
    local filter = FILTERS[filterIndex or side.filter]
    local own, other = side.name, side.name == 'player' and 'vendor' or 'player'
    local ownPile, otherPile = own == 'player' and piles.sell or piles.buy, own == 'player' and piles.buy or piles.sell
    local function wanted(item)
        return filter.key == nil or item.category == filter.key
    end
    for _, item in ipairs(stock[own]) do
        local left = item.count - (ownPile[item.id] or 0)
        if left > 0 and wanted(item) then
            entries[#entries + 1] = { item = item, count = left, marked = false, from = own }
        end
    end
    table.sort(entries, byName)
    local marked = {}
    for _, item in ipairs(stock[other]) do
        local count = otherPile[item.id] or 0
        if count > 0 and wanted(item) then
            marked[#marked + 1] = { item = item, count = count, marked = true, from = other }
        end
    end
    table.sort(marked, byName)
    for _, entry in ipairs(marked) do
        entries[#entries + 1] = entry
    end
    return entries
end

local function fillSide(side)
    side.listed = listedEntries(side)
    local rows = {}
    for index, entry in ipairs(side.listed) do
        rows[index] = {
            ['ListItemText.string'] = rowText(entry),
            _Value = priceText(entry.item.price),
            _NumBartered = entry.marked and entry.count or 0,
            _IsEquipped = entry.item.equipped == true and not entry.marked,
            _IsBarterSelected = entry.marked,
        }
    end
    local picked = side.list.selected()
    side.list.rows(rows)
    if picked > 0 and #rows > 0 then
        side.list.select(math.min(picked, #rows), true)
    end
    cardSide, cardRow = nil, nil
end

-- The numbers in the middle of the screen and on the Accept button, from what the piles come to
local function showQuote()
    local quote = pipboy.barterQuote(pileList(piles.buy), pileList(piles.sell))
    local balance = quote.balance
    -- What the vendor cannot pay, or the player cannot afford, shows faint
    local dim = (balance > 0 and balance > quote.vendorCaps) or (balance <= 0 and -balance > quote.playerCaps)
    shown:batch(function()
        shown:set('BM_CapsFlow', 'visible', not quote.empty)
        shown:set('BM_CapsFlow', '_Value', number(quote.amount))
        shown:set('BM_CapsFlow', 'alpha', dim and DIM_ALPHA or FULL_ALPHA)
        -- (not set before the first thing is picked, when the flow is hidden)
        shown:io('_ArrowDirection', quote.empty and 0 or (balance > 0 and ARROW_SELL or ARROW_BUY))
        shown:set('BM_ButtonX', 'target', quote.accept)
        local caps = word('sInventoryCaps', 'Caps')
        shown:set('BM_Items_CapsLabel', 'string', caps .. '   ' .. number(quote.playerCaps))
        shown:set('BM_Container_CapsLabel', 'string', caps .. '   ' .. number(quote.vendorCaps))
    end)
end

-- The titles over the lists: the kind of thing the tab shows; over the vendor's list the vendor's own name while it shows
-- everything. The code of the game writes them in capitals (in English).
local function setTitles(menu)
    local items, container = sides.player, sides.vendor
    local itemsFilter, containerFilter = FILTERS[items.filter], FILTERS[container.filter]
    shown:batch(function()
        shown:set('BM_ItemsTitle', 'string', word(itemsFilter.setting, itemsFilter.fallback):upper())
        shown:set('BM_ContainerTitle', 'string',
            (container.filter == 1 and (menu.name ~= '' and menu.name or 'Vendor')
                or word(containerFilter.setting, containerFilter.fallback)):upper())
    end)
end

local function fillAll()
    local menu = pipboy.barter()
    if not (shown and menu) then return end
    shown:batch(function()
        fillSide(sides.player)
        fillSide(sides.vendor)
        setTitles(menu)
    end)
    showQuote()
    cardSide, cardRow = nil, nil
end

-- Some of a thing goes into a pile of the deal, or comes out of it
local function moveEntry(entry, count)
    local pile
    if entry.marked then
        pile = entry.from == 'vendor' and piles.buy or piles.sell
        pile[entry.item.id] = (pile[entry.item.id] or 0) - count
        if pile[entry.item.id] <= 0 then pile[entry.item.id] = nil end
    else
        pile = entry.from == 'vendor' and piles.buy or piles.sell
        pile[entry.item.id] = (pile[entry.item.id] or 0) + count
    end
    fillAll()
end

-- A row is clicked: one thing moves at once; of a stack of more than the setting says (iInventoryAskQuantityAt, 5)
-- the player is asked how many
local function activateRow(side, row)
    local entry = side.listed and side.listed[row]
    if not entry then return end
    local askAt = pipboy.setting('iInventoryAskQuantityAt') or 5
    if entry.count > askAt then
        if quantity.ask(entry.count, function(count) moveEntry(entry, count) end) then
            return
        end
    end
    moveEntry(entry, 1)
end

-- The tab of a side goes to the next one (or the one before); a tab with nothing to list is passed over, in the same
-- direction, until one has something or all of them are shown again
local function cycleFilter(side, by)
    local index = side.filter
    for _ = 1, #FILTERS do
        index = (index - 1 + by) % #FILTERS + 1
        if index == 1 or #listedEntries(side, index) > 0 then break end
    end
    side.filter = index
    local menu = pipboy.barter()
    shown:batch(function()
        fillSide(side)
        if menu then setTitles(menu) end
    end)
end

-- Accept: the things change hands
local function accept()
    local quote = pipboy.barterQuote(pileList(piles.buy), pileList(piles.sell))
    local result = pipboy.barterDeal(pileList(piles.buy), pileList(piles.sell))
    if result.status ~= 'done' then
        return
    end
    -- The only sound the menu's code makes: the caps that go up when the player is paid and those that go down when it pays
    if quote.balance > 0 then
        shown:playSound('ITMBottlecapsUp')
    elseif quote.balance < 0 then
        shown:playSound('ITMBottlecapsDown')
    end
    traded = true
    clearPiles()
    refreshStock()
    fillAll()
end

-- Exit: with things in the piles the player is asked first (the box has no Escape; the first button leaves)
local function exit()
    if pilesEmpty() then
        leave()
        return
    end
    local asked = I.FalloutMessage ~= nil and I.FalloutMessage.ask(
        word('sCancelBarter', 'Cancel the barter?'), { word('sYes', 'Yes'), word('sNo', 'No') },
        function(index)
            if index == 1 then leave() end
        end)
    if not asked then
        leave()
    end
end

local function openMenu()
    if shown then return true end
    local opened = menuModule.open(FILE)
    if not opened then return false end
    local made = {
        player = { name = 'player', tile = 'BM_Items_InventoryList', filter = 1 },
        vendor = { name = 'vendor', tile = 'BM_Container_InventoryList', filter = 1 },
    }
    for _, side in pairs(made) do
        side.list = opened:list(side.tile, 'BM_list_template')
        if not side.list then
            opened:close()
            return false
        end
    end
    shown, sides = opened, made
    for _, side in pairs(made) do
        side.list.onClick(function(row) activateRow(side, row) end)
    end
    -- (a click on the title goes to the next tab, as on the arrow to the right of it)
    opened:onClick('BM_Items_LeftFilterArrow', function() cycleFilter(made.player, -1) end)
    opened:onClick('BM_Items_RightFilterArrow', function() cycleFilter(made.player, 1) end)
    opened:onClick('BM_ItemsTitle', function() cycleFilter(made.player, 1) end)
    opened:onClick('BM_Container_LeftFilterArrow', function() cycleFilter(made.vendor, -1) end)
    opened:onClick('BM_Container_RightFilterArrow', function() cycleFilter(made.vendor, 1) end)
    opened:onClick('BM_ContainerTitle', function() cycleFilter(made.vendor, 1) end)
    opened:onClick('BM_ButtonX', accept)
    opened:onClick('BM_ButtonB', exit)
    return true
end

-- The picture of the thing the pointer (or the pick) is on, and its cards: the picture is the record's inventory icon, shown
-- in the file's BM_ItemIcon while a thing is picked
local function showItem(item)
    cards.show(shown, item)
    local icon = item and item.icon or ''
    shown:batch(function()
        shown:set('BM_ItemIcon', 'filename', icon)
        shown:set('BM_ItemIcon', 'visible', icon ~= '' and 1 or 0)
    end)
end

-- The cards of the thing the pointer (or the pick) is on: the list that was moved last
local focusSide = nil

-- Which list has the pointer: the file's `_IsContainerListSelected` is 0 for the player's list and 1 for the vendor's (the
-- code sets it). Two hotrects, each over one half of the menu (alpha 0, on top of the lists), are targets only for the half
-- that is not selected, and "mousing over them switches the focus": until the pointer has come to the half it is on, the
-- list below cannot be hovered or clicked. The keys that go to the other list switch it too (Mac round 12AI B8).
-- What the focus does is written to the log for the first events, to find out why the player's list took no pointer in
-- the running game while the offline run of the same code (the real menu code under a mock of the engine) did (Mac round
-- 12AJ C): the value the file reads, the targets of the two rects and what the two lists answer
local traced, traceLimit = 0, 120
local function trace(text)
    if traced < traceLimit then
        traced = traced + 1
        print('Barter focus: ' .. text .. (traced == traceLimit and ' (further events are not logged)' or ''))
    end
end

local function readTrait(tile, trait)
    local ok, value = pcall(shown.get, shown, tile, trait)
    return ok and tostring(value) or 'error'
end

local function traceState(text)
    trace(string.format('%s: io=%s, items rect target=%s, container rect target=%s, items list enabled=%s, container list enabled=%s',
        text, tostring(shown.menu:getNumber(shown.menu:root(), '_IsContainerListSelected')),
        readTrait('BM_Items_FocusRect', 'target'), readTrait('BM_Container_FocusRect', 'target'),
        readTrait('BM_Items_InventoryList', '_enabled'), readTrait('BM_Container_InventoryList', '_enabled')))
end

local function setListFocus(name)
    if not shown or listFocus == name then return end
    listFocus = name
    shown:io('_IsContainerListSelected', name == 'vendor' and 1 or 0)
    traceState('focus ' .. name)
    -- The pointer is over the list now, not over the rect that was in the way
    local at = shown:pointerAt()
    if at then
        shown.menu:pointerMoved(at.x, at.y)
        shown:refresh()
    end
end

local lastOverName = nil
local function followPointer()
    if not shown then return end
    if shown.over ~= nil then
        local ok, name = pcall(shown.menu.name, shown.menu, shown.over)
        name = ok and name or '?'
        if name ~= lastOverName and (name:find('^BM_Items') or name:find('^BM_Container') or name:find('FocusRect')) then
            lastOverName = name
            traceState('pointer over ' .. name .. ' (list focus ' .. tostring(listFocus) .. ', items rect mouseover '
                .. readTrait('BM_Items_FocusRect', 'mouseover') .. ', container rect mouseover '
                .. readTrait('BM_Container_FocusRect', 'mouseover') .. ')')
        end
    end
    if shown:get('BM_Items_FocusRect', 'mouseover') == 1 then
        setListFocus('player')
    elseif shown:get('BM_Container_FocusRect', 'mouseover') == 1 then
        setListFocus('vendor')
    end
end
local function updateCards()
    if not shown then return end
    for name, side in pairs(sides) do
        local row = side.list.selected()
        if row ~= lastRows[name] then
            lastRows[name] = row
            if row > 0 then
                focusSide = name
                active = name
            end
        end
    end
    local name = focusSide or active
    local side = sides[name]
    local row = side and side.list.selected() or 0
    if row <= 0 then
        name = name == 'player' and 'vendor' or 'player'
        side = sides[name]
        row = side and side.list.selected() or 0
        if row > 0 then
            focusSide = name
            active = name
        end
    end
    if row <= 0 then
        if cardRow ~= 0 then
            cardSide, cardRow = nil, 0
            showItem(nil)
        end
        return
    end
    if cardSide ~= name or cardRow ~= row then
        cardSide, cardRow = name, row
        local entry = side.listed and side.listed[row]
        showItem(entry and entry.item or nil)
    end
end

-- The keys while the menu is there: Left and Right go to the other list, the arrows and pages pick a thing, Enter moves
-- it; the letters the file gives its buttons (A for Accept, X for Exit) click them
local function press(code, symbol)
    if not isOpen() or I.UI.getMode() ~= 'Interface' or messageOpen() then return end
    if quantity.isOpen() then
        quantity.press(code, symbol)
        return
    end
    local arrow = code == input.KEY.UpArrow or code == input.KEY.DownArrow or code == input.KEY.LeftArrow
        or code == input.KEY.RightArrow
    if arrow and sides.player.list.selected() <= 0 and sides.vendor.list.selected() <= 0 then
        -- Nothing is picked: the first arrow picks the first row of the first list that has any, whichever it is (the
        -- executable finds the tile with the highest `xdefault`, and every row has 1)
        for _, name in ipairs({ 'player', 'vendor' }) do
            if sides[name].list.count() > 0 then
                active, focusSide = name, name
                setListFocus(name)
                sides[name].list.select(1, true)
                shown:playSound(ROW_FOCUS)
                updateCards()
                return
            end
        end
        return
    end
    local side = sides[active]
    local count = side.list.count()
    local picked = side.list.selected()
    if code == input.KEY.LeftArrow or code == input.KEY.RightArrow then
        active = active == 'vendor' and 'player' or 'vendor'
        focusSide = active
        setListFocus(active)
        local now = sides[active]
        if now.list.count() > 0 and now.list.selected() <= 0 then
            now.list.select(1, true)
            shown:playSound(ROW_FOCUS)
        end
        updateCards()
    elseif code == input.KEY.DownArrow or code == input.KEY.PageDown then
        local by = code == input.KEY.DownArrow and 1 or 8
        if count > 0 then
            side.list.select(picked <= 0 and 1 or math.min(count, picked + by), true)
            shown:playSound(ROW_FOCUS)
        end
        updateCards()
    elseif code == input.KEY.UpArrow or code == input.KEY.PageUp then
        local by = code == input.KEY.UpArrow and 1 or 8
        if count > 0 then
            side.list.select(picked <= 0 and count or math.max(1, picked - by), true)
            shown:playSound(ROW_FOCUS)
        end
        updateCards()
    elseif code == input.KEY.Enter then
        if picked > 0 then
            shown:playSound(ROW_CLICK)
            activateRow(side, picked)
        end
    elseif symbol then
        shown:pcButton(symbol)
    end
end

-- On the pad X accepts the deal, as the file ties it to the button; the rest are the keys the buttons stand for (the
-- engine leaves the screen with B and clicks with A)
local function onPad(button)
    if not isOpen() or I.UI.getMode() ~= 'Interface' or messageOpen() then return end
    if button == input.CONTROLLER_BUTTON.X then
        press(nil, 'a')
        return
    end
    local code = pad.key(button)
    if code ~= nil then
        pad.log('barter', button, code)
        press(code)
    end
end

local function sync()
    local menu = pipboy.barter()
    if not menu then
        if shown or modeOpen then
            destroyMenu()
            closeMode()
            shownSerial = nil
            clearPiles()
        end
        return
    end
    if menu.serial ~= shownSerial then
        shownSerial = menu.serial
        traded = false
        clearPiles()
        active, focusSide = 'player', nil
        lastRows = { player = 0, vendor = 0 }
        destroyMenu()
        openMode()
        refreshStock()
        if not openMenu() then
            -- The game's files have no such menu: the deal is not offered
            print('barter: the file ' .. FILE .. ' is not there; the menu is not shown')
            pipboy.closeBarter()
            shownSerial = nil
            closeMode()
            return
        end
        fillAll()
        -- The player's list is the one with the focus to begin with (the first arrow key picks its first row)
        listFocus = nil
        setListFocus('player')
    end
    if shown then
        shown:checkSize()
        quantity.update()
        followPointer()
        updateCards()
    end
end

return {
    engineHandlers = {
        onFrame = sync,
        onKeyPress = function(key)
            -- (the exe's menu has no key of its own: the file's letters and the generic keys)
            press(key.code, key.symbol)
        end,
        onControllerButtonPress = onPad,
    },
    eventHandlers = {
        -- However the screen is left (the button, Escape, a script), what was open is closed, and the dialogue under it is
        -- back
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyMenu()
                pipboy.closeBarter()
                shownSerial = nil
                clearPiles()
            end
        end,
    },
}
