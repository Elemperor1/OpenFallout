-- The smallest scenario: where the player is, what the camera does, the ground under the eye, the frame times, one screenshot.
-- It is what `round.py run --scenario scenarios/smoke.lua --game nv --save latest` runs to see that a build starts and draws.
return function(r)
    r.eye('start')
    r.ground('start', { { 256, 0 }, { 0, 256 } })
    r.wait(3)
    r.fps('standing', 2)
    r.shot('start')
    r.actors('near', 3000, 8)
end
