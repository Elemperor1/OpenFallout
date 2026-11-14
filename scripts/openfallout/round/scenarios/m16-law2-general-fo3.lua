-- M16 law2 (Fallout 3): the general page of the Pip-Boy on the real data: the karma (its word, picture and title by level), and the
-- counts of the page. Fallout 3 has no reputations and no rank lines on the page. The karma is set with RewardKarma and the page is
-- opened as a key of the Pip-Boy would and photographed (r.shot). The log has numbers and sizes, never a text.
return { limit = 300, delay = 6, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local function cmd(name, ref, args) r.command(name, ref, args) end
    r.god(true)
    r.wait(2)

    phase('A neutral karma')
    for stat, value in pairs({ [0] = 5, [2] = 3, [3] = 2, [4] = 1, [13] = 4 }) do
        cmd('ModPCMiscStat', nil, { stat, value })
    end
    r.pipboyGeneral('neutral')
    r.pipboyOpen{ page = 'stat', stats = 'general', label = 'neutral' }
    r.wait(3)
    r.shot('general-neutral')
    r.pipboyClose('A')
    r.wait(2)

    phase('B very good karma')
    cmd('RewardKarma', nil, { 900 })
    r.wait(1)
    r.pipboyGeneral('very good')
    r.pipboyOpen{ page = 'stat', stats = 'general', label = 'very good' }
    r.wait(3)
    r.shot('general-very-good')
    r.pipboyClose('B')
    r.wait(2)

    phase('C evil karma')
    cmd('RewardKarma', nil, { -1400 })
    r.wait(1)
    r.pipboyGeneral('evil')
    r.pipboyOpen{ page = 'stat', stats = 'general', label = 'evil' }
    r.wait(3)
    r.shot('general-evil')
    r.pipboyClose('C')
    r.wait(2)

    phase('D very evil karma')
    cmd('RewardKarma', nil, { -600 })
    r.wait(1)
    r.pipboyGeneral('very evil')
    r.pipboyOpen{ page = 'stat', stats = 'general', label = 'very evil' }
    r.wait(3)
    r.shot('general-very-evil')
    r.pipboyClose('D')
    r.wait(1)
    phase('end')
end }
