-- The death of the player in Fallout 3 and New Vegas, as the games' code does it (Mac round 4A H2, FalloutNV.exe 0x93E840):
-- a few seconds after the player has died (fPlayerDeathReloadTime, 5 in the executables) the game closes what is open and,
-- when there is a save, asks in a message box what it says (sMiscPlayerDeadMessage) with two buttons, Reload
-- (sMiscPlayerDeadLoadOption: the most recent save is loaded) and Main Menu (sMiscPlayerDeadMenuOption); with no save it
-- goes straight to the main menu. The box has no timer and no key of its own. What dead is, is the engine's
-- (core.pipboy.playerDead); the box is the message box of the game's own file (falloutmessage.lua).
local core = require('openfallout.core')
local I = require('openfallout.interfaces')

local pipboy = core.pipboy

if not pipboy.available() then
    return {}
end

-- Real seconds since the player was seen dead, and whether the box has been asked for since
local deadFor = 0
local shown = false

-- What the game's settings say (the stand-in words of the port when the install has none)
local function word(name, fallback)
    local ok, text = pcall(pipboy.text, name)
    if ok and type(text) == 'string' and text ~= '' then return text end
    return fallback
end

local function show()
    shown = true
    if not pipboy.hasRecentSave() then
        pipboy.openMainMenu()
        return
    end
    local asked = I.FalloutMessage ~= nil and I.FalloutMessage.ask(
        word('sMiscPlayerDeadMessage', 'Reloading the most recent save game'),
        { word('sMiscPlayerDeadLoadOption', 'Reload'), word('sMiscPlayerDeadMenuOption', 'Main Menu') },
        function(index)
            if index == 1 then
                pipboy.reloadRecent()
            elseif index == 2 then
                pipboy.openMainMenu()
            elseif index == nil then
                -- (the box could not be shown after all, when the game's message file is not usable)
                pipboy.askReload()
            end
        end)
    -- (an install without the game's message file: the engine's own question about loading the last save)
    if not asked then pipboy.askReload() end
end

return {
    engineHandlers = {
        onFrame = function()
            if not pipboy.playerDead() then
                shown = false
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
}
