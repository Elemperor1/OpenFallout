-- The subtitles of the head-up display of Fallout 3 and New Vegas, as the executable keeps them (FalloutNV.exe 0x774FD0,
-- 0x7711F4 and 0x775230; Mac round 12AO B3): a line that a character says is an entry with the seconds it stays from the
-- moment it was said; every 200 ms the display drops the entries that are over, and when none is on the screen it takes
-- the one of the speaker nearest to the player (a line a script forced comes before the others) and shows it until it is over.
-- The tile `Subtitles` of hud_main_menu.xml is empty; the display makes a centred text in it (falloutmenuhud.lua).
local module = {}

-- The display looks at the entries this often (the executable's update of the display, 200 ms)
module.CHECK_SECONDS = 0.2

local Subtitles = {}
Subtitles.__index = Subtitles

function module.new()
    return setmetatable({ entries = {}, current = nil, since = 0 }, Subtitles)
end

-- A line: `text`, `seconds` it stays, `distance` of the speaker from the player, `forced` when a script asked for it
function Subtitles:add(line)
    if type(line.text) ~= 'string' or line.text == '' then return end
    self.entries[#self.entries + 1] = {
        text = line.text,
        left = tonumber(line.seconds) or 0,
        distance = tonumber(line.distance) or math.huge,
        forced = line.forced == true,
    }
end

local function before(a, b)
    if a.forced ~= b.forced then return a.forced end
    return a.distance < b.distance
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
    if self.current == nil then
        for _, entry in ipairs(kept) do
            if self.current == nil or before(entry, self.current) then self.current = entry end
        end
    end
    return self.current and self.current.text or nil
end

-- What is on the screen is gone (the game was loaded, the player died)
function Subtitles:clear()
    self.entries, self.current, self.since = {}, nil, 0
end

return module
