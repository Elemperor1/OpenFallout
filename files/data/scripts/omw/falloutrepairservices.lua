-- The repair services of a character, in Fallout 3 and New Vegas: the menu a conversation opens with ShowRepairMenu (the
-- result of a topic that the character speaks), drawn on the game's own file (repair_services_menu.xml). The list is the
-- weapons and armor the player carries that the character can mend, each with the health it has; a click on one makes the
-- character repair it for caps, the button above the list repairs all of them. What a row asks, what a repair does and who
-- gets the caps are the engine's (core.pipboy.serviceRows and serviceRepair, from FalloutNV.exe 0x7B7570 to 0x7B8B30, Mac
-- round 12AX R2); this script draws the list and gives the choices back.
--
-- Not there: the pad.
local core = require('openfallout.core')
local input = require('openfallout.input')
local I = require('openfallout.interfaces')

local menuModule = require('scripts.omw.falloutmenu')
local hotKeys = require('scripts.omw.falloutmenuhotkeys')

local pipboy = core.pipboy

if not (pipboy and pipboy.available() and pipboy.repairServices) then
    return {}
end

local FILE = 'repair_services_menu.xml'
-- The brightness of a row that cannot be paid for (the executable's 128 against 255), and of the line of a button
local DIM_ALPHA, FULL_ALPHA = 128, 255
-- A row is 0.6 pixels of health bar for each percent
local BAR = 0.6

local shown, list = nil, nil
local modeOpen = false
local shownSerial = nil
-- What the engine says the menu holds now, the row the cards were last made for
local data, cardRow = nil, nil

local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end

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
    if shown then
        shown:close()
        shown, list, data, cardRow = nil, nil, nil, nil
    end
end

-- The player leaves the screen: the vendor says its line, and the mode is given up
local function leave()
    destroyMenu()
    closeMode()
    shownSerial = nil
    pipboy.closeRepairServices()
end

local function setCard(name, traits)
    if shown:find(name) then
        for trait, value in pairs(traits) do shown:set(name, trait, value) end
    end
end

local function setText(name, text, extra)
    if not shown:find(name) then return end
    shown:set(name, 'string', text)
    for trait, value in pairs(extra or {}) do shown:set(name, trait, value) end
end

-- What a setting says that has a number in it (`%d`, `%.0f`) where the game puts one; the setting as it is when it has none
local function formatted(setting, fallback, ...)
    local text = word(setting, fallback)
    local ok, made = pcall(string.format, text, ...)
    return ok and made or text
end

local function statText(stat)
    local value = stat.value
    return value > 0 and string.format('%d', value) or '--'
end

-- The number a card tells and its word: the damage of a weapon, the threshold of armor, else its resistance, else a dash
local function statOf(stat)
    if stat.damage > 0 then
        return { title = word('sInventoryDamage', 'DAM'), value = stat.damage }
    elseif stat.threshold > 0 then
        return { title = word('sInventoryDamageThreshold', 'DT'), value = stat.threshold }
    elseif stat.rating > 0 then
        return { title = word('sInventoryDamageResistance', 'DR'), value = stat.rating }
    end
    return { title = word('sInventoryDamageResistance', 'DR'), value = 0 }
end

local function percent(condition)
    return condition / 10
end

-- The words over the list: the skill of the character and the caps of the player ("%s   %d"; a plus after the limit)
local function labels()
    local limit = pipboy.setting('iCapsLimit') or 1000000
    local caps = data.caps > limit and (string.format('%d', limit) .. '+') or string.format('%d', data.caps)
    setText('RSM_VendorSkillLabel', word('sRepairSkill', 'Repair') .. '   ' .. string.format('%d', data.skill))
    setText('RSM_CapsLabel', word('sInventoryCaps', 'Caps') .. '   ' .. caps)
end

-- The button that repairs everything: the setting is its format with the sum in it, and it is on only while the whole sum
-- can be paid
local function repairAllButton()
    local on = data.total > 0 and data.total <= data.caps
    setText('RSM_RepairAllButton', formatted('sRepairAllItems', 'REPAIR ALL (%d)', data.total))
    if shown:find('RSM_RepairAllButton') then shown:set('RSM_RepairAllButton', 'target', on) end
end

local function hidePreview()
    setCard('RSM_StatsDisplayRect', { visible = false })
    setCard('RSM_ItemIcon', { visible = false })
end

local function fill()
    local picked = list.selected()
    local rows = {}
    for index, entry in ipairs(data.rows) do
        rows[index] = {
            ['ListItemText.string'] = entry.name,
            ['ListItemText.alpha'] = entry.canPay and FULL_ALPHA or DIM_ALPHA,
            ['RSM_Template_Meter.width'] = percent(entry.condition) * BAR,
            ['RSM_Template_ItemMarker.visible'] = entry.worn == true,
            _RepairCost = entry.cost,
            _CanRepair = entry.canPay,
        }
    end
    shown:batch(function()
        labels()
        repairAllButton()
        hidePreview()
        -- The words for an empty list: the file shows them when the list has no rows by a count of the list's children that the
        -- menu engine does not keep as the executable does, so the script says it
        setCard('RSM_EmptyRepairListText', { visible = #rows == 0 })
        list.rows(rows)
        if picked > 0 and #rows > 0 then list.select(math.min(picked, #rows), true) end
    end)
    cardRow = nil
end

local function refresh()
    data = pipboy.serviceRows()
    if not data then
        leave()
        return
    end
    fill()
end

-- The row under the pointer or picked: its picture, its health and what it is now, and what the character would make of it
-- with what that costs; a row that is at the best the character can do has the words that say so instead
local function updateCards()
    local row = math.max(list.selected(), 0)
    if row == cardRow then return end
    cardRow = row
    local entry = data.rows[row]
    if not entry then
        shown:batch(hidePreview)
        return
    end
    local current = entry.condition / 1000
    local cap = data.cap / 1000
    local now = statOf(entry.stat)
    local better = statOf(entry.capStat)
    local icon = hotKeys.iconPath(entry.icon)
    shown:batch(function()
        setCard('RSM_StatsDisplayRect', { visible = true })
        setCard('RSM_ItemIcon', { filename = icon or '', visible = icon ~= nil })
        setCard('RSM_BrokenItemHealth', { user0 = current })
        setCard('RSM_BrokenItemStat', { _Title = now.title, _Value = statText(now) })
        -- Not below what the character can reach (to a tenth of a percent): nothing to buy
        if entry.condition >= data.cap then
            setCard('RSM_FixedItemHealth', { visible = false })
            setText('RSM_ErrorText', formatted('sCantRepairPastMax', 'Cannot repair past %.0f%s', cap * 100, '%'))
            return
        end
        setCard('RSM_FixedItemHealth', { user0 = cap, visible = true })
        setCard('RSM_FixedItemStat', { _Title = better.title, _Value = statText(better) })
        local health = math.floor(0.5 + 100 * math.max(0, cap - current))
        setText('RSM_HealthImprovementText', string.format('+%d%%', health))
        local gain = math.floor(0.5 + math.max(0, better.value - now.value))
        setText('RSM_StatImprovementText', string.format('+%d', gain))
        setText('RSM_RepairCostText', formatted('sRepairCost', 'COST: %d', entry.cost),
            { alpha = entry.canPay and FULL_ALPHA or DIM_ALPHA })
    end)
end

-- A row is clicked: the character repairs it when the player can pay, else the sound of a refusal
local function activate(row)
    local entry = data and data.rows[row]
    if not entry then return end
    if entry.canPay and pipboy.serviceRepair(entry.id, entry.worn, false) > 0 then
        pipboy.playSoundNamed('UIRepairWeapon')
        refresh()
    else
        pipboy.playSoundNamed('UIVATSInsufficientAP')
    end
end

-- The button over the list: every row that asks something is repaired, and the sum is paid once
local function repairAll()
    if not data or not (data.total > 0 and data.total <= data.caps) then return end
    if pipboy.serviceRepair('', false, true) > 0 then
        pipboy.playSoundNamed('UIRepairWeapon')
        refresh()
    end
end

local function openMenu()
    local opened = menuModule.open(FILE)
    if not opened then return false end
    local made = opened:list('RSM_RepairList', 'RSM_RepairListTemplate')
    if not made then
        opened:close()
        return false
    end
    shown, list = opened, made
    list.onClick(function(row) activate(row) end)
    opened:onClick('RSM_RepairAllButton', repairAll)
    opened:onClick('RSM_DoneButton', leave)
    return true
end

-- The keys: the arrows and pages pick a row, Enter repairs with it; A repairs everything and E is Done (the letters the file
-- gives its buttons)
local function press(code, symbol)
    if not shown or I.UI.getMode() ~= 'Interface' or messageOpen() then return end
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
    elseif symbol then
        if not shown:pcButton(symbol) and (symbol == 'a') then
            -- A button that is lit makes no sound when it is clicked; one that is not gives the sound of a refusal
            pipboy.playSoundNamed('UIMenuCancel')
        end
    end
end

local function sync()
    local menu = pipboy.repairServices()
    if not menu then
        if shown or modeOpen then
            destroyMenu()
            closeMode()
            shownSerial = nil
        end
        return
    end
    if menu.serial ~= shownSerial then
        shownSerial = menu.serial
        destroyMenu()
        openMode()
        if not openMenu() then
            -- The game's files have no such menu: nothing is offered
            print('repair services: the file ' .. FILE .. ' is not there; the menu is not shown')
            pipboy.closeRepairServices()
            shownSerial = nil
            closeMode()
            return
        end
        refresh()
    end
    if shown then
        shown:checkSize()
        updateCards()
    end
end

return {
    engineHandlers = {
        onFrame = sync,
        onKeyPress = function(key)
            press(key.code, key.symbol)
        end,
    },
    eventHandlers = {
        -- However the screen is left (the button, Escape, a script), what was open is closed, and the dialogue under it is
        -- back
        UiModeChanged = function(event)
            if modeOpen and event.oldMode == 'Interface' and event.newMode ~= 'Interface' and event.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyMenu()
                pipboy.closeRepairServices()
                shownSerial = nil
            end
        end,
    },
}
