-- The item mod screen of the Pip-Boy in New Vegas (the game's item_mod_menu.xml), opened from the weapons tab of the items
-- page for the weapon under the pointer: the list has the mods that are installed in it (dim, with the marker) and one line
-- for each unit of the mods in the player's pack that the weapon takes in a free slot; one click on a line installs that mod
-- and uses up one copy of the mod item. What the screen shows and where is the file's; what it says and does is the
-- executable's (FalloutNV.exe 0x783310..0x784A40, Mac round 12AX R3 and R3b). The engine knows what a weapon takes and what
-- a mod does (core.pipboy.modOpen, modView, modInstall and modClose); this module draws it and gives the player's choices
-- back. It is a module of falloutpipboyitems.lua, not a script.
--
-- The card on the left of the file tells the condition of the weapon and the damage of a shot with its mods; the cards of
-- the file that would compare a mod with the weapon are never filled by the executable and are not here either. What a
-- mod is for (its description) and its picture show for the line that is under the pointer.
--
-- Not there: the pad.
--
--   local page = require('scripts.omw.falloutpipboymods')
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

local MENU_FILE = 'item_mod_menu.xml'

-- The brightness of a line that cannot be picked (what is installed): half of the full 255 (the executable's 127.5)
local DIM_ALPHA, FULL_ALPHA = 127.5, 255

local shown, list = nil, nil
-- The view of the engine (the weapon and the lines), the line the cards were last made for, and the caller's options
local view, cardRow = nil, nil
local options = {}

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function setCard(name, traits)
    if shown:find(name) then
        for trait, value in pairs(traits) do shown:set(name, trait, value) end
    end
end

-- The weapon: its name, its picture, the card of its condition (the meter reads the share of its health, which a mod of
-- maximum condition can leave above a whole one) and the card of the damage of a shot
local function drawWeapon()
    local item = view.item
    local icon = hotKeys.iconPath(item.icon)
    shown:batch(function()
        setCard('IMM_ItemName', { string = item.name })
        setCard('IMM_ItemIcon', { filename = icon or '', visible = icon ~= nil })
        setCard('IMM_ItemHealth', { user0 = item.condition / 1000, visible = true })
        setCard('IMM_ItemStat', { _Title = word('sInventoryDamage', 'DAM'), _Value = item.damage })
    end)
end

-- What the pointer (or the pick) is on: the words of the mod and its picture, else the picture of the weapon
local function updateCards()
    local row = math.max(list.selected(), 0)
    if row == cardRow then return end
    cardRow = row
    local entry = view.rows[row]
    local icon = hotKeys.iconPath(entry and entry.icon or view.item.icon)
    shown:batch(function()
        setCard('IMM_ModDesc', { string = entry and entry.description or '' })
        setCard('IMM_ItemIcon', { filename = icon or '', visible = icon ~= nil })
    end)
end

local function fill()
    local picked = list.selected()
    local rows = {}
    for index, entry in ipairs(view.rows) do
        rows[index] = {
            ['ListItemText.string'] = entry.name,
            ['ListItemText.alpha'] = entry.installed and DIM_ALPHA or FULL_ALPHA,
            ['IMM_Template_ItemMarker.visible'] = entry.installed == true,
            _IsItemModItem = true,
        }
    end
    shown:batch(function()
        list.rows(rows)
        if picked > 0 and #rows > 0 then list.select(math.min(picked, #rows), true) end
    end)
    cardRow = nil
end

-- The screen goes and the items page comes back (the weapon that was on is put on again); `id` is the row of the weapon as
-- it is now
local function leave()
    local callback = options.close
    local id = view and view.item.id
    module.hide()
    pipboy.playSoundNamed('UIMenuMode')
    if callback then callback(id) end
end

-- A line is clicked: a mod that is installed does nothing (and makes no sound); the other is installed, one copy of the
-- mod item is used up, and the screen stays open with what the weapon is now
local function activate(row)
    local entry = view.rows[row]
    if not entry or entry.installed then return end
    local id = pipboy.modInstall(view.item.id, entry.mod)
    if id == '' then return end
    pipboy.playSoundNamed('UIItemGunsSmallUp')
    local again = pipboy.modView(id)
    if not again then
        leave()
        return
    end
    view = again
    fill()
    drawWeapon()
end

function module.isOpen()
    return shown ~= nil
end

-- Show the screen for the copy that the row names; false when it cannot be shown (the item is no weapon of the player, or
-- the menu file is not there). `options.close(rowId)` is called, after the screen is gone, with the row of the weapon as it is
-- now when the player is done.
function module.show(rowId, opened)
    module.hide()
    local made = pipboy.modOpen(rowId)
    if not made then return false end
    local menu = menuModule.open(MENU_FILE, { box = menuModule.PIPBOY_BOX })
    if not menu then
        pipboy.modClose(made.item.id)
        return false
    end
    local shownList = menu:list('IMM_ItemModList', 'IMM_ItemModListTemplate')
    if not shownList then
        menu:close()
        pipboy.modClose(made.item.id)
        return false
    end
    shown, list, view, options = menu, shownList, made, opened or {}
    cardRow = nil
    shown:onClick('IMM_CancelButton', leave)
    list.onClick(function(row) activate(row) end)
    fill()
    drawWeapon()
    return true
end

-- The screen goes, however it goes (Cancel, another page of the Pip-Boy, the Pip-Boy closed): the engine puts the weapon that
-- was on back on and drops the view
function module.hide()
    if shown then shown:close() end
    if view then pipboy.modClose(view.item.id) end
    shown, list, view, cardRow = nil, nil, nil, nil
end

function module.update()
    if not shown then return end
    shown:checkSize()
    updateCards()
end

-- The arrows and the pages pick a line, Enter installs the mod of the line that is picked, E is Cancel; other keys are not
-- the screen's (Left and Right change the page of the Pip-Boy)
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
        pick(picked <= 0 and 1 or picked + 18)
    elseif code == input.KEY.PageUp then
        pick(picked <= 0 and 1 or picked - 18)
    elseif code == input.KEY.Enter then
        if picked > 0 then activate(picked) end
    elseif symbol == 'e' then
        leave()
    else
        return false
    end
    return true
end

return module
