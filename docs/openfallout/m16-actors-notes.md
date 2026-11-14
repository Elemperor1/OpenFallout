# M16 notes (actors)

The state of a character that scripts and conditions ask about, and the overrides that scripts put on it. What is a number or a rule below was read from the executables (FalloutNV.exe, with spot checks in Fallout3.exe; addresses are given) unless it says **chosen**: a **chosen** rule is the port's own, because the executable's answer lives in code that this package did not read (the AI and the Havok physics of the games) or because the port has nothing the executable asks about. Nothing from the games' data is copied here.

## What was built

- `components/falloutscript/actorstatus.*`: the pure rules. `ActorStatus` holds, for each character, the overrides a script set (`ActorOverride`: AI off, forced run, forced sneak) and the activity the engine tells it a frame at a time (`ActorActivity`: movement word, swimming, animation action, package type, flags, target and stage, scripted idle playing and done, the last idle played, walk speed). The commands read from it; the engine writes to it. `combatGroupOf` works out the group of a fight. All commands of the package are registered by `addActorStatusCommands`.
- Engine glue (new files in `apps/openfallout/mwscript/falloutactorstatus.cpp`): the host answers the questions of `ActorStatusWorld` (teammate count, furniture marker, owner, fatigue, how many characters are looked after, disabling them, playable race), and `applyPlayerStance` puts the forced run and sneak of the player on the player's stance flags.
- Hooks: `mwmechanics/falloutactors.cpp` publishes the package a character follows (type, flags, target, stage) and skips a character whose AI is off, makes a forced run real (`walkTo`) and a forced sneak override it, and lets `SetNoAvoidance` stop the yielding; `mwrender/falloutactoranimation.cpp` publishes the movement word, the swimming flag, the action, the idle state and the walk speed each frame; `mwscript/falloutinterface.cpp` slows a character that sneaks by force by the setting `fMoveSneakMult`.
- Saved state: a section `actoroverrides` (version 1) with the AI-off, forced-run and forced-sneak flags. Older saves lack it and load with no overrides. The activity, the last targets, the Cannibal and Sandman flags and `SetNoAvoidance` are of the moment and are not saved.
- A small Lua binding for the rounds: `core.actorStatus.call(code, actor, ...)` runs a command by its code the way a condition does, and `core.actorStatus.activity(actor)` reads what the engine told. Nothing of the game's scripts uses it.

## Read from the executable

| Command (code) | What it does | Address (FalloutNV.exe) |
|---|---|---|
| GetCurrentAIProcedure 0x108F | the number is `switch(table[package type][stage])`, the table at 0x11A3FF0 (lists of procedure codes for each of the 48 package types), the switch at 0x5A1690; the stage is the process slot 0x280; a stage past the list reads the sentinel, which answers 17; no package answers 0. Fallout 3's table (0xF6EE80) has the same lists and numbers (its sentinel is 53, not 54) | 0x5A1210 |
| GetAnimAction 0x10DB | the word at +0x2EC of the process slot 0x3E4; -1 for none | eval of the command |
| IsMoving 0x1019 | from the movement word of the controller at actor +0x190 (0x8846E0): forward 0x1, back 0x2, left 0x4, right 0x8; the answer is 1, 2, 3 or 4 for the first of these that is set; walk 0x100, run 0x200, sneak 0x400 | 0x5A40D0 |
| IsTurning 0x101A | the turn bits 0x10 (left) and 0x20 (right): 1 left, 2 right | 0x5A4160 |
| IsRunning 0x111F | the run bit 0x200 of the movement word | 0x884730 |
| IsSwimming 0x10B9 | the byte at actor +0x14D | |
| IsWaiting 0x106F | the character has a package and (its flag 0x10000 is set or its procedure code is 13) | 0x8A6210 |
| IsIdlePlaying 0x1070 | the idle at process +0x1B8 exists and has not ended | 0x5A0F10, 0x4985F0 |
| GetIdleDoneOnce 0x113E | process slot 0x60 | 0x5A3570 |
| IsLastIdlePlayed 0x11C3 | process slot 0x38C is the argument | 0x5A4780 |
| SetActorsAI 0x118B, ToggleActorsAI 0x112E, IsActorsAIOff 0x112F | one byte at actor +0xBC (an argument of 0 sets it: AI off) | 0x5D6A60, 0x5BBE10, 0x5BBEC0 |
| SetForceRun 0x10D1 / GetForceRun 0x10D0 | the byte at actor +0x124 | 0x5CE7B0 / 0x5CE730 |
| SetForceSneak 0x10D3 / GetForceSneak 0x10D2 | the byte at actor +0x125 | 0x5CE910 / 0x5CE870 |
| SetNoAvoidance 0x115B | one global byte, not a byte of the actor | 0x5BC200 (byte 0x12682F8) |
| SetAllReachable 0x1159 | does nothing | 0x5BC150 |
| DisableAllActors 0x11A9 | disables every actor of the high process but the one it is called on and those already disabled | 0x5C47E0 |
| GetFatiguePercentage 0x1080 | current over permanent of actor value 22, 1 when the permanent value is 0 | 0x59C760 (0x893530) |
| GetOwnerLastTarget 0x124E (New Vegas only) | the last target of the actor | 0x5DEAA0 |
| StartCannibal 0x107E, StartMisterSandMan 0x107C | call a slot of the named actor with the caller as this; GetCannibal 0x10E3 and GetSandman 0x10E2 answer only for the player | 0x5CFEF0, 0x5CFDF0 |
| AdvancePCLevel 0x10D5 | adds one to the player's level | 0x5CE9F0 |
| ModPCAttribute 0x1110 | the attributes 5 to 11 | 0x5D11B0 (check 0x5D1260) |
| ModPCSkill 0x110F, AdvancePCSkill 0x10D4 | empty commands (they do nothing) | 0x5D4A40 |
| GetIsPlayableRace 0x10FE | the race record's flag 0x1 | 0x59F540, 0x59F610 |
| IsTorchOut, IsShieldOut, GetIsClassDefault, IsRidingHorse, GetPlayerHasLastRiddenHorse, IsPlayersLastRiddenHorse, IsHorseStolen, IsActorUsingATorch | the empty evaluator, they answer 0 | 0x5A2A50 |
| GetDisease 0x1027 | answers 0 | 0x59D250 |
| GetFurnitureMarkerID 0x10A0 | process slot 0x4C4, then 0x921080: the first quadword of the marker entry, 0 for none | 0x59DE90 |
| GetWalkSpeed 0x108E | 0x59C860 and 0x884DC0 were found; the formula was not read | |

