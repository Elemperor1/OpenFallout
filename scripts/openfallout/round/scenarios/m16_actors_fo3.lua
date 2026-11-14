-- M16 actors (Fallout 3): the same checks as m16_actors.lua on two placed raiders of the Capital Wasteland (cell -3,0, 6 m apart):
-- what the script commands about a character answer while it does things (movement, turning, action, the procedure of the package,
-- group membership of a fight) and what the overrides of a script do (AI off, forced sneak, forced run).
-- Phases: S setup, Z the answers of a character at peace, A AI off stops a fighting character and AI on lets it go again,
-- B forced sneak slows a running character, C two characters fight the player and then one another, D the player forced to run and
-- to sneak while the controls ask for a walk, E DisableAllActors. The raiders are put back where the game placed them between
-- the phases, and the player is put 30 m south of them.
local M = 70
local PLUGIN = 'Fallout3.esm'
local SITE = { x = -10600, y = 1560, z = 14158, a = 0x52367, b = 0x52363 }

-- the codes of the commands (the table of the executables; the same numbers in both games)
local OP = {
    GetGroupMemberCount = 0x11A0, GetGroupTargetCount = 0x11A1, GetAnimAction = 0x10DB, GetCurrentAIProcedure = 0x108F,
    SetActorsAI = 0x118B, ToggleActorsAI = 0x112E, IsActorsAIOff = 0x112F, DisableAllActors = 0x11A9,
    IsIdlePlaying = 0x1070, SetForceSneak = 0x10D3, GetForceSneak = 0x10D2, SetForceRun = 0x10D1, GetForceRun = 0x10D0,
    GetFurnitureMarkerID = 0x10A0, IsActor = 0x1161, GetOwnerLastTarget = 0x124E, GetPlayerTeammateCount = 0x11C7,
    IsMoving = 0x1019, IsRunning = 0x111F, IsTurning = 0x101A, IsSwimming = 0x10B9, IsWaiting = 0x106F,
    GetWalkSpeed = 0x108E, GetDisease = 0x1027, GetFatiguePercentage = 0x1080, GetIdleDoneOnce = 0x113E,
}

