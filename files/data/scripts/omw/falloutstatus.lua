-- The head-up display of the player in Fallout 3 and New Vegas: health at the bottom left with the compass under it,
-- action points and the rounds of the weapon at the bottom right, the line of messages above the health, the sneak
-- meter, the experience bar, the radiation, the limbs that are crippled, the reticle and the health of the character
-- under it. The sizes follow what the executables read from the menu files (the menu is 960 units high, as wide as the
-- screen is, with a margin of twice the safe zone of 15 on each side) and the pictures are those of the games' interface
-- atlas when the files have it; the places that the executable sets in code and that were not read (the sneak meter, the
-- experience bar, the radiation, the limbs, the enemy bar) and the field of the compass are stand-ins, and where a
-- picture is missing a plain bar is drawn. The numbers are the engine's (core.pipboy.stats, core.combat.weapon,
-- core.hud). `I.FalloutHud.state()` is what the display shows now, for tests and probes.
local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local self = require('openfallout.self')
local camera = require('openfallout.camera')
local nearby = require('openfallout.nearby')
local vfs = require('openfallout.vfs')
local widgetModule = require('scripts.omw.falloutwidgets')

local pipboy = core.pipboy
local hud = core.hud

-- Only content that has a Pip-Boy has this display
if not pipboy.available() then
    return {}
end

local v2 = util.vector2
local colors = widgetModule

-- The height of the menu, the margin on each side (twice the safe zone), the width and height of a bar, and the units
-- from the top of a block to its bar
local menuHeight = 960
local margin = 30
local blockHeight = 127
local barWidth = 300
local barHeight = 20
local barTop = 41
local textTop = 8
local ammoTop = 80
local hpLeft = 40
local apWidth = 386
-- The window of the compass inside the block of health: its width and height, its place, and how many degrees it shows
local compassWidth = 345
local compassHeight = 64
local compassLeft = 20
local compassTop = 65
-- How long the experience bar stays after the experience changed, and how long a line of the message area is lit up
-- before it fades
local experienceSeconds = 5
local fadeSeconds = 0.5
-- How many lines of messages are shown at once
local messageLines = 3
local messageHeight = 28
-- How far the crosshair looks for a character whose health it shows
local targetRange = 3000

local white = ui.texture { path = 'white' }

-- The interface atlas of the games and where the pieces are in it (the normalised rectangles of its .tai file, times
-- the size of the texture)
local atlasPath = 'textures/interface/interfaceshared0.dds'
local atlasSize = 1024
local atlasPieces = {
    strip = { 0.000488, 0.000488, 0.999023, 0.061523 },
    enemyBar = { 0.000488, 0.062988, 0.499023, 0.061523 },
    leftBracket = { 0.000488, 0.125488, 0.249023, 0.124023 },
    rightBracket = { 0.250488, 0.125488, 0.249023, 0.124023 },
    messageBracket = { 0.500488, 0.125488, 0.249023, 0.124023 },
    experienceBar = { 0.750488, 0.125488, 0.249023, 0.124023 },
    radiationBracket = { 0.750488, 0.062988, 0.249023, 0.061523 },
    radiationArrow = { 0.250488, 0.812988, 0.061523, 0.030273 },
    landmark = { 0.750488, 0.687988, 0.061523, 0.061523 },
    quest = { 0.625488, 0.687988, 0.061523, 0.061523 },
    tick = { 0.344238, 0.812988, 0.030273, 0.030273 },
    experiencePointer = { 0.437988, 0.750488, 0.061523, 0.061523 },
    crosshair = { 0.812988, 0.750488, 0.061523, 0.061523 },
    crosshairFilled = { 0.875488, 0.750488, 0.061523, 0.061523 },
}
local atlasAvailable = false
local ok, exists = pcall(vfs.fileExists, atlasPath)
if ok and exists then atlasAvailable = true end
local pieces = {}

local function piece(name)
    if not atlasAvailable then return nil end
    local found = pieces[name]
    if found == nil then
        local rect = atlasPieces[name]
        found = ui.texture {
            path = atlasPath,
            offset = v2(math.floor(rect[1] * atlasSize), math.floor(rect[2] * atlasSize)),
            size = v2(math.floor(rect[3] * atlasSize + 0.5), math.floor(rect[4] * atlasSize + 0.5)),
        }
        pieces[name] = found
    end
    return found
end

