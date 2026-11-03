-- "How many?": the amount to move, drop or sell from a stack, asked on the game's own menu (quantity_menu.xml). The file
-- lays out the prompt, the meter with its arrows, the amount and the two buttons; this module keeps the amount and says
-- what was chosen. It is a module of the screens that ask (the container, the barter, the Pip-Boy), not a script.
--
--   quantity.ask(12, function(count) ... end)
--   quantity.isOpen(); quantity.press(keyCode, typedLetter); quantity.update(); quantity.close()
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')

local module = {}

local shown = nil
-- The amount that is chosen, the most there is, and what is done with the amount when it is accepted
local amount, most, accepted = 1, 1, nil
-- The step and the jump of the file's comments (user3 and user4), as the code of the game sizes them when the prompt
-- opens (FalloutNV.exe 0x7ABB00 to 0x7ABC4F, Mac round 12AJ B2): a twentieth of the most, in tens above ten, and a quarter
local stepSize, jumpSize = 1, 1

function module.isOpen()
    return shown ~= nil
end

function module.close()
    if shown then
        shown:close()
        shown = nil
    end
end

-- Each frame the prompt is open: the window size (the screen under the prompt follows the window in its own update)
function module.update()
    if shown then
        shown:checkSize()
    end
end

local function show()
    if shown then
        shown:set('QM_AmountMeter', 'user2', amount)
    end
end

local function change(by)
    amount = math.max(1, math.min(most, amount + by))
    show()
end

local function accept()
    local onChosen, count = accepted, amount
    module.close()
    if onChosen then
        onChosen(count)
    end
end

-- Asks for 1 to `max` and calls `onChosen(count)` when the player says OK; nothing happens when it is cancelled.
-- Returns false when the menu file is not there, or has no meter to set (the caller takes what it was asked about as it
-- is).
function module.ask(max, onChosen)
    module.close()
    most = math.max(1, math.floor(max))
    stepSize = math.max(math.floor(most / 20), 1)
    if stepSize > 10 then
        stepSize = stepSize - stepSize % 10
    end
    jumpSize = math.max(math.floor(most / 4), 1)
    local opened = menuModule.open('quantity_menu.xml')
    if not opened then
        return false
    end
    shown, amount, accepted = opened, 1, onChosen
    -- Setting a tile the file does not have is an error; a menu that cannot show the amount is closed, not left open
    local ready = pcall(opened.batch, opened, function()
        opened:set('QM_AmountMeter', 'user0', most)
        opened:set('QM_AmountMeter', 'user3', stepSize)
        opened:set('QM_AmountMeter', 'user4', jumpSize)
        show()
    end)
    if not ready then
        module.close()
        return false
    end
    -- A drag of the meter sets the amount, which stays when the meter is let go
    opened:onDrag('QM_AmountMeter', function()
        local value = tonumber(opened:get('QM_AmountMeter', '_Value')) or 0
        return math.max(1, math.min(most, math.floor(value * most + 0.5)))
    end, function(chosen)
        amount = chosen or amount
        show()
    end)
    opened:onClick('QM_DecreaseArrow', function() change(-1) end)
    opened:onClick('QM_IncreaseArrow', function() change(1) end)
    opened:onClick('QM_OKButton', accept)
    opened:onClick('QM_CancelButton', module.close)
    return true
end

-- The keys while the menu is there: the arrows change the amount by one (the pages, which are the shoulder buttons of
-- the pad, by the jump), Enter says OK. The letters the file gives its buttons (A for OK, E for Cancel) click them; the
-- games take no digits.
function module.press(code, symbol)
    if not shown then
        return false
    end
    if symbol ~= nil and symbol ~= '' and shown:pcButton(symbol) then
        return true
    end
    if code == input.KEY.LeftArrow then
        change(-1)
    elseif code == input.KEY.RightArrow then
        change(1)
    elseif code == input.KEY.PageUp then
        change(-jumpSize)
    elseif code == input.KEY.PageDown then
        change(jumpSize)
    elseif code == input.KEY.Enter then
        accept()
    elseif code == input.KEY.Escape then
        module.close()
    end
    return true
end

return module
