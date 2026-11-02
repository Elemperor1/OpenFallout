-- The data page of the Pip-Boy (DATA) in Fallout 3 and New Vegas, drawn from the game's own menu file
-- (main/map_menu.xml): the headline with the place the player is in and the date, the tabs along the bottom, the world
-- map (its picture, the places the player has found, the arrow of the player, the box in the middle that names the place
-- under it), the list of quests with the objectives of the one that is picked, and the lists of notes and radio stations.
-- What the player knows is the engine's (core.pipboy, core.interact); this module lists it. It is a module of
-- falloutpipboy.lua, not a script.
--
-- Not there yet: the local map (the games draw the cell the player is in from a render of it, which has no data here, so
-- the tab is not shown), the turn of the arrow to where the player faces, the markers of the quests on the map, the text
-- and the audio of the notes and the stations of the radio (their lists are empty), the active quest, and the buttons
-- of the pad (the file shows them only on a console, and the pad focus is not done).
--
--   local page = require('scripts.omw.falloutpipboydata')
--   page.show({ close = function() ... end })   -- true when the page is on the screen, false when the menu file is not there
--   page.press(keyCode)    -- true when the key was for the page
--   page.update()          -- each frame while the page is shown
--   page.hide()
local core = require('openfallout.core')
local input = require('openfallout.input')
local ui = require('openfallout.ui')

local menuModule = require('scripts.omw.falloutmenu')

local pipboy = core.pipboy

local module = {}

local MENU_FILE = 'main/map_menu.xml'

-- The tabs, with the number the file gives each (`_CurrentTab`; 0 is the local map, which is not there)
local TABS = {
    { key = 'world', value = 1, label = 'WORLD MAP' },
    { key = 'quests', value = 2, label = 'QUESTS' },
    { key = 'notes', value = 3, label = 'NOTES' },
    { key = 'radio', value = 4, label = 'RADIO' },
}

local MAP = {
    -- The picture the file shows the world with, the arrow of the player and the box that names a place
    image = 'MM_WorldMap_ParentImage',
    arrow = 'MM_WorldMapCursor',
    box = 'MM_MapHighlightBox',
    -- The middle of the window of the map in the file's units: the point the picture is moved to when it is centred
    -- (the highest the picture can be moved is until its edge is here), and the half size of the box that names a place
    centerX = 427.5,
    centerY = 250,
    half = 80,
    -- How far the picture can be zoomed (the factor of its size in the file) and by how much each turn of the wheel
    leastZoom = 0.25,
    mostZoom = 1.0,
    step = 1.25,
    -- The widths the file leaves to the code: places, quest markers and the arrow of the player
    markerWidth = 34,
    questWidth = 24,
    arrowWidth = 28,
}
-- Where the file puts the objectives of a quest (the rectangle under the button of the quest page) with its button shown
local DATA_RECT_Y = 149
-- The size of one cell of the worlds, the units of the positions of the markers and of the cells of the map rectangle
local CELL = 4096

