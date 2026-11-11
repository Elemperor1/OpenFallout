-- The mountains behind Goodsprings (M13, Jacob's hour of New Vegas, 2026-11-10): the black upright rectangles on the far hills
-- (E1) and the floating grey ribbon over the ridge (E2), seen from the two places of Mac round 13R at about two in the afternoon.
-- The variants of rounds/m13-mountains.json differ in `[Terrain] game lod debug` (nothing, flat, no-high, objects-only, no-land,
-- no-records, wireframe), so one command gives every picture the questions of the round need.
return function(r)
    r.hour(14)
    -- E1: the Joshua trees on the far hills
    r.teleport{ pos = { -69319, 3528 }, onGround = true, yaw = -3.13, label = 'e1' }
    r.wait(12)
    r.look(-3.13, 0)
    r.eye('e1')
    r.ground('e1', { { 0, 0 } })
    r.shot('e1')
    -- E2: the ridge with the ribbon
    r.teleport{ pos = { -68585, 4600 }, onGround = true, yaw = -1.51, label = 'e2' }
    r.wait(12)
    r.look(-1.51, 0)
    r.eye('e2')
    r.ground('e2', { { 0, 0 } })
    r.shot('e2')
    r.fps('e2', 3)
end