-- The pictures of the files that are not in the atlas (the icons of messages, the limbs): nil when the files lack it
local loose = {}
local function looseTexture(path)
    if path == nil or path == '' then return nil end
    path = path:gsub('\\', '/'):lower()
    local found = loose[path]
    if found == nil then
        local has, present = pcall(vfs.fileExists, path)
        if has and present then
            found = ui.texture { path = path }
        else
            found = false
        end
        loose[path] = found
    end
    return found or nil
end

local element = nil
local compassElement = nil
local shown = nil
local shownCompass = nil
local sinceChecked = 0
local sinceCompass = 0
-- The seconds of play that the HUD has been up, the lines of the message area that are lit, the experience that was last
-- seen and the seconds the bar stays up
local clock = 0
local lines = {}
local lastExperience = nil
local experienceLeft = 0
local experienceGain = 0

local function screenScale()
    local screen = ui.screenSize()
    -- A screen wider than tall is 960 units high; any other is 1280 units wide
    local scale = screen.x > screen.y and screen.y / menuHeight or screen.x / 1280
    return scale, screen.x / scale, screen.y / scale
end

local function box(x, y, width, height, color, alpha, scale)
    return {
        type = ui.TYPE.Image,
        props = {
            resource = white,
            position = v2(x * scale, y * scale),
            size = v2(math.max(width, 0) * scale, height * scale),
            color = color,
            alpha = alpha,
        },
    }
end

local function picture(texture, x, y, width, height, color, alpha, scale)
    return {
        type = ui.TYPE.Image,
        props = {
            resource = texture,
            position = v2(x * scale, y * scale),
            size = v2(width * scale, height * scale),
            color = color,
            alpha = alpha,
        },
    }
end

-- 'left' (the default), 'right' or 'center' puts the x of the text at its left edge, its right edge or its middle
local function words(text, x, y, scale, color, align, alpha)
    local size = math.floor(widgetModule.textSize * math.max(scale, 0.5))
    return {
        type = ui.TYPE.Text,
        props = {
            text = text,
            textSize = size,
            textColor = color or colors.normalColor,
            position = v2(x * scale, y * scale),
            anchor = v2(align == 'right' and 1 or align == 'center' and 0.5 or 0, 0),
            alpha = alpha,
        },
    }
end

local function share(now, most)
    if most <= 0 then return 0 end
    return math.max(0, math.min(1, now / most))
end

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- The block of health, which the compass sits in: the bar fills from the left
local function healthBlock(add, state, scale, top)
    local bracket = piece('leftBracket')
    if bracket then
        add(picture(bracket, hpLeft - 20, top, 369, blockHeight, colors.dimColor, 0.6, scale))
    end
    add(words(state.hpWord .. ' ' .. state.health .. '/' .. state.maxHealth, hpLeft + 20, top + textTop, scale))
    add(box(hpLeft + 20, top + barTop, barWidth, barHeight, colors.dimColor, 0.5, scale))
    add(box(hpLeft + 20, top + barTop, barWidth * share(state.health, state.maxHealth), barHeight, colors.normalColor,
        1, scale))
end

-- Action points: the block sits at the right, and the bar fills towards the left. The rounds are under it.
local function pointsBlock(add, state, scale, top, width)
    local left = width - apWidth
    local right = left + 326
    local bracket = piece('rightBracket')
    if bracket then
        add(picture(bracket, width - margin - 386 + 30, top, 386, blockHeight, colors.dimColor, 0.6, scale))
    end
    add(words(state.apWord .. ' ' .. state.points .. '/' .. state.maxPoints, right - 11, top + textTop, scale,
        colors.normalColor, 'right'))
    add(box(right - barWidth, top + barTop, barWidth, barHeight, colors.dimColor, 0.5, scale))
    local filled = barWidth * share(state.points, state.maxPoints)
    add(box(right - filled, top + barTop, filled, barHeight, colors.normalColor, 1, scale))
    if state.rounds ~= nil then
        add(words(state.ammoWord .. ' ' .. state.rounds, right - 11, top + ammoTop, scale, colors.normalColor, 'right'))
    end
end

-- The lines the message area shows, newest at the bottom, each with the picture the games give it
local function messageArea(add, state, scale, top)
    local count = #state.lines
    local first = math.max(1, count - messageLines + 1)
    local y = top - 16 - messageHeight * (count - first + 1)
    for index = first, count do
        local line = state.lines[index]
        local fade = math.min(1, line.left / fadeSeconds)
        local x = margin + hpLeft - 20
        local texture = looseTexture(line.icon)
        if texture then
            add(picture(texture, x, y - 12, 48, 48, colors.normalColor, fade, scale))
            x = x + 56
        end
        add(words(line.text, x, y, scale, colors.focusColor, 'left', fade))
        y = y + messageHeight
    end
