-- The items page of the Pip-Boy (INV) in Fallout 3 and New Vegas, drawn from the game's own menu file
-- (main/inventory_menu.xml): the headline with the weight, health, resistance and caps cards, the list of the things the
-- player carries with the marker of what is worn, the cards of the thing the pointer is on, and the tabs of the kinds of
-- thing (weapons, apparel, aid, miscellaneous, ammunition) as the game's tabline. What the player carries and what a
-- click does is the engine's (core.pipboy); this module lists it and gives the choices of the player back. It is a module
-- of falloutpipboy.lua, not a script.
--
-- Repair opens the game's own repair screen (falloutpipboyrepair.lua) for the weapon or armor under the pointer, and Mod
-- (New Vegas, the weapons tab) the item mod screen (falloutpipboymods.lua) for the weapon under the pointer.
--
-- Not there yet: the picture of the thing (the games show the model of it).
--
--   local page = require('scripts.omw.falloutpipboyitems')
--   page.show()            -- true when the page is on the screen, false when the menu file is not there
--   page.press(keyCode)    -- true when the key was for the page
--   page.update()          -- each frame while the page is shown
--   page.hide()
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')

local menuModule = require('scripts.omw.falloutmenu')
local hotKeys = require('scripts.omw.falloutmenuhotkeys')
local cards = require('scripts.omw.falloutitemcards')
local quantity = require('scripts.omw.falloutquantity')
local repair = require('scripts.omw.falloutpipboyrepair')
local mods = require('scripts.omw.falloutpipboymods')

-- A note to the player goes to the line of messages of the display, as the games print it (the engine's own message
-- window is the fallback)
local function tellPlayer(text)
    if core.hud and core.hud.message then
        core.hud.message(text)
    else
        ui.showMessage(text)
    end
end

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

-- What a row of the list says: the name, a plus when mods are installed in it, and how many there are of it (the games'
-- formats "%s+" and "%s+ (%d)", FalloutNV.exe 0x7830D0 and 0x782850)
local function rowText(item)
    return item.name .. ((item.mods or 0) ~= 0 and '+' or '') .. (item.count > 1 and (' (' .. item.count .. ')') or '')
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
    -- (the threshold is written with one decimal whatever it is: FalloutNV.exe 0x782B73, "%1.1f")
    setCard('IM_Headline_PlayerDTInfo', string.format('%.1f', stats.threshold or 0))
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

-- The button that repairs is lit for a weapon or armor that something can repair, and says what the executable says for the
-- thing under the pointer: "maintain" when its health is three quarters (armor: a half) or better, else "repair" (FalloutNV.exe
-- 0x780FF0 and 0x781860)
local function updateRepairButton(item)
    if not shown:find('IM_RepairButton') then return end
    local thing = item ~= nil and not item.keyring and not keyring
    local can = thing and item.hasCondition == true and pipboy.canRepair(item.id) or false
    local health = (thing and item.hasCondition) and (item.condition or 1000) / 10 or 100
    local limit = (thing and item.category == 'apparel') and 50 or 75
    shown:set('IM_RepairButton', 'target', can)
    if thing then
        shown:set('IM_RepairButton', 'string',
            health >= limit and word('sMaintainItem', 'MAINTAIN') or word('sRepairItem', 'REPAIR'))
    end
end

-- The button of the mods is there on the weapons tab (the executable shows it for that tab alone and keeps it as it was on
-- the ammunition one) and is lit when a weapon is picked (0x780140, 0x781680). Fallout 3 has no mods.
local function updateModButton(item)
    if not shown:find('IM_ModButton') then return end
    local on = pipboy.modOpen ~= nil and pipboy.stats().newVegas == true
    local tab = TABS[tabIndex].key
    if tab == 'weapons' then
        shown:set('IM_ModButton', 'visible', on and not keyring)
    elseif tab ~= 'ammo' then
        shown:set('IM_ModButton', 'visible', false)
    end
    shown:set('IM_ModButton', 'target', on and item ~= nil and not item.keyring and item.category == 'weapons')
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
        updateRepairButton(item)
        updateModButton(item)
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

