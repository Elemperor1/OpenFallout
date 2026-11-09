-- Making things at a workbench, a reloading bench or a campfire in New Vegas. Which menu is open (a script of the bench
-- shows it with ShowRecipeMenu), what each recipe takes and gives, and the making itself are the engine's (core.pipboy);
-- this script lists the recipes and gives the choices of the player back.
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

-- The menu is drawn from the game's own file (recipe_menu.xml): the recipes on the left, what the picked one needs and
-- makes on the right. `shown` is that menu while it is there, `recipesList` and `ingredientsList` its two lists.
local MENU_FILE = 'recipe_menu.xml'
local shown, recipesList, ingredientsList = nil, nil, nil
-- The recipes the list shows, and the one that is picked
local listedNow = {}
local pickedRecipeNow = nil
local moveTab

local function update()
    if element then element:update() end
end

local function destroyElement()
    if element then
        element:destroy()
        element = nil
    end
    if shown then
        shown:close()
        shown, recipesList, ingredientsList = nil, nil, nil
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
    if shown and message ~= '' then
        tellPlayer(message)
    end
end

local function renderLegacy()
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


local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function openMenu()
    if shown then
        return true
    end
    local opened = menuModule.open(MENU_FILE)
    if not opened then
        return false
    end
    local recipes = opened:list('RM_Items_InventoryList', 'RM_list_template')
    local ingredients = opened:list('RM_Items_IngredientList', 'RM_list_template')
    if not (recipes and ingredients) then
        opened:close()
        return false
    end
    shown, recipesList, ingredientsList = opened, recipes, ingredients
    -- A click picks the recipe; the Accept button makes it
    recipes.onClick(function(row)
        local recipe = listedNow[row]
        if recipe then
            selected = recipe.id
            message = ''
            render()
        end
    end)
    opened:onClick('RM_Items_LeftFilterArrow', function() moveTab(-1) end)
    opened:onClick('RM_Items_RightFilterArrow', function() moveTab(1) end)
    opened:onClick('RM_ButtonX', function()
        local recipe = pickedRecipeNow
        if recipe and recipe.status == 'ready' then
            make(recipe)
        end
    end)
    opened:onClick('RM_ButtonB', function() leave() end)
    return true
end

local function renderMenu()
    local menu = pipboy.recipes()
    if not menu or not openMenu() then
        return false
    end
    -- The sub-categories, in the order the recipes name them
    local subs, seen = {}, {}
    for _, recipe in ipairs(menu.recipes) do
        if recipe.subCategoryName ~= '' and not seen[recipe.subCategoryName] then
            seen[recipe.subCategoryName] = true
            subs[#subs + 1] = recipe.subCategoryName
        end
    end
    subNames = subs
    local listed = {}
    for _, recipe in ipairs(menu.recipes) do
        if tab == nil or recipe.subCategoryName == tab then
            listed[#listed + 1] = recipe
        end
    end
    listedNow, listedRecipes, listLength = listed, listed, #listed
    local picked = nil
    for _, recipe in ipairs(listed) do
        if recipe.id == selected then picked = recipe end
    end
    if picked == nil then
        picked = listed[1]
        selected = picked and picked.id or nil
    end
    pickedRecipeNow = picked

    local rows = {}
    for index, recipe in ipairs(listed) do
        rows[index] = {
            ['ListItemText.string'] = recipe.name,
            ['ListItemText.alpha'] = recipe.status == 'ready' and 255 or 128,
            _IsRecipeSelected = recipe.id == selected,
        }
    end
    local ingredientRows = {}
    if picked then
        local needed = neededOf(picked.ingredients)
        for index, item in ipairs(picked.ingredients) do
            local short = item.have < (needed[item.id] or item.count)
            ingredientRows[index] = {
                ['ListItemText.string'] = item.name,
                ['ListItemText.alpha'] = short and 128 or 255,
                _Value = string.format('%d/%d', item.have, item.count),
            }
        end
    end
    shown:batch(function()
        local row = recipesList.selected()
        recipesList.rows(rows)
        if row > 0 and #rows > 0 then
            recipesList.select(math.min(row, #rows), true)
        end
        ingredientsList.rows(ingredientRows)
        shown:set('RM_ItemsTitle', 'string', tab or word('sAll', 'All'))
        shown:set('RM_Items_LeftFilterArrow', 'visible', #subs > 0)
        shown:set('RM_Items_RightFilterArrow', 'visible', #subs > 0)
        shown:set('RM_MadeAtVariable', 'string', menu.name)
        local skill = ''
        if picked and picked.skill >= 0 and picked.level > 0 then
            skill = string.format('%s %d (%d)', skillName(picked.skill), picked.level,
                math.floor(picked.skillValue + 0.5))
        end
        shown:set('RM_SkillRequirement', 'string', skill)
        shown:set('RM_ButtonX', 'target', picked ~= nil and picked.status == 'ready')
    end)
    return true
end

function render()
    if not pipboy.recipes() then
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
    if not isOpen() or #listedRecipes == 0 then
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
    if recipesList then
        recipesList.select(index, true)
    end
end

-- Shows the recipes of the next sub-category (after the last one, all of them)
function moveTab(by)
    if not isOpen() or #subNames == 0 then
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
    if not isOpen() or I.UI.getMode() ~= 'Interface' or messageOpen() then
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

local onPad = pad.handler('craft', function() return isOpen() and I.UI.getMode() == 'Interface' and not messageOpen() end, press)

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
    if shown then
        shown:checkSize()
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
