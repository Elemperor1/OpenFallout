-- The mountains behind Goodsprings (M13, Jacob's hour of New Vegas, 2026-11-10): the black upright rectangles on the far hills
-- (E1) and the floating grey ribbon over the ridge (E2), seen from the two places of Mac round 13R at about two in the afternoon.
-- The variants of rounds/m13-mountains.json differ in `[Terrain] game lod debug` (nothing, flat, no-high, objects-only, no-land,
-- no-records, wireframe), so one command gives every picture the questions of the round need.
-- The places are in the Mojave. The save can be anywhere (Jacob's autosave of 2026-11-10 is in the doctor's house, an interior,
-- where the cell of the player has the interior for its worldspace), so the worldspace is named by a cell that lives there: the
-- first of these that answers. A cell of the record store can be read without being loaded; a reference is read through its cell,
-- which fails (`Object is not available`) when that cell is not loaded, so every property is read inside the pcall.
local F = 'FalloutNV.esm'
-- cells of WastelandNV at (-17,0), (-17,1) and (-18,0): the cells of the places and of the first exterior place of the M13 probes
local CELLS = { 0x0daeb9, 0x0daeb8, 0x0daebb }
-- references of the Mojave that the probes of the M13 session used for their `tele` commands
local ANCHORS = { 0xcdf51, 0x10a201 }

local function try(r, label, get)
    local ok, answer = pcall(function()
        local cell = get()
        if cell == nil then return nil end
        return { cell = cell, exterior = cell.isExterior, worldspace = tostring(cell.worldSpaceId) }
    end)
    if not ok then
        r.log('anchor', label, 'error ' .. tostring(answer))
        return nil
    end
    if answer == nil then
        r.log('anchor', label, 'not found')
        return nil
    end
    r.log('anchor', label, 'worldspace=' .. answer.worldspace .. ' exterior=' .. tostring(answer.exterior))
    return answer.exterior and answer.cell or nil
end

local function mojave(r)
    for _, id in ipairs(CELLS) do
        local cell = try(r, string.format('cell 0x%x', id), function() return r.world.getCellById(r.core.getFormId(F, id)) end)
        if cell then return cell end
    end
    for _, id in ipairs(ANCHORS) do
        local cell = try(r, string.format('ref 0x%x', id), function() return r.lookup({ F, id }).cell end)
        if cell then return cell end
    end
    -- none found: a save that already stands in the open names the worldspace by its own cell
    local here = r.player().cell
    if here and here.isExterior then
        r.log('anchor', 'player cell', 'worldspace=' .. tostring(here.worldSpaceId))
        return here
    end
    error('no cell or reference of the Mojave was found and the save is in an interior: the round cannot tell which worldspace to go to')
end

return function(r)
    local here = r.player().cell
    r.log('start', 'cell', tostring(here and here.worldSpaceId), 'exterior=' .. tostring(here and here.isExterior))
    local worldspace = mojave(r)
    r.hour(14)
    -- E1: the Joshua trees on the far hills (the height is the one the M13 session teleported to; the player falls from there)
    r.teleport{ pos = { -69319, 3528, 8381 }, worldspace = worldspace, yaw = -3.13, label = 'e1' }
    r.wait(12)
    r.look(-3.13, 0)
    r.eye('e1')
    r.ground('e1', { { 0, 0 } })
    r.shot('e1')
    -- E2: the ridge with the ribbon
    r.teleport{ pos = { -68585, 4600, 8353 }, worldspace = worldspace, yaw = -1.51, label = 'e2' }
    r.wait(12)
    r.look(-1.51, 0)
    r.eye('e2')
    r.ground('e2', { { 0, 0 } })
    r.shot('e2')
    r.fps('e2', 3)
end
