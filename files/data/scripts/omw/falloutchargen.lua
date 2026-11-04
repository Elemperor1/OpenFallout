-- The making of the player's character in Fallout 3 and New Vegas: the name, the sex and race, the points of SPECIAL, the
-- tagged skills and the traits. The rules and the state are the engine's (core.chargen): a script of the game asks for a
-- menu, the engine opens its screens one after the other, and this script draws the one that is open and gives the
-- player's choices back. The name is entered on the game's own text menu (dialog/texteditmenu.xml) and the points of SPECIAL
-- and the tags of the skills on its character making menu (chargen/char_gen_menu.xml), each drawn from the file; a screen
-- whose file the install lacks, the traits (which of the game's two trait menus is opened for them is not read yet) and the
-- screen of the sex and race (a menu with a rendered head and sliders, which has nothing to show here) are the stand-in
-- widgets of this script. The points of SPECIAL asked for by ShowLoveTesterMenu (the Vigor Tester) or ShowSPECIALBookMenu
-- are not a menu file but a model whose pages the player turns: falloutspecialscreen.lua draws them.
-- The face of the character is not made here (no sliders, no head to turn).
local core = require('openfallout.core')
local input = require('openfallout.input')
local async = require('openfallout.async')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')

-- A message box drawn over the screen (falloutmessage.lua) has the keys, the pad and the wheel
local function messageOpen()
    return I.FalloutMessage ~= nil and I.FalloutMessage.isOpen()
end

local menuModule = require('scripts.omw.falloutmenu')
-- The screens of SPECIAL that are a model whose pages the player turns (the Vigor Tester, the SPECIAL book)
local special = require('scripts.omw.falloutspecialscreen')
local face = require('scripts.omw.falloutfacescreen')

local chargen = core.chargen
local pipboy = core.pipboy
local v2 = util.vector2

local normalColor = util.color.rgb(0.45, 1, 0.55)
local focusColor = util.color.rgb(1, 1, 1)
local titleColor = util.color.rgb(1, 0.85, 0.4)
local pickedColor = util.color.rgb(1, 0.85, 0.4)
local hintColor = util.color.rgb(0.6, 0.7, 0.6)
local dimColor = util.color.rgb(0.3, 0.45, 0.35)

local textSize = ui._getDefaultFontSize() + 4
local gap = v2(0, 6)
local descriptionLines = 4

local element = nil
local shownSerial = nil
local shownPage = nil
-- This script opened the mode of the screens
local modeOpen = false
-- What was typed for the name, until the screen is done
local typed = nil
-- The lines of the description of what the pointer is over, which change without the screen being made again
local description = nil

-- The screen drawn from a menu file of the game, and what it does to show the character again after a choice
local shown = nil
local updateShown = nil
-- What the keys do on it: `move(delta)` to another row, `change(delta)` to adjust or to tag the row that is picked
local controls = nil
-- What is done every frame while a screen from a file is up (the pointer lights rows without the script being asked)
local follow = nil

local function update()
    if element then element:update() end
end

local function closeShown()
    if shown then
        shown:close()
        shown = nil
    end
    updateShown = nil
    controls = nil
    follow = nil
end

local function destroyElement()
    special.close()
    face.close()
    if element then
        element:destroy()
        element = nil
    end
    closeShown()
    description = nil
end

local function openMode()
    if not modeOpen then
        modeOpen = true
        -- An empty mode: the game stands still and the pointer is there, and nothing of Morrowind's is drawn
        I.UI.addMode('Interface', { windows = {} })
    end
end

local function closeMode()
    if modeOpen then
        modeOpen = false
        I.UI.removeMode('Interface')
    end
end

local render

-- The words of a text in lines that fit the width (text widgets do not wrap)
local function wrap(text, width)
    local limit = math.max(8, math.floor(width / (textSize * 0.5)))
    local lines, current = {}, ''
    for word in text:gmatch('%S+') do
        if current == '' then
            current = word
        elseif #current + 1 + #word <= limit then
            current = current .. ' ' .. word
        else
            lines[#lines + 1] = current
            current = word
        end
    end
    if current ~= '' then lines[#lines + 1] = current end
    return lines
end

local function label(text, color, size)
    return {
        type = ui.TYPE.Text,
        props = { text = text, textSize = size or textSize, textColor = color or focusColor },
    }
end

local function spacer(width, height)
    return { props = { size = v2(width, height) } }
end

-- A widget of a fixed width, so that what is in a column starts at the same place in every row (letters have no width
-- that can be counted)
local function cell(layout, width)
    local container = {
        props = { size = v2(width, textSize + 6) },
        content = ui.content { layout },
    }
    -- What a line of text does when it is clicked is done anywhere in its cell
    if layout.events then
        container.events = layout.events
        layout.events = nil
    end
    return container
end

local function row(...)
    return {
        type = ui.TYPE.Flex,
        props = { horizontal = true, autoSize = true },
        content = ui.content { ... },
    }
end

-- A line of text that does something when it is clicked. `options`: color (when not under the pointer), onFocus (called
-- when the pointer comes), enabled (false: a dim line that does nothing)
local function clickable(text, onClick, options)
    options = options or {}
    local color = options.color or normalColor
    local layout = { type = ui.TYPE.Text, props = { text = text, textSize = textSize, textColor = color } }
    if options.enabled == false then
        layout.props.textColor = dimColor
        return layout
    end
    layout.events = {
        mouseClick = async:callback(onClick),
        focusGain = async:callback(function()
            layout.props.textColor = focusColor
            if options.onFocus then options.onFocus() end
            update()
        end),
        focusLoss = async:callback(function()
            layout.props.textColor = color
            update()
        end),
    }
    return layout
end

-- A place for the description of what the pointer is over: a few lines, always the same height
local function descriptionBox(width, initial)
    description = { width = width, lines = {} }
    local rows = {}
    for i = 1, descriptionLines do
        description.lines[i] = label('', hintColor, textSize - 2)
        rows[i] = description.lines[i]
    end
    function description.set(text)
        local wrapped = wrap(text or '', width)
        for i = 1, descriptionLines do
            description.lines[i].props.text = wrapped[i] or ''
        end
    end
    description.set(initial)
    return {
        type = ui.TYPE.Flex,
        props = { autoSize = true },
        content = ui.content(rows),
    }
end

local function describe(text)
    return function()
        if description then description.set(text) end
    end
end

local function doneButton(onDone, enabled)
    return clickable('Done', onDone, { color = titleColor, enabled = enabled })
end

local function finishPage()
    typed = nil
    chargen.finish()
end

-- Whether the Done button of a screen drawn from the file is lit: nothing is left to spend or to tag
local function doneReady()
    local state = chargen.state()
    if shownPage == 'special' then return state.pointsLeft <= 0 end
    if shownPage == 'skills' then return state.tagsLeft <= 0 end
    return true
end

-- The pages

local nameLabel = nil
local nameLimit = 31

local function nameText()
    return (typed or '') .. '_'
end

-- The name is typed with the keys of the keyboard that this script reads (a text edit takes the keys only after it was
-- clicked, and the player should be able to type at once)
local function namePage(state, add)
    if typed == nil then typed = state.name end
    nameLimit = state.nameLimit
    add(label('Your name', titleColor, textSize + 4))
    add(label('Type the name of your character, then press Enter.', hintColor, textSize - 2))
    nameLabel = label(nameText(), focusColor)
    add {
        template = I.MWUI.templates.box,
        content = ui.content {
            {
                template = I.MWUI.templates.padding,
                content = ui.content {
                    {
                        type = ui.TYPE.Flex,
                        props = { autoSize = true },
                        content = ui.content { spacer(280, 0), nameLabel },
                    },
                },
            },
        },
    }
    add(doneButton(function()
        if chargen.setName(typed or '') ~= '' then
            finishPage()
        end
    end))
end

-- A key pressed on the screen of the name: a letter, a digit or a sign is added, Backspace takes the last character back
local function typeName(key)
    if key.code == input.KEY.Backspace then
        typed = (typed or ''):gsub('[%z\1-\127\194-\244][\128-\191]*$', '')
    elseif key.symbol ~= '' then
        local byte = key.symbol:byte()
        if byte >= 32 and byte < 127 and #(typed or '') < nameLimit then
            typed = (typed or '') .. (key.withShift and key.symbol:upper() or key.symbol)
        end
    else
        return
    end
    if shown then
        shown:set('textedit_text', 'string', nameText())
    elseif nameLabel then
        nameLabel.props.text = nameText()
        update()
    end
end

local function racePage(state, add)
    add(label('Sex and race', titleColor, textSize + 4))
    local function choice(text, picked, onClick)
        return clickable((picked and '> ' or '  ') .. text, onClick, { color = picked and pickedColor or normalColor })
    end
    add(row(
        choice('Male', not state.female, function()
            chargen.setFemale(false)
            render()
        end),
        spacer(24, 0),
        choice('Female', state.female, function()
            chargen.setFemale(true)
            render()
        end)))
    for _, race in ipairs(state.races) do
        local id = race.id
        add(choice(race.name, race.selected, function()
            chargen.setRace(id)
            render()
        end))
    end
    add(doneButton(finishPage))
end

local function specialPage(state, add, width)
    add(label('S.P.E.C.I.A.L.', titleColor, textSize + 4))
    add(label('Points left: ' .. state.pointsLeft, state.pointsLeft == 0 and hintColor or pickedColor))
    local nameWidth = math.floor(width * 0.55)
    for _, attribute in ipairs(state.attributes) do
        local number = attribute.number
        local text = describe(attribute.description)
        add(row(
            cell(label(attribute.name, focusColor), nameWidth),
            cell(clickable('-', function()
                chargen.adjust(number, -1)
                render()
            end, { onFocus = text, enabled = attribute.value > state.minimum }), 28),
            cell(label(string.format('%2d', attribute.value), focusColor), 40),
            cell(clickable('+', function()
                chargen.adjust(number, 1)
                render()
            end, { onFocus = text, enabled = attribute.value < state.maximum and state.pointsLeft > 0 }), 28)))
    end
    add(descriptionBox(width, ''))
    -- The page is done with when every point is spent, as the file's screen has it (this page is also what a failed
    -- screen of the model falls back to)
    add(doneButton(finishPage, state.pointsLeft <= 0))
end

local function skillsPage(state, add, width)
    add(label('Tag skills', titleColor, textSize + 4))
    add(label('Tagged skills start 15 points higher. Tags left: ' .. state.tagsLeft, state.tagsLeft == 0 and hintColor or pickedColor))
    local nameWidth = math.floor(width * 0.7)
    for _, skill in ipairs(state.skills) do
        local number = skill.number
        add(row(
            cell(clickable((skill.tagged and '[x] ' or '[ ] ') .. skill.name, function()
                chargen.toggleTag(number)
                render()
            end, { color = skill.tagged and pickedColor or normalColor, onFocus = describe(skill.description) }),
                nameWidth),
            cell(label(tostring(skill.value), skill.tagged and pickedColor or focusColor), 48)))
    end
    add(descriptionBox(width, ''))
    add(doneButton(finishPage))
end

local function traitsPage(state, add, width)
    add(label('Traits', titleColor, textSize + 4))
    add(label('Pick up to ' .. state.traitLimit .. ' traits. Picks left: ' .. state.traitsLeft, hintColor))
    for _, trait in ipairs(state.traits) do
        local id = trait.id
        add(clickable((trait.picked and '[x] ' or '[ ] ') .. trait.name, function()
            chargen.toggleTrait(id)
            render()
        end, { color = trait.picked and pickedColor or normalColor, onFocus = describe(trait.description) }))
    end
    add(descriptionBox(width, ''))
    add(doneButton(finishPage))
end


-- ---- The screens drawn from the game's own menu files ----

local MENU_NAME = 'dialog/texteditmenu.xml'
local MENU_POINTS = 'chargen/char_gen_menu.xml'
local MENU_TRAITS = 'trait_menu.xml'
-- Where the list of the character making menu starts in its window (the file leaves the place to the code: its y is 0, the
-- title and its lines are above it): the number each page of the executables sets (105 for the points of S.P.E.C.I.A.L., 65
-- for the skills)
local SPECIAL_LIST_Y = 105
local SKILLS_LIST_Y = 65

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- The tile of a click is in a row; the name of the tile in the row that was hit (an arrow of the row, or the row itself)
local function hitName(opened, tile)
    local hit = tile
    while hit do
        local name = opened.menu:name(hit)
        if name == 'CGM_Template_LeftArrow' or name == 'CGM_Template_RightArrow' then return name end
        hit = opened.menu:parent(hit)
    end
    return nil
end

-- The counter under the description, as the executables write it: the setting's text is a format with the number of
-- points that are left, and an 's' follows it unless that number is one. With none left the counter is not shown.
local function showCounter(opened, setting, fallback, left)
    local format = word(setting, fallback)
    local ok, text = pcall(string.format, format, left)
    opened:set('CGM_PointCounter', 'string', (ok and text or format) .. (left ~= 1 and 's' or ''))
    opened:set('CGM_PointCounter', 'visible', left ~= 0)
end

-- The title, as the executables write it: the setting's text is a format for the points given and all that can be given
-- ("x/y"), and is not changed in case
local function showTitle(opened, setting, fallback, current, maximum)
    local format = word(setting, fallback)
    local ok, text = pcall(string.format, format, current, maximum)
    opened:set('CGM_Headline_Title', 'string', ok and text or format)
end

-- What the screen says of the lit row: its description, and its picture, which is the `ICON` of the actor value's record
-- (the executables take it from there and from nothing else)
local function showPick(opened, textTile, iconTile, text, icon)
    opened:set(textTile, 'string', text or '')
    local has = icon ~= nil and icon ~= ''
    opened:set(iconTile, 'filename', has and icon or '')
    opened:set(iconTile, 'visible', has)
end

local function showSelection(opened, text, icon)
    showPick(opened, 'CGM_SelectionText', 'CGM_SelectionIcon', text, icon)
end

-- The title and the counter of the trait menu, as the executables write them: each has a format for many and one for a
-- single trait (the title by the most that can be picked, the counter by the picks that are left), and the number fills it
local function traitText(many, manyFallback, one, oneFallback, number)
    local format = number == 1 and word(one, oneFallback) or word(many, manyFallback)
    local ok, text = pcall(string.format, format, number)
    return ok and text or format
end


-- The list menu of the points and tags (SPECIAL, skills) with the row template for each: nothing if the data lacks them
local function pointsMenu(template, listY)
    local opened = menuModule.open(MENU_POINTS)
    if not opened then return nil end
    -- Kept from here on, so that a failure below is cleaned up by closeShown() where the page is made
    shown = opened
    local made = opened:list('CGM_ItemList', template)
    if not made then
        closeShown()
        return nil
    end
    opened:batch(function()
        opened:set('CGM_ItemList', 'y', listY)
    end)
    return opened, made
end

local function specialMenu()
    local opened, made = pointsMenu('CGM_AttributeTemplate', SPECIAL_LIST_Y)
    if not opened then return false end
    local state = chargen.state()
    -- What the screen started with: Reset goes back to it
    local baseline, numbers, texts, icons = {}, {}, {}, {}
    for index, attribute in ipairs(state.attributes) do
        baseline[attribute.number] = attribute.value
        numbers[index] = attribute.number
        texts[index] = attribute.description or ''
        icons[index] = attribute.icon
    end
    local selected, described = 1, nil
    -- The pointer moves the highlight of the list by itself: the row the keys act on is the one that is lit, and the
    -- description is of that row
    local function pick()
        local row = made.selected()
        if row >= 1 and row <= #numbers then selected = row end
    end
    follow = function()
        local row = made.selected()
        if row ~= described and row >= 1 and row <= #numbers then
            described = row
            showSelection(opened, texts[row], icons[row])
        end
    end
    function updateShown()
        local now = chargen.state()
        local rows, total = {}, 0
        for index, attribute in ipairs(now.attributes) do
            total = total + attribute.value
            rows[index] = {
                string = attribute.name,
                _BaseValue = attribute.value,
                _AddedValue = 0,
                _ExtraValue = 0,
                _ValueString = tostring(attribute.value),
                _MinValue = now.minimum,
                _MaxValue = now.maximum,
            }
        end
        opened:batch(function()
            made.rows(rows)
            made.select(selected, true)
            -- The points given are what the values are above their least (1), as the executables count them
            local current = total - #now.attributes
            opened:io('_CurrPoints', current)
            opened:io('_MaxPoints', current + now.pointsLeft)
            showTitle(opened, 'sAttributesTitle', 'ATTRIBUTES: %d/%d Points Assigned', current, current + now.pointsLeft)
            showCounter(opened, 'sAttributesCount', 'Distribute %d Point', now.pointsLeft)
            showSelection(opened, texts[selected], icons[selected])
            described = selected
        end)
    end
    local function adjust(row, delta)
        chargen.adjust(numbers[row], delta)
    end
    made.onClick(function(row, tile)
        selected = row
        local name = hitName(opened, tile)
        if name == 'CGM_Template_LeftArrow' then
            adjust(row, -1)
        elseif name == 'CGM_Template_RightArrow' then
            adjust(row, 1)
        end
        updateShown()
    end)
    opened:onClick('CGM_ResetButton', function()
        pick()
        -- What was taken first, so that the points are there to give back
        for _, sign in ipairs({ -1, 1 }) do
            for _, attribute in ipairs(chargen.state().attributes) do
                local difference = baseline[attribute.number] - attribute.value
                if difference * sign > 0 then chargen.adjust(attribute.number, difference) end
            end
        end
        updateShown()
    end)
    opened:onClick('CGM_DoneButton', finishPage)
    controls = {
        move = function(delta)
            pick()
            selected = math.max(1, math.min(#numbers, selected + delta))
            updateShown()
        end,
        change = function(delta)
            pick()
            adjust(selected, delta)
            updateShown()
        end,
    }
    updateShown()
    return true
end

local function skillsMenu()
    local opened, made = pointsMenu('CGM_SelectItemTemplate', SKILLS_LIST_Y)
    if not opened then return false end
    -- What the screen started with (skills may arrive already tagged): Reset goes back to it
    local numbers, tagged, texts, icons = {}, {}, {}, {}
    for index, skill in ipairs(chargen.state().skills) do
        numbers[index] = skill.number
        tagged[skill.number] = skill.tagged == true
        texts[index] = skill.description or ''
        icons[index] = skill.icon
    end
    local selected, described = 1, nil
    -- The pointer moves the highlight of the list by itself: the row the keys act on is the one that is lit, and the
    -- description is of that row
    local function pick()
        local row = made.selected()
        if row >= 1 and row <= #numbers then selected = row end
    end
    follow = function()
        local row = made.selected()
        if row ~= described and row >= 1 and row <= #numbers then
            described = row
            showSelection(opened, texts[row], icons[row])
        end
    end
    function updateShown()
        local now = chargen.state()
        local rows = {}
        for index, skill in ipairs(now.skills) do
            rows[index] = {
                string = skill.name,
                _ValueString = tostring(skill.value),
                _selected = skill.tagged == true,
                _MinValue = 0,
                _MaxValue = 100,
            }
        end
        opened:batch(function()
            made.rows(rows)
            made.select(selected, true)
            opened:io('_CurrPoints', now.tagLimit - now.tagsLeft)
            opened:io('_MaxPoints', now.tagLimit)
            showTitle(opened, 'sSkillsTitle', 'SKILLS:  %d/%d Selected', now.tagLimit - now.tagsLeft, now.tagLimit)
            showCounter(opened, 'sSkillsCount', 'Tag %d Skill', now.tagsLeft)
            showSelection(opened, texts[selected], icons[selected])
            described = selected
        end)
    end
    local function toggle(row)
        chargen.toggleTag(numbers[row])
    end
    made.onClick(function(row)
        selected = row
        toggle(row)
        updateShown()
    end)
    opened:onClick('CGM_ResetButton', function()
        pick()
        -- Take off the tags that were added first, so that the ones that were there can be put back within the limit
        for _, skill in ipairs(chargen.state().skills) do
            if skill.tagged == true and not tagged[skill.number] then chargen.toggleTag(skill.number) end
        end
        for _, skill in ipairs(chargen.state().skills) do
            if skill.tagged ~= true and tagged[skill.number] then chargen.toggleTag(skill.number) end
        end
        updateShown()
    end)
    opened:onClick('CGM_DoneButton', finishPage)
    controls = {
        move = function(delta)
            pick()
            selected = math.max(1, math.min(#numbers, selected + delta))
            updateShown()
        end,
        change = function()
            pick()
            toggle(selected)
            updateShown()
        end,
    }
    updateShown()
    return true
end

-- The traits (New Vegas): the list of the file's trait menu, a square on each that is picked, the description and the
-- picture of the lit one, and the counter of the picks that are left. A click picks a trait or takes the pick back (the
-- engine refuses one past the most); Reset takes all back; Continue is lit whatever the number picked, as in the file.
local function traitsMenu()
    local opened = menuModule.open(MENU_TRAITS)
    if not opened then return false end
    shown = opened
    local made = opened:list('LUM_PerkList', 'LUM_PerkTemplate')
    if not made then
        closeShown()
        return false
    end
    local ids, texts, icons = {}, {}, {}
    for index, trait in ipairs(chargen.state().traits) do
        ids[index] = trait.id
        texts[index] = trait.description or ''
        icons[index] = trait.icon
    end
    local selected, described = 1, nil
    local function describe(row)
        showPick(opened, 'TM_DescriptionText', 'LUM_SelectionIcon', texts[row], icons[row])
    end
    local function pick()
        local row = made.selected()
        if row >= 1 and row <= #ids then selected = row end
    end
    follow = function()
        local row = made.selected()
        if row ~= described and row >= 1 and row <= #ids then
            described = row
            describe(row)
        end
    end
    function updateShown()
        local now = chargen.state()
        local rows = {}
        for index, trait in ipairs(now.traits) do
            rows[index] = { string = trait.name, _selected = trait.picked == true, _TextAlpha = 255 }
        end
        opened:batch(function()
            made.rows(rows)
            made.select(selected, true)
            opened:io('_CurrPoints', now.traitLimit - now.traitsLeft)
            opened:io('_MaxPoints', now.traitLimit)
            opened:set('LUM_Headline_Title', 'string',
                traitText('sTraitMenuTitleText', 'CHOOSE UP TO %d TRAITS', 'sTraitMenuTitleTextSingular', 'CHOOSE %d TRAIT',
                    now.traitLimit))
            opened:set('LUM_PointCounter', 'string',
                traitText('sTraitMenuCounter', '%d TRAITS LEFT', 'sTraitMenuCounterSingular', '%d TRAIT LEFT', now.traitsLeft))
            describe(selected)
            described = selected
        end)
    end
    made.onClick(function(row)
        selected = row
        chargen.toggleTrait(ids[row])
        updateShown()
    end)
    opened:onClick('LUM_ResetButton', function()
        pick()
        for _, trait in ipairs(chargen.state().traits) do
            if trait.picked then chargen.toggleTrait(trait.id) end
        end
        updateShown()
    end)
    opened:onClick('LUM_ContinueButton', finishPage)
    controls = {
        move = function(delta)
            pick()
            selected = math.max(1, math.min(#ids, selected + delta))
            updateShown()
        end,
        change = function()
            pick()
            chargen.toggleTrait(ids[selected])
            updateShown()
        end,
    }
    updateShown()
    return true
end

local function nameMenu(state)
    if typed == nil then typed = state.name end
    nameLimit = state.nameLimit
    local opened = menuModule.open(MENU_NAME)
    if not opened then return false end
    shown = opened
    opened:set('textedit_text', 'string', nameText())
    opened:onClick('textedit_button_ok', function()
        if chargen.setName(typed or '') ~= '' then finishPage() end
    end)
    return true
end

-- The screens that are drawn from a menu file: each opens the menu and gives back whether it could
local menuPages = {
    name = nameMenu,
    special = specialMenu,
    skills = skillsMenu,
    traits = traitsMenu,
}

local pages = {
    name = namePage,
    race = racePage,
    special = specialPage,
    skills = skillsPage,
    traits = traitsPage,
}

function render()
    destroyElement()
    nameLabel = nil
    local page = chargen.page()
    if not page or not pages[page.page] then
        return
    end
    shownPage = page.page
    local state = chargen.state()
    -- The Vigor Tester and the SPECIAL book are models that the player turns; the rest of the screens are menu files
    if page.page == 'special' and special.start(page, state) then
        return
    end
    -- The screen of the sex, race and face is the menu file's panel with the lists the code makes
    if page.page == 'race' then
        local ok, drawn = pcall(face.start, page, state, { done = finishPage })
        if ok and drawn then return end
        if not ok then print('chargen face screen: ' .. tostring(drawn)) end
        face.close()
    end
    if menuPages[page.page] then
        local ok, drawn = pcall(menuPages[page.page], state)
        if ok and drawn then return end
        if not ok then print('chargen menu: ' .. tostring(drawn)) end
        closeShown()
    end
    local width = math.max(360, math.floor(ui.screenSize().x * 0.4))

    local rows = { { props = { size = v2(width, 0) } } }
    local function add(layout)
        rows[#rows + 1] = { props = { size = gap } }
        rows[#rows + 1] = layout
    end
    pages[page.page](state, add, width)

    element = ui.create {
        layer = 'Windows',
        template = I.MWUI.templates.boxSolid,
        props = {
            relativePosition = v2(0.5, 0.5),
            anchor = v2(0.5, 0.5),
        },
        content = ui.content {
            {
                template = I.MWUI.templates.padding,
                content = ui.content {
                    {
                        type = ui.TYPE.Flex,
                        props = { autoSize = true },
                        content = ui.content(rows),
                    },
                },
            },
        },
    }
end

local function sync()
    -- The game stands still while a screen is up and the engine hands onFrame a step of 0 then: the time of the frame is
    -- the real one (Mac round 12AZ E)
    local seconds = core.getRealFrameDuration()
    if special.isOpen() and special.update(seconds) then
        -- The screen of the model failed: the page is shown by the stand-in
        render()
    end
    if face.isOpen() then
        face.update(seconds or 0)
    end
    -- A menu drawn from a file keeps the window size it was opened with until it is told the window changed
    if shown then
        shown:checkSize()
        if follow then follow() end
    end
    local page = chargen.page()
    if not page then
        if element or modeOpen then
            destroyElement()
            closeMode()
            typed = nil
            shownSerial = nil
            shownPage = nil
        end
        return
    end
    if page.serial ~= shownSerial then
        shownSerial = page.serial
        typed = nil
        openMode()
        render()
    end
end

return {
    engineHandlers = {
        -- The game stands still while a screen is open, and the screens of the engine open between frames
        onFrame = sync,
        -- The buttons of a pad on the model screens of SPECIAL
        onControllerButtonPress = function(button)
            special.noteDevice(true)
            -- A box over the screen has the pad, as it has the keys; so has the pause menu (Escape opens it over the
            -- Vigor Tester and the book, and the mode is the main menu then)
            if messageOpen() then
                return
            end
            if special.isOpen() and I.UI.getMode() == 'Interface' then
                special.pad(button)
            end
        end,
        onMouseButtonPress = function()
            special.noteDevice(false)
        end,
        onKeyPress = function(key)
            special.noteDevice(false)
            if messageOpen() then
                return
            end
            if special.isOpen() then
                if I.UI.getMode() == 'Interface' then
                    special.press(key.code, key.symbol)
                end
                return
            end
            if face.isOpen() then
                face.press(key.code, key.symbol, key.withShift)
                return
            end
            if not shownPage or not (element or shown) then
                return
            end
            if shown and controls and shownPage ~= 'name' and key.code ~= input.KEY.Enter then
                if key.code == input.KEY.UpArrow then
                    controls.move(-1)
                elseif key.code == input.KEY.DownArrow then
                    controls.move(1)
                elseif key.code == input.KEY.LeftArrow then
                    controls.change(-1)
                elseif key.code == input.KEY.RightArrow or key.code == input.KEY.Space then
                    controls.change(1)
                end
                return
            end
            if key.code == input.KEY.Enter then
                if shownPage == 'name' then
                    if chargen.setName(typed or '') ~= '' then finishPage() end
                elseif (shown or shownPage == 'special') and not doneReady() then
                    -- The Done button of the file is dim until the points are spent or the tags are put: so is Enter (and
                    -- on the stand-in of the SPECIAL page)
                    return
                else
                    finishPage()
                end
            elseif shownPage == 'name' then
                typeName(key)
            end
        end,
    },
    eventHandlers = {
        -- A screen that is left (Escape) is done with what it has so far; the main menu on top of it is not leaving it
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                if special.isOpen() then
                    -- Escape does not leave the Vigor Tester or the book (it opens the pause menu over them): the mode stays
                    modeOpen = false
                    openMode()
                    return
                end
                if face.isOpen() then
                    -- Escape goes up one list of the screen of the face, and the screen cannot be left that way
                    modeOpen = false
                    openMode()
                    face.escape()
                    return
                end
                modeOpen = false
                destroyElement()
                if chargen.page() then
                    if shownPage == 'name' then
                        if typed then chargen.setName(typed) end
                        -- A character has a name: without one the screen comes back (on the next frame)
                        if chargen.state().name == '' then
                            typed = nil
                            shownSerial = nil
                            return
                        end
                    end
                    typed = nil
                    chargen.finish()
                end
                shownSerial = nil
            end
        end,
    },
}
