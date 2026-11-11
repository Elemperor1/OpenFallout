-- The mountains behind Goodsprings (M13, Jacob's hour of New Vegas, 2026-11-10): the black upright rectangles on the far hills
-- (E1) and the floating grey ribbon over the ridge (E2), seen from the two places of Mac round 13R at about two in the afternoon.
-- The variants of rounds/m13-mountains.json differ in `[Terrain] game lod debug` (nothing, flat, no-high, objects-only, no-land,
-- no-records, wireframe), so one command gives every picture the questions of the round need.
-- The places are in the Mojave. A reference that lives there has the worldspace in its cell, so the save can be anywhere (Jacob's
-- autosave of 2026-11-10 is in the doctor's house, an interior, where the cell of the player has the interior for its worldspace):
-- the first of these that is found names it. They are the references the probes of the M13 session used for its `tele` commands.
local ANCHORS = { { 'FalloutNV.esm', 0xcdf51 }, { 'FalloutNV.esm', 0x10a201 } }

local function mojave(r)
    for _, form in ipairs(ANCHORS) do
        local ok, object = pcall(r.lookup, form)
        local cell = ok and object and object.cell or nil
        r.log('anchor', string.format('0x%x', form[2]),
            cell and ('worldspace=' .. tostring(cell.worldSpaceId) .. ' exterior=' .. tostring(cell.isExterior)) or 'not found')
        if cell and cell.isExterior then return cell end
    end
    -- none found: a save that already stands in the open names the worldspace by its own cell
    local here = r.player().cell
    if here and here.isExterior then
        r.log('anchor', 'player cell', 'worldspace=' .. tostring(here.worldSpaceId))
        return here
    end
    error('no reference of the Mojave was found and the save is in an interior: the round cannot tell which worldspace to go to')
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
