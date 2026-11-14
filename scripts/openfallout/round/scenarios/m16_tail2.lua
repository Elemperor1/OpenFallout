-- M16 tail2 (New Vegas): what the leftover commands of the world and actors packages answer on real data, asked through
-- core.actorStatus.call (the way a condition calls them) and core.worldCommands. Phases: S setup (the placed gangers of the camp at
-- site X, taken to open ground), Z the answers of a character at peace (IsFacingUp, IsLeftUp, GetKnockedState, GetForceHitReaction),
-- K a character thrown by PushActorAway answers GetKnockedState 1 while it is thrown and 0 after, P SetPackDuration ends the package
-- of a character after the game hours the number says, G the ini setting of the gore and the pad. It logs numbers only.
local M = 70
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }
local ARENA = { x = -69660, y = 1900, z = 8352 }
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3 }
local PAIR_Y = ARENA.y + 30 * M
local OP = {
    GetKnockedState = 0x106B, IsFacingUp = 0x106A, IsLeftUp = 0x111D, GetCurrentAIProcedure = 0x108F, SetPackDuration = 0x1122,
    GetForceHitReaction = 0x1259, GetWalkSpeed = 0x108E, IsActor = 0x1161,
}

return { limit = 420, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local AS = r.core.actorStatus
    local W = r.core.worldCommands
    local failures = {}
    local function check(name, ok, detail)
        r.log(string.format('CHECK %s %s %s', name, ok and 'PASS' or 'FAIL', detail or ''))
        if not ok then table.insert(failures, name) end
    end
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { NV, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local function ask(op, object, ...)
        local ok, value = pcall(AS.call, op, object and r.form(object) or '', ...)
        if ok then return value end
        return 'error: ' .. tostring(value)
    end
    local function world(name, reference, arguments)
        local ok, id = pcall(W.run, name, reference, arguments)
        if not ok then r.log('ASK ' .. name .. ' failed: ' .. tostring(id)) return nil end
        local value
        r.waitFor(function() value = W.result(id) return value ~= nil end, 10, name)
        r.log(string.format('ASK %s %s -> %s', name, reference or '-', tostring(value)))
        return value
    end

    phase('S setup')
    r.god(true)
    r.activate(DOOR_OUT, 'door out')
    r.waitFor(exterior, 40, 'exterior')
    r.wait(8)
    r.camera('ThirdPerson')
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(12)
    r.settle(3, 30)
    local g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then
        error('S the placed gangers were not found: there is nothing to watch, and a round with no character must not pass')
    end
    r.peace(g1)
    r.peace(g2)
    r.putAt(g1, { pos = { ARENA.x - 140, PAIR_Y, ARENA.z }, yaw = math.pi, label = 'ganger1' })
    r.putAt(g2, { pos = { ARENA.x + 140, PAIR_Y, ARENA.z }, yaw = math.pi, label = 'ganger2' })
    r.teleport{ pos = { ARENA.x, PAIR_Y - 20 * M, ARENA.z }, yaw = 0, onGround = true, label = 'player' }
    r.wait(4)
    r.settle(3, 30)
    r.track({ { label = 'g1', actor = g1 }, { label = 'g2', actor = g2 } }, 1)

    phase('Z at peace')
    local act = AS.activity(r.form(g1))
    r.log(string.format('Z g1 spine up parts Y=%s Z=%s package=%s stage=%s', tostring(act and act.spineYUp), tostring(act and act.spineZUp),
        tostring(act and act.packageType), tostring(act and act.stage)))
    r.log(string.format('Z g1 IsFacingUp=%s IsLeftUp=%s GetKnockedState=%s GetForceHitReaction=%s',
        tostring(ask(OP.IsFacingUp, g1)), tostring(ask(OP.IsLeftUp, g1)), tostring(ask(OP.GetKnockedState, g1)),
        tostring(ask(OP.GetForceHitReaction, g1))))
    check('Z a character at peace is not knocked', ask(OP.GetKnockedState, g1) == 0)
    check('Z the forced reaction is clear', ask(OP.GetForceHitReaction, g1) == 0)

    phase('K thrown')
    -- the player is the origin of the push (the caller of the command), the one pushed is the parameter
    local before = ask(OP.GetKnockedState, g1)
    world('PushActorAway', W.player(), { r.form(g1) })
    local seen, seenAt = {}, {}
    for i = 1, 8 do
        r.wait(0.3)
        local v = ask(OP.GetKnockedState, g1)
        seen[#seen + 1] = v
        r.log(string.format('K %.1f s after the push GetKnockedState=%s', i * 0.3 + 0.4, tostring(v)))
    end
    r.wait(2)
    local after = ask(OP.GetKnockedState, g1)
    check('K before the push 0', before == 0)
    check('K thrown while it flies', seen[1] == 1 or seen[2] == 1, table.concat(seen, ','))
    check('K 0 after the throw', after == 0)

    phase('P package duration')
    r.peace(g1)
    r.peace(g2)
    r.wait(2)
    local a1, a2 = AS.activity(r.form(g1)), AS.activity(r.form(g2))
    r.log(string.format('P before: g1 package=%s stage=%s, g2 package=%s stage=%s', tostring(a1 and a1.packageType), tostring(a1 and a1.stage),
        tostring(a2 and a2.packageType), tostring(a2 and a2.stage)))
    -- a duration of 1 is a hundredth of a game hour, 36 game seconds: about a second of real time at the time scale of the game
    ask(OP.SetPackDuration, g1, 1)
    for i = 1, 5 do
        r.wait(1)
        local c1, c2 = AS.activity(r.form(g1)), AS.activity(r.form(g2))
        r.log(string.format('P %d s after: g1 package=%s stage=%s, g2 package=%s stage=%s', i, tostring(c1 and c1.packageType),
            tostring(c1 and c1.stage), tostring(c2 and c2.packageType), tostring(c2 and c2.stage)))
    end
    local d1, d2 = AS.activity(r.form(g1)), AS.activity(r.form(g2))
    -- the package that ran out is replaced by the next one that is on (a package of another kind), the control keeps its own
    check('P the package with a duration is over and the other is not', d1 ~= nil and d2 ~= nil and d1.packageType ~= a1.packageType
        and d2.packageType == a2.packageType,
        string.format('g1 %s to %s, g2 %s to %s', tostring(a1 and a1.packageType), tostring(d1 and d1.packageType), tostring(a2 and a2.packageType), tostring(d2 and d2.packageType)))

    phase('D lying')
    -- a character that is killed lies down: the spine bone's axes turn with it
    local live = AS.activity(r.form(g2))
    r.log(string.format('D alive g2 spine Y=%s Z=%s IsFacingUp=%s IsLeftUp=%s', tostring(live and live.spineYUp), tostring(live and live.spineZUp),
        tostring(ask(OP.IsFacingUp, g2)), tostring(ask(OP.IsLeftUp, g2))))
    r.command('Kill', r.form(g2), {})
    for i = 1, 4 do
        r.wait(1)
        local dead = AS.activity(r.form(g2))
        r.log(string.format('D %d s after the kill g2 spine Y=%s Z=%s IsFacingUp=%s IsLeftUp=%s', i, tostring(dead and dead.spineYUp),
            tostring(dead and dead.spineZUp), tostring(ask(OP.IsFacingUp, g2)), tostring(ask(OP.IsLeftUp, g2))))
    end

    phase('G settings')
    world('IsGoreDisabled')
    world('IsWin32')
    world('IsNoHumanGoreExe')
    world('EnableLoadingMenu', nil, { 1 })
    world('IsLoadingMenuEnabled')
    world('EnableLoadingMenu', nil, { 0 })
    world('IsLoadingMenuEnabled')

    r.untrack()
    r.log(string.format('DONE failures=%d', #failures))
    if #failures > 0 then error('failed checks: ' .. table.concat(failures, ', ')) end
end }
