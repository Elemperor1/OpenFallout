-- M14 round 14A: what moving and fighting look like in the engine now (New Vegas). A survey: it logs, it does not judge.
-- Phases: S setup (out of the interior), A first person, B third person, C a fight with two armed hostiles, D V.A.T.S.,
-- E detection and sneak, F a fight that starts far away (the character has to walk). Lengths in game units: one metre is about 70 units.
-- The characters are references that stand in the world (the engine's fight state is kept by form id, and the objects that
-- world.createObject makes have none); each is looked up by its form id, and a spawned one of the same kind stands in when not found.
local M = 70
local NV = 'FalloutNV.esm'
local DOOR_OUT = { NV, 0x103e61 }                       -- the door of the doctor's house, the way out to Goodsprings
-- Placed Powder Gangers (leveled, with guns or melee) of the world's persistent cell: they are available once the player stands near them
-- open flat ground east of the doctor's house (the heights of a grid of rays from the door: 8340 to 8370 over 5000 by 3000 units)
local ARENA_C = { x = -69660, y = 1900, z = 8352 }   -- the player stands here for the fight, the characters 20 m north
-- the sneaking and V.A.T.S. arena: the same open ground, a little south (the first one, at x -67700, had the Prospector Saloon between
-- the player and the character: round 14D found V.A.T.S. blocked by its walls, 'something is in the way')
local ARENA_E = { x = -69660, y = 1300, z = 8352 }
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3, melee = 0x1629a5 }   -- cell -6,-10
local SITE_Y = { x = -23198, y = -26000, z = 6300, gun1 = 0x0f1977, melee = 0x0f1978 }                      -- cell -6,-6

local RUN = { A = true, B = true, C = true, E = true, D = true, F = true }   -- which phases run (a quick check sets some to false)

