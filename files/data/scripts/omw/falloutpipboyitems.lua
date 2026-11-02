-- The items page of the Pip-Boy (INV) in Fallout 3 and New Vegas, drawn from the game's own menu file
-- (main/inventory_menu.xml): the headline with the weight, health, resistance and caps cards, the list of the things the
-- player carries with the marker of what is worn, the cards of the thing the pointer is on, and the tabs of the kinds of
-- thing (weapons, apparel, aid, miscellaneous, ammunition) as the game's tabline. What the player carries and what a
-- click does is the engine's (core.pipboy); this module lists it and gives the choices of the player back. It is a module
-- of falloutpipboy.lua, not a script.
--
-- Not there yet: the picture of the thing (the games show the model of it), the hot keys, repair and mods.
--
--   local page = require('scripts.omw.falloutpipboyitems')
--   page.show()            -- true when the page is on the screen, false when the menu file is not there
--   page.press(keyCode)    -- true when the key was for the page
--   page.update()          -- each frame while the page is shown
--   page.hide()
local core = require('openfallout.core')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')
local cards = require('scripts.omw.falloutitemcards')
local quantity = require('scripts.omw.falloutquantity')

local pipboy = core.pipboy

local module = {}

local MENU_FILE = 'main/inventory_menu.xml'

-- The kinds of thing, in the order of the tabs
local TABS = {
    { key = 'weapons', setting = 'sInventoryWeapons', fallback = 'WEAPONS' },
    { key = 'apparel', setting = 'sInventoryApparel', fallback = 'APPAREL' },
    { key = 'aid', setting = 'sInventoryAid', fallback = 'AID' },
    { key = 'misc', setting = 'sInventoryMisc', fallback = 'MISC' },
    { key = 'ammo', setting = 'sInventoryAmmo', fallback = 'AMMO' },
}

local shown, list, tabs = nil, nil, nil
-- The tab that is shown (its place in TABS), what the list holds, and whether the keyring is open
local tabIndex = 1
local listed = {}
local keyring = false
-- The row the cards were last made for
local cardRow = nil

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

local function pairText(now, most)
    return number(now) .. '/' .. number(most)
end

-- The keys are on the keyring, a row at the end of the miscellaneous things that opens it
local function keyringRow()
    return { keyring = true, id = 'keyring', name = word('sKeyring', 'Keyring'), count = 1, equipped = false }
end

