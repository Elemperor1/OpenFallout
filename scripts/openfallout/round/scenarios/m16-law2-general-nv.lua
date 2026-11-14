-- M16 law2 (New Vegas): the general page of the Pip-Boy on the real data. The reputations and the karma are set with the script
-- commands (SetReputation, RewardKarma, ModPCMiscStat); the page is opened as a key of the Pip-Boy would and photographed (r.shot).
-- Phases: A the page of the reputations (13 reputations of the game, 6 given some fame or infamy), B the counts and the karma of a
-- good character, C the same after the karma went to the very evil. The log has numbers, ids and sizes, never a text.
local NV = 'FalloutNV.esm'
-- reputation: { form, fame, infamy }; the side 1 is the fame and the side 0 the infamy
local REPUTATIONS = {
    { 0xf43de, 60, 6 },   -- the cap is 80: fame level 2, infamy level 0
    { 0xf43dd, 0, 100 },  -- the cap is 100: infamy full
    { 0x104c22, 15, 0 },  -- the cap is 15: fame full
    { 0x1558e6, 5, 20 },  -- the cap is 30: fame level 1, infamy level 2
    { 0xffae8, 10, 0 },   -- the cap is 50: fame level 1
    { 0x11e662, 0, 12 },  -- the cap is 20: infamy level 2
}

return { limit = 300, delay = 6, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local function id(n) return tostring(r.core.getFormId(NV, n)) end
    local function cmd(name, ref, args) r.command(name, ref, args) end
    r.god(true)
    r.wait(2)

    phase('S before anything is set')
    r.pipboyGeneral('start')

    phase('A the reputations')
    for _, entry in ipairs(REPUTATIONS) do
        if entry[2] > 0 then cmd('SetReputation', nil, { id(entry[1]), 1, entry[2] }) end
        if entry[3] > 0 then cmd('SetReputation', nil, { id(entry[1]), 0, entry[3] }) end
    end
    cmd('RewardKarma', nil, { 300 })
    for stat, value in pairs({ [0] = 12, [2] = 7, [3] = 4, [4] = 3, [5] = 2, [13] = 9, [36] = 41 }) do
        cmd('ModPCMiscStat', nil, { stat, value })
    end
    r.wait(1)
    r.pipboyGeneral('set')
    r.pipboyOpen{ page = 'stat', stats = 'general', genrep = 1, label = 'reputations' }
    r.wait(3)
    r.shot('general-reputations')
    r.pipboyClose('A')
    r.wait(2)
    -- the names sort as Boomers, Brotherhood of Steel, Caesar's Legion, Goodsprings, NCR, Powder Gangers: rows 5, 4 and 3 are the
    -- ones set above with fame 60 and infamy 6, with the fame full, and with the infamy full
    for _, row in ipairs({ 5, 4, 3 }) do
        r.pipboyOpen{ page = 'stat', stats = 'general', genrep = 1, row = row, label = 'reputation row ' .. row }
        r.wait(3)
        r.shot('general-reputation-row-' .. row)
        r.pipboyClose('row ' .. row)
        r.wait(2)
    end

    phase('B the counts and the karma of a good character')
    r.pipboyOpen{ page = 'stat', stats = 'general', genrep = 0, label = 'counts' }
    r.wait(3)
    r.shot('general-counts-good')
    r.pipboyClose('B')
    r.wait(2)

    phase('C the karma goes to the very evil')
    cmd('RewardKarma', nil, { -1100 })
    r.wait(1)
    r.pipboyGeneral('evil')
    r.pipboyOpen{ page = 'stat', stats = 'general', genrep = 0, label = 'counts evil' }
    r.wait(3)
    r.shot('general-counts-very-evil')
    r.pipboyClose('C')
    r.wait(1)
    phase('end')
end }