-- ---- The hot keys ----
-- As the executable's items menu does it (FalloutNV.exe 0x781BA0 and 0x780140; Mac round 12AO B2): a number key that is
-- pressed or held shows the wheel of the eight slots (the file's IM_HotKeyWheel) with that slot lit; while the wheel is up,
-- picking a thing (a click or Enter) puts it on the lit slot instead of using it; when no number key is down any more the
-- wheel goes. There is no message and no sound for a thing put on a slot, and a thing already on a slot moves to the new one.
-- The second key is the swap of ammunition and takes nothing.
local wheelSlot = nil
-- Frames to wait before the keys are looked at, so that a key that is let go at once still shows the wheel for a frame
local wheelGrace = 0

-- A refusal: the words of the game's setting and the sound the executable makes (UIVATSInsufficientAP)
local function refuse(setting, fallback)
    local text = pipboy.text(setting)
    tellPlayer(text ~= '' and text or fallback)
    pipboy.playSoundNamed('UIVATSInsufficientAP')
end

-- The slot (counted from 0) of a number key; nil for any other key
local function slotOfKey(code)
    for slot = 0, hotKeys.SLOTS - 1 do
        if code == input.KEY['_' .. (slot + 1)] then return slot end
    end
    return nil
end

-- The slot whose key is down now (the swap of ammunition is no slot to choose)
local function heldSlot()
    for slot = 0, hotKeys.SLOTS - 1 do
        if slot ~= hotKeys.AMMO_SWAP and input.isKeyPressed(input.KEY['_' .. (slot + 1)]) then return slot end
    end
    return nil
end

-- The wheel as the file wants it: shown with the lit slot, the name and the picture of what the lit slot holds, and in
-- each slot the picture of its thing (an empty slot has none)
local function drawWheel()
    if not shown or not shown:find('IM_HotKeyWheel') then return end
    local rows = wheelSlot ~= nil and pipboy.hotKeys() or {}
    local lit = rows[(wheelSlot or 0) + 1]
    local named = lit ~= nil and lit.id ~= '' and lit or nil
    local litIcon = named and hotKeys.iconPath(named.icon) or nil
    shown:batch(function()
        shown:set('IM_HotKeyWheel', '_SelectedHotkey', wheelSlot or -1)
        shown:set('IM_HotKeyWheel', '_SelectedText', named and hotKeys.itemText(named) or '')
        if shown:find('IM_HotkeyIcon') then
            shown:set('IM_HotkeyIcon', 'filename', litIcon or '')
            shown:set('IM_HotkeyIcon', 'visible', litIcon ~= nil)
        end
        for slot = 0, hotKeys.SLOTS - 1 do
            local name = 'HK_Item_' .. slot
            if slot ~= hotKeys.AMMO_SWAP and shown:find(name) then
                local row = rows[slot + 1]
                local icon = row ~= nil and row.id ~= '' and hotKeys.iconPath(row.icon) or nil
                shown:set(name, '_HotKeyIcon', icon or '')
                shown:set(name, '_HotKeyAssigned', icon ~= nil and 1 or 0)
            end
        end
        shown:set('IM_HotKeyWheel', 'visible', wheelSlot ~= nil)
    end)
end

-- Show the wheel with the slot lit, or take it away (nil)
local function setWheel(slot)
    wheelSlot = slot
    drawWheel()
end

-- The thing that was picked goes on the lit slot. Ammunition does nothing at all; a thing that cannot be used (or a
-- book) is refused with the words of the game's setting and its sound.
local function assignToWheel(item)
    if not item or item.keyring or wheelSlot == nil then return end
    if item.category == 'ammo' then return end
    if item.book or not equippable(item) then
        refuse('sCantHotkeyItem', 'This item cannot be assigned to a hotkey.')
        return
    end
    if pipboy.assignHotKey(wheelSlot, item.id) == 'cannot' then
        refuse('sCantHotkeyItem', 'This item cannot be assigned to a hotkey.')
    end
    drawWheel()
end

-- Each frame: the wheel stays while a number key is down, and the lit slot follows the key
local function updateWheel()
    if wheelSlot == nil then return end
    if wheelGrace > 0 then
        wheelGrace = wheelGrace - 1
        return
    end
    local held = heldSlot()
    if held ~= wheelSlot then setWheel(held) end
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
    -- With the wheel of the hot keys up the thing goes on the lit slot, and is not used
    if wheelSlot ~= nil then
        assignToWheel(item)
        return
    end
    if item.keyring then
        keyring = true
        shown:io('_KeyringOpen', true)
        fill()
        if list.count() > 0 then list.select(1, true) end
        return
    end
    -- A book is read like any other thing that is used: the points of a skill book, the book used up and the message
    -- are given at once and the Pip-Boy stays as it is. The game has no screen of the book for it (Mac round 12AV S3:
    -- the screen of a book is opened only by the console)
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

-- The screen of the repair (or of the mods) is for the copy that is picked; when the player is done the page of the items comes
-- back with that copy picked (its row is the one it has now)
local function backFromRepair(id)
    if not module.show() then return end
    local text = tostring(id or '')
    local form = text:match('^(.-)~') or text
    -- The copy as it is now if the list has it, else the first copy of the item
    local found
    for index, item in ipairs(listed) do
        if item.id == text then
            found = index
            break
        end
        if item.form == form then found = found or index end
    end
    if found then list.select(found, true) end
    cardRow = nil
end

local function openRepair()
    if keyring or not shown then return end
    local item = listed[list.selected()]
    if not item or item.keyring or not item.hasCondition or not pipboy.canRepair(item.id) then return end
    pipboy.playSoundNamed('UIMenuMode')
    quantity.close()
    shown:close()
    shown, list, tabs = nil, nil, nil
    cardRow = nil
    if not repair.show(item.id, { close = backFromRepair }) then
        module.show()
    end
end

local function openMods()
    if keyring or not shown or pipboy.modOpen == nil then return end
    local item = listed[list.selected()]
    if not item or item.keyring or item.category ~= 'weapons' then return end
    pipboy.playSoundNamed('UIMenuMode')
    quantity.close()
    local id = item.id
    shown:close()
    shown, list, tabs = nil, nil, nil
    cardRow = nil
    if not mods.show(id, { close = backFromRepair }) then
        module.show()
    end
end

local function open()
    local opened = menuModule.open(MENU_FILE, { box = menuModule.PIPBOY_BOX })
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
    -- Repair and Mod are the game's own screens. No cards until a thing is picked.
    opened:onClick('IM_RepairButton', openRepair)
    opened:onClick('IM_ModButton', openMods)
    cards.show(opened, nil)
    return true
end

function module.isOpen()
    return shown ~= nil or repair.isOpen() or mods.isOpen()
end

-- Show the page (open the menu if it is not open) with what the player carries now; false when it cannot be shown
function module.show()
    -- The repair screen and the mod screen are the page while they are up
    if repair.isOpen() or mods.isOpen() then return true end
    if not shown and not open() then
        return false
    end
    fill()
    if list.count() > 0 and list.selected() <= 0 then list.select(1, true) end
    return true
end

function module.hide()
    repair.hide()
    mods.hide()
    quantity.close()
    if shown then
        shown:close()
    end
    shown, list, tabs = nil, nil, nil
    keyring = false
    cardRow = nil
    wheelSlot, wheelGrace = nil, 0
end

-- Each frame the page is shown: the window size, the wheel of the hot keys, and the cards of what the pointer is on
function module.update()
    if repair.isOpen() then
        repair.update()
        return
    end
    if mods.isOpen() then
        mods.update()
        return
    end
    if not shown then return end
    shown:checkSize()
    updateWheel()
    quantity.update()
    updateCards()
end

-- What a key does while the page is there: the arrows and pages pick a thing, Enter uses it, D drops it, [ and ] go to
-- the tab before and after, Backspace closes the keyring. False when the key is not the page's (Left and Right change
-- the page of the Pip-Boy).
function module.press(code, symbol)
    if repair.isOpen() then return repair.press(code, symbol) end
    if mods.isOpen() then return mods.press(code, symbol) end
    if not shown then return false end
    if quantity.isOpen() then
        return quantity.press(code, symbol)
    end
    -- The number keys are the keys of the slots: one that is pressed or held shows the wheel with its slot lit (not on the
    -- keyring, and the second key is the ammunition swap, which takes nothing); the wheel goes when no number key is down
    local slot = slotOfKey(code)
    if slot ~= nil then
        if not keyring and slot ~= hotKeys.AMMO_SWAP then
            wheelGrace = 1
            if wheelSlot ~= slot then setWheel(slot) end
        end
        return true
    end
    -- R is the repair button: it acts when it is lit, and makes the sound of a refusal when it is not
    if symbol == 'r' and not keyring then
        if not shown:pcButton('r') then pipboy.playSoundNamed('UIMenuCancel') end
        return true
    end
    -- X is the button of the mods, there on the weapons tab of New Vegas
    if symbol == 'x' and not keyring and pipboy.modOpen ~= nil and TABS[tabIndex].key == 'weapons' then
        if not shown:pcButton('x') then pipboy.playSoundNamed('UIMenuCancel') end
        return true
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
