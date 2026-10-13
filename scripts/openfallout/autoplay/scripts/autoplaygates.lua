-- The places where the intro of Fallout 3 and New Vegas waits for the player to do something that pressing a button does not
-- do: walk into a trigger volume or activate an object. autoplayworld.lua reads this table. When a quest has stood at a stage
-- for `settle` real seconds (nothing else changed in it), the gates of that stage are tried, one at a time: 'walk' puts the
-- player at the reference (the middle of its volume), 'activate' makes the player activate it. A gate is tried `tries` times
-- at most (its own `tries` if it has one: activating a piece of furniture a second time gets the player up from it).
-- The references are the ones the Mac found in the base plugins (static survey, rounds 7 and 8); `ref` is the form id without
-- the number of the plugin in the load order (the plugin itself is `file`). The stages are those the quest waits at.
return {
    settle = 20,
    tries = 2,
    gates = {
        -- Fallout 3, Vault 101 (cell 0x028138): the Dad's trigger (a sphere, sets stage 12 for the player); the way out of the
        -- crib (any actor, sets 40); the baby book (OnActivate, gives SPECIAL; activated while the stage is 30 or more); the
        -- trigger at the end of the quest (sets 90 when the stage is 80, and the stage then goes to 100); the gate of the
        -- playpen (a door, OnActivate sets 30 when the stage is 20, which the Dad's packages bring about: the stage is 14
        -- until he has closed the gate himself).
        { quest = 'CG01', stage = 10, file = 'Fallout3.esm', ref = 0x02EA54, how = 'walk' },
        -- Stage 14 waits for the Dad to close the gate of the playpen (his Travel package CG01DadCloseGate). The player stands
        -- in the sphere of his trigger, which is where he walks, so the player goes back to where it began: the marker that
        -- the Dad's package leads him to is on the way to it, and the Dad leaves the sphere (round 11). Since PR 57 the stage
        -- comes by itself 16 s after 14 (round 13), before this gate is tried; it stays as a net.
        { quest = 'CG01', stage = 14, file = 'Fallout3.esm', ref = 0x02EA4F, how = 'walk' },
        { quest = 'CG01', stage = 20, file = 'Fallout3.esm', ref = 0x047803, how = 'activate' },
        { quest = 'CG01', stage = 30, file = 'Fallout3.esm', ref = 0x02EA53, how = 'walk' },
        { quest = 'CG01', stage = 40, file = 'Fallout3.esm', ref = 0x02ECC0, how = 'activate' },
        { quest = 'CG01', stage = 80, file = 'Fallout3.esm', ref = 0x02ECC2, how = 'walk' },
        -- Vault 101, the birthday party (CG02): the quest stands at stage 10 until the player is in the trigger at the cake
        -- (sets 15), or Old Lady Palmer greets the player (round 13, static; not run yet)
        { quest = 'CG02', stage = 10, file = 'Fallout3.esm', ref = 0x09F64F, how = 'walk' },
        -- New Vegas, Doc Mitchell's house (cell 0x103df9): the Vigor Tester's trigger (stage 60); the Vigor Tester itself
        -- (OnActivate, gives SPECIAL, sets 65 when the stage is 60 and the objective shows); Doc Mitchell, who talks when the
        -- stage is 80 and the player has sat on the couch (the goodbye lines of his topics set 85, and the traits and tags
        -- come in the talk after it); the trigger of the door (sets 115)
        { quest = 'VCG01', stage = 55, file = 'FalloutNV.esm', ref = 0x104C16, how = 'walk' },
        { quest = 'VCG01', stage = 60, file = 'FalloutNV.esm', ref = 0x104C13, how = 'activate' },
        -- Stage 80 waits for the player to sit on the couch while Doc sits in his chair (round 12): the couch is a FURN, and
        -- activating it puts the player there and makes it the furniture the player uses, which the script of the couch's trigger
        -- looks at; that script then makes Doc talk. Doc himself is activated if that has not brought the talk about.
        -- Once: a second activation of the couch gets the player up (round 13).
        { quest = 'VCG01', stage = 80, file = 'FalloutNV.esm', ref = 0x103E55, how = 'activate', tries = 1 },
        { quest = 'VCG01', stage = 80, file = 'FalloutNV.esm', ref = 0x104C0F, how = 'activate', tries = 1 },
        { quest = 'VCG01', stage = 110, file = 'FalloutNV.esm', ref = 0x107236, how = 'walk' },
        -- Stage 115 is Doc's farewell: his package of the Dialogue type (VCG01DocMitchellFarewellDialogueStart) makes him talk
        -- to the player, and the talk ends the intro (stage 200). Round 13 stopped here before the engine had that package; if
        -- it stops again, activating Doc starts the same talk:
        -- { quest = 'VCG01', stage = 115, file = 'FalloutNV.esm', ref = 0x104C0F, how = 'activate', tries = 1 },
    },
}
