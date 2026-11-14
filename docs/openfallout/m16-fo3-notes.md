# M16 fo3: the Fallout 3 counterparts of the first-wave rules

The rules of the first wave (spawn, damage, law, actors, effects, world) were read from FalloutNV.exe 1.4.0.525. This note
puts the Fallout 3 1.7 addresses next to them, says where the two executables do the same and where they do not, and lists
what could not be found. Addresses are Fallout3.exe unless a line says New Vegas. Nothing here is a game text; the numbers
come from the executables, the data files and measurements. Where a rule was changed, the unit test is named.

## Verdict in one table

| Rule | Fallout 3 | New Vegas | Same? |
| --- | --- | --- | --- |
| Level adjust step (counter +1, above `iCalcLevelAdjustUp` back to `-Down`, at least 1) | 0x4EC070 | 0x567E10 | yes |
| Zone level (flag bit 1 of the byte at +0x1E scales with `fLevelScalingMult`) | 0x4BFB20 | 0x526190 | yes |
| Zone level is computed once, when the zone's first cell loads, and kept at +0x2C | 0x4BFBA0 (called 0x43A141, 0x436041) | 0x526270 (called 0x453F07, 0x4582DC) | yes; the first wave recomputed it: fixed |
| Cell respawn clock (`iHoursToRespawnCell`, default 72) | 0x4BFAF0, zone check 0x4BFBD0 | 0x526120 | yes |
| Actor respawn | 0x6F5600 | 0x881C90 | yes |
| `PlaceLeveledActorAtMe` | 0x52EC90 | 0x5D9810 | same steps |
| Disposition formula (clamp 0..100, same character 100, perk entry 14) | 0x580C80 (caller 0x70D1C6) | 0x642A60 | yes |
| Karma classes (0 good, 1 neutral, 2 evil, 3 very good, 4 very evil) | 0x44F380 | same numbering | yes |
| Karma of a kill | tail of the death routine 0x71B1DB-0x71B255 | table | differs: fixed (in the law2 package) |
| Karma of a theft | 0x70C030 | same rule | yes (already in the law glue) |
| Crime-tracking test of a faction (flags2 bit 0) | 0x70B5D0 | 0x8B8360 | yes |
| Dismember roll (`rand() % 100`, same formulas) | 0x582FD0 | 0x646E50 | yes |
| Destruction percent and stage (health -1 is "none", times 100, unsigned maximum, ceiling) | stage 0x44A5E0, ceiling helper 0x44A250, command 0x513870 | 0x477430, 0x476B20 | yes |
| `DamageObject` | exec 0x536AA0, calls the reference's slot 0x144 | 0x5D4B90 | yes |
| `DispelAllSpells` (effect types 0, 2, 3, 7, 8) | exec 0x523550 -> 0x6B7270 | 0x8249D0 | yes |
| `GetConcussed` (archetype 0x21) | 0x513B90 | same archetype | yes |
| `IsSnowing` (1/255, 0.999, 0.001) | 0x5149E0, 0x50EAD0 | 0x59EA10 | yes |
| `IsCloudy` (share of the weather shown plus share left of the one it comes from) | 0x514900 | 0x59E7F0 | yes |
| `IsPleasant` | 0x514880 | 0x59E700 | yes |
| `ExitGame` (credits global to 1, then the game ends) | 0x52E540 | the credits chain | yes |
| `GetDLCCount` | not in the Fallout 3 table | in the New Vegas table | Fallout 3 has no such command |
| `SetActorsAI` (byte at actor +0xB4; argument 0 gives 0 = off) | 0x52CF70 | | same meaning |
| `ToggleActorsAI`, `IsActorsAIOff` (answers 1 when the byte is 0) | 0x520B70, 0x520BF0 | | same meaning |
| `SetForceRun` (byte at +0x118), `SetForceSneak` (byte at +0x119) | 0x529C30, 0x529D50 | | same meaning |
| `IsGuard` (bit 1 of the class byte at +0x54) | eval 0x511420, reader 0x54D490 | | data bug fixed |
| Form-type numbers | table 0xF4A74C, indices 0 to 0x66 | | the same names as New Vegas |

## What was different, and what was done

