# M16 notes: needs, hardcore mode, resting and travelling

What the `needs` package of M16 changed and what it rests on. Everything below that is a number or a rule was read from the
executables (`FalloutNV.exe` and `Fallout3.exe`; the addresses are those of New Vegas unless said otherwise) or from the games'
data files. Where a value is a choice of this port and not read, the text says so ("chosen").

## What the code does now

| Part | Where | Source |
|---|---|---|
| One rule for "may the player wait, sleep or fast travel here and now" | `components/falloutscript/restrictions.*` (pure, tested in `testrestrictions.cpp`), facts gathered in `apps/openfallout/mwscript/falloutneeds.cpp` | the three checks below |
| `SetHardcore`, `IsHardcore`, `IsAlwaysHardcore`, `DisableHardcoreTracking`, the Gameplay switch and the save of the mode | `HardcoreMode` and `addHardcoreCommands` in `hardcore.*`, section `hardcore` of the save (`falloutsave.cpp`, version 1) | 0x969E90, 0x5DE920, 0x7D1400, 0x9590F0, 0x95A3B0 |
| Needs rising and the stages | `HardcoreNeeds` in `hardcore.*` (the rates, the stage spells), driven by `updateHardcore` in `falloutinteraction.cpp` | 0x969C30 |
| The messages of a stage change | `HardcoreNeeds::noticeLines`, `needNotice` | 0x8C5610, 0x8C5890, 0x8C5B10 |
| The row of the needs in the HUD | `NeedsDisplay` in `hardcore.*`, `needs` table of `core.hud.status()`, `falloutmenuhud.lua` | 0x7739F0, 0x773E90, 0x76BFE0 |
| The dehydration, hunger and sleep pages of the Pip-Boy | `falloutpipboystats.lua` (modes 3 to 5, only when the mode is on) | 0x7E0060 |
| Ammunition weighs only in hardcore | `carriedAmmoWeight` (item info and carried weight) | 0x48EBC0 |
| A companion can die only in hardcore | `companionIsEssential`, first test of `isEssential` | 0x87F3D0 |
| `GetPCSleepHours`, `ShowSleepWaitMenu` | `queries.cpp` and the glue in `falloutneeds.cpp` | 0x5CDE00, 0x5E0090 |
| Fast travel takes game time and heals | `Restrictions::travelHours`, `travelHealingSeconds`, `travelTo` | 0x93CDF0, 0x88B510, 0x647F00 |

## May the player rest or travel (the one rule)

The executables ask the same facts in three places, each in a fixed order, and answer with the game setting that holds the
message of the first fact that fails. `Restrictions::refusal(action, facts)` returns the name of that setting (or nothing).

* Wait: the key, and `ShowSleepWaitMenu` with its second argument 1: 0x969FA0 (Fallout 3: 0x788350). Order: trespassing,
  alarm sounding, under water (7/8 of the height, 0x885520, constant 0x1084D20), a hostile near (0x9764A0), in the air
  (controller state 1 or 2), a place that forbids it (flag 0x80000 of the interior cell or of the worldspace outside, 0x5444C0; or the
  wait switch byte +0x66E, the second switch of `EnableFastTravel`, 0x944360), radiation (flags 0x5098E0 on the object 0x11E0E80,
  and the process, virtual 0x764, for waiting only), an effect that takes health (0x822E00).
* Sleep: the bed, 0x5095B0 (Fallout 3: 0x4ADE00). Order: an owned bed (0x567790 and 0x5785E0), the place that forbids waiting, trespassing, a hostile
  near, radiation, an effect that takes health. Radiation and health damage have sleep words of their own; a place that forbids
  waiting answers with the word of waiting.
* Fast travel: the predicate 0x93D660 (Fallout 3: 0x774F40) behind the click on the map (0x796FD0). Order: a hostile near, an alarm, health
  damage, the script switch (+0x66D bit 0, the first switch of `EnableFastTravel`), overencumbered (0x954CC0; New Vegas
  exempts a player whose perk entry point 0x33 (51) gives 1.0 or more), a spent player (health or fatigue below 1), in the air (0x8849C0),
  the place the records forbid to travel from (0x544520), a marker not found yet.
* A hostile is a character that is awake, enabled, not dead and after the player or hostile to it within
  `fHostileActorExteriorDistance` (3000) outside or `fHostileActorInteriorDistance` (2000) inside. Both settings are absent
  from the master files; 3000 and 2000 are the executables' defaults (0x9764A0).
