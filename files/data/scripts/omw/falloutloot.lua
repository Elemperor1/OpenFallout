-- Taking things from a container, or from a character that is dead, in Fallout 3 and New Vegas. What is open and what
-- moves from one holder to the other is the engine's (core.pipboy); this script lists the things and gives the choices
-- of the player back. Things can be taken, not put back yet.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

-- A message box drawn over the screen (falloutmessage.lua) has the keys, the pad and the wheel
local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end
local widgetModule = require('scripts.omw.falloutwidgets')
local pad = require('scripts.omw.falloutpad')
local menuModule = require('scripts.omw.falloutmenu')
local quantity = require('scripts.omw.falloutquantity')
local cards = require('scripts.omw.falloutitemcards')

local pipboy = core.pipboy

if not pipboy.available() then
    return {}
end

local v2 = util.vector2
local colors = widgetModule
local textSize = widgetModule.textSize

local element = nil
local modeOpen = false
local shownSerial = nil
-- The first line of the list that is shown, and how many lines fit the screen
local offset = 1
local listLines = 12
local listLength = 0
-- The thing the keys and the pad have picked out (its place in the sorted list), drawn once a key has moved it
local selected, selectFocus = 1, false

local render
local widgets

-- The screen is drawn from the game's own menu file (container_menu.xml): the player's things on the left, the
-- container's on the right. `shown` is that menu while it is there; `sides` the two lists with what each lists.
local MENU_FILE = 'container_menu.xml'
local shown, sides, activeSide = nil, nil, 'container'
-- The thing whose cards are shown: the side and the row
local cardSide, cardRow = nil, nil

-- The tabs the arrows over a list go through: everything, then one kind of thing at a time
local FILTERS = {
    { setting = 'sInventoryItems', fallback = 'Items' },
    { key = 'weapons', setting = 'sInventoryWeapons', fallback = 'Weapons' },
    { key = 'apparel', setting = 'sInventoryApparel', fallback = 'Apparel' },
    { key = 'aid', setting = 'sInventoryAid', fallback = 'Aid' },
    { key = 'misc', setting = 'sInventoryMisc', fallback = 'Misc' },
    { key = 'ammo', setting = 'sInventoryAmmo', fallback = 'Ammo' },
}

local function update()
    if element then element:update() end
end

local function destroyElement()
    if element then
        element:destroy()
        element = nil
    end
    quantity.close()
    if shown then
        shown:close()
        shown, sides, cardSide, cardRow = nil, nil, nil, nil
    end
end

local function isOpen()
    return element ~= nil or shown ~= nil
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

-- The player leaves the screen with its button: what was open is closed here, because the mode is no longer ours
-- when the event that tells of it comes
local function leave()
    destroyElement()
    pipboy.closeLoot()
    shownSerial = nil
    closeMode()
end

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

local function sortedItems(holder)
    local items = pipboy.items(holder)
    table.sort(items, function(a, b)
        local x, y = a.name:lower(), b.name:lower()
        if x ~= y then return x < y end
        return a.id < b.id
    end)
    return items
end

-- Everything in the container goes to the player
local function takeAll()
    local loot = pipboy.loot()
    if not loot then
        return
    end
    local player = pipboy.player()
    for _, item in ipairs(sortedItems(loot.holder)) do
        pipboy.transfer(loot.holder, player, item.id, item.count)
    end
    render()
end