The procedure numbers of each package type are in `actorstatus.cpp` (`procedureNumber`; type 6 gives {3, 11}, type 12 {13}, type 8 {20, 19, 3, 12}, and so on).

## Chosen, not read

- **Which stage a package is in.** The executable keeps the stage in the process (slot 0x280) and its AI moves it; the port's AI is not that AI. The port says the first stage of the list while the character walks toward the package's goal, the last stage of the list while it stands at the goal, and a stage past the list once the package is done (the sentinel, which answers 17). A package of the port's own (the order to attack, type 16) is reported as a UseWeapon package.
- **Combat groups.** GetGroupMemberCount and GetGroupTargetCount: the group is everyone who fights the same target, and when the target is the player or a teammate, everyone who fights any member of that party; the targets are the characters they fight. A character in no fight has no group (both counts 0). The executable's group (the combat controller's list) was not read.
- **The marker value of GetFurnitureMarkerID**: the kind of the furniture the character uses (what the port's furniture state holds), 0 when it uses none.
- **Gait.** A forced sneak slows by `fMoveSneakMult` (when above 0) and overrides a run; a forced run of a character that walks makes it run unless it sneaks. The player's forced run and sneak go through the stance flags of the engine's own creature stats, so the existing code for the gait of the player applies.
- **Action timing.** The animation action comes from the playing weapon-move group: attack 2, equip 0, unequip 1, recoil 8; a scripted idle shows as 14 (forced by script), a furniture pose as 13 (special idle). Block, reload, stagger, dodge and wait-lower are in the table of numbers but nothing sets them yet.
- **Walk speed** is the velocity of the walk animation of the character (units a second), 0 when it has none.
- **DisableAllActors** disables the characters that the engine looks after (those that have published an activity); the player is not one of them.
- **Cannibal and Sandman flags** stay set until a new game or a load. The executable's GetCannibal and GetSandman read the player's own action state (a virtual at +0x364 of the player, 0x5A2890), which the feeding animation ends; the port plays no feeding action and so has no end signal for the flag. An end that was invented here (a movement, a time) could end the flag while a script still polls it, so none was added.
- **Stand-ins** (`docs/openfallout/coverage/stand-ins.csv`): PickIdle, LoopGroup, SkipAnim, SetClass, GetActorAggroRadiusViolated and IsContinuingPackagePCNear are accepted and do nothing (a reason each); MatchRace, AgeRace, SetPCYoung and SetPCToddler stay the stand-ins that were already in `switches.cpp` (rebuilding a race on a live character is not built).

## What was run

Unit tests: `apps/components_tests/falloutscript/testactorstatus.cpp`, 46 tests: the movement answers and their first-set order, the procedure table, overrides, the save section (round trip, determinism, a cut-short section), combat groups and every command through `Interpreter::call`. The whole components test suite passes but for the 15 failures that were there before; the engine tests pass.

Hidden rounds (`--headless`, scenarios `scripts/openfallout/round/scenarios/m16_actors.lua` for New Vegas and `m16_actors_fo3.lua` for Fallout 3, round files `m16-actors-nv.json` and `m16-actors-fo3.json`). The scenario calls the commands by their codes through `core.actorStatus.call` and reads the numbers back; each check is a `CHECK` line of the log. The characters are two placed gangers (New Vegas) and two placed raiders (Fallout 3), the player in god mode at 30 m from them.

- **AI off (A).** A character that fights the player and runs toward it (about 550 units in 2.5 s in New Vegas, 920 in Fallout 3) stops at once when `SetActorsAI 0` is called on it: it moved 0.0 units in the next 3 s and `IsMoving` answered 0; `IsActorsAIOff` reads back 1; after `SetActorsAI 1` it acts again (it shoots, or runs on), and `ToggleActorsAI` flips the flag both ways. This is the animation too: no move word, no action.
- **Forced sneak (B).** The same running character (about 370 units/s) with `SetForceSneak 1` goes at 35 to 50 units/s in both games, and its movement word has the sneak bit (0x400); with the override cleared it runs again (338 units/s).
- **Forced run and sneak of the player (D).** The controls hold a walk for 2.5 s; the best 0.75 s stretch was 190 units/s walking, 329 with `SetForceRun 1`, 73 with `SetForceSneak 1` (New Vegas); 223, 395 and 83 in Fallout 3. `IsRunning` (movement bit 0x200) answers 1 only while the run is forced.
- **Groups (C).** Two characters that fight the player: `GetGroupMemberCount` 2 for each and `GetGroupTargetCount` 1; one of them at peace: 1; both at peace: 0. In a fight of the two against each other the counts follow the rule above (the player was also still in a fight with one of them in New Vegas: the member count of the other was 2).
- **Procedure and action over time (Z, C).** A character at peace with a package reports its type (type 12 gives `GetCurrentAIProcedure` 13 in New Vegas, type 13 gives 19 in Fallout 3), steady over eight seconds; a fighting character shows the equip action (0) as it draws and the attack action (2) while it shoots (New Vegas: a check; Fallout 3: the raiders aimed for seven seconds without a shot, so the attack action was not seen there); the movement word goes through run (0x201), turn (0x211, 0x221) and standing (0x0).
- **Other** (Z, E): `IsActor` 1 for characters and the player and 0 for a door; `GetFatiguePercentage` 1, `GetDisease` 0, `GetFurnitureMarkerID` 0, `GetPlayerTeammateCount` 0; `GetOwnerLastTarget` (New Vegas) answers the player after a fight; `DisableAllActors` disabled both characters and not the player.

A round found one defect of the first build, which is fixed: a character put in another place got a new animation, which took back everything the mind had told (its package), and the mind did not say it again (`forgetBody` and `forgetMind` now take back only each one's own fields; a test holds it).

What a round could not show: the figures for `GetWalkSpeed` (the walk animation's own velocity is all the port has, and 85 units/s was the answer for every character with the same skeleton); the stage of a package other than the first (the characters that were reachable follow one package at a time and stay in its first stage); `IsTurning` for a character that turns on the spot while talking; `IsIdlePlaying` and `GetIdleDoneOnce` (no round had a script play an idle on a character); `IsSwimming`; a saved game with an override in it (the section round trip is a unit test; no round saved and loaded); and what the games' own scripts do with these commands in place (no round ran such a script). The rounds' pass or fail is the scenario's own checks, not the executable's behavior.

## Not done

- **GetKnockedState** (0x106B): the damage package has it (it owns the knock state).
- **Pure-rule tests of the engine glue**: `apps/openfallout_tests` has no host for `FalloutScripts`, so the glue (publishing from the animation and the AI, the hooks) is covered by the rounds only.
- **Block, reload, stagger, dodge and wait-lower actions** of GetAnimAction (see above).
- **The executable's stage machine of packages**, the combat group list, GetWalkSpeed's formula (see above).
- **SetClass** (stand-in): the class of a character is not rebuilt.
- **MatchRace, AgeRace, SetPCYoung, SetPCToddler**: stand-ins, as before.
- **The rounds drive the commands through a Lua binding, not through a game script** that uses them: no placed script of the games was found that depends on these commands at a point a round reaches.
