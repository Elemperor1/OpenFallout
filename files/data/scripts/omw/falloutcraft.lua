-- Making things at a workbench, a reloading bench or a campfire in New Vegas. Which menu is open (a script of the bench
-- shows it with ShowRecipeMenu), what each recipe takes and gives, and the making itself are the engine's (core.pipboy);
-- this script lists the recipes and gives the choices of the player back.
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
-- The sub-category the list is of (nil for all), the recipe that is picked, and the words after the last making
local tab = nil
local selected = nil
local message = ''
-- The first line of the list that is shown, and how many lines fit the screen
local offset = 1
local listLines = 10
local listLength = 0
-- The sub-categories and the recipes of the list as the last drawing had them, for the keys
local subNames, listedRecipes = {}, {}

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
    pipboy.closeRecipes()
    shownSerial = nil
    closeMode()
end

local function skillName(number)
    for _, skill in ipairs(pipboy.skills()) do
        if skill.number == number then
            return skill.name
        end
    end
    return 'Skill'
end

local function countText(item)
    return string.format('%s x%d (%d)', item.name, item.count, item.have)
end

-- What the recipe needs of each item in all: an item it names more than once is needed for each mention at once
local function neededOf(ingredients)
    local total = {}
    for _, item in ipairs(ingredients) do
        total[item.id] = (total[item.id] or 0) + item.count
    end
    return total
end

-- `needed` is what the recipe needs of each item for an ingredient, nil for what it makes
local function itemLine(item, needed)
    local color = (needed and item.have < (needed[item.id] or item.count)) and colors.badColor or colors.normalColor
    return widgets.label(countText(item), color, textSize - 2)
end

-- Makes the recipe and says how it went
local function make(recipe)
    local outcome = pipboy.craft(recipe.id)
    if outcome == 'made' then
        message = recipe.name .. ' made'
    elseif outcome == 'skill' then
        message = 'Your skill is too low'
    elseif outcome == 'items' then
        message = 'You do not have enough'
    end
    render()
end