-- The picture of a marker: the games' code has one table of fourteen pictures, by the kind of place the marker's record
-- says it is (1 to 14, the same order in both games' executables), and a picture for a place the player has seen on the
-- map but cannot travel to yet. A kind outside the table (the code would read past it) has the dot of a waypoint.
local MARKER_FOLDER = 'Interface\\Icons\\World Map\\icon_map_'
local MARKER_KINDS = {
    'city', 'settlement', 'encampment', 'natural_landmark', 'cave', 'factory', 'monument', 'military', 'office',
    'ruins_town', 'ruins_urban', 'ruins_sewer', 'metro', 'vault',
}
local MARKER_UNDISCOVERED = MARKER_FOLDER .. 'undiscovered.dds'
local MARKER_DEFAULT = 'Interface\\Worldmap\\waypoint_node.dds'

local shown, lists, tabs = nil, nil, nil
local tabIndex = 1
-- What the page was asked for (what to do when the player travels), the facts of the world and the markers found
local options = {}
local info, markers = nil, {}
-- The tiles of the markers (by their place in `markers`), the one that the box names, and the quests of the list
local marked = {}
local named = nil
local quests = {}
-- The place and the size of the picture of the world: the units of the file (the picture may be moved and zoomed)
local zoom, moveX, moveY = 0.5, 0, 0
local objectivesFor = nil
-- Where the picture was when the pointer took hold of it, while it is dragged
local dragFrom = nil

local function number(value)
    return string.format('%d', math.floor(value + 0.5))
end

-- The time and the date for the headline: "9:41 PM 10.23.2281" (the order and the look of the games' own are not known)
function module.dateText(now)
    if not now or not now.year then return '' end
    local hours = math.floor(now.hour or 0)
    local minutes = math.floor(((now.hour or 0) - hours) * 60)
    local clock = (hours % 12 == 0) and 12 or (hours % 12)
    return string.format('%d:%02d %s %02d.%02d.%d', clock, minutes, hours < 12 and 'AM' or 'PM', (now.month or 0) + 1,
        now.day or 1, now.year)
end

-- Where a place of the world is on the picture, as the part of its width and height from the north-west corner (0 to 1),
-- by the rectangle of cells the worldspace gives the picture, as both executables do: left = north-west cell x * 4096, right
-- = (south-east cell x + 1) * 4096, top = (north-west cell y + 1) * 4096, bottom = south-east cell y * 4096, and the rows go
-- up to the north. nil when the worldspace gives none.
function module.mapPoint(world, x, y)
    if not world or (world.width or 0) <= 0 or (world.height or 0) <= 0 then return nil end
    local across = world.southEastX - world.northWestX + 1
    local down = world.northWestY - world.southEastY + 1
    if across <= 0 or down <= 0 then return nil end
    return (x / CELL - world.northWestX) / across, (world.northWestY + 1 - y / CELL) / down
end

local function interact()
    return core.interact
end

local function readInfo()
    local api = interact()
    local ok, got = pcall(function() return api and api.mapInfo and api.mapInfo() or nil end)
    return ok and got or nil
end

local function readMarkers(world)
    local api = interact()
    local ok, got = pcall(function() return api and api.map and api.map() or {} end)
    local found = {}
    if not ok then return found end
    for _, marker in ipairs(got) do
        -- The places of the world whose picture it is (a marker with no world is a place inside a building)
        if world == nil or marker.space == nil or marker.space == world.space then
            found[#found + 1] = marker
        end
    end
    return found
end

-- ---- The world map ----

-- The size of the picture in the units of the file
local function imageSize()
    return shown:get(MAP.image, 'width') or 0, shown:get(MAP.image, 'height') or 0
end

-- Keep the picture where the window still has some of it under the middle
local function clampMove()
    local width, height = imageSize()
    moveX = math.max(MAP.centerX - width, math.min(MAP.centerX, moveX))
    moveY = math.max(MAP.centerY - height, math.min(MAP.centerY, moveY))
end

-- The tile of the box that names a place: the place nearest to the middle of the window, if one is inside the box
local function updateBox()
    local width, height = imageSize()
    local best, bestAway = nil, nil
    for index, marker in ipairs(markers) do
        local tile = marked[index]
        if tile then
            local x = moveX + marker.u * width
            local y = moveY + marker.v * height
            local dx, dy = x - MAP.centerX, y - MAP.centerY
            if math.abs(dx) <= MAP.half and math.abs(dy) <= MAP.half then
                local away = dx * dx + dy * dy
                if not bestAway or away < bestAway then best, bestAway = index, away end
            end
        end
    end
    if best == named then return end
    shown:batch(function()
        if named and marked[named] then shown:set(marked[named], '_selected', false) end
        if best then shown:set(marked[best], '_selected', true) end
        if shown:find(MAP.box) then
            shown:set(MAP.box, '_Title', best and markers[best].name or '')
        end
    end)
    named = best
end

-- Move and size the picture of the world (the markers and the arrow follow it by the file's own expressions)
local function placeMap()
    shown:batch(function()
        shown:set(MAP.image, '_Magnification', zoom)
        clampMove()
        shown:set(MAP.image, 'x', moveX)
        shown:set(MAP.image, 'y', moveY)
    end)
    updateBox()
end

-- Put the picture where the player is, with the place centred under the box
local function centreOn(u, v)
    local width, height = imageSize()
    moveX = MAP.centerX - (u or 0.5) * width
    moveY = MAP.centerY - (v or 0.5) * height
    placeMap()
end

-- Change the zoom, keeping the point under the middle of the window where it is
local function zoomBy(notches)
    local width, height = imageSize()
    if width <= 0 or height <= 0 then return end
    local u, v = (MAP.centerX - moveX) / width, (MAP.centerY - moveY) / height
    local wanted = zoom * (MAP.step ^ (-notches))
    zoom = math.max(MAP.leastZoom, math.min(MAP.mostZoom, wanted))
    shown:set(MAP.image, '_Magnification', zoom)
    width, height = imageSize()
    moveX, moveY = MAP.centerX - u * width, MAP.centerY - v * height
    placeMap()
end

function module.markerIcon(marker)
    if not marker.travel then return MARKER_UNDISCOVERED end
    local kind = MARKER_KINDS[marker.type or 0]
    return kind and (MARKER_FOLDER .. kind .. '.dds') or MARKER_DEFAULT
end

-- The picture, the arrow and the markers of the places the player has found
local function fillWorld()
    info = readInfo()
    markers = readMarkers(info)
    marked, named = {}, nil
    shown:removeChildren(MAP.image)
    local points = {}
    shown:batch(function()
        if info and info.file and info.file ~= '' then
            shown:set(MAP.image, 'filename', info.file)
        end
        shown:set(MAP.image, '_Magnification', zoom)
        shown:set(MAP.image, 'user0', MAP.markerWidth)
        shown:set(MAP.image, 'user1', MAP.questWidth)
        shown:set(MAP.image, 'user2', MAP.arrowWidth)
        local kept = {}
        for _, marker in ipairs(markers) do
            local u, v = module.mapPoint(info, marker.x, marker.y)
            if u and u >= 0 and u <= 1 and v >= 0 and v <= 1 then
                marker.u, marker.v = u, v
                kept[#kept + 1] = marker
            end
        end
        markers = kept
        for index, marker in ipairs(markers) do
            local tile = shown:instantiate(MAP.image, 'MapMarkerTemplate', 'MapMarker' .. index)
            marked[index] = tile
            shown:set(tile, '_x', marker.u)
            shown:set(tile, '_y', marker.v)
            shown:set(tile, '_LocationName', marker.name or '')
            shown:set(tile, '_MarkerIndex', index)
            shown:set(tile, 'filename', module.markerIcon(marker))
            shown:set(tile, 'user0', marker.travel and true or false)
        end
        if info and info.outside then
            local u, v = module.mapPoint(info, info.x, info.y)
            if u then points.player = { u = u, v = v } end
        end
        if shown:find(MAP.arrow) then
            shown:set(MAP.arrow, 'visible', points.player ~= nil)
            if points.player then
                shown:set(MAP.arrow, '_x', points.player.u)
                shown:set(MAP.arrow, '_y', points.player.v)
            end
        end
    end)
    local width = imageSize()
    if width > 0 then
        local centre = points.player or { u = 0.5, v = 0.5 }
        centreOn(centre.u, centre.v)
    end
end

-- The place a click at this point of the window is on, if one: the markers are not clickable tiles, so the page looks
local function markerAt(x, y)
    local slack = 4
    local best, bestAway = nil, nil
    for index, tile in pairs(marked) do
        local rect = shown:rect(tile)
        if rect and x >= rect.x - slack and x <= rect.x + rect.width + slack and y >= rect.y - slack
            and y <= rect.y + rect.height + slack then
            local dx, dy = x - (rect.x + rect.width / 2), y - (rect.y + rect.height / 2)
            local away = dx * dx + dy * dy
            if not bestAway or away < bestAway then best, bestAway = index, away end
        end
    end
    return best
end

local function travelTo(index)
    local marker = markers[index]
    if not marker then return end
    if not marker.travel then
        ui.showMessage(marker.name ~= '' and marker.name or 'Unknown')
        return
    end
    local api = interact()
    local why = api and api.travel and api.travel(marker.id) or ''
    if why == '' then
        if options.close then options.close() end
    else
        ui.showMessage(why)
    end
end

-- ---- Quests ----

local function questRows()
    quests = {}
    for _, quest in ipairs(pipboy.quests()) do
        -- A quest that is done is not listed, as in the games
        if not quest.completed then quests[#quests + 1] = quest end
    end
    local rows = {}
    for index, quest in ipairs(quests) do
        rows[index] = { string = quest.name, _ShowEmptyMarker = true }
    end
    return rows
end

-- The objectives of the quest that is picked: the places the player was sent to, the ones done dim with their square
-- filled
local function fillObjectives()
    local row = lists.quests.selected()
    local quest = row > 0 and quests[row] or nil
    local key = quest and quest.id or false
    if key == objectivesFor then return end
    objectivesFor = key
    local rows = {}
    if quest then
        for index, objective in ipairs(quest.objectives or {}) do
            rows[index] = {
                string = objective.text,
                _ShowEmptyMarker = true,
                _selected = objective.completed and true or false,
                _TextAlpha = objective.completed and 128 or 255,
            }
        end
    end
    shown:batch(function()
        lists.objectives.rows(rows)
        shown:set('MM_DataRect', '_ItemType', quest and 4 or 0)
        shown:set('MM_DataRect', 'visible', quest ~= nil)
    end)
end

local function fillQuests()
    objectivesFor = nil
    lists.quests.rows(questRows())
    if lists.quests.count() > 0 and lists.quests.selected() <= 0 then lists.quests.select(1, true) end
    fillObjectives()
end

-- ---- The page ----

local function fillHeadline()
    shown:batch(function()
        -- The engine names the place the player is in (a cell without a name has its worldspace's); the worldspace of the
        -- map stands in when it gives none
        local place = info and ((info.location ~= '' and info.location) or info.world) or ''
        shown:set('MM_Headline_LocationInfo', 'string', place or '')
        shown:set('MM_Headline_TimeDateInfo', 'string', module.dateText(info))
    end)
end

local function fill()
    fillWorld()
    fillQuests()
    lists.notes.rows({})
    lists.radio.rows({})
    shown:batch(function()
        shown:set('MM_DataRect', 'visible', TABS[tabIndex].key == 'quests' and #quests > 0)
    end)
    fillHeadline()
end

local function pickTab(index)
    tabIndex = index
    if tabs then tabs.pick(index) end
    shown:batch(function()
        -- The quest page has the objectives beside its list; the other pages have nothing there
        shown:set('MM_DataRect', 'visible', TABS[index].key == 'quests' and #quests > 0)
    end)
end

local function open()
    local opened = menuModule.open(MENU_FILE, { box = menuModule.PIPBOY_BOX })
    if not opened then return false end
    local made = {
        quests = opened:list('MM_QuestsList', 'MM_ListMarkerTemplate'),
        objectives = opened:list('MM_QuestObjectivesList', 'MM_ListMarkerTemplate'),
        notes = opened:list('MM_NotesList', 'MM_ListTemplate'),
        radio = opened:list('MM_RadioStationList', 'MM_ListMarkerTemplate'),
    }
    if not (made.quests and made.objectives and made.notes and made.radio and opened:find(MAP.image)) then
        opened:close()
        return false
    end
    local labels, values = {}, {}
    for index, tab in ipairs(TABS) do
        labels[index], values[index] = tab.label, tab.value
    end
    local ok, result = pcall(function()
        return opened:tabs('MM_Tabline', 'TabButtonTemplate', labels, tabIndex, function(index) pickTab(index) end,
            { values = values })
    end)
    if not ok then
        print('data menu: ' .. tostring(result))
        opened:close()
        return false
    end
    shown, lists, tabs = opened, made, result
    opened:batch(function()
        -- The lists that can be picked from take the pointer (the objectives are only read)
        for _, name in ipairs({ 'MM_QuestsList', 'MM_NotesList' }) do
            opened:set(name, '_enabled', true)
        end
        -- The picture of the world takes the pointer (the file leaves out `target`, which a hot rectangle has by default
        -- in the games; the menu reader here gives none by default)
        opened:set(MAP.image, 'target', true)
        -- The buttons are for what is not there yet (Show Location, Place Marker, Make Active, the notes): not shown
        for _, name in ipairs({ 'MM_ButtonA', 'MM_ButtonX', 'MM_ButtonY' }) do
            if opened:find(name) then opened:set(name, 'visible', false) end
        end
        -- What the file puts under the buttons stays where it is with a button there (the file moves it up when no button
        -- is shown)
        opened:set('MM_DataRect', 'y', DATA_RECT_Y)
    end)
    opened:onDrag(MAP.image, function() return nil end, function() dragFrom = nil end, function()
        -- The picture follows the pointer: it is where it was when the pointer took hold, moved by how far the pointer
        -- has gone since (the file's `dragx` and `dragstartx` are the pointer in the window of the map, not in the
        -- picture, which moves)
        local startX, startY = opened:get(MAP.image, 'dragstartx'), opened:get(MAP.image, 'dragstarty')
        if not dragFrom or dragFrom.startX ~= startX or dragFrom.startY ~= startY then
            dragFrom = { startX = startX, startY = startY, x = moveX, y = moveY }
        end
        moveX = dragFrom.x + opened:get(MAP.image, 'dragx') - startX
        moveY = dragFrom.y + opened:get(MAP.image, 'dragy') - startY
        placeMap()
    end)
    opened:onWheel(function(notches)
        if TABS[tabIndex].key == 'world' then zoomBy(notches) end
    end)
    opened:onClick(MAP.image, function()
        local at = opened:pointerAt()
        -- A drag that ends on the picture is not a click
        if not at or (at.startX and (math.abs(at.x - at.startX) > 6 or math.abs(at.y - at.startY) > 6)) then return end
        local index = markerAt(at.x, at.y)
        if index then travelTo(index) end
    end)
    return true
end

function module.isOpen()
    return shown ~= nil
end

-- Show the page (open the menu if it is not open) with what the player knows now; false when it cannot be shown.
-- `close` is what is done when the player has travelled from the map (the Pip-Boy closes).
function module.show(asked)
    options = asked or options
    if not shown and not open() then
        return false
    end
    -- A menu file that is not what the page expects is left, and the older page is shown
    local ok, problem = pcall(function()
        fill()
        pickTab(tabIndex)
    end)
    if not ok then
        print('data menu: ' .. tostring(problem))
        module.hide()
        return false
    end
    return true
end

function module.hide()
    if shown then
        shown:close()
    end
    shown, lists, tabs = nil, nil, nil
    marked, named, objectivesFor, dragFrom = {}, nil, nil, nil
end

-- Each frame the page is shown: the window size, and the objectives of the quest that is picked
function module.update()
    if not shown then return end
    shown:checkSize()
    if TABS[tabIndex].key == 'quests' then fillObjectives() end
end

-- What a key does while the page is there: [ and ] go to the tab before and after, the arrows and pages pick a quest,
-- Enter travels to the place that the box on the map names. False when the key is not the page's (Left and Right change
-- the page of the Pip-Boy).
function module.press(code)
    if not shown then return false end
    local key = TABS[tabIndex].key
    if code == input.KEY.LeftBracket then
        pickTab((tabIndex - 2) % #TABS + 1)
    elseif code == input.KEY.RightBracket then
        pickTab(tabIndex % #TABS + 1)
    elseif key == 'world' and code == input.KEY.Enter then
        if named then travelTo(named) end
    elseif key == 'quests' and (code == input.KEY.DownArrow or code == input.KEY.PageDown
        or code == input.KEY.UpArrow or code == input.KEY.PageUp) then
        local list = lists.quests
        local count, picked = list.count(), list.selected()
        local down = code == input.KEY.DownArrow or code == input.KEY.PageDown
        local by = (code == input.KEY.DownArrow or code == input.KEY.UpArrow) and 1 or 8
        if count > 0 then
            if picked <= 0 then
                list.select(down and 1 or count, true)
            else
                list.select(down and math.min(count, picked + by) or math.max(1, picked - by), true)
            end
        end
        fillObjectives()
    else
        return false
    end
    return true
end

return module
