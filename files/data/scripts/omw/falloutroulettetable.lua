-- The betting cloth of the roulette table of Fallout New Vegas, as this port draws it: the game's own table is a 3D scene
-- that the pointer picks bets from (NV_Roulette-Points.NIF, the executable's 159 places); here the cloth is a plain layout
-- of cells, and this module says which bet a place on it stands for. It works in cells and knows nothing of the screen.
--
-- The layout is the American table in the portrait way: three columns of numbers, twelve rows (the number of a cell is
-- 3 * row + column + 1, rows and columns counted from 0), the zeroes in a row of their own above them, the three columns
-- bets below them, the three dozens on the left and the six even-money bets (1-18, even, red, black, odd, 19-36) on
-- their left. A place on the cloth is x across (0 at the left edge of the numbers, 3 at their right edge) and y down (0 at
-- the top edge of the first row; the zeroes are above, between -1 and 0).
local module = {}

-- How near a line a pointer has to be to mean the bet on the line (a share of a cell)
module.TOLERANCE = 0.22

module.ROWS = 12
module.COLUMNS = 3

-- The even-money bets from the top of their strip (two rows each)
local EVEN_MONEY = {
    { kind = 'half', index = 1 },
    { kind = 'even' },
    { kind = 'red' },
    { kind = 'black' },
    { kind = 'odd' },
    { kind = 'half', index = 2 },
}

local function number(row, column)
    return 3 * row + column + 1
end

local function clamp(value, low, high)
    return math.max(low, math.min(high, value))
end

-- The bet at a place on the cloth: a table with `kind` ("straight", "split", "street", "corner", "sixline", "five", "dozen",
-- "column", "half", "even", "odd", "red" or "black"; the threes of the cloth are "street", also the three that hold a
-- zero and the 2) and, for the bets on numbers, `numbers` (0 to 36, 37 for the double
-- zero) or, for the others, `index` (which dozen, column or half); nil for a place that is not on the cloth.
function module.hit(x, y)
    local t = module.TOLERANCE
    if x < -2 or x >= 3 or y < -1 or y >= 13 then
        return nil
    end
    if x < -1 then
        if y < 0 or y >= module.ROWS then return nil end
        local bet = EVEN_MONEY[math.floor(y / 2) + 1]
        return { kind = bet.kind, index = bet.index }
    elseif x < 0 then
        if y < 0 or y >= module.ROWS then return nil end
        return { kind = 'dozen', index = math.floor(y / 4) + 1 }
    end
    if y >= module.ROWS then
        return { kind = 'column', index = math.floor(x) + 1 }
    end
    if y < -t then
        -- The zeroes: 0 over the first column and a half of the second, 00 over the rest, a pair where they meet
        if math.abs(x - 1.5) < t then
            return { kind = 'split', numbers = { 0, 37 } }
        end
        return { kind = 'straight', numbers = { x < 1.5 and 0 or 37 } }
    end
    if y < 0 then
        -- The border of the first row with the zeroes
        if x < t then
            return { kind = 'five', numbers = { 0, 37, 1, 2, 3 } }
        end
        -- The pairs of the zeroes with the 1 and the 3 are over their cells; between the 1 and the 3 are the threes that
        -- hold the 2 (there is no pair of a zero with the 2), at the lines between the columns and under the line of the
        -- two zeroes
        if x < 1 then return { kind = 'split', numbers = { 0, 1 } } end
        if x >= 2 then return { kind = 'split', numbers = { 37, 3 } } end
        if x < 1.25 then return { kind = 'street', numbers = { 0, 1, 2 } } end
        if x < 1.75 then return { kind = 'street', numbers = { 0, 37, 2 } } end
        return { kind = 'street', numbers = { 37, 2, 3 } }
    end
    local row = clamp(math.floor(y), 0, module.ROWS - 1)
    local column = clamp(math.floor(x), 0, 2)
    local nearX = math.floor(x + 0.5)
    local nearY = math.floor(y + 0.5)
    local onV = math.abs(x - nearX) < t and nearX >= 1 and nearX <= 2
    local onH = math.abs(y - nearY) < t and nearY >= 1 and nearY <= module.ROWS - 1
    if x < t then
        -- The left border of the numbers: a row of three, or two rows of them
        if onH then
            local first = number(nearY - 1, 0)
            return { kind = 'sixline', numbers = { first, first + 1, first + 2, first + 3, first + 4, first + 5 } }
        end
        local first = number(row, 0)
        return { kind = 'street', numbers = { first, first + 1, first + 2 } }
    end
    if onV and onH then
        local first = number(nearY - 1, nearX - 1)
        return { kind = 'corner', numbers = { first, first + 1, first + 3, first + 4 } }
    end
    if onV then
        local left = number(row, nearX - 1)
        return { kind = 'split', numbers = { left, left + 1 } }
    end
    if onH then
        local above = number(nearY - 1, column)
        return { kind = 'split', numbers = { above, above + 3 } }
    end
    return { kind = 'straight', numbers = { number(row, column) } }
end

-- The key of a bet that is the same for the areas the engine lists (`areas` of core.casino.roulette) and for what `hit`
-- says, whatever the order of the numbers is
local function keyOf(kind, numbers)
    local sorted = {}
    for i, value in ipairs(numbers) do sorted[i] = value end
    table.sort(sorted)
    return kind .. ':' .. table.concat(sorted, ',')
end

-- The places of the areas the engine lists, by what they are: `find(bet)` gives the place (from 1) of the area of a bet.
-- The areas of a dozen, column or half get an `index` (their place among those of their kind, from 1), which the engine does
-- not list and `rectOf` needs.
function module.index(areas)
    local byNumbers, byKind = {}, {}
    for place, area in ipairs(areas) do
        if area.kind == 'dozen' or area.kind == 'column' or area.kind == 'half' then
            local list = byKind[area.kind] or {}
            list[#list + 1] = place
            byKind[area.kind] = list
            -- Which of its kind it is (the first dozen, the second half): what `hit` and `rectOf` call its index
            area.index = #list
        elseif area.kind == 'even' or area.kind == 'odd' or area.kind == 'red' or area.kind == 'black' then
            byKind[area.kind] = { place }
        else
            byNumbers[keyOf(area.kind, area.numbers)] = place
        end
    end
    return {
        find = function(bet)
            if bet == nil then return nil end
            if bet.numbers then
                return byNumbers[keyOf(bet.kind, bet.numbers)]
            end
            local list = byKind[bet.kind]
            return list and list[bet.index or 1] or nil
        end,
    }
end

-- Where the cell of a number lies, as the place of its top left corner (zeroes: the two halves of the row above)
local function cellOf(pocket)
    if pocket == 0 then return 0, -1, 1.5, 1 end
    if pocket == 37 then return 1.5, -1, 1.5, 1 end
    local row, column = math.floor((pocket - 1) / 3), (pocket - 1) % 3
    return column, row, 1, 1
end

-- The rectangle (x, y, width, height in cells) that holds what a bet is on, for lighting it and for putting its chips
function module.rectOf(area)
    local kind = area.kind
    if kind == 'dozen' then
        return -1, (area.index - 1) * 4, 1, 4
    elseif kind == 'column' then
        return area.index - 1, module.ROWS, 1, 1
    elseif kind == 'half' or kind == 'even' or kind == 'red' or kind == 'black' or kind == 'odd' then
        local slot = 0
        if kind == 'half' then slot = area.index == 1 and 0 or 5
        elseif kind == 'even' then slot = 1
        elseif kind == 'red' then slot = 2
        elseif kind == 'black' then slot = 3
        elseif kind == 'odd' then slot = 4 end
        return -2, slot * 2, 1, 2
    end
    local left, top, right, bottom = math.huge, math.huge, -math.huge, -math.huge
    for _, pocket in ipairs(area.numbers) do
        local x, y, w, h = cellOf(pocket)
        left, top = math.min(left, x), math.min(top, y)
        right, bottom = math.max(right, x + w), math.max(bottom, y + h)
    end
    return left, top, right - left, bottom - top
end

-- The middle of the place where a bet is made (a pair is on the line between its numbers, a corner at their meeting
-- point), where its chips are drawn
function module.centerOf(area)
    local x, y, w, h = module.rectOf(area)
    return x + w / 2, y + h / 2
end

return module
