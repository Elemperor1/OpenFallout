-- The cards of an item on the screens that list items (the container, the barter, the Pip-Boy): damage, resistance,
-- weight, value and ammunition. The cards are laid out by the game's own file (item_stats_display.xml, in
-- the menu as the tile that includes it); the game's code gives each its value and says whether it is shown, which is
-- what this module does. It is a module of the screens, not a script.
local core = require('openfallout.core')

local pipboy = core.pipboy

local module = {}

-- The cards of the file that this module sets, by the name the file gives them
local CARDS = { 'DamageResistInfo', 'DamageThresholdInfo', 'DAMInfo', 'DPSInfo', 'WeightInfo', 'ValueInfo', 'AmmoInfo',
    'ConditionInfo', 'EffectsInfo', 'StrengthReqInfo', 'ModInfoOne', 'ModInfoTwo', 'ModInfoThree' }

local function word(setting, fallback)
    local text = pipboy.text(setting)
    return text ~= '' and text or fallback
end

-- A number of the cards: nothing above 0 is two dashes, below 1 it has a decimal, else it is whole
local function amount(value)
    if not (value > 0) then return '--' end
    if value < 1 then return string.format('%.1f', value) end
    return string.format('%.0f', value)
end

-- Which cards an item has and what is on them, as {card, value} pairs. New Vegas tells the weight and value of the whole
-- stack, the threshold of armor and the pellets of a shot; Fallout 3 tells those of one.
local function cardsOf(item, nv)
    local list = {}
    local function add(card, value, title, meter, notch)
        list[#list + 1] = { card = card, value = value, title = title, meter = meter, notch = notch }
    end
    local one = item.cardWeight and item.cardWeight > 0 and item.cardWeight or item.weight or 0
    local function weight()
        if nv then
            local total = one * item.count
            add('WeightInfo', total > 0 and string.format('%.2f', total) or '--')
        else
            add('WeightInfo', amount(one))
        end
    end
    local function value()
        add('ValueInfo', amount(nv and item.value * item.count or math.ceil(item.value)))
    end
    -- A weapon and a piece of armor have a condition, shown as a bar (the file's meter takes a value from 0 to 1) under the
    -- word of the setting sInventoryCondition, with no number (the executable sets none, 0x707E30). The pip on the bar is
    -- the file's own image at a fixed place: the sixth user trait of the card says which, 1 for a weapon (three quarters,
    -- where its damage starts to fall) and 0 for armor and clothing (a half, where its rating starts to fall).
    local function condition(weapon)
        if item.hasCondition then
            add('ConditionInfo', '', word('sInventoryCondition', 'CND'),
                math.max(0, math.min(1, (item.condition or 1000) / 1000)), weapon and 1 or 0)
        end
    end
    if item.category == 'weapons' then
        local damage = item.cardDamage and item.cardDamage > 0 and item.cardDamage or item.damage or 0
        if item.pellets and item.pellets > 1 and nv then
            add('DAMInfo', string.format('%.1fx%d', damage, item.pellets))
        else
            add('DAMInfo', string.format('%d', math.ceil(damage)))
        end
        weight()
        value()
        local ammo = '--'
        if item.hasAmmo then
            ammo = item.clip > 0 and string.format('%s (%d/%d)', item.ammoName, item.loaded, item.reserve)
                or item.ammoName
        end
        add('AmmoInfo', ammo, word('sInventoryAmmo', 'AMMO'))
        condition(true)
    elseif item.category == 'apparel' then
        if item.rating > 0 or item.threshold > 0 then
            add('DamageResistInfo', string.format('%d', math.floor(item.rating)))
            if nv and item.threshold > 0 then
                add('DamageThresholdInfo', string.format('%d', math.floor(item.threshold)))
            end
        end
        weight()
        value()
        condition(false)
    else
        weight()
        value()
    end
    return list
end

-- Show the cards of an item (nothing shows no card)
function module.show(shown, item)
    local nv = pipboy.stats().newVegas
    local wanted = {}
    if item then
        for _, card in ipairs(cardsOf(item, nv)) do
            wanted[card.card] = card
        end
    end
    shown:batch(function()
        for _, name in ipairs(CARDS) do
            if shown:find(name) then
                local card = wanted[name]
                if card then
                    shown:set(name, '_Value', card.value)
                    if card.title then shown:set(name, '_Title', card.title) end
                    -- The condition is a bar: the meter of the card reads the fifth user trait
                    if card.meter then shown:set(name, 'user5', card.meter) end
                    if card.notch then shown:set(name, 'user6', card.notch) end
                    -- The file hides some cards by their alpha (the damage, the strength needed) until the code shows them
                    shown:set(name, 'alpha', 255)
                end
                shown:set(name, 'visible', card ~= nil)
            end
        end
    end)
end

return module
