-- The screen of a level up in Fallout 3 and New Vegas, drawn from the game's own menu file (levelup_menu.xml): the title
-- with the level, the skills to give the points of the level to (one page), and, when the level gives a perk, the perks to
-- pick from (a second page), with the picture and the words of the row that is lit and the counter of what is left to give.
-- The rules are the engine's (core.pipboy: the points of the level, the skills, the perks that are offered); this module
-- gives the player's choices back. It is a module of falloutpipboy.lua, not a script, and is what the Pip-Boy's own
-- stand-in widgets give way to when the menu file is in the install.
--
-- How the executables do it (read from both, see docs/openfallout/menus.md):
--   * The title is one setting (sLevelUpTitleText) with the player's level, set when the menu is made and not changed on the
--     perk page.
--   * The counter is a setting for one point or pick left and one for more (a format with the number); with none left it is
--     not shown. The same on the page of the perks, where there is one pick.
--   * The points of the skills are given at once (the arrows of a row take one point from what is left and add it to the
--     skill); Reset takes back what was given on this screen. Continue is lit when every point is given, Done on the last
--     page when a perk is picked. The perk is added when Done is pressed.
--
-- Not there yet: the sounds of the screen.
--
--   local screen = require('scripts.omw.falloutlevelup')
--   screen.begin(level)         -- a new screen for the level the player reached (state is kept until the next begin)
--   screen.show({ finish = function(perkId) ... end })   -- true when it is on the screen, false when the file is not there
--   screen.press(keyCode)       -- true when the key was for the screen
--   screen.update()             -- each frame while the screen is shown
--   screen.hide()
local core = require('openfallout.core')
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')

local pipboy = core.pipboy

local module = {}

local MENU_FILE = 'levelup_menu.xml'

-- The most a skill is given (the executables clamp the value shown to 0..100)
local SKILL_MAX = 100

