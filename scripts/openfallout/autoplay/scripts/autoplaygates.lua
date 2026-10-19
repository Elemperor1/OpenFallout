-- The places where the intro of Fallout 3 and New Vegas waits for the player to do something that pressing a button does not
-- do: walk into a trigger volume, activate an object or shoot at one. autoplayworld.lua reads this table. When a quest has
-- stood at a stage for `settle` real seconds (nothing else changed in it), the gates of that stage are tried, one at a time:
-- 'walk' puts the player at the reference (the middle of its volume), 'activate' makes the player activate it (or each of the
-- `refs`), 'fire' makes the player shoot at it with the weapon in hand (`shots` times, or, with `untilDead`, until it is
-- dead; autoplay.lua does the shooting), 'teleport' takes the player to the cell `cell` of the plugin at the place `at` (for
-- a room the data has no door to), 'setstage' puts the quest at the stage `to` (a stand-in for what the engine cannot bring
-- about yet). A gate is tried `tries` times at most (its own `tries` if it has one: activating a piece of furniture a second
-- time gets the player up from it). A gate can have its own `settle` (the quest has to stand still that long first) and its own
-- `gap` (the real seconds since the gate before it): the stand-ins that come after the gate that is meant to bring the stage
-- about are given a minute, so that what the engine does by itself is not covered up. The log says which gates were used
-- ("AUTOPLAY gate setstage ..."): each stand-in that is used is something the engine lacks.
-- `boxes` names the button that autoplay.lua presses in a message box (a negative number counts from the last button); any
-- other box gets the first.
-- The references are the ones the Mac found in the base plugins (static survey, rounds 7 and 8); `ref` is the form id without
-- the number of the plugin in the load order (the plugin itself is `file`). The stages are those the quest waits at.
return {
    settle = 20,
    tries = 2,
    boxes = {
        -- CG04VaultExitMessage, at the door out of Vault 101: five buttons, and the last one leaves (the others stay)
        { file = 'Fallout3.esm', ref = 0x05CF94, button = -1 },
    },
    gates = {
        -- Fallout 3, Vault 101 (cell 0x028138): the Dad's trigger (a sphere, sets stage 12 for the player); the way out of the
        -- crib (any actor, sets 40); the baby book (OnActivate, gives SPECIAL; activated while the stage is 30 or more); the
        -- trigger at the end of the quest (sets 90 when the stage is 80, and the stage then goes to 100); the gate of the
        -- playpen (a door, OnActivate sets 30 when the stage is 20, which the Dad's packages bring about: the stage is 14
        -- until he has closed the gate himself).
        { quest = 'CG01', stage = 10, file = 'Fallout3.esm', ref = 0x02EA54, how = 'walk' },
        { quest = 'CG01', stage = 20, file = 'Fallout3.esm', ref = 0x047803, how = 'activate' },
        { quest = 'CG01', stage = 30, file = 'Fallout3.esm', ref = 0x02EA53, how = 'walk' },
        { quest = 'CG01', stage = 40, file = 'Fallout3.esm', ref = 0x02ECC0, how = 'activate' },
        { quest = 'CG01', stage = 80, file = 'Fallout3.esm', ref = 0x02ECC2, how = 'walk' },
        -- Vault 101, the birthday party (CG02; rounds 13 and 14, static): the quest stands at stage 10 until the Dad's Say To
        -- package gives 12, and goes on to 15 when the player is in the trigger at the cake or Old Lady Palmer greets the player.
        -- The gifts (Palmer, Amata, Stanley) bring 34; the intercom (35) and the talks with the Dad and Jonas take it to 44.
        -- 50 to 70 are the shooting range and the radroach: the result of 44 puts the BB gun and 50 rounds in the hands of the
        -- player; the script of each of the three targets counts every blow it hears of (any weapon) and sets 55 at the third,
        -- which enables the radroach; its script sets 70 when it dies (a BB gun does 4 and the radroach has 5 health). The
        -- player shoots at the first target and then at the radroach (Mac round 7: both stages came by themselves, 3 shots and 3
        -- shots, so there is no stand-in after them).
        { quest = 'CG02', stage = 10, file = 'Fallout3.esm', ref = 0x09F64F, how = 'walk' },
        { quest = 'CG02', stage = 12, file = 'Fallout3.esm', ref = 0x09F64F, how = 'walk' },
        { quest = 'CG02', stage = 15, file = 'Fallout3.esm', ref = 0x0309A8, how = 'activate' },
        { quest = 'CG02', stage = 20, file = 'Fallout3.esm', ref = 0x0300F2, how = 'activate' },
        { quest = 'CG02', stage = 21, file = 'Fallout3.esm', ref = 0x031649, how = 'activate' },
        { quest = 'CG02', stage = 35, file = 'Fallout3.esm', ref = 0x031D48, how = 'activate', tries = 1 },
        { quest = 'CG02', stage = 35, how = 'setstage', to = 38, settle = 60 }, -- the intercom, if its talk does not do it
        { quest = 'CG02', stage = 38, file = 'Fallout3.esm', ref = 0x0300EF, how = 'activate' },
        { quest = 'CG02', stage = 40, file = 'Fallout3.esm', ref = 0x0300E8, how = 'activate' },
        { quest = 'CG02', stage = 42, file = 'Fallout3.esm', ref = 0x0300EF, how = 'activate' },
        { quest = 'CG02', stage = 44, file = 'Fallout3.esm', ref = 0x0300EF, how = 'activate' },
        { quest = 'CG02', stage = 44, how = 'setstage', to = 50, settle = 60 }, -- the Dad's walk to the range, if he does not
        { quest = 'CG02', stage = 50, file = 'Fallout3.esm', ref = 0x0304EB, how = 'fire', shots = 3, tries = 1 },
        { quest = 'CG02', stage = 55, file = 'Fallout3.esm', ref = 0x0300EF, how = 'activate' },
        { quest = 'CG02', stage = 60, file = 'Fallout3.esm', ref = 0x0306CD, how = 'fire', shots = 20, untilDead = true,
            tries = 1 },
        { quest = 'CG02', stage = 70, file = 'Fallout3.esm', ref = 0x0300EF, how = 'activate' },
        { quest = 'CG02', stage = 80, file = 'Fallout3.esm', ref = 0x060C81, how = 'walk' },
        -- The classroom (CG03): the Dad, the trigger by the door, Amata, Mr. Brotch (who offers the test, which needs the player
        -- to sit at a desk: the stand-in at 42 takes the way of the player who skips it), the triggers by the desks and the door
        { quest = 'CG03', stage = 6, file = 'Fallout3.esm', ref = 0x01CE3A, how = 'activate' },
        { quest = 'CG03', stage = 10, file = 'Fallout3.esm', ref = 0x057AEB, how = 'walk' },
        { quest = 'CG03', stage = 15, file = 'Fallout3.esm', ref = 0x01CE25, how = 'activate' },
        { quest = 'CG03', stage = 20, how = 'setstage', to = 25, settle = 60 }, -- the talk of Amata and Butch, if it stops
        { quest = 'CG03', stage = 25, file = 'Fallout3.esm', ref = 0x01CFB8, how = 'activate' },
        { quest = 'CG03', stage = 42, how = 'setstage', to = 45 },
        { quest = 'CG03', stage = 45, file = 'Fallout3.esm', ref = 0x02B799, how = 'walk' },
        { quest = 'CG03', stage = 55, file = 'Fallout3.esm', ref = 0x02B799, how = 'walk' },
        -- The script of the classroom trigger waits for the seven classmates to be sitting, which their packages of the type Find
        -- bring about (the engine does not follow those yet)
        { quest = 'CG03', stage = 55, how = 'setstage', to = 60, settle = 60 },
        { quest = 'CG03', stage = 70, file = 'Fallout3.esm', ref = 0x01CFB8, how = 'activate', settle = 90 }, -- Mr. Brotch again
        { quest = 'CG03', stage = 80, file = 'Fallout3.esm', ref = 0x02B797, how = 'walk' },
        -- The escape (CG04): Amata, the trigger in the corridor, the radroaches (a fight: the stand-in), Butch, the trigger in
        -- the atrium, the Overseer, Amata, the trigger by the reactor, the three switches, the control pod, Amata, and the door
        -- out, whose box needs its last button (boxes above)
        { quest = 'CG04', stage = 3, file = 'Fallout3.esm', ref = 0x0230E4, how = 'activate' },
        { quest = 'CG04', stage = 10, file = 'Fallout3.esm', ref = 0x02A325, how = 'walk' },
        { quest = 'CG04', stage = 12, how = 'setstage', to = 15 },
        { quest = 'CG04', stage = 15, file = 'Fallout3.esm', ref = 0x02A325, how = 'walk' },
        { quest = 'CG04', stage = 18, file = 'Fallout3.esm', ref = 0x0230E3, how = 'activate' },
        { quest = 'CG04', stage = 20, file = 'Fallout3.esm', ref = 0x0230E3, how = 'activate' },
        -- The stop boxes of the radroaches are in the atrium (cell 0x024511), and Lua reaches the objects of the loaded cells
        -- only: the player goes through the door of the vault (activate) at 21 and then walks into the box at 33
        { quest = 'CG04', stage = 21, file = 'Fallout3.esm', ref = 0x054285, how = 'activate', tries = 2 },
        { quest = 'CG04', stage = 33, file = 'Fallout3.esm', ref = 0x02D4B6, how = 'walk', tries = 2 },
        { quest = 'CG04', stage = 35, file = 'Fallout3.esm', ref = 0x0230E1, how = 'activate' },
        { quest = 'CG04', stage = 38, file = 'Fallout3.esm', ref = 0x0230E4, how = 'activate' },
        { quest = 'CG04', stage = 50, file = 'Fallout3.esm', ref = 0x057E98, how = 'walk' },
        -- The switches in the reactor room (cell 0x02A186, which no door of the data leads to: the player is taken there): the
        -- script of each switch sets 110 and counts one more, and at the third sets 120 (GetOpenState is 3 for an object that
        -- is no door), so the three are activated at once and both stages come in the same frame (Mac round 7: no stand-in
        -- was needed). The player is taken back to the atrium (by the overseer's terminal, not into the trigger of the office,
        -- whose script sets 60 again) before the guards that 110 sends there get to the player; the door in the atrium that
        -- leads to the entrance, where the control pod is, is used by the player first: the pod is in another cell
        { quest = 'CG04', stage = 60, file = 'Fallout3.esm', cell = 0x02A186, at = { -1351.0, -3092.4, 7954.9 }, how = 'teleport',
            tries = 1 },
        { quest = 'CG04', stage = 60, file = 'Fallout3.esm', refs = { 0x02987C, 0x0660C0, 0x0660C1 }, how = 'activate',
            gap = 5 },
        { quest = 'CG04', stage = 120, file = 'Fallout3.esm', cell = 0x024511, at = { -4640.0, -508.0, 8488.0 },
            how = 'teleport', tries = 1, settle = 2, gap = 2 },
        { quest = 'CG04', stage = 120, file = 'Fallout3.esm', ref = 0x05398D, how = 'activate', tries = 1, gap = 5 },
        { quest = 'CG04', stage = 120, file = 'Fallout3.esm', ref = 0x02987D, how = 'activate' },
        { quest = 'CG04', stage = 142, file = 'Fallout3.esm', ref = 0x0230E4, how = 'activate' },
        { quest = 'CG04', stage = 145, file = 'Fallout3.esm', ref = 0x024720, how = 'activate' },
        -- New Vegas, Doc Mitchell's house (cell 0x103df9): the Vigor Tester's trigger (stage 60); the Vigor Tester itself
        -- (OnActivate, gives SPECIAL, sets 65 when the stage is 60 and the objective shows); Doc Mitchell, who talks when the
        -- stage is 80 and the player has sat on the couch (the goodbye lines of his topics set 85, and the traits and tags
        -- come in the talk after it); the trigger of the door (sets 115)
        { quest = 'VCG01', stage = 55, file = 'FalloutNV.esm', ref = 0x104C16, how = 'walk' },
        { quest = 'VCG01', stage = 60, file = 'FalloutNV.esm', ref = 0x104C13, how = 'activate' },
        -- Stage 80 waits for the player to sit on the couch while Doc sits in his chair (round 12): the couch is a FURN, and
        -- activating it puts the player there and makes it the furniture the player uses, which the script of the couch's trigger
        -- looks at; that script then makes Doc talk (stage 85 comes by itself). Once: a second activation of the couch gets the
        -- player up (round 13).
        { quest = 'VCG01', stage = 80, file = 'FalloutNV.esm', ref = 0x103E55, how = 'activate', tries = 1 },
        { quest = 'VCG01', stage = 110, file = 'FalloutNV.esm', ref = 0x107236, how = 'walk' },
        -- Stage 115 is Doc's farewell: his package of the Dialogue type (VCG01DocMitchellFarewellDialogueStart) makes him talk
        -- to the player, and the talk ends the intro (stage 200). If it stops here, activating Doc starts the same talk:
        -- { quest = 'VCG01', stage = 115, file = 'FalloutNV.esm', ref = 0x104C0F, how = 'activate', tries = 1 },
    },
}