function render()
    destroyElement()
    local menu = pipboy.recipes()
    if not menu then
        return
    end
    widgets = widgetModule.new(update)
    local width = math.max(560, math.floor(ui.screenSize().x * 0.45))
    local rows = { { props = { size = v2(width, 0) } } }
    local function add(layout)
        rows[#rows + 1] = { props = { size = widgetModule.gap } }
        rows[#rows + 1] = layout
    end
    add(widgets.label(menu.name ~= '' and menu.name or 'Recipes', colors.titleColor, textSize + 4))

    -- The sub-categories, in the order the recipes name them
    local subs, seen = {}, {}
    for _, recipe in ipairs(menu.recipes) do
        if recipe.subCategoryName ~= '' and not seen[recipe.subCategoryName] then
            seen[recipe.subCategoryName] = true
            subs[#subs + 1] = recipe.subCategoryName
        end
    end
    if #subs > 0 then
        local cells = {
            widgets.cell(widgets.clickable('All', function()
                tab, offset = nil, 1
                render()
            end, { color = tab == nil and colors.pickedColor or colors.normalColor }), 70),
        }
        for _, name in ipairs(subs) do
            cells[#cells + 1] = widgets.cell(widgets.clickable(name, function()
                tab, offset = name, 1
                render()
            end, { color = tab == name and colors.pickedColor or colors.normalColor }), 110)
        end
        add(widgets.row(table.unpack(cells)))
    end

    subNames = subs
    local listed = {}
    for _, recipe in ipairs(menu.recipes) do
        if tab == nil or recipe.subCategoryName == tab then
            listed[#listed + 1] = recipe
        end
    end
    listedRecipes = listed
    listLength = #listed
    if #listed == 0 then
        add(widgets.label('Nothing to make', colors.hintColor))
    end
    local pickedRecipe = nil
    for _, recipe in ipairs(listed) do
        if recipe.id == selected then pickedRecipe = recipe end
    end
    if pickedRecipe == nil then
        pickedRecipe = listed[1]
        selected = pickedRecipe and pickedRecipe.id or nil
    end
    if offset > math.max(1, #listed - listLines + 1) then
        offset = math.max(1, #listed - listLines + 1)
    end
    for index = offset, math.min(#listed, offset + listLines - 1) do
        local recipe = listed[index]
        local color = recipe.id == selected and colors.pickedColor
            or (recipe.status == 'ready' and colors.normalColor or colors.dimColor)
        add(widgets.clickable(recipe.name, function()
            selected = recipe.id
            message = ''
            render()
        end, { color = color }))
    end
    if #listed > listLines then
        add(widgets.label(string.format('%d-%d of %d (wheel, Page Up/Down)', offset,
            math.min(#listed, offset + listLines - 1), #listed), colors.hintColor, textSize - 4))
    end

    if pickedRecipe then
        add(widgets.label('', colors.hintColor, textSize - 6))
        if pickedRecipe.skill >= 0 and pickedRecipe.level > 0 then
            add(widgets.label(string.format('%s %d (yours %d)', skillName(pickedRecipe.skill), pickedRecipe.level,
                math.floor(pickedRecipe.skillValue + 0.5)),
                pickedRecipe.status == 'skill' and colors.badColor or colors.normalColor, textSize - 2))
        end
        local needed = neededOf(pickedRecipe.ingredients)
        for _, item in ipairs(pickedRecipe.ingredients) do
            add(itemLine(item, needed))
        end
        add(widgets.label('Makes', colors.hintColor, textSize - 4))
        for _, item in ipairs(pickedRecipe.outputs) do
            add(itemLine(item, nil))
        end
    end
    if message ~= '' then
        add(widgets.label(message, colors.titleColor, textSize - 2))
    end
    add(widgets.row(
        widgets.cell(widgets.clickable('Make', function()
            make(pickedRecipe)
        end, { enabled = pickedRecipe ~= nil and pickedRecipe.status == 'ready', color = colors.titleColor }), 140),
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

-- The recipe that is picked in the list of the drawing: its place
local function pickedIndex()
    for index, recipe in ipairs(listedRecipes) do
        if recipe.id == selected then
            return index
        end
    end
    return 1
end

-- Picks the next recipe (down when `by` is positive), and moves the list with it when the recipe is not shown
local function moveRecipe(by)
    if not element or #listedRecipes == 0 then
        return
    end
    local index = math.max(1, math.min(#listedRecipes, pickedIndex() + by))
    selected = listedRecipes[index].id
    message = ''
    if index < offset then
        offset = index
    elseif index > offset + listLines - 1 then
        offset = index - listLines + 1
    end
    render()
end

-- Shows the recipes of the next sub-category (after the last one, all of them)
local function moveTab(by)
    if not element or #subNames == 0 then
        return
    end
    local index = 0
    for place, name in ipairs(subNames) do
        if name == tab then
            index = place
        end
    end
    index = (index + by) % (#subNames + 1)
    tab, offset = index > 0 and subNames[index] or nil, 1
    render()
end

-- What a key (or a button of the pad that stands for one) does while the menu is there: up and down pick a recipe,
-- left and right a sub-category, Enter makes the recipe
local function press(code)
    -- (a menu opened over the screen takes the keys and the buttons)
    if not element or I.UI.getMode() ~= 'Interface' then
        return
    end
    if code == input.KEY.DownArrow then
        moveRecipe(1)
    elseif code == input.KEY.UpArrow then
        moveRecipe(-1)
    elseif code == input.KEY.PageDown then
        moveRecipe(listLines)
    elseif code == input.KEY.PageUp then
        moveRecipe(-listLines)
    elseif code == input.KEY.RightArrow then
        moveTab(1)
    elseif code == input.KEY.LeftArrow then
        moveTab(-1)
    elseif code == input.KEY.Enter then
        local recipe = listedRecipes[pickedIndex()]
        if recipe then
            make(recipe)
        end
    end
end

local onPad = pad.handler('craft', function() return element ~= nil and I.UI.getMode() == 'Interface' end, press)

local function sync()
    local menu = pipboy.recipes()
    if not menu then
        if element or modeOpen then
            destroyElement()
            closeMode()
            shownSerial = nil
        end
        return
    end
    if menu.serial ~= shownSerial then
        shownSerial = menu.serial
        offset, tab, selected, message = 1, nil, nil, ''
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
        -- However the screen is left (the button, Escape, a script), the menu is closed
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyElement()
                pipboy.closeRecipes()
                shownSerial = nil
            end
        end,
    },
}
