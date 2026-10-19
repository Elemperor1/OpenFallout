-- The menu side of the probe of the saves in autoplayworld.lua: saves the game and loads it back when the world script asks
-- (a menu script is the only kind that may). The description of a save is how the world script names it, and the log says the
-- file it was written to. Lines start with AUTOPLAY.
local menu = require('openfallout.menu')

-- The slots that were there when the probe saved under a description, so that the slot it made is the one that is new
-- (a second save under the same name gets a name of its own, and sorting the names does not tell which is the newer)
local before = {}

local function slotsOf(description)
    local dir = menu.getCurrentSaveDir()
    local found = {}
    if dir ~= nil then
        for slot, info in pairs(menu.getSaves(dir)) do
            if info.description == description then table.insert(found, slot) end
        end
    end
    table.sort(found)
    return dir, found
end

local function madeBy(description, found)
    local known = before[description] or {}
    local made = {}
    for _, slot in ipairs(found) do
        if not known[slot] then table.insert(made, slot) end
    end
    return made[#made] or found[#found]
end

return {
    eventHandlers = {
        AutoplaySave = function(data)
            local _, existing = slotsOf(data.description)
            before[data.description] = {}
            for _, slot in ipairs(existing) do before[data.description][slot] = true end
            local ok, err = pcall(menu.saveGame, data.description)
            local dir, found = slotsOf(data.description)
            print('AUTOPLAY', 'save', data.description, ok and 'ok' or tostring(err),
                'dir=' .. tostring(dir), 'files=' .. table.concat(found, ','))
        end,
        AutoplayLoad = function(data)
            local dir, found = slotsOf(data.description)
            if #found == 0 then
                print('AUTOPLAY', 'load', data.description, 'no slot')
                return
            end
            local slot = madeBy(data.description, found)
            print('AUTOPLAY', 'load', data.description, slot)
            local ok, err = pcall(menu.loadGame, dir, slot)
            if not ok then print('AUTOPLAY', 'load', 'error', tostring(err)) end
        end,
    },
}