-- The picture that the executables put on the corner of the skills of the weapons, by actor value (New Vegas' file has the
-- tile for it; Fallout 3's has none)
local BADGES = {
    [33] = 'Interface\\Icons\\TypeIcons\\weap_skill_icon_big_guns.dds',
    [34] = 'Interface\\Icons\\TypeIcons\\weap_skill_icon_energy.dds',
    [35] = 'Interface\\Icons\\TypeIcons\\weap_skill_icon_explosives.dds',
    [38] = 'Interface\\Icons\\TypeIcons\\weap_skill_icon_melee.dds',
    [41] = 'Interface\\Icons\\TypeIcons\\weap_skill_icon_sm_arms.dds',
    [45] = 'Interface\\Icons\\TypeIcons\\weap_skill_icon_unarmed.dds',
}

-- What the screen is for, kept when the window is left and comes back (the screen of a level is not given up)
local level = nil
-- 0: the skills, 1: the perks
local page = 0
-- The points given to each skill on this screen (by the number of the skill)
local added = {}
-- The id of the perk that is picked
local picked = nil

local options = {}
local shown = nil
-- The two lists of the file, the rows each was made from (what the screen says of a row is read from these) and the row
-- that was described last
local lists = nil
local infos = { {}, {} }
local described = { 0, 0 }
-- Whether the screen has a page of perks
local hasPerks = false

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- A setting's text with a number put into it (the text if it has no place for one)
local function formatted(setting, fallback, ...)
    local format = word(setting, fallback)
    local ok, text = pcall(string.format, format, ...)
    return ok and text or format
end

local function addedTotal()
    local total = 0
    for _, points in pairs(added) do total = total + points end
    return total
end

-- The skills the screen lists: the engine's, but those that the executables leave out
local function skillList()
    local list = {}
    for _, skill in ipairs(pipboy.skills()) do
        if not skill.levelUpSkip then list[#list + 1] = skill end
    end
    return list
end

local function inRange(row, page)
    return row >= 1 and row <= #infos[page + 1]
end

-- The points of the page that can be given: what is left, and no more than the skills have room for under 100 (the
-- executables count the room the same way, so that Continue does not wait for points no skill can take)
local function usablePoints(left, skills)
    local room = 0
    for _, skill in ipairs(skills) do
        room = room + math.max(0, SKILL_MAX - math.max(0, math.min(SKILL_MAX, math.floor(skill.value))))
    end
    return math.min(left, room)
end

-- The value that a skill row shows: what the skill is now, as the whole number it is between 0 and 100
local function shownValue(skill)
    return math.max(0, math.min(SKILL_MAX, math.floor(skill.value)))
end

-- What the screen says of the row that is lit: the picture and the words of a skill, and on the page of the perks the
-- level the perk asks for before its description
local function describe(row)
    local info = infos[page + 1][row]
    if not info or not shown then return end
    local has = info.icon ~= nil and info.icon ~= ''
    shown:batch(function()
        shown:set('LUM_SelectionText', 'string', info.text or '')
        shown:set('LUM_SelectionIcon', 'filename', has and info.icon or '')
        shown:set('LUM_SelectionIcon', 'visible', has)
        -- The mark of the weapon skills (New Vegas' file only)
        if shown:find('stats_icon_badge') then
            local badge = info.badge
            shown:set('stats_icon_badge', 'filename', badge or '')
            shown:set('stats_icon_badge', 'visible', badge ~= nil)
        end
    end)
    described[page + 1] = row
end

local function activeList()
    return lists and (page == 0 and lists.skills or lists.perks)
end

-- The row the pointer or the keys have lit on this page
local function litRow()
    local list = activeList()
    local row = list and list.selected() or 0
    return inRange(row, page) and row or 0
end

local function counterText(single, singleFallback, plural, pluralFallback, remaining)
    if remaining == 1 then return word(single, singleFallback) end
    return formatted(plural, pluralFallback, remaining)
end

-- The operator and the change of the number that the executables write for the comparison of an actor value condition
-- (0 equal, 1 not equal, 2 greater, 3 greater or equal, 4 less, 5 less or equal): a space and the number as it is, except for
-- greater (the number less one), less (" < ") and less or equal (" < " and the number plus one)
local COMPARISONS = {
    [2] = { ' ', -1 },
    [4] = { ' < ', 0 },
    [5] = { ' < ', 1 },
}

-- The requirements of a perk after its level, as the executables write the conditions of the perk: an actor value at a
-- number ("Strength 5", skipped when its number is 0 or the value has no name), a perk the player has ("Sneak 2", the
-- rank left out when it is 0), each joined to the next by ", " or, for a condition that is "or"-ed, by " or "; the first one
-- gets a ", " in front when the text has the level. The other kinds of conditions write nothing but count as a condition,
-- and when none is written at all the text has "--" in place of them.
local function conditionsText(perk, withLevel)
    local text, written = '', false
    local list = perk.requirements or {}
    for index, condition in ipairs(list) do
        local lead = (index == 1 and withLevel) and ', ' or ''
        local hasNext = index < #list
        if condition.kind == 'value' then
            local compared = COMPARISONS[condition.comparison] or { ' ', 0 }
            local number = condition.number + compared[2]
            if condition.name ~= '' and number ~= 0 then
                local piece = condition.name .. compared[1] .. tostring(number)
                if condition['or'] and hasNext then
                    text = text .. lead .. ' ' .. piece .. ' ' .. word('sOr', 'or') .. ' '
                else
                    text = text .. lead .. piece .. (hasNext and ', ' or '')
                end
            end
            written = true
        elseif condition.kind == 'perk' then
            if condition.name ~= '' then
                text = text .. lead .. condition.name .. (condition.number > 0 and (' ' .. tostring(condition.number)) or '')
            end
            written = true
        else
            text = text .. lead
            written = true
        end
    end
    return written and text or '--'
end

-- The words under a perk's row, as the executables build them: the requirements ("Requirements: Level 6, Strength 5", the
-- level as the abbreviation and the number, only when the perk asks for more than level 1), a new line, the number of
-- ranks the perk has ("Ranks: 3") and a blank line, then the description
local function perkText(perk)
    local withLevel = (perk.minLevel or 0) > 1
    local text = word('sRequirementsText', 'Requirements') .. ': '
    if withLevel then
        text = text .. word('sLevelAbbrev', 'Level') .. ' ' .. tostring(perk.minLevel)
    end
    return text .. conditionsText(perk, withLevel) .. '\n' .. word('sRanksText', 'Ranks') .. ': ' .. tostring(perk.ranks)
        .. '\n\n' .. (perk.description or '')
end

-- A perk the player already has at some rank, with more ranks to go, shows the rank it would be picked at in brackets
local function perkName(perk)
    if perk.ranks > 1 and perk.rank > 0 then
        return string.format('%s (%d)', perk.name, perk.rank + 1)
    end
    return perk.name
end

-- The order of the list: the perks that can be picked first, then those the level has not reached, each by the level the
-- perk asks for and then by name
local function sortedPerks(offered)
    local list = {}
    for index, perk in ipairs(offered) do
        list[index] = { perk = perk, order = index }
    end
    table.sort(list, function(left, right)
        local a, b = left.perk, right.perk
        local aOpen, bOpen = a.available ~= false, b.available ~= false
        if aOpen ~= bOpen then return aOpen end
        if (a.minLevel or 0) ~= (b.minLevel or 0) then return (a.minLevel or 0) < (b.minLevel or 0) end
        local aName, bName = a.name:lower(), b.name:lower()
        if aName ~= bName then return aName < bName end
        return left.order < right.order
    end)
    for index, entry in ipairs(list) do list[index] = entry.perk end
    return list
end

-- Put everything the screen shows right: the rows of both lists, the counter, and what is said of the lit row
local function fill()
    if not shown then return end
    local stats = pipboy.stats()
    local skills = skillList()
    local offered = stats.perkPoints > 0 and sortedPerks(pipboy.offeredPerks()) or {}
    hasPerks = #offered > 0
    if page == 1 and not hasPerks then page = 0 end

    local skillRows, perkRows = {}, {}
    infos = { {}, {} }
    for index, skill in ipairs(skills) do
        local points = added[skill.number] or 0
        skillRows[index] = {
            string = skill.name,
            -- The base is what the skill was before this screen's points, the extra what perks, items and the like add
            _BaseValue = skill.base - points,
            _ExtraValue = skill.value - skill.base,
            _AddedValue = points,
            _OverflowValue = 0,
            _DisplayString = string.format('%d', shownValue(skill)),
        }
        infos[1][index] = {
            number = skill.number, name = skill.name, text = skill.description or '', icon = skill.icon,
            badge = BADGES[skill.number], value = skill.value,
        }
    end
    for index, perk in ipairs(offered) do
        perkRows[index] = {
            string = perkName(perk),
            _selected = picked == perk.id,
            -- A perk that the player does not qualify for yet is dim
            _TextAlpha = perk.available == false and 128 or 255,
        }
        infos[2][index] = {
            id = perk.id, available = perk.available ~= false, icon = perk.icon,
            text = perkText(perk),
        }
    end
    -- A perk that was picked and is not offered any more is let go
    if picked then
        local found = false
        for _, info in ipairs(infos[2]) do found = found or info.id == picked end
        if not found then picked = nil end
    end

    local left = usablePoints(stats.skillPoints, skills)
    local given = addedTotal()
    local lit = litRow()
    shown:batch(function()
        lists.skills.rows(skillRows)
        lists.perks.rows(perkRows)
        shown:io('_CurrentPage', page)
        shown:io('_EndPage', hasPerks and 1 or 0)
        if page == 0 then
            shown:io('_CurrPoints', given)
            shown:io('_MaxPoints', given + left)
        else
            shown:io('_CurrPoints', picked and 1 or 0)
            shown:io('_MaxPoints', 1)
        end
        local remaining = page == 0 and left or (picked and 0 or 1)
        if page == 0 then
            shown:set('LUM_PointCounter', 'string',
                counterText('sLevelUpSkillCounter', 'ASSIGN 1 SKILL POINT', 'sLevelUpSkillCounterPl',
                    'ASSIGN %d SKILL POINTS', remaining))
        else
            shown:set('LUM_PointCounter', 'string',
                counterText('sLevelUpPerkCounter', 'CHOOSE 1 PERK', 'sLevelUpPerkCounterPl', 'CHOOSE %d PERKS',
                    remaining))
        end
        shown:set('LUM_PointCounter', 'visible', remaining > 0)
        local list = activeList()
        if lit == 0 and #infos[page + 1] > 0 then lit = 1 end
        if lit > 0 then
            list.select(lit, true)
            describe(lit)
        end
    end)
end

-- Whether Continue (or Done) is lit: every point of the page is given (the executables also take the points that no skill
-- has room for: the engine hands out only what the skills can take)
local function ready()
    if page == 0 then return usablePoints(pipboy.stats().skillPoints, skillList()) <= 0 end
    return picked ~= nil
end

local function giveSkillPoint(row, delta)
    local info = infos[1][row]
    if not info then return end
    if delta > 0 then
        if pipboy.stats().skillPoints > 0 and info.value < SKILL_MAX and pipboy.spendSkillPoints(info.number, 1) > 0 then
            added[info.number] = (added[info.number] or 0) + 1
        end
    elseif (added[info.number] or 0) > 0 and pipboy.refundSkillPoints(info.number, 1) > 0 then
        added[info.number] = added[info.number] - 1
    end
end

local function pickPerk(row)
    local info = infos[2][row]
    if not info then return end
    -- A click on the perk that is picked puts it back; a perk that is dim cannot be picked (the executables ignore the click)
    if picked == info.id then
        picked = nil
    elseif info.available then
        picked = info.id
    end
end

-- What the executables do for Continue and Done: another page, or the end of the screen
local function goTo(next)
    if next > (hasPerks and 1 or 0) then
        if options.finish then options.finish(picked) end
        return
    end
    page = math.max(0, next)
    fill()
end

local function reset()
    if page == 0 then
        for number, points in pairs(added) do
            pipboy.refundSkillPoints(number, points)
        end
        added = {}
    else
        picked = nil
    end
    fill()
end

-- The name of the tile that was hit: an arrow of the row, or the row (the arrows are images in it)
local function hitName(tile)
    local hit = tile
    while hit do
        local name = shown.menu:name(hit)
        if name == 'LUM_Template_LeftArrow' or name == 'LUM_Template_RightArrow' then return name end
        hit = shown.menu:parent(hit)
    end
    return nil
end

local function open()
    local opened = menuModule.open(MENU_FILE)
    if not opened then return false end
    local made = {
        skills = opened:list('LUM_SkillList', 'LUM_SkillTemplate'),
        perks = opened:list('LUM_PerkList', 'LUM_PerkTemplate'),
    }
    if not (made.skills and made.perks) then
        opened:close()
        return false
    end
    shown, lists = opened, made
    described = { 0, 0 }
    opened:set('LUM_Headline_Title', 'string',
        formatted('sLevelUpTitleText', 'WELCOME TO LEVEL %d', level or pipboy.stats().level))
    made.skills.onClick(function(row, tile)
        local name = hitName(tile)
        if name == 'LUM_Template_LeftArrow' then
            giveSkillPoint(row, -1)
        elseif name == 'LUM_Template_RightArrow' then
            giveSkillPoint(row, 1)
        end
        fill()
        made.skills.select(row)
    end)
    made.perks.onClick(function(row)
        pickPerk(row)
        fill()
        made.perks.select(row)
    end)
    opened:onClick('LUM_ResetButton', reset)
    opened:onClick('LUM_ContinueButton', function()
        if ready() then goTo(page + 1) end
    end)
    opened:onClick('LUM_BackButton', function()
        if page > 0 then goTo(page - 1) end
    end)
    return true
end

function module.begin(reached)
    level = reached
    page = 0
    added = {}
    picked = nil
end

function module.isOpen()
    return shown ~= nil
end

-- Show the screen (open the menu if it is not open) as it stands; false when it cannot be shown
function module.show(asked)
    options = asked or options
    if not shown and not open() then
        return false
    end
    local ok, problem = pcall(fill)
    if not ok then
        print('level up menu: ' .. tostring(problem))
        module.hide()
        return false
    end
    return true
end

function module.hide()
    if shown then
        shown:close()
    end
    shown, lists = nil, nil
    infos = { {}, {} }
    described = { 0, 0 }
end

-- Each frame the screen is shown: the window size, and the words of the row that the pointer lit
function module.update()
    if not shown then return end
    shown:checkSize()
    local row = litRow()
    if row > 0 and row ~= described[page + 1] then describe(row) end
end

-- What a key does while the screen is there: Up and Down light another row, Left and Right (or Space) give a point to the
-- skill that is lit (take one back) or pick the perk, Enter is Continue or Done, Backspace is Back and R is Reset. False
-- when the key is not the screen's.
function module.press(code)
    if not shown then return false end
    local row = litRow()
    local count = #infos[page + 1]
    if code == input.KEY.UpArrow or code == input.KEY.DownArrow then
        if count == 0 then return true end
        local to = row == 0 and 1 or math.max(1, math.min(count, row + (code == input.KEY.DownArrow and 1 or -1)))
        activeList().select(to, true)
        describe(to)
    elseif code == input.KEY.LeftArrow or code == input.KEY.RightArrow or code == input.KEY.Space then
        if row == 0 then return true end
        if page == 0 then
            giveSkillPoint(row, code == input.KEY.LeftArrow and -1 or 1)
        else
            pickPerk(row)
        end
        fill()
    elseif code == input.KEY.Enter then
        if ready() then goTo(page + 1) end
    elseif code == input.KEY.Backspace then
        if page > 0 then goTo(page - 1) end
    elseif code == input.KEY.R then
        reset()
    else
        return false
    end
    return true
end

return module