* The words are game settings: `sNoWaitTrespass`, `sNoWaitWhileAlarmSounding`, `sNoWaitUnderWater`,
  `sNoWaitHostilActorsNear` (spelled without the e in the executable), `sNoWaitInAir`, `sNoWaitInCell`, `sNoWaitInRadiation`,
  `sNoWaitTakingHealthDamage`, `sNoSleepInOwnedBed`, `sNoSleepTrespass`, `sNoSleepHostileActorsNear`, `sNoSleepInRadiation`,
  `sNoSleepTakingHealthDamage`, `sNoFastTravelHostileActorsNear`, `sNoFastTravelAlarm`, `sFastTravelNoTravelHealthDamage`,
  `sNoFastTravelScriptBlock`, `sNoFastTravelOverencumbered`, `sNoFastTravelInAir`, `sNoFastTravelCell`,
  `sNoFastTravelUndiscovered`. The engine logs `Rest refused: <setting>` when it refuses to wait or sleep.
* `StopWaiting` (0x10B8) and the AI package `Wait` (0x10B7) are different things: the first ends a wait that is on, the second is
  a package of an actor. They are left to the actors' code.
* `IsPCSleeping`, `GetPCSleepHours` (0x10BC, reads player +0x654, 0x5CDE00), `SetPCSleepHours`, `WakeUpPC` and `ShowSleepWaitMenu` (0x1279, 0x5E0090) are answered from the state of the rest.

## Fast travel time

0x93CDF0: the hours are `trunc(path / speed / 3600 * TimeScale)`; the path is the length of the route the game works out, the speed
is 0x647F00 = the walking speed 0x647D10 times `fMoveRunMult` (4.0); the base speed is the actor value 0x15 times 0.01 times
`fMoveBaseSpeed` (77), times 0.85 with one crippled leg and 0.75 with two. The player is healed as the hours go by: 0x88B510, 2.0
seconds of the regeneration rate for each hour. The port sums the hours in the loop of `travelTo` one hour at a time so that the needs and the effects go on during them; a trip ends at the first hour after which the player is dead (the needs or an effect took the last of the health), with no more hours and no move, as a rest ends.
Chosen: the path is the straight line (no navmesh route is available to the rules), and an unknown length gives 0 hours.

## Hardcore mode (New Vegas only; Fallout 3 has none)

* State: an int in the player (+0x7BC; `IsHardcore` is true for exactly 1) and a byte (+0xE38) the tracking flag that
  `IsAlwaysHardcore` returns. A new game sets the byte to 1; `SetHardcore` with 0, with any value other than 1, `DisableHardcoreTracking`, or a
  save whose byte is 0 clears it. `SetHardcore` is 0x969E90; the command, 0x5DE920 (only 0 and 1); the Gameplay switch, 0x7D1400.
  Turning the mode off sets the three needs back to nothing. Save: 0x9590F0, load: 0x95A3B0 (the port's own section `hardcore`, version 1).
* The needs updater, 0x969C30: each need has an interval of its rate setting times the time scale divided by 60
  (`fHCDehydrationRate`... the three rates), and it does nothing while the Fighting control of the player is disabled (player +0x680 bit 4).
  The time stamps are statics that the executable sets again after a load and does not save; the port does the same (it does not save them).
* A need that changes its stage tells the player: the word for the level going up or down (the faces in pain and very happy), then
  `sRadiationSick` and the name of the new stage, or the word for being well again (0x8C5610, 0x8C5890, 0x8C5B10). The port logs `Hardcore: need <n> ... stage spell 0x<id>`
  for each of them.
* The row in the HUD: 0x7739F0 (a row), 0x773E90 (the style of a tile), 0x76BFE0 (the layout: the rectangle of the tiles is at
  the x of the health plus 150, the labels at the cumulative width plus 10). A tile is clear below `fHCHalfAlphaTextThreshold` (200),
  half bright to `fHCFullAlphaTextThreshold` (400), full, red from `fHCRedTextThreshold` (600), and flashes (alpha 128 to 255 at 0.5 per millisecond)
  from `fHCFlashTextThreshold` (800). The three needs are drawn only when the mode is on; radiation and the crippled limbs always.
* Ammunition weighs only in the hardcore mode (0x48EBC0); a companion is essential only outside it (0x87F3D0: teammate and not hardcore).
* Stimpaks that heal over time and the doctor's bag are data: their effects are chosen by `IsHardcore` conditions (the condition
  function 0x24A is the same handler as the command), so no code of the engine is needed beyond the command.

## What was run

