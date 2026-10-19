-- The menu side of the probe of the saves in autoplayworld.lua: saves the game and loads it back when the world script asks
-- (a menu script is the only kind that may). The description of a save is how the world script names it, and the log says the
-- file it was written to. Lines start with AUTOPLAY.
local menu = require('openfallout.menu')

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

return {
    eventHandlers = {
        AutoplaySave = function(data)
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
            print('AUTOPLAY', 'load', data.description, found[#found])
            local ok, err = pcall(menu.loadGame, dir, found[#found])
            if not ok then print('AUTOPLAY', 'load', 'error', tostring(err)) end
        end,
    },
}
