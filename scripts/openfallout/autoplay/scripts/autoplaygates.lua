-- The places where the intro of Fallout 3 and New Vegas waits for the player to do something that pressing a button does not
-- do: walk into a trigger volume or activate an object. autoplayworld.lua reads this table. When a quest has stood at a stage
-- for `settle` real seconds (nothing else changed in it), the gates of that stage are tried, one at a time: 'walk' puts the
-- player at the reference (the middle of its volume), 'activate' makes the player activate it. A gate is tried `tries` times
-- at most.
-- The references are the ones the Mac found in the base plugins (static survey, rounds 7 and 8); `ref` is the form id without
-- the number of the plugin in the load order (the plugin itself is `file`). The stages are those the quest waits at.
return {
    settle = 20,
    tries = 2,
    gates = {
        -- Fallout 3, Vault 101 (cell 0x028138): the Dad's trigger (a sphere, sets stage 12 for the player); the way out of the
        -- crib (any actor, sets 40); the baby book (OnActivate, gives SPECIAL; activated while the stage is 30 or more); the
        -- trigger at the end of the quest (sets 100). The gate of the playpen, which the player activates at stage 20,
        -- is not in the table yet: nobody has its id.
        { quest = 'CG01', stage = 10, file = 'Fallout3.esm', ref = 0x02EA54, how = 'walk' },
        { quest = 'CG01', stage = 30, file = 'Fallout3.esm', ref = 0x02EA53, how = 'walk' },
        { quest = 'CG01', stage = 40, file = 'Fallout3.esm', ref = 0x02ECC0, how = 'activate' },
        { quest = 'CG01', stage = 90, file = 'Fallout3.esm', ref = 0x02ECC2, how = 'walk' },
        -- New Vegas, Doc Mitchell's house (cell 0x103df9): the Vigor Tester's trigger (stage 60); the Vigor Tester itself
        -- (OnActivate, gives SPECIAL, sets 65 when the stage is 60 and the objective shows); the trigger of the door (sets 115)
        { quest = 'VCG01', stage = 55, file = 'FalloutNV.esm', ref = 0x104C16, how = 'walk' },
        { quest = 'VCG01', stage = 60, file = 'FalloutNV.esm', ref = 0x104C13, how = 'activate' },
        { quest = 'VCG01', stage = 110, file = 'FalloutNV.esm', ref = 0x107236, how = 'walk' },
    },
}