### 1. The guard flag of a class (both games)

`ESM4::Class` read the guard flag from bytes 4 to 8 of the `DATA` of a `CLAS` record. Those bytes are the second tagged
skill. The flags are the u32 at offset 16 (the executable reads the byte at class +0x54 and tests bit 1 for `IsGuard`).
With the wrong offset about 25 Fallout 3 classes and 36 New Vegas classes were counted as guards; with the right one no
class of either game has the guard flag in the data, so `IsGuard` is false for every class unless a mod sets it. Fixed in
`components/esm4/loadclas.cpp`; the tests are `ESM4ClassTest.readsTheFlagsAfterTheTagSkills`, `aTaggedSkillIsNotAFlag`
and `aShortDataHasNoFlags` (`apps/components_tests/esm4/testclass.cpp`).

### 2. Karma of a kill in Fallout 3

The death routine of Fallout 3 pays karma in this order: a victim of karma class 4 (very evil) pays `fKarmaModKillingEvilActor`
whether or not any faction of it tracks crime; a victim of class 2 (evil) pays nothing; the other classes (0, 1, 3) pay
`fKarmaModMurderingNonEvilNPC` (or `...Creature` when the victim is a creature) only when the victim holds a faction that
tracks crime (0x70B5D0) and the kill counts as a crime. The first wave used the New Vegas table for both games, which paid
class 2 a gain in Fallout 3. The law2 package found the same routine independently and has the fix with its tests
(`killKarmaFallout3`, selected by `mKarmaGrades`); this package's own copy of the change was dropped so that the two do not
collide, after its numbers were found equal.

### 3. The level of an encounter zone is kept (both games)

Both executables compute the level of a zone on the first load of one of its cells and keep it (Fallout 3 0x4BFBA0, New
Vegas 0x526270); nothing resets it later, so a leveled actor placed at level 20 into a zone that was first loaded at level 3
is still given the level the zone got at 3. The first wave recomputed the level at every placement. `SpawnLevels` now holds
the first level of every zone (`lockZoneLevel` ignores zone 0 and levels below 1 and keeps the first), the spawn code locks it
when a cell loads (`spawnCellLoaded`) and `placeLeveledActor` reads it first. The lock is saved: section `spawn` is version 2
and a version 1 save loads without zone levels. Tests: `FalloutScriptSpawnRulesTest.aZoneKeepsTheFirstLevelItWasGiven`,
`aSaveOfTheFirstVersionHasNoZoneLevels` and the changed save test.

### 4. The stall of the Fallout 3 new-game autoplay at CG02 stage 80 (not from M16)

The autoplay of the integration head stood at CG02 stage 80 ("stand next to the Dad so that Jonas can take the picture")
for the rest of the run. Not M16: the build before the first wave stalls at the same place, with the same numbers.

- The rule: the script of the trigger volume (0x0C6DE0) sets `PlayerReady` to 1 only when `GetHeadingAngle` from the player to
  Jonas is between -45 and 45 degrees, while the Dad and Jonas have done their waiting packages; the quest script then goes
  to stage 90.
