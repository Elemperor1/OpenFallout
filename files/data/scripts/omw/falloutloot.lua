-- Taking things from a container, or from a character that is dead, in Fallout 3 and New Vegas. What is open and what
-- moves from one holder to the other is the engine's (core.pipboy); this script lists the things and gives the choices
-- of the player back. Things can be taken, not put back yet.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local widgetModule = require('scripts.omw.falloutwidgets')
local pad = require('scripts.omw.falloutpad')

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

local function update()
    if element then element:update() end
end

local function destroyElement()
    if element then
        element:destroy()
        element = nil
    end
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

function render()
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
    if selectFocus then
        pipboy.transfer(loot.holder, pipboy.player(), item.id, item.count)
    else
        selectFocus = true
    end
    render()
end

-- What a key (or a button of the pad that stands for one) does while the screen is there: the arrows and pages pick a
-- thing, Enter takes it and R takes everything
local function press(code)
    if not element then
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
local onPad = pad.handler('loot', function() return element ~= nil end, press,
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
        openMode()
        render()
    end
end

return {
    engineHandlers = {
        onFrame = sync,
        onKeyPress = function(key)
            press(key.code)
        end,
        onControllerButtonPress = onPad,
        onMouseWheel = function(vertical)
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
