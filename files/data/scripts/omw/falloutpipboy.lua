-- The Pip-Boy of the player in Fallout 3 and New Vegas: the tabs STAT (status, S.P.E.C.I.A.L., skills, perks), INV (the
-- inventory by kind of item) and DATA (the quests), and the screen of a level up. What a character is and carries is the
-- engine's (core.pipboy); this script shows it in the window "Inventory", which it takes over from the one of Morrowind
-- for content that has a Pip-Boy, and gives the choices of the player back. The windows that Morrowind shows beside the
-- inventory are left empty. There is no map, no radio, no repair and no hot keys yet.
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local widgetModule = require('scripts.omw.falloutwidgets')

local pipboy = core.pipboy

-- Windows are replaced only when the loaded content has a Pip-Boy: Morrowind's keep their own
if not pipboy.available() then
    return {}
end

local v2 = util.vector2
local colors = widgetModule
local textSize = widgetModule.textSize

local element = nil
local visible = false
-- 'pipboy', or 'levelup' for the screen of the points a level gave
local view = 'pipboy'
local tab = 'stat'
local statTab = 'status'
local invTab = 'weapons'
-- The list on the screen: the line the window starts at, and (in the inventory) the item that is picked
local offset = 1
local selected = 1
-- How many lines of a list fit the screen
local listLines = 12
-- A level up that has not been shown
local levelUpPending = false
-- Seconds since the engine was asked for level ups
local sinceChecked = 0
-- Seconds before the screen of a level up that was left is asked for again (the window takes a frame to open)
local reopenWait = 0
-- What the mouse wheel and the keys move in the list
local listLength = 0

local tabs = { 'stat', 'inv', 'data' }
local statTabs = { 'status', 'special', 'skills', 'perks' }
local invTabs = { 'weapons', 'apparel', 'aid', 'misc', 'ammo' }

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

-- The screen of the level up (levelUpPage): the page that is shown, the points that were given to each skill on this
-- screen, and the perk that is picked
local levelPage = 1
local levelAdded = {}
local levelPerk = nil

local function openLevelUp()
    view = 'levelup'
    levelPage = 1
    levelAdded = {}
    levelPerk = nil
end

-- A word of the game, from the settings of the files
local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

local function upper(text)
    return tostring(text):upper()
end

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

local function pairText(now, most)
    return number(now) .. ' / ' .. number(most)
end

local function closeWindow()
    if I.UI.getMode() == 'Interface' then
        I.UI.removeMode('Interface')
    end
end

local function openWindow()
    I.UI.setMode('Interface', { windows = { 'Inventory' } })
end

