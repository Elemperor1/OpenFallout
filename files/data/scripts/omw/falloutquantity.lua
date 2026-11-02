-- "How many?": the amount to move, drop or sell from a stack, asked on the game's own menu (quantity_menu.xml). The file
-- lays out the prompt, the meter with its arrows, the amount and the two buttons; this module keeps the amount and says
-- what was chosen. It is a module of the screens that ask (the container, the barter, the Pip-Boy), not a script.
--
--   quantity.ask(12, function(count) ... end)
--   quantity.isOpen(); quantity.press(keyCode); quantity.update(); quantity.close()
local input = require('openfallout.input')

local menuModule = require('scripts.omw.falloutmenu')

local module = {}

local shown = nil
-- The amount that is chosen, the most there is, and what is done with the amount when it is accepted
local amount, most, accepted = 1, 1, nil

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
-- Returns false when the menu file is not there (the caller takes what it was asked about as it is).
function module.ask(max, onChosen)
    module.close()
    most = math.max(1, math.floor(max))
    local opened = menuModule.open('quantity_menu.xml')
    if not opened then
        return false
    end
    shown, amount, accepted = opened, 1, onChosen
    opened:batch(function()
        opened:set('QM_AmountMeter', 'user0', most)
        show()
    end)
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

-- The keys while the menu is there: the arrows change the amount (the pages by more), Enter says OK
function module.press(code)
    if not shown then
        return false
    end
    if code == input.KEY.LeftArrow then
        change(-1)
    elseif code == input.KEY.RightArrow then
        change(1)
    elseif code == input.KEY.PageUp then
        change(-5)
    elseif code == input.KEY.PageDown then
        change(5)
    elseif code == input.KEY.Enter then
        accept()
    elseif code == input.KEY.Escape then
        module.close()
    end
    return true
end

return module
