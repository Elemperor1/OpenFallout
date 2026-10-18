-- The death of the player in Fallout 3 and New Vegas: a few seconds after the player has died (the settings of the
-- files say how many) the game stands still and asks whether to load a save or to quit. What dead is, is the engine's
-- (core.pipboy.playerDead).
local core = require('openfallout.core')
local ui = require('openfallout.ui')
local util = require('openfallout.util')
local I = require('openfallout.interfaces')
local widgetModule = require('scripts.omw.falloutwidgets')

local pipboy = core.pipboy

if not pipboy.available() then
    return {}
end

local v2 = util.vector2
local colors = widgetModule
local textSize = widgetModule.textSize

local element = nil
local modeOpen = false
-- Real seconds since the player was seen dead
local deadFor = 0
local shown = false

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

local function show()
    shown = true
    if not modeOpen then
        modeOpen = true
        I.UI.addMode('Interface', { windows = {} })
    end
    destroyElement()
    widgets = widgetModule.new(update)
    local width = math.max(360, math.floor(ui.screenSize().x * 0.3))
    local rows = {
        { props = { size = v2(width, 0) } },
        { props = { size = widgetModule.gap } },
        widgets.label('YOU ARE DEAD', colors.badColor, textSize + 8),
        { props = { size = widgetModule.gap } },
        widgets.clickable('Load the last save', function() pipboy.askReload() end, { color = colors.titleColor }),
        { props = { size = widgetModule.gap } },
        widgets.clickable('Quit', function() core.quit() end, { color = colors.titleColor }),
    }
    element = ui.create(widgets.panel(rows))
end

return {
    engineHandlers = {
        onFrame = function()
            if not pipboy.playerDead() then
                if shown then
                    destroyElement()
                    shown = false
                    if modeOpen then
                        modeOpen = false
                        I.UI.removeMode('Interface')
                    end
                end
                deadFor = 0
                return
            end
            -- The world goes on while the player lies there (the time is the real one: the game may be paused)
            deadFor = deadFor + core.getRealFrameDuration()
            if not shown and deadFor >= (pipboy.setting('fPlayerDeathReloadTime') or 5) then
                show()
            end
        end,
    },
    eventHandlers = {
        -- A screen that was left (Escape) while the player is dead comes back on the next frame
        UiModeChanged = function(data)
            if modeOpen and data.oldMode == 'Interface' and data.newMode ~= 'Interface' and data.newMode ~= 'MainMenu' then
                modeOpen = false
                destroyElement()
                shown = false
            end
        end,
    },
}