Unit tests: every rule has tests (`testrestrictions.cpp`, `testhardcore.cpp`, `testqueries.cpp`, `testswitches.cpp`,
`testcombatcommands.cpp`, and the glue test `FalloutGameOptions` in `apps/openfallout_tests`). `components-tests` ran 7018 tests with only
the 15 known failures of the shader-prefix tests, `openfallout-tests` 837 of 837, and `python3 -m pytest scripts/openfallout` passed (172 passed,
79 skipped). `scripts/openfallout/test_coverage_dashboard.py` passes and the dashboard counts SetHardcore, IsHardcore, IsAlwaysHardcore,
DisableHardcoreTracking, GetPCSleepHours and ShowSleepWaitMenu as done and read.

Hidden rounds on the real data (`scripts/openfallout/round/rounds/m16-needs-nv.json` and `m16-needs-fo3.json`, scenarios
`m16_needs_nv.lua` and `m16_needs_fo3.lua`; the new steps `r.rest`, `r.travel`, `r.hardcore`, `r.needs`). Both PASS, focus 0 of
37 and 0 of 17 samples, no error lines. What each showed (the engine's own log and the round's `ROUND` lines):

| Check | New Vegas (Goodsprings save) | Fallout 3 (new start in the wasteland) |
|---|---|---|
| a wait of 1 hour in the open | the screen opened, the rest ran, 1.00 game hours passed | the same, 1.00 |
| a hostile near (a placed character put into a fight with the player) | refused: `Rest refused: sNoWaitHostilActorsNear`, no screen, one HUD message; after the fight was ended a wait of 1 hour ran | the same |
| a place that forbids waiting (interior cell with the flag 0x80000: Gomorrah01 / Vault101a) | `Rest refused: sNoWaitInCell`, no screen | the same |
| in the air before the cell check | (an earlier run, the player falling in the cell: `sNoWaitInAir`, the order of the executable) | |
| a bed (activation of the bed reference, sleep menu, 8 hours) | sleeping screen opened, 8.00 hours passed | the same, 8.01 |
| fast travel (the nearest usable marker) | went; 0 hours (the straight line was 9.4 thousand units: about 0.25 h at the time scale of 30) | went; 0 hours (15.5 thousand units) |
| hardcore asked for | off at the start; `core.interact.setHardcore(true)` turns it on | asked, stayed off (Fallout 3 has no hardcore mode) |
| hardcore on, three waits of 24 hours | dehydration 288, 576, 863; hunger 96, 192, 288; sleep 48, 96, 144 (12, 4 and 2 a game hour: the rates 10, 25 and 50 minutes at the scale of 30) | |
| stage spells | need 0 stages 0x115fa1c, 0x115fa1d, 0x115fa1e, 0x115fa1f, need 1 stage 0x115fa21 (log lines `Hardcore: need N went up, stage spell 0x...`); turning the mode off logged both going down to 0x0 and the levels went to 0 | |
| the display | pictures `hud_needs_1..3`: the tile of dehydration at alpha 124, then 255, then the red colour at 863 (flashing), hunger at alpha 124 at 288; the labels sit above the health bar | |
| a sleep in hardcore | 8 hours of sleep from zero needs: dehydration 96, hunger 33, sleep deprivation 0 (it fell by the restoration of 60 each hour) | |
| the fatal stage | an earlier run let dehydration pass 1000 on the fourth day: the health went to a large negative number (the last stage takes all of it) | |

Not shown by a round: a save and a load of the hardcore state (covered by the unit test of the save section only), the stimpak
and doctor's bag conditions (data; no item was used), the companion change and the ammunition weight (unit tests only), the overencumbered
and spent refusals of fast travel (unit tests only). The first two runs in the log of the work had an engine rule that was wrong in the
scenario, not in the engine: the round step polled inside a predicate; it is fixed in `roundworld.lua`.

## Not done

* Trespass and alarm are not facts of this engine yet: `playerTrespassing` and `playerAlarmSounding` are false until the law package supplies them.
* The path of fast travel is a straight line; the armour and weapon terms of the speed 0x647D10 were not read.
* Radiation as a fact is approximated by a running effect that raises the rads; the executable's flags (0x5098E0) belong to the region and the cell and are not ported.
* The flag at 0x7D6E60 (global 0x11A0294) that the settings code reads was not understood, so it is not implemented.
* The Pip-Boy's effects list does not name the need stages; the stage effects are not caught up after a fast travel.
* Companion transfer rules and food and drink effects beyond the data conditions are not done.
