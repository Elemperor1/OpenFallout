-- The subtitles of the head-up display of Fallout 3 and New Vegas, as the executable keeps them (FalloutNV.exe 0x774FD0,
-- 0x7711F4 and 0x775230; Mac round 12AO B3): a line that a character says is an entry with the seconds it stays from the
-- moment it was said; every 200 ms the display drops the entries that are over, and when none is on the screen it takes
-- the one of the speaker nearest to the player within 500 units (a line a script forced comes before the others) and shows
-- it until it is over. The text is the response alone: the executable puts no name before it (Mac round 12AQ B3).
-- The tile `Subtitles` of hud_main_menu.xml is empty; the display makes a centred text in it (falloutmenuhud.lua).
local module = {}

-- The display looks at the entries this often (the executable's update of the display, 200 ms)
module.CHECK_SECONDS = 0.2
-- How far the speaker of a line that is not forced may be for it to be shown (the squared limit of the executable is 250000)
module.REACH = 500

local Subtitles = {}
Subtitles.__index = Subtitles

function module.new()
    return setmetatable({ entries = {}, current = nil, since = 0 }, Subtitles)
end

-- A line: `text`, `seconds` it stays, `distance` of the speaker from the player, `forced` when a script asked for it, `kind`
-- (0 the player's own line, 1 nobody's, 2 another character's; 2 when it is not given)
function Subtitles:add(line)
    if type(line.text) ~= 'string' or line.text == '' then return end
    self.entries[#self.entries + 1] = {
        text = line.text,
        left = tonumber(line.seconds) or 0,
        distance = tonumber(line.distance) or math.huge,
        forced = line.forced == true,
        kind = tonumber(line.kind) or 2,
    }
end

-- The entry that is shown when none is on the screen (FalloutNV.exe 0x775230): the entries are taken in the order they
-- were said; a line a script forced takes over whatever was chosen; any other is chosen when its speaker is within 500
-- units and nearer than the one chosen, unless it is the player's own line (kind 0) and the one chosen is another's
local function choose(entries)
    local best = nil
    for _, entry in ipairs(entries) do
        if entry.forced then
            best = entry
        elseif best == nil then
            if entry.distance <= module.REACH then best = entry end
        elseif not best.forced and entry.distance < best.distance and entry.kind >= best.kind then
            best = entry
        end
    end
    return best
end

-- A frame: the text that is on the screen now, nil for none
function Subtitles:step(dt)
    for _, entry in ipairs(self.entries) do entry.left = entry.left - dt end
    self.since = self.since + dt
    if self.since < module.CHECK_SECONDS then
        return self.current and self.current.text or nil
    end
    self.since = 0
    local kept = {}
    for _, entry in ipairs(self.entries) do
        if entry.left > 0 then kept[#kept + 1] = entry end
    end
    self.entries = kept
    if self.current ~= nil and self.current.left <= 0 then self.current = nil end
    if self.current == nil then self.current = choose(kept) end
    return self.current and self.current.text or nil
end

-- What is on the screen is gone (the game was loaded, the player died)
function Subtitles:clear()
    self.entries, self.current, self.since = {}, nil, 0
end

return module