-- The tabs of a row: the active one is picked out
local function tabRow(add, names, active, label, onPick)
    local items = {}
    for i, name in ipairs(names) do
        if i > 1 then items[#items + 1] = widgets.spacer(18, 0) end
        local picked = name == active
        items[#items + 1] = widgets.clickable(
            (picked and '[' or ' ') .. label(name) .. (picked and ']' or ' '),
            function() onPick(name) end,
            { color = picked and colors.pickedColor or colors.normalColor })
    end
    add(widgets.row(table.unpack(items)))
end

-- The lines of a list that fit the window, from the offset; the hint of how far the list goes
local function listWindow(add, lines)
    listLength = #lines
    if offset > math.max(1, #lines - listLines + 1) then
        offset = math.max(1, #lines - listLines + 1)
    end
    if offset < 1 then offset = 1 end
    local last = math.min(#lines, offset + listLines - 1)
    for i = offset, last do
        add(lines[i](i))
    end
    if #lines > listLines then
        add(widgets.label(string.format('%d-%d of %d (wheel, Page Up/Down)', offset, last, #lines),
            colors.hintColor, textSize - 4))
    end
end

local function line(...)
    local parts = { ... }
    return function() return widgets.row(table.unpack(parts)) end
end

-- Status

local function statusLines(stats)
    local width = 150
    local function pair(name, value, color)
        return widgets.row(
            widgets.cell(widgets.label(name, colors.hintColor), width),
            widgets.cell(widgets.label(value, color or colors.focusColor), width))
    end
    local lines = {}
    local function add(layout) lines[#lines + 1] = function() return layout end end
    add(widgets.label(upper(word('sStatsLvl', 'LVL')) .. ' ' .. stats.level .. '   ' .. stats.karmaTitle,
        colors.titleColor))
    add(pair(word('sStatsHP', 'HP'), pairText(stats.health, stats.maxHealth)))
    add(pair(word('sStatsAP', 'AP'), pairText(stats.actionPoints, stats.maxActionPoints)))
    local toNext = stats.nextLevel > stats.levelStart
        and string.format('%d / %d', stats.experience, stats.nextLevel) or number(stats.experience)
    add(pair(word('sStatsXP', 'XP'), toNext))
    add(pair(word('sInventoryCaps', 'Caps'), number(stats.caps)))
    add(pair(word('sInventoryWg', 'Weight'), pairText(stats.weight, stats.maxWeight),
        stats.weight > stats.maxWeight and colors.badColor or colors.focusColor))
    add(pair(word('sInventoryDR', 'DR'), number(stats.armorRating)))
    add(pair('DT', number(stats.threshold)))
    add(pair('Karma', number(stats.karma)))
    return lines
end

local function levelUpButton(add, stats)
    if stats.skillPoints > 0 or stats.perkPoints > 0 then
        add(widgets.clickable(
            string.format('%s: %d skill points, %d perks', word('sLevelUp', 'LEVEL UP'), stats.skillPoints,
                stats.perkPoints),
            function()
                openLevelUp()
                render()
            end, { color = colors.titleColor }))
    end
end

local function specialLines()
    local lines = {}
    for _, attribute in ipairs(pipboy.attributes()) do
        local a = attribute
        lines[#lines + 1] = line(
            widgets.cell(widgets.label(a.name, colors.focusColor), 200),
            widgets.cell(widgets.label(number(a.value), colors.focusColor), 60),
            widgets.label(a.value ~= a.base and string.format('(%s)', number(a.base)) or '', colors.hintColor))
    end
    return lines
end

local function skillLines()
    local lines = {}
    for _, skill in ipairs(pipboy.skills()) do
        local s = skill
        local tag = s.tagged and '* ' or '  '
        lines[#lines + 1] = line(
            widgets.cell(widgets.label(tag .. s.name, s.tagged and colors.pickedColor or colors.focusColor), 260),
            widgets.cell(widgets.label(number(s.value), colors.focusColor), 60),
            widgets.label(s.level, colors.hintColor))
    end
    return lines
end

local function perkLines()
    local lines = {}
    local perks = pipboy.perks()
    for _, perk in ipairs(perks) do
        local p = perk
        local rank = p.ranks > 1 and string.format(' (%d)', p.rank) or ''
        lines[#lines + 1] = line(widgets.label(p.name .. rank, colors.focusColor))
        for _, text in ipairs(widgetModule.wrap(p.description, 460)) do
            lines[#lines + 1] = line(widgets.label('    ' .. text, colors.hintColor, textSize - 2))
        end
    end
    if #perks == 0 then
        lines[1] = line(widgets.label(word('sNoPerks', 'No perks'), colors.hintColor))
    end
    return lines
end

local function statPage(add)
    local stats = pipboy.stats()
    tabRow(add, statTabs, statTab, function(name)
        if name == 'status' then return upper(word('sStatsStatus', 'STATUS')) end
        if name == 'special' then return upper(word('sStatsSPECIAL', 'S.P.E.C.I.A.L.')) end
        if name == 'skills' then return upper(word('sStatsSkills', 'SKILLS')) end
        return upper(word('sStatsPerks', 'PERKS'))
    end, function(name)
        statTab = name
        offset = 1
        render()
    end)
    local lines
    if statTab == 'status' then
        lines = statusLines(stats)
    elseif statTab == 'special' then
        lines = specialLines()
    elseif statTab == 'skills' then
        lines = skillLines()
    else
        lines = perkLines()
    end
    listWindow(add, lines)
    levelUpButton(add, stats)
end

-- Inventory

local function sortedItems(category)
    local items = {}
    for _, item in ipairs(pipboy.items()) do
        if item.category == category then items[#items + 1] = item end
    end
    table.sort(items, function(a, b)
        local x, y = a.name:lower(), b.name:lower()
        if x ~= y then return x < y end
        return a.id < b.id
    end)
    return items
end

local function itemDetail(item)
    if item.category == 'weapons' then
        return string.format('%s %s', word('sInventoryDam', 'DAM'), number(item.damage))
    elseif item.category == 'apparel' then
        if item.threshold > 0 then
            return string.format('DT %s', number(item.threshold))
        end
        return string.format('%s %s', word('sInventoryDR', 'DR'), number(item.rating))
    end
    return ''
end

local function itemLines(items, categoryTab)
    local lines = {}
    for index, item in ipairs(items) do
        local i, it = index, item
        lines[#lines + 1] = function()
            local picked = i == selected
            local name = (it.equipped and '[E] ' or '    ') .. it.name .. (it.count > 1 and (' (' .. it.count .. ')') or '')
            local color = picked and colors.pickedColor or (it.equipped and colors.focusColor or colors.normalColor)
            return widgets.row(
                widgets.cell(widgets.clickable((picked and '> ' or '  ') .. name, function()
                    selected = i
                    pipboy.use(it.id)
                    render()
                end, { color = color }), 330),
                widgets.cell(widgets.label(itemDetail(it), colors.hintColor), 90),
                widgets.cell(widgets.label(string.format('%s', number(it.weight * it.count)), colors.hintColor), 60),
                widgets.cell(widgets.label(number(it.value * it.count), colors.hintColor), 70),
                widgets.clickable(word('sInventoryDrop', 'Drop'), function()
                    pipboy.drop(it.id, 1)
                    render()
                end, { size = textSize - 4 }))
        end
    end
    if #lines == 0 then
        lines[1] = line(widgets.label('-', colors.hintColor))
    end
    return lines
end

local function invPage(add)
    local stats = pipboy.stats()
    add(widgets.row(
        widgets.cell(widgets.label(word('sInventoryCaps', 'Caps') .. ' ' .. number(stats.caps), colors.pickedColor), 160),
        widgets.label(word('sInventoryWg', 'Wg') .. ' ' .. pairText(stats.weight, stats.maxWeight),
            stats.weight > stats.maxWeight and colors.badColor or colors.hintColor)))
    tabRow(add, invTabs, invTab, function(name)
        if name == 'weapons' then return upper(word('sInventoryWeapons', 'WEAPONS')) end
        if name == 'apparel' then return upper(word('sInventoryApparel', 'APPAREL')) end
        if name == 'aid' then return upper(word('sInventoryAid', 'AID')) end
        if name == 'misc' then return upper(word('sInventoryMisc', 'MISC')) end
        return upper(word('sInventoryAmmo', 'AMMO'))
    end, function(name)
        invTab = name
        offset = 1
        selected = 1
        render()
    end)
    local items = sortedItems(invTab)
    listLength = #items
    if selected > #items then selected = #items end
    if selected < 1 then selected = 1 end
    -- The window follows the pick
    if selected < offset then offset = selected end
    if selected > offset + listLines - 1 then offset = selected - listLines + 1 end
    add(widgets.row(
        widgets.cell(widgets.label('    ' .. word('sInventoryName', 'NAME'), colors.hintColor, textSize - 4), 330),
        widgets.cell(widgets.label(word('sInventoryDam', 'DAM') .. '/' .. word('sInventoryDR', 'DR'),
            colors.hintColor, textSize - 4), 90),
        widgets.cell(widgets.label(word('sInventoryWg', 'WG'), colors.hintColor, textSize - 4), 60),
        widgets.cell(widgets.label(word('sInventoryVal', 'VAL'), colors.hintColor, textSize - 4), 70)))
    listWindow(add, itemLines(items, invTab))
    add(widgets.label('Enter uses, wears or holds the item; D drops it', colors.hintColor, textSize - 4))
end

-- Data

local function questLines()
    local lines = {}
    local quests = pipboy.quests()
    for _, quest in ipairs(quests) do
        local q = quest
        lines[#lines + 1] = line(widgets.label(q.name .. (q.completed and ' (done)' or ''),
            q.completed and colors.dimColor or colors.titleColor))
        for _, objective in ipairs(q.objectives) do
            local o = objective
            for index, text in ipairs(widgetModule.wrap(o.text, 460)) do
                local mark = index == 1 and (o.completed and '[x] ' or '[ ] ') or '    '
                lines[#lines + 1] = line(widgets.label('  ' .. mark .. text,
                    o.completed and colors.dimColor or colors.normalColor, textSize - 2))
            end
        end
    end
    if #quests == 0 then
        lines[1] = line(widgets.label('No quests', colors.hintColor))
    end
    return lines
end

local function dataPage(add)
    listWindow(add, questLines())
end

-- The level up works as the menu of the games does (menu code 1027): the skills first, where every point has to be spent
-- before Continue works, then, when the level gives a perk, the page of the perks, where one perk is picked. The points
-- of a skill are given at once and Reset takes back those of this screen; the perk is only added at Done. A perk that
-- the player does not qualify for yet is dim and cannot be picked.

local function counterText(single, plural, count)
    return count == 1 and single or (plural:gsub('%%d', tostring(count)))
end

local function finishLevelUp()
    if levelPerk then
        pipboy.choosePerk(levelPerk)
    end
    -- A perk point that is left (several levels were reached) gets a screen of its own
    if pipboy.stats().perkPoints > 0 and #pipboy.offeredPerks() > 0 then
        openLevelUp()
    else
        view = 'pipboy'
    end
    render()
end

local function resetLevelUp()
    if levelPage == 1 then
        for skill, points in pairs(levelAdded) do
            pipboy.refundSkillPoints(skill, points)
        end
        levelAdded = {}
    else
        levelPerk = nil
    end
    render()
end

local function skillRows(add, skills, left)
    for _, skill in ipairs(skills) do
        local s = skill
        local shown = math.min(100, math.max(0, math.floor(s.value)))
        add(widgets.row(
            widgets.cell(widgets.label(s.name, colors.focusColor), 260),
            widgets.cell(widgets.label(number(shown), colors.focusColor), 50),
            widgets.cell(widgets.clickable('-', function()
                if pipboy.refundSkillPoints(s.number, 1) > 0 then
                    levelAdded[s.number] = levelAdded[s.number] - 1
                end
                render()
            end, { enabled = (levelAdded[s.number] or 0) > 0 }), 30),
            widgets.clickable('+', function()
                if pipboy.spendSkillPoints(s.number, 1) > 0 then
                    levelAdded[s.number] = (levelAdded[s.number] or 0) + 1
                end
                render()
            end, { enabled = left > 0 and s.value < 100 })))
    end
end

local function perkRows(add, offered)
    for _, perk in ipairs(offered) do
        local p = perk
        local picked = levelPerk == p.id
        local rank = p.ranks > 1 and string.format(' (%d)', p.rank + 1) or ''
        add(widgets.clickable((picked and '> ' or '  ') .. p.name .. rank, function()
            -- A click on the perk that is picked puts it back
            levelPerk = (not picked) and p.id or nil
            render()
        end, { enabled = p.available ~= false, color = picked and colors.pickedColor or nil }))
        for _, text in ipairs(widgetModule.wrap(p.description, 460)) do
            add(widgets.label('    ' .. text, colors.hintColor, textSize - 2))
        end
    end
end

local function levelUpPage(add)
    local stats = pipboy.stats()
    local skills = pipboy.skills()
    local offered = stats.perkPoints > 0 and pipboy.offeredPerks() or {}
    local hasPerkPage = #offered > 0
    if levelPage == 2 and not hasPerkPage then
        levelPage = 1
    end
    local lastPage = levelPage == 2 or not hasPerkPage

    local title = word('sLevelUpTitleText', 'WELCOME TO LEVEL %d')
    add(widgets.label(upper((title:gsub('%%d', tostring(stats.level)))), colors.titleColor, textSize + 4))

    local left, complete
    if levelPage == 1 then
        left = stats.skillPoints
        local room = false
        for _, skill in ipairs(skills) do
            room = room or skill.value < 100
        end
        -- Every point has to be spent, unless no skill can take another (the 100 is the limit of the value, the
        -- bonuses of perks, items and traits counting, and the host gives points by the same rule)
        complete = left == 0 or not room
        if left > 0 then
            add(widgets.label(counterText(word('sLevelUpSkillCounter', 'ASSIGN 1 SKILL POINT'),
                word('sLevelUpSkillCounterPl', 'ASSIGN %d SKILL POINTS'), left), colors.pickedColor))
        end
        skillRows(add, skills, left)
    else
        left = levelPerk and 0 or 1
        complete = levelPerk ~= nil
        if left > 0 then
            add(widgets.label(word('sLevelUpPerkCounter', 'CHOOSE 1 PERK'), colors.pickedColor))
        end
        perkRows(add, offered)
    end

    add(widgets.row(
        widgets.cell(widgets.clickable(word('sReset', 'Reset'), resetLevelUp, {}), 120),
        widgets.cell(widgets.clickable(lastPage and word('sDone', 'Done') or word('sContinue', 'Continue'), function()
            if lastPage then
                finishLevelUp()
            else
                levelPage = 2
                render()
            end
        end, { color = colors.titleColor, enabled = complete }), 160),
        levelPage == 2 and widgets.clickable(word('sBack', 'Back'), function()
            levelPage = 1
            render()
        end, {}) or widgets.label('', colors.hintColor)))
end

function render()
    destroyElement()
    if not visible then
        return
    end
    widgets = widgetModule.new(update)
    local width = math.max(520, math.floor(ui.screenSize().x * 0.5))
    local rows = { { props = { size = v2(width, 0) } } }
    local function add(layout)
        rows[#rows + 1] = { props = { size = widgetModule.gap } }
        rows[#rows + 1] = layout
    end
    if view == 'levelup' then
        levelUpPage(add)
    else
        tabRow(add, tabs, tab, function(name)
            if name == 'stat' then return upper(word('sStatsStats', 'STAT')) end
            if name == 'inv' then return upper(word('sInventoryItems', 'INV')) end
            return 'DATA'
        end, function(name)
            tab = name
            offset = 1
            selected = 1
            render()
        end)
        if tab == 'stat' then
            statPage(add)
        elseif tab == 'inv' then
            invPage(add)
        else
            dataPage(add)
        end
    end
    element = ui.create(widgets.panel(rows))
end

local function show()
    visible = true
    offset = 1
    render()
end

local function hide()
    visible = false
    -- The screen of a level up is not a window to leave: it keeps what was done on it and comes back (onFrame)
    destroyElement()
end

-- The windows Morrowind shows beside the inventory have nothing to show here
I.UI.registerWindow('Inventory', show, hide)
for _, name in ipairs({ 'Stats', 'Magic', 'Map' }) do
    I.UI.registerWindow(name, function() end, function() end)
end

local function scroll(by)
    if not visible or view ~= 'pipboy' then
        return
    end
    if tab == 'inv' then
        local last = math.max(1, listLength)
        selected = math.max(1, math.min(last, selected + by))
        if selected < offset then offset = selected end
        if selected > offset + listLines - 1 then offset = selected - listLines + 1 end
    else
        offset = math.max(1, math.min(math.max(1, listLength - listLines + 1), offset + by))
    end
    render()
end

local function switchTab(by)
    local index = 1
    for i, name in ipairs(tabs) do
        if name == tab then index = i end
    end
    tab = tabs[(index - 1 + by) % #tabs + 1]
    offset, selected = 1, 1
    render()
end

local function pickedItem()
    if tab ~= 'inv' then return nil end
    return sortedItems(invTab)[selected]
end

return {
    engineHandlers = {
        onFrame = function(dt)
            -- A level up opens its screen at the first moment nothing else is on the screen
            sinceChecked = sinceChecked + core.getRealFrameDuration()
            if sinceChecked >= 0.25 then
                sinceChecked = 0
                if #pipboy.takeLevelUps() > 0 then
                    levelUpPending = true
                end
            end
            reopenWait = math.max(0, reopenWait - core.getRealFrameDuration())
            if not visible and I.UI.getMode() == nil and not pipboy.playerDead() then
                if view == 'levelup' then
                    -- The screen was left (Escape, another window) before the points were given: it comes back as it
                    -- was, unless nothing is left to give (the game was loaded, say)
                    local stats = pipboy.stats()
                    if stats.skillPoints > 0 or (stats.perkPoints > 0 and #pipboy.offeredPerks() > 0) then
                        levelUpPending = false
                        if reopenWait <= 0 then
                            reopenWait = 0.5
                            openWindow()
                        end
                    else
                        view = 'pipboy'
                    end
                elseif levelUpPending then
                    levelUpPending = false
                    openLevelUp()
                    openWindow()
                end
            end
        end,
        onKeyPress = function(key)
            if not visible or view ~= 'pipboy' then
                return
            end
            if key.code == input.KEY.LeftArrow then
                switchTab(-1)
            elseif key.code == input.KEY.RightArrow then
                switchTab(1)
            elseif key.code == input.KEY.DownArrow then
                scroll(1)
            elseif key.code == input.KEY.UpArrow then
                scroll(-1)
            elseif key.code == input.KEY.PageDown then
                scroll(listLines)
            elseif key.code == input.KEY.PageUp then
                scroll(-listLines)
            elseif key.code == input.KEY.Enter then
                local item = pickedItem()
                if item then
                    pipboy.use(item.id)
                    render()
                end
            elseif key.code == input.KEY.D then
                local item = pickedItem()
                if item then
                    pipboy.drop(item.id, 1)
                    render()
                end
            end
        end,
        onMouseWheel = function(vertical)
            if vertical > 0 then
                scroll(-1)
            elseif vertical < 0 then
                scroll(1)
            end
        end,
    },
}