-- What the list of a tab holds: the things of the tab (armor the files keep from the player is not there, nor are keys),
-- by name without regard to case, and of two with the same name the one that is worn first. With the keyring open the
-- keys, and nothing else.
local function sortedItems(category)
    local items, keys = {}, false
    for _, item in ipairs(pipboy.items()) do
        if item.key then
            keys = true
            if keyring then items[#items + 1] = item end
        elseif not keyring and not item.hidden and item.category == category then
            items[#items + 1] = item
        end
    end
    table.sort(items, function(a, b)
        local x, y = a.name:lower(), b.name:lower()
        if x ~= y then return x < y end
        if a.equipped ~= b.equipped then return a.equipped end
        return a.id < b.id
    end)
    if keys and not keyring and category == 'misc' then items[#items + 1] = keyringRow() end
    return items
end

-- What a row of the list says: the name, and how many there are of it
local function rowText(item)
    return item.name .. (item.count > 1 and (' (' .. item.count .. ')') or '')
end

-- Whether the thing can be used or put on: the cards show a mark beside it if it can
local function equippable(item)
    return item ~= nil and not item.keyring and not keyring and item.usable ~= false
end

local function setCard(name, value)
    if shown:find(name) then
        shown:set(name, '_Value', value)
    end
end

-- The cards of the headline: weight carried, health, resistance (the threshold in New Vegas) and caps
local function fillHeadline()
    local stats = pipboy.stats()
    local limit = pipboy.setting('iCapsLimit') or 1000000
    setCard('IM_Headline_PlayerWGInfo', pairText(stats.weight, stats.maxWeight))
    setCard('IM_Headline_PlayerHPInfo', pairText(stats.health, stats.maxHealth))
    setCard('IM_Headline_PlayerDRInfo', number(math.floor(stats.armorRating)))
    local threshold = stats.threshold or 0
    setCard('IM_Headline_PlayerDTInfo',
        threshold == math.floor(threshold) and string.format('%d', threshold) or string.format('%.1f', threshold))
    setCard('IM_Headline_PlayerCapsInfo', stats.caps > limit and (string.format('%d', limit) .. '+') or number(stats.caps))
    -- The games have one of the two: resistance in Fallout 3, the threshold in New Vegas
    if shown:find('IM_Headline_PlayerDRInfo') then
        shown:set('IM_Headline_PlayerDRInfo', 'visible', not stats.newVegas)
    end
    if shown:find('IM_Headline_PlayerDTInfo') then
        shown:set('IM_Headline_PlayerDTInfo', 'visible', stats.newVegas == true)
    end
end

-- Make the rows of the list again, with the pick kept on the same thing
local function fill()
    local pickedId
    local pickedRow = list.selected()
    if pickedRow > 0 and listed[pickedRow] then pickedId = listed[pickedRow].id end
    listed = sortedItems(TABS[tabIndex].key)
    local rows = {}
    for index, item in ipairs(listed) do
        rows[index] = {
            ['ListItemText.string'] = rowText(item),
            ['IM_Template_ItemMarker.visible'] = item.equipped == true,
        }
    end
    shown:batch(function()
        shown:io('_KeyringOpen', keyring)
        fillHeadline()
        list.rows(rows)
        local row = 0
        for index, item in ipairs(listed) do
            if item.id == pickedId then row = index end
        end
        if row == 0 and #rows > 0 then row = math.min(math.max(pickedRow, 1), #rows) end
        if row > 0 then list.select(row, true) end
    end)
    cardRow = nil
end

-- The cards, and the mark beside a thing that can be used, for the row that is picked or under the pointer; a list with
-- no row to pick (an empty category) shows no card and nothing to use
local function updateCards()
    local row = math.max(list.selected(), 0)
    if row == cardRow then return end
    cardRow = row
    local item = listed[row]
    shown:batch(function()
        cards.show(shown, item and not item.keyring and item or nil)
        shown:io('_EquippableItem', equippable(item))
    end)
end

-- A stack of more than the setting is asked how many to drop; fewer drop one
local function dropItem(item)
    if keyring or not item or item.keyring then return end
    local threshold = pipboy.setting('iInventoryAskQuantityAt') or 5
    if item.count > threshold then
        local max = math.min(item.count, pipboy.dropLimit)
        if quantity.ask(max, function(count)
            pipboy.drop(item.id, count)
            if shown then fill() end
        end) then
            return
        end
    end
    pipboy.drop(item.id, 1)
    fill()
end

local function pickTab(index)
    if index == tabIndex and not keyring then return end
    tabIndex = index
    keyring = false
    tabs.pick(index)
    fill()
    if list.count() > 0 then list.select(1, true) end
    cardRow = nil
end

-- A thing is picked: the keyring opens, anything else is used (put on, taken off, eaten, read)
local function activate(row)
    local item = listed[row]
    if not item then return end
    if item.keyring then
        keyring = true
        shown:io('_KeyringOpen', true)
        fill()
        if list.count() > 0 then list.select(1, true) end
        return
    end
    if equippable(item) then
        pipboy.use(item.id)
    end
    fill()
end

local function closeKeyring()
    if keyring then
        keyring = false
        fill()
        if list.count() > 0 then list.select(1, true) end
    end
end

local function open()
    local opened = menuModule.open(MENU_FILE)
    if not opened then return false end
    local made = opened:list('IM_InventoryList', 'IM_InventoryListTemplate')
    if not made then
        opened:close()
        return false
    end
    shown, list = opened, made
    local labels = {}
    for index, tab in ipairs(TABS) do
        labels[index] = word(tab.setting, tab.fallback):upper()
    end
    local ok, result = pcall(function()
        return opened:tabs('IM_Tabline', 'TabButtonTemplate', labels, tabIndex, pickTab)
    end)
    if not ok then
        print('inventory menu: ' .. tostring(result))
        opened:close()
        shown, list = nil, nil
        return false
    end
    tabs = result
    list.onClick(function(row) activate(row) end)
    opened:onClick('IM_CancelButton', closeKeyring)
    -- Repair and mods are not there yet: the buttons are not shown; no cards until a thing is picked
    cards.show(opened, nil)
    opened:batch(function()
        for _, name in ipairs({ 'IM_RepairButton', 'IM_ModButton' }) do
            if opened:find(name) then opened:set(name, 'visible', false) end
        end
    end)
    return true
end

function module.isOpen()
    return shown ~= nil
end

-- Show the page (open the menu if it is not open) with what the player carries now; false when it cannot be shown
function module.show()
    if not shown and not open() then
        return false
    end
    fill()
    if list.count() > 0 and list.selected() <= 0 then list.select(1, true) end
    return true
end

function module.hide()
    quantity.close()
    if shown then
        shown:close()
    end
    shown, list, tabs = nil, nil, nil
    keyring = false
    cardRow = nil
end

-- Each frame the page is shown: the window size, and the cards of what the pointer is on
function module.update()
    if not shown then return end
    shown:checkSize()
    updateCards()
end

-- What a key does while the page is there: the arrows and pages pick a thing, Enter uses it, D drops it, [ and ] go to
-- the tab before and after, Backspace closes the keyring. False when the key is not the page's (Left and Right change
-- the page of the Pip-Boy).
function module.press(code)
    if not shown then return false end
    if quantity.isOpen() then
        return quantity.press(code)
    end
    local count = list.count()
    local picked = list.selected()
    if code == input.KEY.DownArrow or code == input.KEY.PageDown then
        local by = code == input.KEY.DownArrow and 1 or 8
        if count > 0 then list.select(picked <= 0 and 1 or math.min(count, picked + by), true) end
        updateCards()
    elseif code == input.KEY.UpArrow or code == input.KEY.PageUp then
        local by = code == input.KEY.UpArrow and 1 or 8
        if count > 0 then list.select(picked <= 0 and count or math.max(1, picked - by), true) end
        updateCards()
    elseif code == input.KEY.Enter then
        if picked > 0 then activate(picked) end
    elseif code == input.KEY.D then
        if picked > 0 then dropItem(listed[picked]) end
    elseif code == input.KEY.LeftBracket then
        pickTab((tabIndex - 2) % #TABS + 1)
    elseif code == input.KEY.RightBracket then
        pickTab(tabIndex % #TABS + 1)
    elseif code == input.KEY.Backspace then
        closeKeyring()
    else
        return false
    end
    return true
end

return module
