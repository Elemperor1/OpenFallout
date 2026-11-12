-- M15 round 15B (Fallout 3): the music of a cell is a folder of tracks of its music record (the director starts one after its
-- delay and rests between two). Checked by the log lines "Music: ..." with the master volume at 0; the player stands in a

return { limit = 240, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    r.god(true)
    phase('A interior')
    r.eye('start')
    r.wait(25)
    r.settle(3, 30)
    r.wait(35)
    r.eye('end')
end }