end

local sneakColors = {
    hidden = colors.normalColor,
    caution = colors.titleColor,
    detected = colors.badColor,
    danger = colors.badColor,
}

-- The word of the sneak meter, in the middle at the bottom while the player sneaks
local function sneakMeter(add, state, scale, width, height)
    local color = sneakColors[state.sneak] or colors.normalColor
    local bracket = piece('experienceBar')
    local top = height - margin - 40
    if bracket then
        add(picture(bracket, (width - 356) / 2, top - 4, 356, 34, color, 0.4, scale))
    else
        add(box((width - 356) / 2, top - 4, 356, 34, colors.dimColor, 0.4, scale))
    end
    add(words(state.sneakText, width / 2, top, scale, color, 'center'))
end

-- The experience bar, which comes up when the experience changed
local function experienceMeter(add, state, scale, width, height)
    local barLeft = (width - barWidth) / 2
    local top = height / 4
    add(words(state.levelWord .. ' ' .. state.level, barLeft, top - 28, scale, colors.normalColor))
    if state.gain ~= 0 then
        add(words((state.gain > 0 and '+' or '') .. state.gain, barLeft + barWidth, top - 28, scale, colors.titleColor,
            'right'))
    end
    add(box(barLeft, top, barWidth, 12, colors.dimColor, 0.5, scale))
    add(box(barLeft, top, barWidth * state.experienceShare, 12, colors.normalColor, 1, scale))
    add(words(tostring(state.levelStart), barLeft, top + 16, scale, colors.hintColor))
    add(words(tostring(state.nextLevel), barLeft + barWidth, top + 16, scale, colors.hintColor, 'right'))
end

-- The radiation the player has, at the right above the block of action points
local function radiationMeter(add, state, scale, width, top)
    local right = width - margin - 20
    local y = top - 60
    local bracket = piece('radiationBracket')
    if bracket then
        add(picture(bracket, right - 367, y, 367, 60, colors.badColor, 0.5, scale))
    end
    add(words(word('sStatsRadsAbbrev', 'RADS') .. ' ' .. state.rads, right - 10, y + 24, scale, colors.badColor,
        'right'))
    add(box(right - 300, y + 8, 300, 6, colors.dimColor, 0.5, scale))
    add(box(right - 300, y + 8, 300 * share(state.rads, state.radsMax), 6, colors.badColor, 1, scale))
end

-- The limbs that are crippled, in a row above the block of health
local function limbIcons(add, state, scale, top)
    local x = margin + hpLeft + 20
    for _, limb in ipairs(state.crippled) do
        local texture = looseTexture('textures/interface/stats/' .. limb .. '.dds')
        if texture then
            add(picture(texture, x, top - 76, 56, 56, colors.badColor, 1, scale))
        else
            add(words(limb, x, top - 52, scale, colors.badColor))
        end
        x = x + 64
    end
end

-- The crosshair, in the middle of the screen while the weapon is out
local function reticle(add, state, scale, width, height)
    local texture = piece(state.targetHostile and 'crosshairFilled' or 'crosshair')
    local x = width / 2 - 32
    local y = height / 2 - 32
    if texture then
        add(picture(texture, x, y, 64, 64, colors.normalColor, 1, scale))
    else
        add(box(width / 2 - 1, height / 2 - 8, 2, 16, colors.normalColor, 1, scale))
        add(box(width / 2 - 8, height / 2 - 1, 16, 2, colors.normalColor, 1, scale))
    end
end

-- The name and health of the character under the crosshair, in the middle at the top
local function enemyBar(add, state, scale, width)
    local target = state.target
    local color = target.hostile and colors.badColor or colors.normalColor
    local top = 40
    local bracket = piece('enemyBar')
    if bracket then
        add(picture(bracket, (width - 500) / 2, top, 500, 64, color, 0.6, scale))
    end
    local barLeft = (width - 330) / 2
    add(box(barLeft, top + 16, 330, 14, colors.dimColor, 0.5, scale))
    add(box(barLeft, top + 16, 330 * share(target.health, target.maxHealth), 14, color, 1, scale))
    add(words(target.name, width / 2, top + 34, scale, color, 'center'))
end

