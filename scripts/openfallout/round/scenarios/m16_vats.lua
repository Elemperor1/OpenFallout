-- M16 vats (New Vegas): the V.A.T.S. values and queries, the mines, and the reticle. Phases: S setup, Q the queries while V.A.T.S. is
-- closed (all values 0, the areas around the player, the threat), T SetVATSTarget turns a character round, V an unarmed attack in
-- V.A.T.S. and the values it plays, M a mine is laid, set off by a character, and another is disabled first. Numbers and ids only.
local M = 70
local GAME = 'FalloutNV.esm'
local DOOR_OUT = { GAME, 0x103e61 }
local ARENA = { x = -69660, y = 1900, z = 8352 }
local SITE_X = { x = -21860, y = -38900, z = 8100, gun1 = 0x1629a4, gun2 = 0x1629a3 }
-- the frag mine (a weapon of both games, the animation type of a land mine)
local MINE = { GAME, 0x433c }

return { limit = 900, delay = 4, run = function(r)
    local function phase(name) r.log('PHASE ' .. name) end
    local player = r.player()
    local cmd = r.core.combat.command
    local pid = r.core.combat.player()
    local function lookup(id)
        local ok, object = pcall(r.lookup, { GAME, id })
        local ok2, text = false, nil
        if ok and object ~= nil then ok2, text = pcall(r.describe, object) end
        r.log(string.format('ref 0x%x %s', id, ok2 and text or ('not available: ' .. tostring(text or object))))
        return ok2 and object or nil
    end
    local pass, fail = 0, 0
    local function check(name, ok, detail)
        if ok then pass = pass + 1 else fail = fail + 1 end
        r.log(string.format('%s %s%s', ok and 'PASS' or 'FAIL', name, detail and (' (' .. tostring(detail) .. ')') or ''))
    end
    local function nonzero(label)
        local list = {}
        for i = 0, 17 do
            local v = cmd('GetVATSValue', pid, i, 0)
            if v ~= 0 then list[#list + 1] = i .. '=' .. tostring(v) end
        end
        r.log(label .. ' non-zero GetVATSValue indexes: [' .. table.concat(list, ',') .. ']')
        return list
    end

    ---------------------------------------------------------------- S: setup
    phase('S setup')
    r.god(true)
    local function exterior() return player.cell ~= nil and player.cell.isExterior end
    if not exterior() then
        r.activate(DOOR_OUT, 'door out')
        r.waitFor(exterior, 40, 'exterior')
        r.wait(8)
    end
    r.camera('ThirdPerson')
    local g1, g2 = nil, nil
    r.teleport{ pos = { SITE_X.x, SITE_X.y, SITE_X.z }, yaw = 0, onGround = true, label = 'site X' }
    r.wait(12)
    r.settle(3, 30)
    g1, g2 = lookup(SITE_X.gun1), lookup(SITE_X.gun2)
    if g1 == nil or g2 == nil then error('S the placed characters were not found: a round with nobody to test on must not pass') end
    r.peace(g1)
    r.peace(g2)
    -- the player goes to a character and stands `d` units south of it, looking north: a mine laid at 70 units lands under it at 70
    local function near(who, d, label)
        r.teleport{ pos = { who.position.x, who.position.y - d, who.position.z + 10 }, yaw = 0, onGround = true, label = label }
        r.wait(2)
    end
    near(g1, 50, 'the player next to g1')
    r.settle(3, 30)

    ---------------------------------------------------------------- Q: the queries with V.A.T.S. closed
    phase('Q queries')
    local open = nonzero('Q closed')
    check('Q every value is 0 while V.A.T.S. is closed', #open == 0, table.concat(open, ','))
    for _, side in ipairs({ 'Front', 'Back', 'Left', 'Right' }) do
        local free = cmd('GetVATS' .. side .. 'AreaFree', pid)
        r.log(string.format('Q GetVATS%sAreaFree = %s', side, tostring(free)))
        check('Q the free area of the ' .. side .. ' is a number of 0 or more', type(free) == 'number' and free >= 0)
    end
    near(g1, 300, 'the player 300 units from g1')
    for _, side in ipairs({ 'Front', 'Back', 'Left', 'Right' }) do
        r.log(string.format('Q GetVATS%sTargetVisible(g1) = %s', side, tostring(cmd('GetVATS' .. side .. 'TargetVisible', pid, r.form(g1)))))
    end
    local ratio = cmd('GetThreatRatio', pid, r.form(g1))
    r.log('Q GetThreatRatio(player, g1) = ' .. tostring(ratio))
    check('Q the threat ratio is a number above 0', type(ratio) == 'number' and ratio > 0)
    r.log('Q IsPlayerGrabbedRef(g1) = ' .. tostring(cmd('IsPlayerGrabbedRef', pid, r.form(g1))) .. ', GetPlayerGrabbedRef = ' ..
        tostring(cmd('GetPlayerGrabbedRef', pid)))
    r.log('Q GetVatsTargetHeight = ' .. tostring(cmd('GetVatsTargetHeight', pid)))

    ---------------------------------------------------------------- T: SetVATSTarget
    phase('T targets')
    near(g1, 50, 'the player at arm length from g1')
    do
        local d = r.core.vats.describe(g1)
        r.log(string.format('T describe(g1): name "%s", distance %s, health %s of %s; GetDistance %s, GetDead %s, g1 at (%.0f %.0f %.0f), player at (%.0f %.0f %.0f)',
            tostring(d.name), tostring(d.distance), tostring(d.health), tostring(d.maxHealth), tostring(cmd('GetDistance', pid, r.form(g1))),
            tostring(cmd('GetDead', r.form(g1))), g1.position.x, g1.position.y, g1.position.z, player.position.x, player.position.y, player.position.z))
        r.log('T canTarget(g2) = ' .. tostring(r.core.vats.canTarget(g2)))
    end
    check('T g1 is a target at arm length', r.core.vats.canTarget(g1) == true)
    cmd('SetVATSTarget', r.form(g1), 0)
    r.wait(0.5)
    check('T g1 is no target after SetVATSTarget 0', r.core.vats.canTarget(g1) == false)
    cmd('SetVATSTarget', r.form(g1), 1)
    r.wait(0.5)
    check('T g1 is a target again after SetVATSTarget 1', r.core.vats.canTarget(g1) == true)

    ---------------------------------------------------------------- V: an attack in V.A.T.S.
    phase('V vats')
    r.core.vats.setOpen(true)
    local state = r.core.vats.state()
    r.log(string.format('V state: usable %s, AP %s of %s, cost %s, range %s, weapon "%s"', tostring(state.usable),
        tostring(state.actionPoints), tostring(state.maxActionPoints), tostring(state.cost), tostring(state.range), tostring(state.weapon)))
    local parts = r.core.vats.parts(g1)
    r.log('V parts of g1: ' .. #parts)
    for _, part in ipairs(parts) do
        r.log(string.format('V part %d %s group %s chance %.1f', part.index, tostring(part.name), tostring(part.group), part.chance))
    end
    check('V g1 has parts to aim at', #parts > 0)
    local aimed = parts[1]
    for _, part in ipairs(parts) do if part.group == 25 then aimed = part end end
    if aimed ~= nil then
        local result = r.core.vats.attack(g1, aimed.index)
        r.log(string.format('V attack on part %d: refused %s, hit %s, chance %.1f, damage %s, killed %s', aimed.index,
            tostring(result.refused), tostring(result.hit), result.chance or 0, tostring(result.damage), tostring(result.killed)))
        -- (the four checks below are about an attack that played: one that was refused is a failure of its own)
        check('V the V.A.T.S. attack was accepted', result.refused == nil, tostring(result.refused))
        if result.refused == nil then
            local seen = nonzero('V playing')
            r.log(string.format('V playing: GetVATSValue 7 (hits) = %s, 11 (fatal) = %s, 5 with the part group = %s, 15 with 0 = %s',
                tostring(cmd('GetVATSValue', pid, 7, 0)), tostring(cmd('GetVATSValue', pid, 11, 0)),
                tostring(cmd('GetVATSValue', pid, 5, aimed.group or 0)), tostring(cmd('GetVATSValue', pid, 15, 0))))
            check('V the hit value is the result', (cmd('GetVATSValue', pid, 7, 0) == 1) == (result.hit == true))
            check('V the fatal value is the result', (cmd('GetVATSValue', pid, 11, 0) == 1) == (result.killed == true))
            check('V the part asked for is the part aimed at', cmd('GetVATSValue', pid, 5, aimed.group or 0) == 1)
            check('V something is non-zero while an attack plays', #seen > 0)
            r.log('V GetVatsTargetHeight while it plays = ' .. tostring(cmd('GetVatsTargetHeight', pid)))
        end
    end
    r.core.vats.setOpen(false)
    local after = nonzero('V closed again')
    check('V every value is 0 again when V.A.T.S. closes', #after == 0, table.concat(after, ','))
    if r.core.combat.isDead(r.form(g1)) then cmd('ResurrectActor', r.form(g1), 1) r.wait(1) end

    ---------------------------------------------------------------- M: mines
    phase('M mines')
    local mineForm = r.core.getFormId(MINE[1], MINE[2])
    local function health(who) return cmd('GetActorValue', r.form(who), 16) end
    -- a mine is laid 70 units in front of the player, which is where g1 stands: g1 sets it off and loses health to it
    near(g1, 70, 'the player 70 units from g1')
    local hp0 = health(g1)
    r.core.combat.fireWeapon(pid, mineForm)
    r.wait(4)
    local hp1 = health(g1)
    r.log(string.format('M g1 health %s -> %s, dead %s', tostring(hp0), tostring(hp1), tostring(r.core.combat.isDead(r.form(g1)))))
    check('M g1 lost health to the mine', type(hp0) == 'number' and type(hp1) == 'number' and hp1 < hp0)
    if r.core.combat.isDead(r.form(g1)) then cmd('ResurrectActor', r.form(g1), 1) r.wait(1) end
    -- a second mine is laid away from anyone, disabled with DisableAllMines, and g2 is put on it
    near(g2, 1500, 'the player 1500 units from g2')
    local spot = { x = player.position.x, y = player.position.y + 70, z = player.position.z }
    local hq0 = health(g2)
    -- (the mine lies 70 units from its owner, who can set it off: it is disabled in the same step that lays it, with no frame
    -- between the two, so that it is switched off before anything can set it off. A wait between them is a few frames, and the
    -- least delay a mine has between being set off and going off, 0.2 s, is not a limit that a wait can stay under.)
    r.core.combat.fireWeapon(pid, mineForm)
    cmd('DisableAllMines', pid)
    r.wait(0.5)
    r.putAt(g2, { pos = { spot.x, spot.y, spot.z }, yaw = math.pi, label = 'g2 on the disabled mine' })
    r.wait(4)
    local hq1 = health(g2)
    r.log(string.format('M g2 health %s -> %s', tostring(hq0), tostring(hq1)))
    check('M g2 keeps its health on a disabled mine', type(hq0) == 'number' and type(hq1) == 'number' and hq1 >= hq0)
    r.log('M GetPlantedExplosive(player) = ' .. tostring(cmd('GetPlantedExplosive', pid)))

    ---------------------------------------------------------------- D: plastic explosive and DetonatePlacedExplosives
    phase('D detonate')
    -- the plastic explosive (a thrown mine whose projectile has the detonate flag and no alt trigger): it lies until a script says so
    local charge = r.core.getFormId(GAME, 0x1221c1)
    if r.core.combat.isDead(r.form(g2)) then cmd('ResurrectActor', r.form(g2), 1) r.wait(1) end
    near(g2, 70, 'the player 70 units from g2')
    r.core.combat.fireWeapon(pid, charge)
    r.wait(4)
    local hd0 = health(g2)
    r.log('D g2 health with the charge lying and nobody having said so: ' .. tostring(hd0))
    cmd('DetonatePlacedExplosives', pid)
    r.wait(2)
    local hd1 = health(g2)
    r.log(string.format('D g2 health %s -> %s, dead %s', tostring(hd0), tostring(hd1), tostring(r.core.combat.isDead(r.form(g2)))))
    check('D the charge hurt g2 only after DetonatePlacedExplosives', type(hd0) == 'number' and type(hd1) == 'number' and hd1 < hd0)

    ---------------------------------------------------------------- R: the reticle
    phase('R reticle')
    r.log('R ObjectUnderReticle = ' .. tostring(cmd('ObjectUnderReticle', pid)))
    r.log('R PlaceAtReticle of nothing = ' .. tostring(cmd('PlaceAtReticle', pid, 0)))
    r.log(string.format('RESULT %d passed, %d failed', pass, fail))
    if fail > 0 then error(string.format('%d checks failed', fail)) end
end }
