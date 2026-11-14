-- The stats page of the Pip-Boy (STAT) in Fallout 3 and New Vegas, drawn from the game's own menu file
-- (main/stats_menu.xml): the headline with the level, health, action points and experience, the five tabs along the
-- bottom (status, S.P.E.C.I.A.L., skills, perks, general), the lists of the three that are lists with the description of
-- the one that is picked, and the condition of the body. What the character is is the engine's (core.pipboy); this
-- module lists it. It is a module of falloutpipboy.lua, not a script.
--
-- The dehydration, hunger and sleep pages of the hardcore mode (their buttons are there only when the mode is on) show the
-- level on the meter of the file: the arrow stands at the level over the level of the last stage.
--
-- Not there yet: the pictures of the skills, perks and attributes (the games show models and textures chosen by code), the
-- per-limb condition, the effects list (and the name of a need over its meter), the healing buttons, and the lines of the
-- general page.
--
--   local page = require('scripts.omw.falloutpipboystats')
--   page.show()            -- true when the page is on the screen, false when the menu file is not there
--   page.press(keyCode)    -- true when the key was for the page
--   page.update()          -- each frame while the page is shown
--   page.hide()
local core = require('openfallout.core')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')

local pipboy = core.pipboy

local module = {}

local MENU_FILE = 'main/stats_menu.xml'

-- The tabs in the order of the game's `user0` of the menu (0 to 4): the tile that is the button, and the list tile and
-- the rows' template if the page is a list
local TABS = {
    { key = 'status', button = 'stats_tailline_status' },
    { key = 'special', button = 'stats_tailline_SPECIAL', list = 'stats_special_container' },
    { key = 'skills', button = 'stats_tailline_skills', list = 'stats_skills_container' },
    { key = 'perks', button = 'stats_tailline_perks', list = 'stats_perks_container' },
    { key = 'general', button = 'stats_tailline_general' },
}
local TEMPLATE = 'stats_list_template'