return { limit = 600, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local AS = r.core.actorStatus
    local failures = {}
    local function check(name, ok, detail)
        r.log(string.format('CHECK %s %s %s', name, ok and 'PASS' or 'FAIL', detail or ''))
        if not ok then table.insert(failures, name) end
    end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { PLUGIN, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local function distance(a, b)
        local dx, dy = a.x - b.x, a.y - b.y
        return math.sqrt(dx * dx + dy * dy)
    end
    local function copy(p) return { x = p.x, y = p.y, z = p.z } end
    -- the answer of a command about an object ("" for no object), the text of the error when it failed
    local function ask(op, object, ...)
        local ok, value = pcall(AS.call, op, object and r.form(object) or '', ...)
        if ok then return value end
        return 'error: ' .. tostring(value)
    end
    local function bit(value, mask) return math.floor((value or 0) / mask) % 2 == 1 end
    -- one line of what the commands say about the object now
    local function sample(label, object)
        local act = AS.activity(r.form(object))
        local function n(op) return tostring(ask(op, object)) end
        r.log(string.format('%s: moving=%s running=%s turning=%s anim=%s proc=%s waiting=%s idle=%s walkSpeed=%s aiOff=%s sneak=%s run=%s group=%s/%s | stage=%s pkg=%s move=0x%x',
            label, n(OP.IsMoving), n(OP.IsRunning), n(OP.IsTurning), n(OP.GetAnimAction), n(OP.GetCurrentAIProcedure),
            n(OP.IsWaiting), n(OP.IsIdlePlaying), n(OP.GetWalkSpeed), n(OP.IsActorsAIOff), n(OP.GetForceSneak),
            n(OP.GetForceRun), n(OP.GetGroupMemberCount), n(OP.GetGroupTargetCount), act and act.stage or 'none',
            act and act.packageType or 'none', act and act.movement or 0))
        return act
    end

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    r.camera('ThirdPerson')
    r.teleport{ pos = { SITE.x, SITE.y - 6 * M, SITE.z + 300 }, yaw = 0, onGround = true, label = 'the raiders of cell -3,0' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE.a), lookup(SITE.b)
    if g1 == nil or g2 == nil then
        error('S the placed raiders were not found: there is nothing to watch, and a round with no character must not pass')
    end
    local home1, home2 = copy(g1.position), copy(g2.position)
    -- `reset(metres)` puts the pair back where the game placed it, at peace, and the player that far south of it
    local function reset(metres, label)
        r.peace(g1)
        r.peace(g2)
        r.putAt(g1, { pos = { home1.x, home1.y, home1.z }, yaw = math.pi, label = 'raider a ' .. label })
        r.putAt(g2, { pos = { home2.x, home2.y, home2.z }, yaw = math.pi, label = 'raider b ' .. label })
        r.teleport{ pos = { home1.x, home1.y - metres * M, home1.z + 300 }, yaw = 0, onGround = true, label = 'player ' .. label }
        r.wait(2.5)
    end
    reset(30, 'at the camp')
    r.settle(3, 30)
    r.eye('camp')
    r.track({ { label = 'g1', actor = g1 }, { label = 'g2', actor = g2 } }, 1)

    ---------------------------------------------------------------- Z: a character at peace
    phase('Z at peace')
    sample('Z g1 at peace', g1)
    sample('Z g2 at peace', g2)
    sample('Z player', player)
    check('Z IsActor of a character', ask(OP.IsActor, g1) == 1)
    check('Z IsActor of the player', ask(OP.IsActor, player) == 1)
    check('Z AI is on', ask(OP.IsActorsAIOff, g1) == 0)
    check('Z no group at peace', ask(OP.GetGroupMemberCount, g1) == 0 and ask(OP.GetGroupTargetCount, g1) == 0)
    -- the procedure and the action over time of two characters at peace, a second apart: what a package makes of them
    local packages = 0
    for i = 1, 8 do
        r.wait(1)
        for _, pair in ipairs({ { 'g1', g1 }, { 'g2', g2 } }) do
            local act = sample(string.format('Z %s %d s at peace', pair[1], i), pair[2])
            if act ~= nil and act.packageType >= 0 then packages = packages + 1 end
        end
    end
    check('Z a character that follows a package reports it', packages > 0, string.format('%d samples with a package', packages))
    r.log(string.format('Z GetFatiguePercentage g1=%s player=%s, GetPlayerTeammateCount=%s, GetDisease g1=%s, GetFurnitureMarkerID g1=%s',
        tostring(ask(OP.GetFatiguePercentage, g1)), tostring(ask(OP.GetFatiguePercentage, player)),
        tostring(ask(OP.GetPlayerTeammateCount, nil)), tostring(ask(OP.GetDisease, g1)), tostring(ask(OP.GetFurnitureMarkerID, g1))))

    ---------------------------------------------------------------- A: AI off
    phase('A AI off')
    reset(30, 'for the AI test')
    local start = copy(g1.position)
    r.fight(g1, player, 'g1 -> player')
    for i = 1, 5 do r.wait(0.5) sample(string.format('A fighting %.1f s', i * 0.5), g1) end
    local running = copy(g1.position)
    check('A it ran toward the player before', distance(start, running) > 100, string.format('%.0f units in 2.5 s', distance(start, running)))
    ask(OP.SetActorsAI, g1, 0)
    check('A AI off reads back', ask(OP.IsActorsAIOff, g1) == 1)
    r.wait(0.8)
    local stopped = copy(g1.position)
    sample('A AI off, 0.8 s later', g1)
    for i = 1, 6 do r.wait(0.5) sample(string.format('A AI off %.1f s', 0.8 + i * 0.5), g1) end
    local still = copy(g1.position)
    check('A AI off stops the character', distance(stopped, still) < 15, string.format('moved %.1f units in 3 s', distance(stopped, still)))
    check('A AI off character does not move on', ask(OP.IsMoving, g1) == 0)
    r.state('g1 AI off', g1)
    ask(OP.SetActorsAI, g1, 1)
    check('A AI on reads back', ask(OP.IsActorsAIOff, g1) == 0)
    -- (a ganger with a gun stops where it can shoot: it acts again when it moves or shoots)
    local acted = false
    for i = 1, 6 do
        r.wait(0.5)
        local act = sample(string.format('A AI on again %.1f s', i * 0.5), g1)
        if act ~= nil and act.animAction == 2 then acted = true end
    end
    local again = copy(g1.position)
    check('A it acts again when AI is on again', acted or distance(still, again) > 80,
        string.format('%.0f units in 3 s, shot=%s', distance(still, again), tostring(acted)))
    -- toggled the other way
    ask(OP.ToggleActorsAI, g1)
    check('A ToggleActorsAI turns it off', ask(OP.IsActorsAIOff, g1) == 1)
    ask(OP.ToggleActorsAI, g1)
    check('A ToggleActorsAI turns it on', ask(OP.IsActorsAIOff, g1) == 0)

    ---------------------------------------------------------------- B: forced sneak
    phase('B forced sneak')
    reset(35, 'for the sneak test')
    local b0 = copy(g1.position)
    r.fight(g1, player, 'g1 -> player')
    r.wait(1.0)
    local b1 = copy(g1.position)
    r.wait(1.5)
    local b2 = copy(g1.position)
    local runSpeed = distance(b1, b2) / 1.5
    sample('B running', g1)
    ask(OP.SetForceSneak, g1, 1)
    check('B GetForceSneak reads back', ask(OP.GetForceSneak, g1) == 1)
    r.wait(0.3)
    local b3 = copy(g1.position)
    r.wait(1.5)
    local b4 = copy(g1.position)
    local sneakSpeed = distance(b3, b4) / 1.5
    local act = sample('B forced sneak', g1)
    r.log(string.format('B g1 runs at %.0f units/s, with sneak forced at %.0f units/s', runSpeed, sneakSpeed))
    check('B forced sneak slows the character', sneakSpeed < runSpeed * 0.8 and runSpeed > 100,
        string.format('%.0f against %.0f units/s', sneakSpeed, runSpeed))
    check('B the movement word says sneaking', act ~= nil and bit(act.movement, 0x400))
    ask(OP.SetForceSneak, g1, 0)
    check('B GetForceSneak cleared', ask(OP.GetForceSneak, g1) == 0)
    r.wait(0.5)
    local b5 = copy(g1.position)
    r.wait(1.0)
    local b6 = copy(g1.position)
    r.log(string.format('B g1 after the override is cleared: %.0f units/s', distance(b5, b6) / 1.0))
    r.peace(g1)

    ---------------------------------------------------------------- C: groups and procedures
    phase('C groups')
    reset(30, 'for the group test')
    r.fight(g1, player, 'g1 -> player')
    r.fight(g2, player, 'g2 -> player')
    -- (seven seconds: a character that has a long way to run is not in range to shoot before)
    local attacked = false
    for i = 1, 14 do
        r.wait(0.5)
        local act = sample(string.format('C g1 %.1f s', i * 0.5), g1)
        if act ~= nil and act.animAction == 2 then attacked = true end
        sample(string.format('C g2 %.1f s', i * 0.5), g2)
    end
    -- (these raiders stand and aim for seven seconds without a shot, so the attack action is only reported here: the New Vegas round holds it as a check)
    r.log('C attack action seen in seven seconds: ' .. tostring(attacked))
    check('C two fighters of the player: member count 2', ask(OP.GetGroupMemberCount, g1) == 2 and ask(OP.GetGroupMemberCount, g2) == 2,
        string.format('%s and %s', tostring(ask(OP.GetGroupMemberCount, g1)), tostring(ask(OP.GetGroupMemberCount, g2))))
    check('C two fighters of the player: target count 1', ask(OP.GetGroupTargetCount, g1) == 1,
        tostring(ask(OP.GetGroupTargetCount, g1)))
    r.peace(g2)
    r.wait(0.5)
    check('C one fighter left: member count 1', ask(OP.GetGroupMemberCount, g1) == 1, tostring(ask(OP.GetGroupMemberCount, g1)))
    r.peace(g1)
    r.wait(1)
    check('C no fight: member count 0', ask(OP.GetGroupMemberCount, g1) == 0, tostring(ask(OP.GetGroupMemberCount, g1)))
    -- the two fight one another
    reset(30, 'for the duel')
    r.putAt(g2, { pos = { home1.x + 140, home1.y - 8 * M, home1.z }, yaw = math.pi, label = 'raider b 8 m from raider a' })
    r.wait(1)
    r.fight(g1, g2, 'g1 -> g2')
    r.fight(g2, g1, 'g2 -> g1')
    for i = 1, 6 do r.wait(0.5) sample(string.format('C duel g1 %.1f s', i * 0.5), g1) sample(string.format('C duel g2 %.1f s', i * 0.5), g2) end
    r.log(string.format('C duel: g1 group=%s/%s, g2 group=%s/%s', tostring(ask(OP.GetGroupMemberCount, g1)),
        tostring(ask(OP.GetGroupTargetCount, g1)), tostring(ask(OP.GetGroupMemberCount, g2)), tostring(ask(OP.GetGroupTargetCount, g2))))
    do
        local function names(list) local out = {} for _, id in ipairs(list) do out[#out + 1] = id end return table.concat(out, ',') end
        r.log('C duel: attackers of g1 = [' .. names(r.core.combat.attackers(r.form(g1))) .. '], attackers of g2 = [' ..
            names(r.core.combat.attackers(r.form(g2))) .. '], g1 is ' .. r.form(g1) .. ', g2 is ' .. r.form(g2))
    end
    check('C a duel has a fighter in each group', ask(OP.GetGroupMemberCount, g1) >= 1 and ask(OP.GetGroupMemberCount, g2) >= 1)
    r.state('g1 duel', g1)
    r.state('g2 duel', g2)
    r.peace(g1)
    r.peace(g2)

    ---------------------------------------------------------------- D: the player forced to run and to sneak
    phase('D forced gait of the player')
    r.drawn(false)
    -- the player is walked forward 2.5 s with the walk control, in the three states: plain, run forced, sneak forced
    -- (the place is read every quarter of a second while the control is held, and the speed is the best of the 0.75 s stretches:
    -- a frame that hangs on a loaded computer makes a stretch slower, never faster)
    local function walk(label, setup)
        reset(30, label)
        r.wait(1)
        if setup then setup() end
        r.wait(0.8)
        local places = { copy(player.position) }
        r.move{ forward = 1, run = false, seconds = 2.5 }
        for i = 1, 10 do
            r.wait(0.25)
            places[#places + 1] = copy(player.position)
            if i == 4 then sample('D player ' .. label, player) end
        end
        local speed = 0
        for i = 4, #places do speed = math.max(speed, distance(places[i - 3], places[i]) / 0.75) end
        r.log(string.format('D %s: %.0f units/s (%.0f units in 2.5 s)', label, speed, distance(places[1], places[#places])))
        return speed
    end
    local walkSpeed = walk('walking', nil)
    local runForced = walk('walking with run forced', function() ask(OP.SetForceRun, player, 1) end)
    check('D GetForceRun reads back', ask(OP.GetForceRun, player) == 1)
    ask(OP.SetForceRun, player, 0)
    local sneakForced = walk('walking with sneak forced', function() ask(OP.SetForceSneak, player, 1) end)
    check('D GetForceSneak reads back', ask(OP.GetForceSneak, player) == 1)
    ask(OP.SetForceSneak, player, 0)
    r.log(string.format('D player speeds: walk %.0f, run forced %.0f, sneak forced %.0f units/s', walkSpeed, runForced, sneakForced))
    check('D forced run is faster than a walk', runForced > walkSpeed * 1.3 and walkSpeed > 50, string.format('%.0f against %.0f', runForced, walkSpeed))
    check('D forced sneak is slower than a walk', sneakForced < walkSpeed * 0.9, string.format('%.0f against %.0f', sneakForced, walkSpeed))

    ---------------------------------------------------------------- E: DisableAllActors
    phase('E DisableAllActors')
    reset(30, 'for the last test')
    r.wait(2)
    local function enabled(object)
        local ok, value = pcall(function() return object.enabled end)
        return ok and tostring(value) or 'error'
    end
    r.log('E before: g1 enabled=' .. enabled(g1) .. ' g2 enabled=' .. enabled(g2))
    ask(OP.DisableAllActors, nil)
    r.wait(1)
    r.log('E after: g1 enabled=' .. enabled(g1) .. ' g2 enabled=' .. enabled(g2) .. ' player enabled=' .. enabled(player))
    check('E DisableAllActors disables the characters', enabled(g1) == 'false' and enabled(g2) == 'false')
    r.untrack()
    r.log('PHASE done')
    if #failures > 0 then error('checks failed: ' .. table.concat(failures, '; ')) end
end }
