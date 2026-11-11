-- The buttons of a game controller on the screens of Fallout 3 and New Vegas. The screens read keys (the arrows, the
-- pages, Enter) and the clicks of the pointer. While a screen is open the engine already moves the pointer with the left
-- stick, clicks with A and leaves the screen with B (mwinput/controllermanager.cpp), but it hands the right stick to the
-- widgets of MyGUI and the D-pad to MyGUI as arrow keys, and neither reaches a script. So the buttons below stand for the
-- keys, and a screen reads them as it reads those keys.
--
-- A acts on what the player picked. The D-pad moves the pick of a screen, and A then acts on the pick (it stands for
-- Enter) and the engine does not click where the pointer was left: the games' own screens take A for the lit row
-- (FalloutNV.exe, Mac rounds 12AH A4 and 12AI A3: with a pad the pointer is hidden and the screen picks by selection, A
-- clicks the tile that is selected). The stick moving the pointer gives the pointer back, and A then clicks where it is.
-- The last of the two that the player used is the one A goes to, which the engine tells (`_padPick`).
--
-- The triggers are axes of the pad: the engine tells a screen that one is down as the buttons `LeftTrigger` and
-- `RightTrigger` while a screen is open.
local input = require('openfallout.input')

local KEY = input.KEY
local BUTTON = input.CONTROLLER_BUTTON

local module = {}

-- The key each button stands for on every screen. (A is Enter while the pad holds a pick; Y is Enter too, which the
-- screens that give Y a meaning of its own take back.)
local keys = {
    [BUTTON.DPadUp] = KEY.UpArrow,
    [BUTTON.DPadDown] = KEY.DownArrow,
    [BUTTON.DPadLeft] = KEY.LeftArrow,
    [BUTTON.DPadRight] = KEY.RightArrow,
    [BUTTON.LeftShoulder] = KEY.PageUp,
    [BUTTON.RightShoulder] = KEY.PageDown,
    [BUTTON.Y] = KEY.Enter,
}

-- What a screen says of a button (`own`: a table of button to key, or a function of the button for a screen that
-- changes with what it shows): a key, or `false` for none where the default has one
local function chosen(button, own)
    if type(own) == 'function' then
        return own(button)
    end
    return own and own[button]
end

-- Buttons that stand for the same key on every screen are listed above; a screen can give a button another key, or `false`
-- where there is none
function module.key(button, own)
    local code = chosen(button, own)
    if code == false then
        return nil
    end
    if code ~= nil then
        return code
    end
    return keys[button]
end

-- The pad on the Pip-Boy. The games' own pages put the buttons on the screen (inventory_menu.xml: A equips the thing, X
-- drops it, Y repairs it, B closes; read from the files in the M14 thread), and A is Enter on the pick that the D-pad
-- moved. What moves between the tabs is not in the files (the executable's handlers of the pages were not read), so the
-- port's choice is the one that the shoulders and the sides of the pad say: the bumpers turn the page (STAT, INV, DATA),
-- the sides of the D-pad go to the tab before and after in the page (WEAPONS, APPAREL, ...), up and down pick a row, and
-- the triggers turn a page of the list.
local pipboyKeys = {
    [BUTTON.X] = KEY.D,
    [BUTTON.Y] = KEY.R,
    [BUTTON.LeftShoulder] = KEY.LeftArrow,
    [BUTTON.RightShoulder] = KEY.RightArrow,
    [BUTTON.DPadLeft] = KEY.LeftBracket,
    [BUTTON.DPadRight] = KEY.RightBracket,
    [BUTTON.LeftTrigger] = KEY.PageUp,
    [BUTTON.RightTrigger] = KEY.PageDown,
}

-- The buttons of the Pip-Boy as a function for `handler`. `plain()` says that a question or a screen of its own is up
-- over the list (how many, repair, mods) or that it is the level up: the buttons are then the plain arrows and pages of
-- that screen, and X and Y have no meaning there.
function module.pipboyOwn(plain)
    return function(button)
        if plain() then
            if button == BUTTON.X or button == BUTTON.Y then
                return false
            end
            return nil
        end
        return pipboyKeys[button]
    end
end

-- The keys that move the pick of a screen (a tab, a page, a row): a button that stands for one holds the pick
local movers = {}
for _, name in ipairs({ 'UpArrow', 'DownArrow', 'LeftArrow', 'RightArrow', 'PageUp', 'PageDown', 'LeftBracket',
    'RightBracket' }) do
    if KEY[name] ~= nil then movers[KEY[name]] = true end
end

-- Whether the pad holds a pick: the engine says (and lets it go when the stick moves the pointer)
function module.holdsPick()
    return input._padPick ~= nil and input._padPick() == true
end

-- A screen holds the pick that the D-pad made: A is no click at the pointer from now on
function module.holdPick()
    if input._setPadPick ~= nil then input._setPadPick(true) end
end

-- A screen that has no pick of its own (the tables of the casinos, the models of SPECIAL) gives A back to the pointer, so
-- that a pick which an earlier screen held does not leave A without a meaning here
function module.releasePick()
    if input._setPadPick ~= nil then input._setPadPick(false) end
end

-- How many presses were logged: the log is for finding out what a pad did, not for every press of a long game (the
-- engine writes an `Input:` line for every button, with the screens that did not take it)
local logged, logLimit = 0, 200

function module.log(screen, button, code)
    if logged < logLimit then
        logged = logged + 1
        print(string.format('Pad: screen=%s button=%s key=%s%s', screen, tostring(button), tostring(code),
            logged == logLimit and ' (further presses are not logged)' or ''))
    end
end

-- The key that a press of a button stands for on a screen, or nil for none (`own` as for `key`). This is where a button
-- that moves the pick makes the pad hold one, and where A is Enter on the pick: the engine clicks where the pointer is
-- unless the pad holds a pick. A screen that reads the buttons by itself takes its key from here.
function module.translate(button, own)
    if button == BUTTON.A then
        if not module.holdsPick() then
            return nil
        end
        local code = chosen(button, own)
        if code == nil then
            return KEY.Enter
        elseif code == false then
            return nil
        end
        return code
    end
    local code = module.key(button, own)
    if code ~= nil and movers[code] then
        module.holdPick()
    end
    return code
end

-- The function that hears the buttons of one screen: `isOpen` says whether the screen is there, `press` takes the key
-- code a button stands for (the same function the keys of the screen are read with).
function module.handler(screen, isOpen, press, own)
    return function(button)
        if not isOpen() then
            return
        end
        local code = module.translate(button, own)
        if code == nil then
            return
        end
        module.log(screen, button, code)
        press(code)
    end
end

return module