local function renderLegacy()
    destroyElement()
    local loot = pipboy.loot()
    if not loot then
        return
    end
    widgets = widgetModule.new(update)
    local width = math.max(480, math.floor(ui.screenSize().x * 0.4))
    local rows = { { props = { size = v2(width, 0) } } }
    local function add(layout)
        rows[#rows + 1] = { props = { size = widgetModule.gap } }
        rows[#rows + 1] = layout
    end
    local player = pipboy.player()
    local items = sortedItems(loot.holder)
    listLength = #items
    selected = math.max(1, math.min(selected, math.max(1, #items)))
    add(widgets.label(loot.name ~= '' and loot.name or 'Container', colors.titleColor, textSize + 4))
    if #items == 0 then
        add(widgets.label('Empty', colors.hintColor))
    end
    if offset > math.max(1, #items - listLines + 1) then
        offset = math.max(1, #items - listLines + 1)
    end
    for index = offset, math.min(#items, offset + listLines - 1) do
        local item = items[index]
        add(widgets.row(
            widgets.cell(widgets.clickable(item.name .. (item.count > 1 and (' (' .. item.count .. ')') or ''),
                function()
                    pipboy.transfer(loot.holder, player, item.id, item.count)
                    render()
                end, { color = selectFocus and index == selected and colors.pickedColor or colors.normalColor }), 340),
            widgets.cell(widgets.label(number(item.weight * item.count), colors.hintColor), 60),
            widgets.label(number(item.value * item.count), colors.hintColor)))
    end
    if #items > listLines then
        add(widgets.label(string.format('%d-%d of %d (wheel, Page Up/Down)', offset,
            math.min(#items, offset + listLines - 1), #items), colors.hintColor, textSize - 4))
    end
    add(widgets.row(
        widgets.cell(widgets.clickable('Take all', takeAll, { enabled = #items > 0, color = colors.titleColor }), 140),
        widgets.clickable('Close', leave, { color = colors.titleColor })))
    element = ui.create(widgets.panel(rows))
end

local function scroll(by)
    if not element then
        return
    end
    local moved = math.max(1, math.min(math.max(1, listLength - listLines + 1), offset + by))
    if moved ~= offset then
        offset = moved
        -- the thing that was picked out may be out of view now: the keys start again from what is shown
        selectFocus = false
        selected = math.max(offset, math.min(selected, offset + listLines - 1))
        render()
    end
end

-- Moves the pick down (when `by` is positive) or up, and the list with it when the thing is not shown
local function moveSelected(by)
    local loot = element and pipboy.loot()
    if not loot then
        return
    end
    local count = #sortedItems(loot.holder)
    if count == 0 then
        return
    end
    if selectFocus then
        selected = math.max(1, math.min(count, selected + by))
    else
        -- the first press picks the first thing shown (the last one shown when it goes up)
        selectFocus = true
        selected = math.min(count, by > 0 and offset or offset + listLines - 1)
    end
    if selected < offset then
        offset = selected
    elseif selected > offset + listLines - 1 then
        offset = selected - listLines + 1
    end
    render()
end

-- Takes the thing that is picked out, as a click on it does; the first press only shows which one it is
local function takeSelected()
    local loot = element and pipboy.loot()
    if not loot then
        return
    end
    local item = sortedItems(loot.holder)[selected]
    if not item then
        return
    end
    if selectFocus and selected >= offset and selected <= offset + listLines - 1 then
        pipboy.transfer(loot.holder, pipboy.player(), item.id, item.count)
    else
        -- nothing is picked out, or it is not shown: bring the pick into view, and take nothing
        selectFocus = true
        selected = math.max(offset, math.min(selected, offset + listLines - 1))
    end
    render()
end


local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- What a row of a list says: the name, and how many there are of it
local function rowText(item)
    return item.name .. ((item.mods or 0) ~= 0 and '+' or '') .. (item.count > 1 and (' (' .. item.count .. ')') or '')
end

-- The things one side lists, in the order of the name: the player's side leaves out the keys (they are on the keyring)
-- and the armor the files keep from the player, and both leave out what the tab above does not list
local function listedItems(side)
    local listed = {}
    local filter = FILTERS[side.filter]
    for _, item in ipairs(sortedItems(side.holder())) do
        local hide = side.player and (item.key or item.hidden)
        if not hide and (filter.key == nil or item.category == filter.key) then
            listed[#listed + 1] = item
        end
    end
    return listed
end

local function otherSide(side)
    return side == sides.items and sides.container or sides.items
end

-- Make the rows of a side again, with the pick kept where it was
local function fillSide(side)
    side.listed = listedItems(side)
    local rows = {}
    for index, item in ipairs(side.listed) do
        rows[index] = {
            ['ListItemText.string'] = rowText(item),
            ['CM_list_template_ItemMarker.visible'] = item.equipped == true,
        }
    end
    local picked = side.list.selected()
    side.list.rows(rows)
    if picked > 0 and #rows > 0 then
        side.list.select(math.min(picked, #rows), true)
    end
    -- The thing on the same row may be another one now (a filter changed): the cards are made again
    cardSide, cardRow = nil, nil
end

local function setTitles(loot)
    local items, container = sides.items, sides.container
    local itemsFilter, containerFilter = FILTERS[items.filter], FILTERS[container.filter]
    shown:batch(function()
        shown:set('CM_ItemsTitle', 'string', word(itemsFilter.setting, itemsFilter.fallback))
        shown:set('CM_ContainerTitle', 'string',
            container.filter == 1 and (loot.name ~= '' and loot.name or 'Container')
                or word(containerFilter.setting, containerFilter.fallback))
        shown:set('CM_Items_CapsLabel', 'string',
            word('sInventoryCaps', 'Caps') .. ' ' .. number(pipboy.stats().caps))
    end)
end

local function fillAll()
    local loot = pipboy.loot()
    if not (shown and loot) then
        return
    end
    shown:batch(function()
        fillSide(sides.items)
        fillSide(sides.container)
        setTitles(loot)
    end)
    cardSide, cardRow = nil, nil
end

-- Moves some of a thing from one side to the other
local function moveItem(from, item, count)
    local to = otherSide(from)
    if from.player and item.equipped then
        ui.showMessage('Unequip it first.')
        return
    end
    pipboy.transfer(from.holder(), to.holder(), item.id, count)
    fillAll()
end

-- A thing is picked: a stack of several asks how many, else it moves
local function activateRow(side, row)
    local item = side.listed and side.listed[row]
    if not item then
        return
    end
    if item.count > 1 then
        if quantity.ask(item.count, function(count) moveItem(side, item, count) end) then
            return
        end
    end
    moveItem(side, item, item.count)
end

-- The tab of a side goes to the next one (or the one before)
local function cycleFilter(side, by)
    side.filter = (side.filter - 1 + by) % #FILTERS + 1
    local loot = pipboy.loot()
    shown:batch(function()
        fillSide(side)
        if loot then setTitles(loot) end
    end)
end

local function openMenu()
    if shown then
        return true
    end
    local opened = menuModule.open(MENU_FILE)
    if not opened then
        return false
    end
    local made = {
        items = { name = 'CM_Items_InventoryList', filter = 1, player = true,
            holder = function() return pipboy.player() end },
        container = { name = 'CM_Container_InventoryList', filter = 1,
            holder = function() local loot = pipboy.loot() return loot and loot.holder or '' end },
    }
    for _, side in pairs(made) do
        side.list = opened:list(side.name, 'CM_list_template')
        if not side.list then
            opened:close()
            return false
        end
    end
    shown, sides = opened, made
    for _, side in pairs(made) do
        side.list.onClick(function(row) activateRow(side, row) end)
    end
    opened:onClick('CM_Items_LeftFilterArrow', function() cycleFilter(made.items, -1) end)
    opened:onClick('CM_Items_RightFilterArrow', function() cycleFilter(made.items, 1) end)
    opened:onClick('CM_Container_LeftFilterArrow', function() cycleFilter(made.container, -1) end)
    opened:onClick('CM_Container_RightFilterArrow', function() cycleFilter(made.container, 1) end)
    opened:onClick('CM_TakeAllButton', function() takeAll() end)
    opened:onClick('CM_ExitButton', function() leave() end)
    return true
end

local function renderMenu()
    if not openMenu() then
        return false
    end
    fillAll()
    return true
end

function render()
    if not pipboy.loot() then
        destroyElement()
        return
    end
    if element then
        destroyElement()
    end
    if not renderMenu() then
        renderLegacy()
    end
end

-- The cards of the thing the pointer (or the pick) is on. Both lists keep their pick, so the cards follow the list
-- that was moved last (the pointer over a row, a key, or Left and Right to the other list), and stay while none moves.
-- The keys act on that list too (`activeSide`), so Enter moves the thing whose cards are shown.
local focusSide = nil
local function updateCards()
    if not shown then
        return
    end
    for name, side in pairs(sides) do
        local row = side.list.selected()
        if row ~= side.lastRow then
            side.lastRow = row
            if row > 0 then
                focusSide = name
                activeSide = name
            end
        end
    end
    local name = focusSide or activeSide
    local side = sides[name]
    local row = side and side.list.selected() or 0
    if row <= 0 then
        -- the list that was moved last has no pick (it was emptied): the other one's pick is shown
        name = name == 'items' and 'container' or 'items'
        side = sides[name]
        row = side and side.list.selected() or 0
        if row > 0 then
            -- the keys act on the list whose pick is shown
            focusSide = name
            activeSide = name
        end
    end
    if row <= 0 then
        -- neither list has a row to pick: no card
        if cardRow ~= 0 then
            cardSide, cardRow = nil, 0
            cards.show(shown, nil)
        end
        return
    end
    if cardSide ~= name or cardRow ~= row then
        cardSide, cardRow = name, row
        cards.show(shown, side.listed and side.listed[row] or nil)
    end
end

-- The keys while the menu of the file is there: Left and Right go to the other list, the arrows and pages pick a
-- thing, Enter moves it and R takes everything
local function pressMenu(code, symbol)
    if quantity.isOpen() then
        quantity.press(code, symbol)
        return
    end
    local side = sides[activeSide]
    local count = side.list.count()
    local picked = side.list.selected()
    if code == input.KEY.LeftArrow or code == input.KEY.RightArrow then
        activeSide = activeSide == 'container' and 'items' or 'container'
        focusSide = activeSide
        local now = sides[activeSide]
        if now.list.count() > 0 and now.list.selected() <= 0 then
            now.list.select(1, true)
        end
        updateCards()
    elseif code == input.KEY.DownArrow or code == input.KEY.PageDown then
        local by = code == input.KEY.DownArrow and 1 or 8
        if count > 0 then
            side.list.select(picked <= 0 and 1 or math.min(count, picked + by), true)
        end
        updateCards()
    elseif code == input.KEY.UpArrow or code == input.KEY.PageUp then
        local by = code == input.KEY.UpArrow and 1 or 8
        if count > 0 then
            side.list.select(picked <= 0 and count or math.max(1, picked - by), true)
        end
        updateCards()
    elseif code == input.KEY.Enter then
        if picked > 0 then
            activateRow(side, picked)
        end
    elseif code == input.KEY.R then
        takeAll()
    end
end

-- What a key (or a button of the pad that stands for one) does while the screen is there: the arrows and pages pick a
-- thing, Enter takes it and R takes everything
local function press(code, symbol)
    -- (a menu opened over the screen takes the keys and the buttons)
    if not isOpen() or I.UI.getMode() ~= 'Interface' or messageOpen() then
        return
    end
    if shown then
        pressMenu(code, symbol)
        return
    end
    if code == input.KEY.DownArrow then
        moveSelected(1)
    elseif code == input.KEY.UpArrow then
        moveSelected(-1)
    elseif code == input.KEY.PageDown then
        moveSelected(listLines)
    elseif code == input.KEY.PageUp then
        moveSelected(-listLines)
    elseif code == input.KEY.Enter then
        takeSelected()
    elseif code == input.KEY.R then
        takeAll()
    end
end

-- On the pad Y takes the thing and X takes everything
local onPad = pad.handler('loot', function() return isOpen() and I.UI.getMode() == 'Interface' and not messageOpen() end, press,
    { [input.CONTROLLER_BUTTON.X] = input.KEY.R })

local function sync()
    local loot = pipboy.loot()
    if not loot then
        if element or modeOpen then
            destroyElement()
            closeMode()
            shownSerial = nil
        end
        return
    end
    if loot.serial ~= shownSerial then
        shownSerial = loot.serial
        offset = 1
        selected, selectFocus = 1, false
        activeSide = 'container'
        focusSide = nil
        openMode()
        render()
    end
    if shown then
        shown:checkSize()
        quantity.update()
        updateCards()
    end
end

return {
    engineHandlers = {
        onFrame = sync,
        onKeyPress = function(key)
            press(key.code, key.symbol)
        end,
        onControllerButtonPress = onPad,
        onMouseWheel = function(vertical)
            if messageOpen() then
                return
            end
            -- the lists of the file's menu take the wheel themselves
            if shown then
                return
            end
            if vertical > 0 then
                scroll(-1)
            elseif vertical < 0 then
                scroll(1)
            end
        end,
    },
    eventHandlers = {
        -- A button of the pad, sent by a script (the buttons of a real pad come as onControllerButtonPress)
        PadButton = function(data)
            onPad(data.button)
        end,
        -- However the screen is left (the button, Escape, a script), what was open is closed, and the scripts of the
        -- container hear of it
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyElement()
                pipboy.closeLoot()
                shownSerial = nil
            end
        end,
    },
}
