-- The buttons of a game controller on the screens of Fallout 3 and New Vegas. The screens read keys (the arrows, the
-- pages, Enter) and the clicks of the pointer. While a screen is open the engine already moves the pointer with the left
-- stick, clicks with A and leaves the screen with B (mwinput/controllermanager.cpp), but it hands the right stick to the
-- widgets of MyGUI and the D-pad to MyGUI as arrow keys, and neither reaches a script. So the buttons below stand for the
-- keys, and a screen reads them as it reads those keys. A has no meaning here: the engine clicks with it, and a meaning
-- of this script would act a second time.
local input = require('openfallout.input')

local KEY = input.KEY
local BUTTON = input.CONTROLLER_BUTTON

local module = {}

-- The key each button stands for on every screen
local keys = {
    [BUTTON.DPadUp] = KEY.UpArrow,
    [BUTTON.DPadDown] = KEY.DownArrow,
    [BUTTON.DPadLeft] = KEY.LeftArrow,
    [BUTTON.DPadRight] = KEY.RightArrow,
    [BUTTON.LeftShoulder] = KEY.PageUp,
    [BUTTON.RightShoulder] = KEY.PageDown,
    [BUTTON.Y] = KEY.Enter,
}

-- Buttons that stand for the same key on every screen are listed above; a screen can give a button another key, or one
-- where there is none (`own`: button to key)
function module.key(button, own)
    return own and own[button] or keys[button]
end

-- How many presses were logged: the log is for finding out what a pad did, not for every press of a long game
local logged, logLimit = 0, 200

function module.log(screen, button, code)
    if logged < logLimit then
        logged = logged + 1
        print(string.format('Pad: screen=%s button=%s key=%s%s', screen, tostring(button), tostring(code),
            logged == logLimit and ' (further presses are not logged)' or ''))
    end
end

-- The function that hears the buttons of one screen: `isOpen` says whether the screen is there, `press` takes the key
-- code a button stands for (the same function the keys of the screen are read with).
function module.handler(screen, isOpen, press, own)
    return function(button)
        if not isOpen() then
            return
        end
        local code = module.key(button, own)
        if code == nil then
            return
        end
        module.log(screen, button, code)
        press(code)
    end
end

return module