- The measurement (a game loaded from a save at stage 80 and the logging of the player's yaw from the start): the player
  starts the party at the heading of the start marker (-74.5 degrees, where Jonas would be inside the 45), is turned
  by the talks (the dialogue view turns the player toward the head of the speaker, M13), and at stage 70 the Dad greets the
  player while he stands at the picture place, 2.7 km away: the player ends facing him at -174 degrees. The walk gate then
  puts the player into the trigger and keeps the heading, which is 65 degrees off Jonas (43 off the Dad). `GetHeadingAngle`
  answers 65.4 there; the Dad and Jonas have done their packages (the stage goes on as soon as the player faces Jonas).
- Proof: on the loaded save, turning the player toward the middle of Jonas and the Dad (angles 4.7 and -18.2) gives stages 90, 95
  and 100 within ten seconds; and with the fix below the quest goes to stage 98 within five seconds of the gate.
- The fix is in the autoplay (`scripts/openfallout/autoplay`): a `walk` gate can have `face`, a reference of the same plugin
  that the player is turned to when it is put at the gate's reference, like a player who looks at the photographer; the stage
  80 gate faces Jonas (0x0300E8). The engine rule (the dialogue view turns the player to the speaker) was not changed: whether
  the player's reference should be turned at all in the games' conversations was not read from the executables here.
- The full Fallout 3 new-game autoplay on the fixed branch (with the law2 package merged) goes through: stage 90 comes 0.1 s after
  the gate at stage 80 (try 1), CG03 stage 60 at 774 s, CG04 at 1073 s, and the run ends at "intro over" MQ01 stage 50 at 1712 s,
  35 gates ok, none of them a stand-in, no error line (the round's own verdict says FAIL only because the scenario that waits for
  2,300 s is cut short by the autoplay quitting at the end of the intro).
- Before the dialogue view (M13) the heading stayed that of the start marker, which is why the autoplay passed earlier.

## Same in both games, checked in the executable

- Settings: the defaults compiled into Fallout3.exe equal the engine defaults for `fAlign*`, `fDisp*`, `fKarmaMod*`,
  `iStealWarnings` and `iPickPocketWarnings` (2), `iCrimeAlarmRecDistance` (10000), `iKarmaMin`/`iKarmaMax` (-1000, 1000),
  `iKarmaChangeThreshold` (250), `fAITrespassWarningTimer` (10) and `fCrimeDispAttack` (-10). Fallout 3 settings are static
  structures in the data segment (vtable, value at +4, name pointer), not calls at start-up as in New Vegas.
- The dismember settings differ in the executable (5, 25, 50, 50, 50, 75, 50, 0.25 for Fallout 3) but both ESM files override
  them with the same values (75, 50, 0, 25, 25, 15, 10, 0.03), so the engine's numbers are right for both.
- The `DEST` header is 8 bytes and a `DSTD` stage is 20 bytes in Fallout 3, the same sizes as the engine's structures.
- `FACT` `DATA` flags: TrackCrime is flags2 bit 0 in both games (0x70B5D0), as the law glue reads it.
- The default dialogue topics of the law (`Steal` 0xDF, `Pickpocket` 0xE3, `TrespassNoCrime` 0xEB, `GuardTrespass` 0xF2) have the
  same form ids and editor ids in Fallout3.esm as in the New Vegas file, so the glue's lines apply to both.
- `GetLineOfSight`: the structure differs in Fallout 3 and was not compared in depth (see the list below).

## What was run

- Unit tests on the merged branch (this package and law2): components-tests 6,911 pass, 15 fail (the known NifOsg and
  `MiscToEulerAnglesZYX` ones), openfallout-tests 837 pass.
- Hidden rounds on Fallout 3 (nobody at the screen): `m16-spawn-fo3` PASS (135 s, the zone level of a placement is the same
  for every placement of the zone), `m16-law-fo3` PASS (125 s), a game loaded from a save at CG02 stage 80 with the fixed gate
  (stage 98 within five seconds), and the full new-game autoplay (intro over at 1,712 s, see above).
- The same loaded save on the pre-M16 build and a logging run of the yaw of the player on both builds found that the stall at
  stage 80 does not come from M16.

## Not done, and why

- The Fallout 3 leveled-list walker (vtable getters 0x4AFA40 and 0x4AF1B0) and the readers of `iLevItemLevelDifferenceMax`
  and the list-count multipliers were not found; the pick rule of the first wave is New Vegas' and was only confirmed against
  Fallout 3 by its constants, not by tracing the walker.
- Trespass (0x72B980, 0x752969) was located but not compared step by step with the New Vegas package ladder.
- `GetLineOfSight` and the ray flags of Fallout 3 were not compared.
- The function that applies a `DSTD` stage (what the flags of a stage do) was not located; only the percent and stage numbers
  were compared.
- `MGEF` and `FACT` were checked on the data side only (field sizes), not in the code that consumes them.
- The Fallout 3 side of the `Crime` hook into faction reputation was not looked for (Fallout 3 has no infamy).

## Where it was read

Command addresses come from the command tables of the two executables (`exec` and `eval` entries); settings from the static
setting structures of Fallout3.exe. The lines for the rules above are in `docs/openfallout/coverage/tracker/reads.csv`.