return { limit = 900, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local e1
    local guarded = false
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    local function lookup(id)
        local ok, object = pcall(r.lookup, { NV, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    -- takes the player to a site of placed characters and waits for the cells (and the boxes of the add-ons) to settle
    local function go(site, label)
        r.teleport{ pos = { site.x, site.y, site.z }, yaw = 0, onGround = true, label = label }
        r.wait(12)
        r.settle(3, 30)
        r.eye(label)
    end

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    r.eye('start')
    r.mode('start')
    r.status('start')
    r.inventory('start')
    r.weapon('start')
    r.activate(DOOR_OUT, 'door out')
    r.waitFor(exterior, 40, 'exterior')
    r.wait(10)
    r.eye('outside')
    r.mode('outside')
    r.actors('near', 3000, 8)
    r.inventory('outside')
    r.equip(nil, 1)
    r.weapon('equipped')
    r.camera('FirstPerson')
    r.look(0, 0)
    r.wait(2)
    r.fps('baseline idle, nothing sampled', 3)
    r.track({ { label = 'player', actor = player } }, 0.25)

    if RUN.A then
    ---------------------------------------------------------------- A: first person
    phase('A first person')
    r.shot('A0_fp_holstered')
    r.drawn(true)
    r.wait(0.3); r.shot('A1_fp_draw_0.3s', false)
    r.wait(1.5); r.shot('A2_fp_drawn_idle', false)
    r.look(0, 0.1)
    r.wait(0.5); r.shot('A3_fp_aiming', false)
    for i = 1, 4 do
        r.fire('A_first_' .. i)
        r.shot('A4_fp_fire' .. i, false)
        r.wait(0.8)
    end
    r.log('A reload: no reload call exists in core.combat or in the player scripts (nothing to ask for)')
    r.use(true)         -- the engine's own attack control, as a mouse press would give it
    r.wait(0.4); r.shot('A5_fp_use_control', false)
    r.use(false)
    r.wait(1)
    r.equip(nil, 2)     -- the second ranged weapon with rounds, when there is one
    r.weapon('second')
    r.wait(1.5); r.shot('A6_fp_second_weapon', false)
    r.fire('A_second_1')
    r.shot('A7_fp_second_fire', false)
    r.wait(0.8)
    r.drawn(false)
    r.wait(1); r.shot('A8_fp_holstered_again', false)
    r.untrack()
    r.fps('A first person, not sampled', 4)
    r.track({ { label = 'player', actor = player } }, 0.25)

    end
    if RUN.B then
    ---------------------------------------------------------------- B: third person
    phase('B third person')
    r.equip(nil, 1)
    r.camera('ThirdPerson')
    r.wait(1.5)
    r.mode('third')
    r.shot('B0_tp_idle')
    r.drawn(true)
    r.wait(1.5); r.shot('B1_tp_drawn', false)
    r.fire('B_fire_1'); r.shot('B2_tp_fire', false)
    r.wait(1)
    r.move{ forward = 1, seconds = 3 }
    r.wait(1.5); r.shot('B3_tp_walk_drawn', false); r.mode('walk drawn')
    r.wait(2)
    r.move{ forward = 1, run = true, seconds = 3 }
    r.wait(1.5); r.shot('B4_tp_run_drawn', false); r.mode('run drawn')
    r.wait(2)
    r.move{ forward = 1, sneak = true, seconds = 3 }
    r.wait(1.5); r.shot('B5_tp_sneak_drawn', false); r.mode('sneak drawn')
    r.wait(2)
    r.crouch(true)
    r.wait(1.5); r.shot('B6_tp_crouch_idle_drawn', false); r.mode('crouch idle')
    r.crouch(false)
    r.wait(1)
    r.drawn(false)
    r.wait(1)
    r.shot('B7_tp_holstered_idle', false)
    r.move{ forward = 1, seconds = 3 }
    r.wait(1.5); r.shot('B8_tp_walk_holstered', false); r.mode('walk holstered')
    r.wait(2)
    r.move{ forward = 1, run = true, seconds = 3 }
    r.wait(1.5); r.shot('B9_tp_run_holstered', false); r.mode('run holstered')
    r.wait(2)
    r.move{ forward = 1, sneak = true, seconds = 3 }
    r.wait(1.5); r.shot('B10_tp_sneak_holstered', false); r.mode('sneak holstered')
    r.wait(2)
    r.move{ forward = -1, seconds = 2 }
    r.wait(2.5)
    r.move{ side = 1, seconds = 2 }
    r.wait(2.5)
    r.move{ forward = 1, seconds = 0.4 }  -- a short step
    r.wait(0.5)
    r.untrack()
    r.fps('B third person, not sampled', 4)

    end
    if RUN.C then
    ---------------------------------------------------------------- C: a fight
    phase('C fight')
    r.drawn(true)
    r.camera('ThirdPerson')
    local here = player.position
    r.teleport{ pos = { here.x, here.y, here.z }, yaw = 0, label = 'face north', onGround = true }
    r.wait(1)
    go(SITE_X, 'site X')
    local g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then
        error('C the placed gangers were not found: there is nothing to fight, and a round with no fight must not pass')
    end
    -- they are taken to the open ground (their camp is a rocky slope) before the player goes there
    r.putAt(g1, { pos = { ARENA_C.x - 70, ARENA_C.y + 20 * M, ARENA_C.z }, yaw = math.pi, label = 'ganger1 to the arena' })
    r.putAt(g2, { pos = { ARENA_C.x + 170, ARENA_C.y + 20 * M, ARENA_C.z }, yaw = math.pi, label = 'ganger2 to the arena' })
    r.teleport{ pos = { ARENA_C.x, ARENA_C.y, ARENA_C.z }, yaw = 0, onGround = true, label = 'to the arena' }
    r.wait(10)
    r.settle(3, 30)
    r.eye('arena')
    r.camera('FirstPerson')
    r.look(0, 0.03)
    r.wait(2)
    r.inspect('ganger1', g1)
    r.inspect('ganger2', g2)
    r.track({ { label = 'player', actor = player }, { label = 'g1', actor = g1 }, { label = 'g2', actor = g2 } }, 0.5)
    r.wait(2)
    r.shot('C0_before_fight_20m')
    r.untrack()
    r.fps('C 20 m, before the fight, not sampled', 3)
    r.track({ { label = 'player', actor = player }, { label = 'g1', actor = g1 }, { label = 'g2', actor = g2 } }, 0.5)
    -- god mode is off while the characters shoot, to see what they do to the player (a shotgun took the player from 100 to nothing in
    -- under five seconds in an earlier run of this scenario); it comes on as soon as the health is under 60, which is polled every 0.25 s
    local function guard()
        local hp = r.hp()
        if hp and hp < 60 and not guarded then
            guarded = true
            r.god(true)
            r.log('C the player health fell to ' .. string.format('%.0f', hp) .. ': god mode on again')
        end
        return hp
    end
    local function fighting(o)
        local ok, v = pcall(r.core.combat.inCombat, r.form(o))
        return ok and v
    end
    r.log('C player health before the fight: ' .. tostring(r.hp()))
    r.god(false)
    r.fight(g1, player, 'g1 -> player')
    r.fight(g2, player, 'g2 -> player')
    for i = 1, 5 do
        for _ = 1, 6 do r.wait(0.25); guard() end
        r.shot('C1_fight_' .. i, false)
        r.state('g1', g1); r.state('g2', g2)
    end
    if not (fighting(g1) or fighting(g2)) then
        -- the engine dropped the fight that was started (the state said inCombat at once and false half a second later): the player
        -- hits the first one with a shot, as the games' own fights begin, and the survey goes on with what the engine does then
        r.log('C the started fight did not hold (inCombat false for both after 7 s): the player fires at g1 directly')
        r.aimAt(g1, 'g1')
        r.fire('C_g1_first_hit', g1)
        r.shot('C1_first_hit', false)
        for i = 6, 14 do
            for _ = 1, 6 do r.wait(0.25); guard() end
            r.shot('C1_fight_' .. i, false)
            r.state('g1', g1); r.state('g2', g2)
            r.log('C player health: ' .. tostring(r.hp()))
            if i == 8 or i == 12 then
                r.untrack()
                r.fps('C fight ' .. i .. ', not sampled', 3)
                r.track({ { label = 'player', actor = player }, { label = 'g1', actor = g1 }, { label = 'g2', actor = g2 } }, 0.5)
            end
        end
    end
    if not guarded then r.god(true) end   -- it is on from here (nothing hurt the player enough to switch it back on)
    r.log('C player health after the shooting at it: ' .. tostring(r.hp()))
    r.actors('during fight', 3000, 8)
    -- the player shoots them: the camera is turned at the first, the shot goes where the camera ray hits
    local function shootAt(target, label, tag)
        r.log('C player shoots ' .. label)
        local effective = 0
        local missed = 0
        for i = 1, 30 do
            r.aimAt(target, label)
            -- what the camera's ray hits; after a shot that did no damage the next one goes at the character itself (round 14F: the
            -- ray went past the character 26 times of 28, so the one that did damage was followed by shots that did none)
            local row = r.fire('C_' .. label .. '_shot_' .. i, (missed > 0) and target or nil)
            if row and row.hit and (row.damage or 0) > 0 then
                effective = effective + 1
                missed = 0
            else
                missed = missed + 1
            end
            if i % 3 == 1 then r.shot(tag .. i, false) end
            r.wait(0.5)
            local ok, dead = pcall(r.core.combat.isDead, r.form(target))
            if ok and dead then break end
            guard()
        end
        r.log('C shots on ' .. label .. ' that did damage: ' .. effective)
    end
    shootAt(g1, 'g1', 'C2_shoot_g1_')
    r.wait(1.5); r.shot('C3_g1_after', false)
    shootAt(g2, 'g2', 'C4_shoot_g2_')
    r.wait(2)
    for i = 1, 3 do r.wait(1); r.shot('C5_after_' .. i, false) end
    r.state('g1 final', g1); r.state('g2 final', g2)
    r.inspect('g1 corpse', g1)
    r.inspect('g2 corpse', g2)
    r.actors('after fight', 3000, 8)
    r.god(true)
    r.log('C player health after the fight: ' .. tostring(r.hp()))
    -- close pictures of what is left
    local function closeup(target, label, tag)
        local p = target.position
        r.teleport{ pos = { p.x, p.y - 4 * M, p.z }, yaw = 0, onGround = true, label = 'closeup ' .. label }
        r.wait(1.5)
        r.aimAt(target, label, 60)
        r.wait(0.5)
        r.shot(tag, false)
        r.state(label, target)
    end
    closeup(g1, 'g1 corpse', 'C6_g1_4m')
    closeup(g2, 'g2', 'C6_g2_4m')
    r.untrack()
    r.fps('C after the fight, not sampled', 3)

    end
    if RUN.E then
    ---------------------------------------------------------------- E: detection and sneak
    phase('E detection')
    r.drawn(false)
    r.camera('ThirdPerson')
    r.untrack()
    go(SITE_Y, 'site Y')
    e1 = lookup(SITE_Y.gun1)
    if e1 == nil then
        error('E the placed ganger was not found: there is nothing to sneak up on, and a round with no ganger must not pass')
    end
    -- the data has the Powder Gangers neutral to the player (round 14F: "does not attack the player on sight"), so the two factions are made
    -- enemies, as a quest of the game does, and the character that notices the player goes for it
    r.enemy('FormId:0x1101f42', 'FormId:0x101b2a4', 'Powder Gangers and the player')
    r.crouch(true)
    r.teleport{ pos = { ARENA_E.x, ARENA_E.y, ARENA_E.z }, yaw = 0, onGround = true, label = 'to the arena, crouched' }
    r.wait(6)
    r.settle(3, 30)
    r.eye('arena e')
    r.track({ { label = 'player', actor = player }, { label = 'e1', actor = e1 } }, 0.5)
    -- two passes: the character faces away from the player, then toward the player; crouched at 15, 8 and 3 m
    local function pass(name, npcYaw)
        r.putAt(e1, { pos = { ARENA_E.x, ARENA_E.y + 40 * M, ARENA_E.z }, yaw = npcYaw, label = name })
        r.teleport{ pos = { ARENA_E.x, ARENA_E.y, ARENA_E.z }, yaw = 0, onGround = true, label = 'back at the start of ' .. name }
        r.wait(4)
        for _, d in ipairs({ 15, 8, 3 }) do
            local e = e1.position
            r.teleport{ pos = { e.x, e.y - d * M, e.z }, yaw = 0, label = name .. ' crouched at ' .. d .. ' m', onGround = true }
            r.wait(0.8)
            r.status(name .. ' ' .. d .. 'm 1')
            r.actorState(name .. ' e1 ' .. d, e1)
            r.shot(name .. '_' .. d .. 'm_a')
            r.wait(2.5)
            r.status(name .. ' ' .. d .. 'm 2')
            r.actorState(name .. ' e1 ' .. d, e1)
            r.shot(name .. '_' .. d .. 'm_b', false)
            r.wait(2.5)
            r.status(name .. ' ' .. d .. 'm 3')
            r.actorState(name .. ' e1 ' .. d, e1)
        end
    end
    pass('E1_away', 0)
    pass('E2_facing', math.pi)
    r.crouch(false)
    r.wait(2)
    r.status('E standing at 3 m')
    r.actorState('E e1 standing', e1)
    r.shot('E_standing_3m', false)
    -- the stealth lines come every three seconds: the player that stands (not hidden by sneaking) is looked at twice, and the character
    -- that sees it goes for it (round 14F: crouched with a high Sneak the player was never seen, standing nothing was said)
    r.wait(4)
    r.actorState('E e1 standing, later', e1)
    r.shot('E_standing_3m_later', false)
    r.wait(4)
    r.actorState('E e1 standing, much later', e1)
    end
    if RUN.D then
    ---------------------------------------------------------------- D: V.A.T.S.
    phase('D vats')
    r.drawn(true)
    r.camera('FirstPerson')
    local here2 = player.position
    -- the target stands 10 m from the player in the first direction in which nothing is between its head and the camera
    for _, bearing in ipairs({ 0.0, math.pi / 2, math.pi, 3 * math.pi / 2 }) do
        r.place(e1, { dist = 10 * M, bearing = bearing, label = 'vats target' })
        r.wait(2)
        r.aimAt(e1, 'v1')
        r.wait(0.5)
        local probe = r.vatsInfo('probe bearing ' .. string.format('%.2f', bearing), e1)
        if probe and probe.clear then break end
    end
    r.wait(1)
    r.shot('D0_before_vats')
    r.eye('before vats')
    r.vatsInfo('before opening', e1)
    r.vats()
    r.wait(1.5)
    r.messages('after the open event')
    r.vatsInfo('after opening', e1)
    r.mode('vats open')
    r.eye('vats open')
    r.shot('D1_vats_open', false)
    r.wait(1)
    r.shot('D1_vats_open_b', false)
    r.vats('Y')
    r.wait(1); r.shot('D2_vats_queued', false)
    r.vats('X')
    for i = 1, 8 do
        r.wait(0.6)
        r.shot('D3_vats_attack_' .. i, false)
        r.eye('vats attack ' .. i)
        r.mode('vats attack ' .. i)
    end
    r.wait(2)
    r.mode('vats after')
    r.eye('vats after')
    r.shot('D4_vats_after', false)
    r.state('v1', e1)
    r.inspect('v1', e1)
    r.untrack()
    end
    if RUN.F then
    ---------------------------------------------------------------- F: a fight that starts out of the engage distance
    -- round 14H: in C the characters stood within their engage distance (about 1,400 units for a rifle) and held; here the ganger that
    -- noticed the player is put 35 m away (2,450 units) and told to fight, to see whether it walks up to the distance it shoots from
    phase('F far fight')
    r.drawn(false)
    r.camera('ThirdPerson')
    r.teleport{ pos = { ARENA_C.x, ARENA_C.y, ARENA_C.z }, yaw = 0, onGround = true, label = 'F to the arena' }
    r.wait(3)
    r.putAt(e1, { pos = { ARENA_C.x, ARENA_C.y + 35 * M, ARENA_C.z }, yaw = math.pi, label = 'e1 35 m north' })
    r.wait(2)
    r.track({ { label = 'player', actor = player }, { label = 'e1', actor = e1 } }, 0.5)
    r.fight(e1, player, 'e1 -> player, far')
    for i = 1, 12 do
        r.wait(1.5)
        r.actorState('F e1 ' .. i, e1)
        if i % 3 == 1 then r.shot('F_far_' .. i, false) end
    end
    r.untrack()
    end
    phase('done')
end }
