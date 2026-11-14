-- M16 effects (New Vegas): the challenges and the quest the Pip-Boy follows, on the real data. The commands run through the round-only
-- tool debug.scriptCommand: the values they give and what the engine does about them are in the engine log ("Command: ...",
-- "Challenge: ..."), in the order of the STEP lines.
-- Steps: C1 a challenge of the counter 37 counts three steps and is completed (its script, a notice and the counter of the
-- completed challenges), C2 a challenge that starts disabled is unlocked and counts, Q the active quest is set.
local NV = 'FalloutNV.esm'

return { limit = 240, delay = 6, run = function(r)
    local function id(n) return tostring(r.core.getFormId(NV, n)) end
    local WIN, MASTER, STOP = id(0x165777), id(0x16ba4e), id(0x15eb2b)
    local function step(label) r.log('STEP ' .. label) end
    local function cmd(name, ref, args) r.command(name, ref, args) end

    r.god(true)
    r.wait(2)

    -- C1: the challenge counts the Pip-Boy counter 37 (three steps complete it)
    step('C1 before')
    cmd('GetChallengeCompleted', nil, { WIN })
    -- the next challenge of the counter starts disabled: its script is unlocked by the first one's, as the game's data says
    cmd('ChallengeLocked', nil, { MASTER })
    cmd('GetPCMiscStat', nil, { 37 })
    cmd('GetPCMiscStat', nil, { 27 })
    r.messages('before the steps')
    for i = 1, 3 do
        step('C1 step ' .. i .. ' (ModPCMiscStat 37 1)')
        cmd('ModPCMiscStat', nil, { 37, 1 })
        cmd('GetChallengeCompleted', nil, { WIN })
    end
    step('C1 after')
    cmd('ChallengeLocked', nil, { MASTER })
    cmd('GetPCMiscStat', nil, { 37 })
    cmd('GetPCMiscStat', nil, { 27 })
    r.messages('after the completion')

    -- C2: a challenge that starts disabled (of the counter 9) is unlocked by the command and counts from then on
    step('C2 before the unlock')
    cmd('ChallengeLocked', nil, { STOP })
    cmd('GetPCMiscStat', nil, { 9 })
    cmd('ModPCMiscStat', nil, { 9, 2 })
    step('C2 unlock')
    cmd('UnlockChallenge', nil, { STOP })
    cmd('ChallengeLocked', nil, { STOP })
    cmd('ModPCMiscStat', nil, { 9, 25 })
    cmd('GetChallengeCompleted', nil, { STOP })
    cmd('GetPCMiscStat', nil, { 27 })
    r.messages('after the unlock')

    -- Q: the quest the Pip-Boy follows
    step('Q list before')
    local before = r.pipboyQuests('before')
    if before == nil or before.count == nil or before.count < 2 then
        r.error('Q the Pip-Boy lists fewer than two quests: there is nothing to choose between')
        return
    end
    local pick
    for _, row in ipairs(before.rows) do
        if not row.active then pick = row end
    end
    step('Q force the active quest to a listed quest that is not followed')
    r.log('Q chosen ' .. tostring(pick.id) .. ' completed=' .. tostring(pick.completed))
    cmd('ForceActiveQuest', nil, { pick.id })
    local after = r.pipboyQuests('after')
    r.log('Q the followed quest is the chosen one: ' .. tostring(after.active == pick.id))
    r.wait(1)
end }
