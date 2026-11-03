-- The repair screen of the Pip-Boy in New Vegas and Fallout 3 (the game's repair_menu.xml), opened from the items page for
-- one weapon or piece of armor of the player: the list has that item first and, under it, each thing that can repair it
-- (another copy of the same item that is not on, or a thing that the item's repair list names); one click on a thing repairs the
-- item with it and uses the thing up. What the screen shows and where is the file's; what it says and does is the
-- executable's (FalloutNV.exe 0x7B5400..0x7B7F70, Mac rounds 12AW S12 and 12AX R1). The engine knows what repairs what and
-- how far (core.pipboy.repairView and repair); this module draws it and gives the player's choices back. It is a module of
-- falloutpipboyitems.lua, not a script.
--
-- Not there: the pulse of the "select an item" text (what the executable's alpha animation does is a guess), and the pad.
--
--   local page = require('scripts.omw.falloutpipboyrepair')
--   page.show(rowId, { close = function(rowId) ... end })   -- true when the screen is up
--   page.press(keyCode, symbol)  -- true when the key was for the screen
--   page.update()                -- each frame while it is shown
--   page.hide()
local core = require('openfallout.core')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')
local hotKeys = require('scripts.omw.falloutmenuhotkeys')

local pipboy = core.pipboy

local module = {}

local MENU_FILE = 'repair_menu.xml'

local shown, list = nil, nil
-- The view of the engine (the item and the things that repair it), the rows of the list and the row the cards are for
local view, rows, cardRow = nil, {}, nil
local options = {}

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- A repaired copy that is at least this good (tenths of a percent) leaves the screen: it has nothing more to mend (0x1075D30,
-- 99 percent)
local DONE_AT = 990
-- How many rows a Page key moves: the height of the list over the height of a row (468 / 26)
local PAGE = 18

-- The numbers of an item at a condition as the cards tell them (the stat is what the item is: a weapon's damage, armor's
-- threshold, or its resistance when it has none): the word of the setting, the number, and the number for the improvement
-- (0 for a dash). `preview` is the stat that the other card shows: the executable decides armor by what the repaired item
-- would have.
local function statOf(stat, previewStat)
    if stat.damage > 0 then
        return { title = word('sInventoryDamage', 'DAM'), value = stat.damage }
    end
    local basis = previewStat or stat
    if basis.threshold > 0 then
        return { title = word('sInventoryDamageThreshold', 'DT'), value = stat.threshold }
    elseif basis.rating > 0 then
        return { title = word('sInventoryDamageResistance', 'DR'), value = stat.rating }
    end
    return { title = word('sInventoryDamageResistance', 'DR'), value = 0 }
end

local function statText(card)
    return card.value > 0 and string.format('%d', card.value) or '--'
end

local function setCard(name, traits)
    if shown:find(name) then
        for trait, value in pairs(traits) do shown:set(name, trait, value) end
    end
end

local function setText(name, text, visible)
    if not shown:find(name) then return end
    shown:set(name, 'string', text)
    if visible ~= nil then shown:set(name, 'visible', visible) end
end

local function percent(condition)
    return condition / 10
end

-- The row of a copy: the name, the health bar (60 pixels for the whole health), the mark of what is worn, and the health the
-- repair would leave (for the cards)
local function rowOf(entry, first)
    return {
        ['ListItemText.string'] = entry.name,
        ['RM_Template_Meter.width'] = percent(entry.condition) * 0.6,
        ['RM_Template_ItemMarker.visible'] = entry.worn == true,
        _IsRepairItem = first == true,
        user0 = math.min((first and entry.condition or entry.result) / 1000, 1),
    }
end

-- The health card and the stat card of the item itself (the left ones), as the executable sets them on opening and after a
-- repair
local function drawBroken(previewStat)
    local item = view.item
    local card = statOf(item.stat, previewStat)
    shown:batch(function()
        setCard('RM_BrokenItemHealth', { user0 = item.condition / 1000, visible = true })
        setCard('RM_BrokenItemStat', { _Title = card.title, _Value = statText(card) })
        setCard('RM_Headline_PlayerSkillInfo', { _Value = string.format('%d', view.skill) })
    end)
end

-- The brackets round the first row (the item being repaired) follow it when the list scrolls: the file leaves their place to
-- the code (the executable binds them to the row's place and height)
local bracketY, bracketHeight
local function placeBrackets()
    local row = shown:find('RM_RepairListTemplateRect')
    if not row or not shown:find('RM_LeftBracket') then return end
    local y, height = shown:get(row, 'y'), shown:get(row, 'height')
    if y == bracketY and height == bracketHeight then return end
    bracketY, bracketHeight = y, height
    shown:batch(function()
        shown:set('RM_LeftBracket', 'y', y)
        shown:set('RM_LeftBracket', 'height', height)
    end)
end

local function hidePreview()
    shown:batch(function()
        setCard('RM_FixedItemHealth', { visible = false })
        setText('RM_ChooseItemText', '', false)
    end)
end

-- What the pointer (or the pick) is on: the item itself says to choose something to repair it with, a thing that repairs
-- it shows what the item would be (the cards on the right, the gain over the cards on the left)
local function updateCards()
    local row = math.max(list.selected(), 0)
    if row == cardRow then return end
    cardRow = row
    if row <= 0 then
        hidePreview()
        drawBroken(nil)
        return
    end
    if row == 1 then
        shown:batch(function()
            setCard('RM_FixedItemHealth', { visible = false })
            if #rows > 1 then
                setText('RM_ChooseItemText', word('sSelectItemToRepair', 'Select an item to repair with'), true)
            else
                setText('RM_ChooseItemText', '', false)
            end
        end)
        drawBroken(nil)
        return
    end
    local donor = view.donors[row - 1]
    if not donor then return end
    local item = view.item
    local broken = statOf(item.stat, donor.stat)
    local fixed = statOf(donor.stat, donor.stat)
    local health = math.max(0, math.floor(0.5 + (donor.result - item.condition) / 10))
    local gain = math.max(0, math.floor(0.5 + math.max(0, fixed.value - broken.value)))
    shown:batch(function()
        setText('RM_ChooseItemText', '', false)
        setCard('RM_BrokenItemStat', { _Title = broken.title, _Value = statText(broken) })
        setCard('RM_FixedItemHealth', { user0 = donor.result / 1000, visible = true })
        setCard('RM_FixedItemStat', { _Title = fixed.title, _Value = statText(fixed) })
        setText('RM_HealthImprovementText', string.format('+%d%%', health))
        setText('RM_StatImprovementText', string.format('+%d', gain))
    end)
end

local function fill()
    rows = { rowOf(view.item, true) }
    for index, donor in ipairs(view.donors) do
        rows[index + 1] = rowOf(donor, false)
    end
    local picked = list.selected()
    shown:batch(function()
        list.rows(rows)
        if picked > 0 and #rows > 0 then list.select(math.min(picked, #rows), true) end
    end)
    cardRow = nil
    bracketY = nil
end

-- The screen goes and the items page comes back (the sound of the menu mode); `id` is the row of the copy as it is now
local function leave(id)
    local callback = options.close
    module.hide()
    pipboy.playSoundNamed('UIMenuMode')
    if callback then callback(id) end
end

-- A thing is picked: the item is repaired with it unless it is the item itself. The screen goes back to the items when
-- one row is left or the item is as good as it gets.
local function activate(row)
    if row <= 1 then return end
    local donor = view.donors[row - 1]
    if not donor then return end
    local now = pipboy.repair(view.item.id, donor.id)
    if now < 0 then return end
    pipboy.playSoundNamed('UIRepairWeapon')
    local newId = view.item.form .. '~' .. now
    local again = pipboy.repairView(newId)
    if not again or #again.donors == 0 or again.item.condition >= DONE_AT then
        leave(newId)
        return
    end
    view = again
    fill()
    drawBroken(nil)
end

function module.isOpen()
    return shown ~= nil
end

-- Show the screen for the copy that the row names; false when it cannot be shown (the item has nothing to repair it with, or
-- the menu file is not there). `options.close(rowId)` is called, after the screen is gone, with the row of the copy as it is
-- now when the player is done.
function module.show(rowId, opened)
    module.hide()
    local made = pipboy.repairView(rowId)
    if not made then return false end
    local menu = menuModule.open(MENU_FILE, { box = menuModule.PIPBOY_BOX })
    if not menu then return false end
    local shownList = menu:list('RM_RepairList', 'RM_RepairListTemplate')
    if not shownList then
        menu:close()
        return false
    end
    shown, list, view, options = menu, shownList, made, opened or {}
    cardRow = nil
    shown:onClick('RM_CancelButton', function() leave(view and view.item.id) end)
    list.onClick(function(row) activate(row) end)
    fill()
    shown:batch(function()
        local icon = hotKeys.iconPath(view.item.icon)
        setCard('RM_ItemIcon', { filename = icon or '', visible = icon ~= nil })
        setCard('RM_BrokenItemHealth', { visible = true })
    end)
    drawBroken(nil)
    return true
end

function module.hide()
    if shown then shown:close() end
    shown, list, view, rows, cardRow = nil, nil, nil, {}, nil
end

function module.update()
    if not shown then return end
    shown:checkSize()
    placeBrackets()
    updateCards()
end

-- The arrows and the pages pick a row (the first press of a key picks the item itself), Enter repairs with the row that is
-- picked, E is Cancel; other keys are not the screen's (Left and Right change the page of the Pip-Boy)
function module.press(code, symbol)
    if not shown then return false end
    local count = list.count()
    local picked = list.selected()
    local function pick(row)
        if count > 0 then list.select(math.max(1, math.min(count, row)), true) end
        updateCards()
    end
    if code == input.KEY.DownArrow then
        pick(picked <= 0 and 1 or picked + 1)
    elseif code == input.KEY.UpArrow then
        pick(picked <= 0 and 1 or picked - 1)
    elseif code == input.KEY.PageDown then
        pick(picked <= 0 and 1 or picked + PAGE)
    elseif code == input.KEY.PageUp then
        pick(picked <= 0 and 1 or picked - PAGE)
    elseif code == input.KEY.Enter then
        if picked > 0 then activate(picked) end
    elseif symbol == 'e' then
        leave(view.item.id)
    else
        return false
    end
    return true
end

return module