-- The layout for what the player is
local function layout(state, scale, width, height)
    local content = {}
    local function add(node) content[#content + 1] = node end
    local top = height - margin - blockHeight
    healthBlock(add, state, scale, top)
    pointsBlock(add, state, scale, top, width)
    if #state.lines > 0 then messageArea(add, state, scale, top) end
    if #state.crippled > 0 then limbIcons(add, state, scale, top) end
    if state.rads > 0 then radiationMeter(add, state, scale, width, top) end
    if state.sneaking then sneakMeter(add, state, scale, width, height) end
    if state.experienceShare ~= nil then experienceMeter(add, state, scale, width, height) end
    if state.reticle then reticle(add, state, scale, width, height) end
    if state.target ~= nil then enemyBar(add, state, scale, width) end
    return {
        layer = 'HUD',
        props = { relativeSize = v2(1, 1) },
        content = ui.content(content),
    }
end

-- The letters of the compass and the places on it: the window sits in the block of health
local cardinals = { { 0, 'N' }, { 45, 'NE' }, { 90, 'E' }, { 135, 'SE' }, { 180, 'S' }, { 225, 'SW' }, { 270, 'W' },
    { 315, 'NW' } }
local compassField = 120

local function compassLayout(compass, scale, width, height)
    local content = {}
    local function add(node) content[#content + 1] = node end
    local left = margin + hpLeft - 20 + compassLeft
    local top = height - margin - blockHeight + compassTop
    local strip = piece('strip')
    if strip then
        add(picture(strip, left, top, compassWidth, compassHeight, colors.dimColor, 0.7, scale))
    else
        add(box(left, top + compassHeight - 4, compassWidth, 2, colors.dimColor, 0.7, scale))
    end
    -- The points of the compass that are in the window
    for _, point in ipairs(cardinals) do
        local across = (point[1] - compass.heading + 540) % 360 - 180
        if math.abs(across) <= compassField / 2 then
            local x = left + (0.5 + across / compassField) * compassWidth
            add(words(point[2], x, top + 28, scale, #point[2] == 1 and colors.focusColor or colors.hintColor, 'center'))
        end
    end
    -- The places that are marked
    local sizes = { quest = 24, landmark = 24, enemy = 12 }
    local rows = { quest = 4, landmark = 30, enemy = 36 }
    for _, marker in ipairs(compass.markers) do
        local texture = piece(marker.kind == 'quest' and 'quest' or marker.kind == 'landmark' and 'landmark' or 'tick')
        local size = sizes[marker.kind] or 16
        local x = left + marker.share * compassWidth - size / 2
        local color = marker.kind == 'enemy' and colors.badColor or colors.focusColor
        if texture then
            add(picture(texture, x, top + rows[marker.kind], size, size, color, 1, scale))
        else
            add(box(x, top + rows[marker.kind], size / 2, size / 2, color, 1, scale))
        end
    end
    -- The player's own mark in the middle
    add(box(left + compassWidth / 2 - 1, top + 6, 2, 12, colors.normalColor, 0.8, scale))
    return {
        layer = 'HUD',
        props = { relativeSize = v2(1, 1) },
        content = ui.content(content),
    }
end

-- The character the crosshair is on, if it is within reach of the eyes: what the engine knows of it
local function aimedTarget(stats)
    local from = camera.getPosition()
    local origin = self.position
    local range = math.max(targetRange, stats.range)
    local to = from + camera.viewportToWorldVector(v2(0.5, 0.5)) * (range + (from - origin):length())
    local result = nearby.castRay(from, to, { ignore = self, collisionType = nearby.COLLISION_TYPE.Default })
    if result.hitObject == nil then return nil end
    return hud.target(result.hitObject)
end

-- Takes the messages that were printed, and lets the lit ones burn down
local function updateLines(dt)
    for _, message in ipairs(hud.takeMessages()) do
        lines[#lines + 1] = { text = message.text, icon = message.icon, left = message.seconds }
    end
    local kept = {}
    for _, line in ipairs(lines) do
        line.left = line.left - dt
        if line.left > 0 then kept[#kept + 1] = line end
    end
    lines = kept
    -- Nobody reads more than a few at once: the oldest of a flood go
    while #lines > 2 * messageLines do table.remove(lines, 1) end
end

-- What the experience bar shows: it comes up for a few seconds when the experience changed
local function experienceState(stats, dt)
    if lastExperience ~= nil and stats.experience ~= lastExperience then
        experienceGain = (experienceLeft > 0 and experienceGain or 0) + (stats.experience - lastExperience)
        experienceLeft = experienceSeconds
    end
    lastExperience = stats.experience
    if experienceLeft <= 0 then return nil end
    experienceLeft = experienceLeft - dt
    return {
        gain = experienceGain,
        level = stats.level,
        levelStart = stats.levelStart,
        nextLevel = stats.nextLevel,
        share = stats.nextLevel > stats.levelStart
            and math.max(0, math.min(1, (stats.experience - stats.levelStart) / (stats.nextLevel - stats.levelStart)))
            or 1,
    }
end

-- What the display shows now, nothing while a window is open or the player is dead
local function current(dt)
    if I.UI.getMode() ~= nil or pipboy.playerDead() then
        return nil
    end
    local stats = pipboy.stats()
    local weapon = core.combat.weapon()
    local status = hud.status()
    updateLines(dt)
    local experience = experienceState(stats, dt)
    local target = nil
    if status.weaponDrawn then
        target = aimedTarget(weapon)
        if target ~= nil and target.dead then target = nil end
    end
    local lit = {}
    for index, line in ipairs(lines) do
        -- A line is in steps of a tenth of a second, which is as often as the display looks
        lit[index] = { text = line.text, icon = line.icon, left = math.floor(line.left * 10) / 10 }
    end
    return {
        health = math.floor(stats.health + 0.5),
        maxHealth = math.floor(stats.maxHealth + 0.5),
        points = math.floor(stats.actionPoints + 0.5),
        maxPoints = math.floor(stats.maxActionPoints + 0.5),
        rounds = weapon.ammoUse > 0 and weapon.rounds or nil,
        hpWord = word('sHitPointsShort', 'HP'),
        apWord = word('sActionPointsShort', 'AP'),
        ammoWord = word('sInventoryAmmo', 'AMMO'),
        levelWord = word('sLevel', 'LEVEL'),
        level = stats.level,
        levelStart = experience and experience.levelStart or nil,
        nextLevel = experience and experience.nextLevel or nil,
        gain = experience and experience.gain or 0,
        experienceShare = experience and experience.share or nil,
        rads = math.floor(status.rads + 0.5),
        radsMax = status.radsMax,
        sneaking = status.sneaking,
        sneak = status.sneak,
        sneakText = status.sneakText ~= '' and status.sneakText or status.sneak:upper(),
        crippled = status.crippled,
        reticle = status.weaponDrawn,
        targetHostile = target ~= nil and target.hostile,
        target = target,
        lines = lit,
        screen = ui.screenSize(),
    }
end

local function same(a, b)
    if type(a) ~= 'table' or type(b) ~= 'table' then
        if type(a) == 'userdata' and type(b) == 'userdata' then return a.x == b.x and a.y == b.y end
        return a == b
    end
    for key, value in pairs(a) do
        if not same(value, b[key]) then return false end
    end
    for key in pairs(b) do
        if a[key] == nil then return false end
    end
    return true
end

local function refresh(dt)
    local state = current(dt)
    if same(state, shown) then
        return
    end
    shown = state
    if element then
        element:destroy()
        element = nil
    end
    if state then
        local scale, width, height = screenScale()
        element = ui.create(layout(state, scale, width, height))
    end
end

-- The compass moves with every turn of the player, so it is drawn on its own and looked at more often
local function refreshCompass()
    local compass = nil
    if shown ~= nil then
        compass = hud.compass()
        -- A tenth of a degree is not seen
        compass.heading = math.floor(compass.heading * 10) / 10
        for _, marker in ipairs(compass.markers) do
            marker.share = math.floor(marker.share * 500) / 500
            marker.distance = math.floor(marker.distance)
        end
    end
    if same(compass, shownCompass) then
        return
    end
    shownCompass = compass
    if compassElement then
        compassElement:destroy()
        compassElement = nil
    end
    if compass then
        local scale, width, height = screenScale()
        compassElement = ui.create(compassLayout(compass, scale, width, height))
    end
end

return {
    interfaceName = 'FalloutHud',
    interface = {
        version = 1,
        -- What the display shows now (nil while it is not up) and the compass
        state = function() return shown end,
        compass = function() return shownCompass end,
    },
    engineHandlers = {
        onFrame = function()
            local dt = core.getRealFrameDuration()
            sinceChecked = sinceChecked + dt
            sinceCompass = sinceCompass + dt
            clock = clock + dt
            -- A change of a tenth of a second is not seen
            if sinceChecked >= 0.1 then
                refresh(sinceChecked)
                sinceChecked = 0
            end
            if sinceCompass >= 0.05 then
                sinceCompass = 0
                refreshCompass()
            end
        end,
    },
}
