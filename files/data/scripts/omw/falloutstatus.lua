-- The head-up display of the player in Fallout 3 and New Vegas: health at the bottom left, action points and the rounds
-- of the weapon at the bottom right. The places follow what the executables read from the files (the menu is 960 units
-- high, as wide as the screen is, with a margin of twice the safe zone of 15 on each side); the bars are plain, and what
-- the games show besides (the compass, the enemy health, the experience, the radiation, the messages and the reticle)
-- is not here. The numbers are the engine's (core.pipboy.stats, core.combat.weapon).
local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local widgetModule = require('scripts.omw.falloutwidgets')

local pipboy = core.pipboy

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

local white = ui.texture { path = 'white' }
local element = nil
local shown = nil
local sinceChecked = 0

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

local function words(text, x, y, scale, color, alignRight)
    local size = math.floor(widgetModule.textSize * math.max(scale, 0.5))
    return {
        type = ui.TYPE.Text,
        props = {
            text = text,
            textSize = size,
            textColor = color or colors.normalColor,
            position = v2(x * scale, y * scale),
            anchor = v2(alignRight and 1 or 0, 0),
        },
    }
end

local function share(now, most)
    if most <= 0 then return 0 end
    return math.max(0, math.min(1, now / most))
end

-- The layout for what the player is: health, action points, and for a weapon that uses ammunition its rounds
local function layout(state, scale, width, height)
    local content = {}
    local function add(node) content[#content + 1] = node end
    local top = height - margin - blockHeight
    -- Health: the bar fills from the left
    add(words(state.hpWord .. ' ' .. state.health .. '/' .. state.maxHealth, hpLeft + 20, top + textTop, scale))
    add(box(hpLeft + 20, top + barTop, barWidth, barHeight, colors.dimColor, 0.5, scale))
    add(box(hpLeft + 20, top + barTop, barWidth * share(state.health, state.maxHealth), barHeight, colors.normalColor,
        1, scale))
    -- Action points: the block sits at the right, and the bar fills towards the left
    local left = width - apWidth
    local right = left + 326
    add(words(state.apWord .. ' ' .. state.points .. '/' .. state.maxPoints, right - 11, top + textTop, scale,
        colors.normalColor, true))
    add(box(right - barWidth, top + barTop, barWidth, barHeight, colors.dimColor, 0.5, scale))
    local filled = barWidth * share(state.points, state.maxPoints)
    add(box(right - filled, top + barTop, filled, barHeight, colors.normalColor, 1, scale))
    if state.rounds ~= nil then
        add(words(state.ammoWord .. ' ' .. state.rounds, right - 11, top + ammoTop, scale, colors.normalColor, true))
    end
    return {
        layer = 'HUD',
        props = { relativeSize = v2(1, 1) },
        content = ui.content(content),
    }
end

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- What the display shows now, nothing while a window is open or the player is dead
local function current()
    if I.UI.getMode() ~= nil or pipboy.playerDead() then
        return nil
    end
    local stats = pipboy.stats()
    local weapon = core.combat.weapon()
    return {
        health = math.floor(stats.health + 0.5),
        maxHealth = math.floor(stats.maxHealth + 0.5),
        points = math.floor(stats.actionPoints + 0.5),
        maxPoints = math.floor(stats.maxActionPoints + 0.5),
        rounds = weapon.ammoUse > 0 and weapon.rounds or nil,
        hpWord = word('sHitPointsShort', 'HP'),
        apWord = word('sActionPointsShort', 'AP'),
        ammoWord = word('sInventoryAmmo', 'AMMO'),
        screen = ui.screenSize(),
    }
end

local function same(a, b)
    if a == nil or b == nil then return a == b end
    for _, pair in ipairs({ { a, b }, { b, a } }) do
        for key, value in pairs(pair[1]) do
            if key == 'screen' then
                if value.x ~= pair[2].screen.x or value.y ~= pair[2].screen.y then return false end
            elseif pair[2][key] ~= value then
                return false
            end
        end
    end
    return true
end

local function refresh()
    local state = current()
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

return {
    engineHandlers = {
        onFrame = function()
            -- A change of a tenth of a second is not seen
            sinceChecked = sinceChecked + core.getRealFrameDuration()
            if sinceChecked < 0.1 then
                return
            end
            sinceChecked = 0
            refresh()
        end,
    },
}