-- The modes of the status page: the button of each, in the order of the container's `user0` (the last three are the
-- hardcore mode's: dehydration, hunger, sleep; the executable allows modes 0 to 5 in it and 0 to 2 otherwise, 0x7E0060)
local MODES = {
    'stats_CND_button', 'stats_RAD_button', 'stats_EFF_button', 'stats_H20_button', 'stats_FOD_button',
    'stats_SLP_button',
}
-- The need that each mode shows (the key of the engine's status), counted from 1 as MODES is
local NEED_MODES = { [4] = 'dehydration', [5] = 'hunger', [6] = 'sleep' }
-- What the games' hardcore mode adds and that is not there: the buttons are not shown
local HIDDEN = { 'stats_stimpak_button', 'stats_healing_mode', 'stats_radaway_button', 'stats_radx_button', 'stats_drbag_button' }
local statusMode = 1
local needsShown = {}

local shown, lists = nil, nil
local tabIndex = 1
-- The rows each list was made from (to say what the picked row is), and the row the description was last made for
local rows = {}
local describedKey = nil

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

local function pairText(now, most)
    return number(now) .. '/' .. number(most)
end

-- The rows of the three lists, from what the character is now
local function makeRows()
    local made = { special = {}, skills = {}, perks = {} }
    for _, attribute in ipairs(pipboy.attributes()) do
        made.special[#made.special + 1] = {
            text = attribute.name, value = number(attribute.value), description = attribute.description or '',
        }
    end
    for _, skill in ipairs(pipboy.skills()) do
        made.skills[#made.skills + 1] = {
            text = skill.name, value = number(skill.value), description = skill.description or '',
        }
    end
    for _, perk in ipairs(pipboy.perks()) do
        made.perks[#made.perks + 1] = {
            text = perk.name, value = perk.ranks > 1 and number(perk.rank) or nil, description = perk.description or '',
        }
    end
    return made
end

local function listRows(entries)
    local out = {}
    for index, entry in ipairs(entries) do
        local row = { ['ListItemText.string'] = entry.text }
        if entry.value then row.user1 = tonumber(entry.value) end
        out[index] = row
    end
    return out
end

-- The parts of the body the status page draws: the tile of each, the picture of it whole, and the name the engine gives
-- a crippled one (a texture of Interface\Stats)
local LIMBS = {
    { tile = 'stats_player_head', name = 'head', picture = 'head' },
    { tile = 'stats_player_torso', name = 'torso', picture = 'torso' },
    { tile = 'stats_player_leftarm', name = 'leftarm', picture = 'left_arm' },
    { tile = 'stats_player_rightarm', name = 'rightarm', picture = 'right_arm' },
    { tile = 'stats_player_leftleg', name = 'leftleg', picture = 'left_leg' },
    { tile = 'stats_player_rightleg', name = 'rightleg', picture = 'right_leg' },
}

local function isCrippled(limb, crippled)
    for _, texture in ipairs(crippled) do
        if texture:lower():find(limb.picture, 1, true) then return true end
    end
    return false
end

-- The body of the condition page: the part pictures (the games choose the textures in code), the meter of each, and the
-- word for a part that is crippled. The engine tells which parts are crippled; how hurt the others are, it does not.
local function fillBody()
    local status = core.hud.status()
    local crippled = status.crippled or {}
    shown:batch(function()
        if shown:find('stats_player_face') then
            shown:set('stats_player_face', 'filename', 'Interface\\Stats\\face_00.dds')
        end
        for _, limb in ipairs(LIMBS) do
            local broken = isCrippled(limb, crippled)
            if shown:find(limb.tile) then
                shown:set(limb.tile, 'filename',
                    'Interface\\Stats\\' .. limb.picture .. (broken and '_broken' or '') .. '.dds')
            end
            local meter = limb.tile .. '_pct_meter'
            if shown:find(meter) then shown:set(meter, '_Value', broken and 0 or 1) end
            local label = limb.tile .. '_crippled'
            if shown:find(label) then shown:set(label, 'visible', broken) end
        end
    end)
end

-- The level on the meter of the page that is open, when it is one of the needs: the most that the meter reaches and the level
-- the arrow stands at (the file reads both from the menu's `user2` and `user11`)
local function fillNeeds()
    local need = NEED_MODES[statusMode]
    if not need then return end
    local status = core.hud.status()
    local needs = status.needs
    if not needs then return end
    local level = math.floor((needs[need] or 0) + 0.5)
    local most = math.floor((needs[need .. 'Most'] or 1000) + 0.5)
    if needsShown.level == level and needsShown.most == most then return end
    needsShown.level, needsShown.most = level, most
    shown:batch(function()
        shown:io('user2', most)
        shown:io('user11', level)
    end)
end

-- The headline: level, health, action points and experience
local function fillHeadline(stats)
    shown:batch(function()
        shown:io('user8', number(stats.level))
        shown:io('user5', pairText(stats.health, stats.maxHealth))
        shown:io('user6', pairText(stats.actionPoints, stats.maxActionPoints))
        local toNext = stats.nextLevel > stats.levelStart and (number(stats.experience) .. '/' .. number(stats.nextLevel))
            or number(stats.experience)
        shown:io('user7', toNext)
    end)
end

local function currentTab()
    return TABS[tabIndex]
end

-- The list of the tab that is shown, if it is a list
local function currentList()
    local tab = currentTab()
    return tab.list and lists[tab.key] or nil
end

local function fill()
    local stats = pipboy.stats()
    rows = makeRows()
    shown:batch(function()
        fillHeadline(stats)
        for _, tab in ipairs(TABS) do
            local list = lists[tab.key]
            if list then list.rows(listRows(rows[tab.key])) end
        end
        fillBody()
        if shown:find('stats_karma_alignment') then
            shown:set('stats_karma_alignment', 'string', stats.karmaTitle or '')
        end
    end)
    describedKey = nil
end

-- The description of the row that is picked (or under the pointer) in the list that is shown
local function updateDescription()
    local list = currentList()
    local tab = currentTab()
    local text, key = '', tab.key
    if list then
        local row = list.selected()
        local entry = row > 0 and rows[tab.key][row] or nil
        text = entry and entry.description or ''
        key = tab.key .. ':' .. row
    end
    if key == describedKey then return end
    describedKey = key
    shown:batch(function()
        -- The description is under the picture of the thing, which the file shows with the picture (the picture itself is
        -- chosen by the games' code and is not drawn here)
        if shown:find('stats_icon') then
            shown:set('stats_icon', 'visible', list ~= nil and text ~= '')
        end
        shown:set('stats_description', 'string', text)
        if shown:find('stats_description_scrollbar') then
            shown:setScrollbar('stats_description_scrollbar', 0)
        end
    end)
end

local function pickTab(index)
    tabIndex = index
    shown:batch(function()
        shown:io('user0', index - 1)
    end)
    local list = currentList()
    if list and list.count() > 0 and list.selected() <= 0 then list.select(1, true) end
    describedKey = nil
    updateDescription()
end

local function open()
    local opened = menuModule.open(MENU_FILE, { box = menuModule.PIPBOY_BOX })
    if not opened then return false end
    local made = {}
    for _, tab in ipairs(TABS) do
        if tab.list then
            local list = opened:list(tab.list, TEMPLATE)
            if not list then
                opened:close()
                return false
            end
            made[tab.key] = list
        end
    end
    shown, lists = opened, made
    for index, tab in ipairs(TABS) do
        opened:onClick(tab.button, function() pickTab(index) end)
    end
    local needs = core.hud.status().needs
    local hardcore = needs ~= nil and needs.hardcore == true
    for mode, name in ipairs(MODES) do
        opened:onClick(name, function()
            if opened:find('stats_status_container') then
                statusMode = mode
                needsShown = {}
                opened:set('stats_status_container', 'user0', mode - 1)
                fillNeeds()
            end
        end)
    end
    statusMode = 1
    needsShown = {}
    opened:batch(function()
        for _, name in ipairs(HIDDEN) do
            if opened:find(name) then opened:set(name, 'visible', false) end
        end
        -- The needs of the hardcore mode have their pages only when it is on
        for mode, need in pairs(NEED_MODES) do
            if not hardcore and opened:find(MODES[mode]) then opened:set(MODES[mode], 'visible', false) end
        end
        opened:io('user0', tabIndex - 1)
    end)
    return true
end

function module.isOpen()
    return shown ~= nil
end

-- Show the page (open the menu if it is not open) with what the character is now; false when it cannot be shown
function module.show()
    if not shown and not open() then
        return false
    end
    fill()
    pickTab(tabIndex)
    return true
end

function module.hide()
    if shown then
        shown:close()
    end
    shown, lists = nil, nil
    describedKey = nil
end

-- Each frame the page is shown: the window size, and the description of what the pointer or the pick is on
function module.update()
    if not shown then return end
    shown:checkSize()
    updateDescription()
    fillNeeds()
end

-- What a key does while the page is there: the arrows and pages pick a row of the list, [ and ] go to the tab before and
-- after. False when the key is not the page's (Left and Right change the page of the Pip-Boy).
function module.press(code)
    if not shown then return false end
    local list = currentList()
    if code == input.KEY.LeftBracket then
        pickTab((tabIndex - 2) % #TABS + 1)
    elseif code == input.KEY.RightBracket then
        pickTab(tabIndex % #TABS + 1)
    elseif list and (code == input.KEY.DownArrow or code == input.KEY.PageDown) then
        local count = list.count()
        local picked = list.selected()
        if count > 0 then
            list.select(picked <= 0 and 1 or math.min(count, picked + (code == input.KEY.DownArrow and 1 or 8)), true)
        end
        updateDescription()
    elseif list and (code == input.KEY.UpArrow or code == input.KEY.PageUp) then
        local count = list.count()
        local picked = list.selected()
        if count > 0 then
            list.select(picked <= 0 and count or math.max(1, picked - (code == input.KEY.UpArrow and 1 or 8)), true)
        end
        updateDescription()
    else
        return false
    end
    return true
end

return module
