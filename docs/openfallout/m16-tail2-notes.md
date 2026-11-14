# M16 tail2: the leftovers of the world, actors and effects packages

Second-wave package. It takes the commands and entry points that the first wave of `world`, `actors` and `effects` left, reads what the
executable does for each, builds what does not need renderer work and says what it did not. Addresses are of `FalloutNV.exe` unless the text
says Fallout 3 (`Fallout3.exe`). Marks: **exe** (read in the executable), **seen** (seen running in a round), **chosen** (nothing in the
executables settles it, the port picked).

Usage of a command by the plugins (`coverage/tracker/command-usage.csv`) decided the order: `ExitGame` 17 calls, `IsGoreDisabled` 14,
`SetPackDuration` 12, `Reset3DState` 12, the race changes 20 (Fallout 3 only), `GetKnockedState` 5, `EnableLoadingMenu` 2.
`GetAnimAction` is called 49 times, but a scan of the compiled-source text of the New Vegas scripts shows it compared only with 13 (a
special idle: 36 uses) and 2 (an attack: 5 uses), both of which the port already answers.

## Built

### The game's ini settings reach the script rules (`IsGoreDisabled`, gore, the 360 pad)

- **exe** `IsGoreDisabled` (eval 0x5A5A50, Fallout 3 0x5143B0, exec 0x5DC5A0): answers 1 when the setting `bDisableAllGore` of the General section
  (the byte of the setting object at 0x11DF7F8, registered at 0xFA4570 with the default 0; Fallout 3 byte 0x107ADC4) is exactly 1. The callers
  that test it are the damage and dismember code (0x81B8E0, 0x87F260, 0x88FB00, 0x899CB0 and neighbours).
- The ini files were read in `main.cpp` and dropped. They now reach the script host (`Engine::setGameIni`, `FalloutScripts::setGameIni`,
  `gameIniInteger`). `FalloutScript::World::iniInteger` reads a value as the Windows call does (spaces, sign, digits to the first other
  character; nothing when there is no digit), `World::goreDisabledBy` is the 1-test.
- Wired: `IsGoreDisabled`, the `mGoreOff` of the dismember settings (`goreOfKill` returns before it cuts or bursts anything), and
  `bDisable360Controller` of the Interface section for `IsWin32` (the interface hook `padDisabled` existed, nothing implemented it; the setting's
  name is **exe** 0x106F7A8, how the executable combines it is the first wave's reading).
- `IsNoHumanGoreExe` stays a fixed 0 (0x5DFDE0).

### `GetKnockedState`, `GetForceHitReaction`

- **exe** `GetKnockedState` (eval 0x5A08C0): the actor's process is cast to `MiddleHighProcess` (RTTI 0x118C64C), its byte at +0x13C is read by
  the virtual at 0x40C (0x8D8540; the setter 0x8D8560 is virtual 0x410), and the answer is 1 for the states 3 and 4, 0 for the others.
- The port has one knock, the throw of a blow or `PushActorAway` for `fKnockbackTime` seconds. `HitReactions::KnockTracker` keeps who is thrown
  and for how long; both throw sites start it, the damage update advances it, a new game clears it. A thrown character is state 3 (**chosen**: the
  executable's names of the states 0 to 6 were not read; there is no knockdown or get-up animation to be state 4).
- **exe** `GetForceHitReaction` (eval 0x5A5B30): the byte at actor +0x1B1, which the damage function 0x899CB0 clears at 0x89AC62, sets to 1 at
  0x89BEE0 when a byte (+0xC) of the hit data is 1 and clears again at 0x89C3BD after the call that plays the reaction. Nothing outside that
  call sees it set, so the 0 the port answers is exact and the command is listed as `exact` in `stand-ins.csv`.

### `IsFacingUp`, `IsLeftUp`

- **exe** eval 0x5A0710 and 0x5A0800 (Fallout 3 0x514DD0, 0x514E80): take the node whose name object is at 0x11C61B4 (set at 0x4B7ADB from the
  string "Bip01 Spine"; the function falls back to "Bip01 Spine01"), and answer 1 when the entry of its world rotation at +0x84 (row 2,
  column 1) is above 0 (`IsFacingUp`) or the one at +0x88 (row 2, column 2) is above 0 (`IsLeftUp`) (0xC6B7B0, 0xC6B860: `0 < value`, so a value of exactly 0 answers 0).
- The actor animation publishes the up part of the spine bone's Y and Z axes in the character's frame each frame (the loader keeps rotations
  transposed, so the axis image of the row convention is the games' column); the commands answer `> 0`. No plugin calls either command
  (0 uses), so this is exact in rule and unproven on real data.

### `ScriptEffectElapsedSeconds` (the first wave's meaning is confirmed)

- **exe** exec 0x5C43A0 reads the float at +4 of the object behind the event list's pointer at +0x10 (0 when the script has none). I first took that
  for the active effect's elapsed time, which the effect's update (0x804560, 0x8048E2 to 0x8048EB) grows by the seconds it covers, and built a
  per-effect running total. **That was wrong and is reverted**: the 14 plugin scripts of New Vegas that call the command all treat it as the
  seconds of one update (`fTimer` counts down by it every update in the holo-flicker effects of the Dead Money add-on; the health implant,
  the action point implant and the radiation absorber set a per-update amount to it divided by 10 or 20, which with a running total would
  explode). The writer of that float was not found (the +4 float at the active effect is a different field), so the first wave's value, the
  seconds the update covers (0 in the start and finish blocks), stays; it is **chosen** on the evidence of how the scripts use it.

### `DispelAllSpells` and enchantments (correction)

- **exe** 0x8249D0: for each active effect the magic item's type (virtual +0x18) 0, 2, 3, 7 or 8 is dispelled (0x804210); another type is cast to
  `EnchantmentItem` (RTTI 0x1183594) and dispelled only when 0x5F36F0 (the field +0x34 **of the enchantment**, its count of effect items) is 0.
  The first wave had read +0x34 as a field of the active effect. An enchantment that has effects is therefore not dispelled, which is what the
  rule already did, so nothing changes in the code; only the reason does.

### `SetPackDuration`

- **exe** exec 0x5D1AB0 and 0x8F4020: the character's package is over when the game hours since it began, times 100, reach the duration (0 = no
  limit; `World::packageTimeIsUp`).
- Engine: `FalloutActors::setPackageDuration` (through `MechanicsManager::setFalloutPackageDuration`) keeps the duration and the game hour in the
  character's mind; `review` marks the package done when the time is up (the same `mDone` and `Done` event as a goal reached), and a new package
  clears the duration. The duration counts from the call (**chosen**: the executable's start moment is the package's own; the port's mind keeps
  its own). A package that ran out and is still the one followed (no other is on) does not carry on its goal: the character stops where it
  is, and a new `SetPackDuration` on it makes it run again until the new duration is up.

### The credits after `ExitGame`

- **exe** (round notes 12AQ): `ExitGame` (0x5D8B60) sets the hard-coded global `PlayCredits` (form 0x63) to 1, or 2 with the perk 0xED568; the
  start menu's constructor (0x7CBAF0) opens the credits (0x7CF9A0, credits menu 0x417) while the global is above 0; the end handler 0x75F750 writes 0
  and, with a matching save, asks `sContinuePlayingAfterCredits`.
- The host keeps the request (`FalloutScripts::requestCredits`, not touched by a new game or a load), `ExitGame` sets it, the menu script API
  has `creditsRequested()` and `endCredits()`, and `falloutstart.lua` shows the credits (`falloutcredits.lua`, the Wild Wasteland text for 2) as
  soon as the title screen is made, and clears the request when they end. **Not done**: the end question, the end music, the held-Ctrl variant.
  Not run in a round (a round that leaves a game needs a menu screenshot path).

### `Infiltrator` (entry point 32), `EnableLoadingMenu`, `IsLoadingMenuEnabled`

- **exe** 0x430AE0 (called by the activation of doors and containers at 0x5180B0, 0x516DC0 and by the lock screen at 0x775A00): the lock is broken
  when the count at +0xC of the lock data is at least 1, or at least 2 when the player's entry point 32 (starting from 0.0) is not 0. The writer
  of the count was not located; the port counts each failed forcing (`FalloutScripts::mLockBreaks`, `Lockpick::lockBroken`, the notice is shown
  only when the lock is now broken). Entry point 32 is `EntryPoint::ignoreBrokenLock`. One perk of each game has data for it.
- **exe** `EnableLoadingMenu` (0x5DFBA0): a number above 0 shows the loading menu through 0x457D70, any other hides it; `IsLoadingMenuEnabled`
  (0x5DFC10) is whether the pointer at 0x11DA0C0 is not null. Built as a flag only (`WorldState::loadingMenu`); the menu is not shown by the
  command (stand-in, listed).
- Entry point 51 (fast travel always, `LongHaul`) was already applied by the needs package (`falloutneeds.cpp`); its second call site 0x93CE33 was
  not read.

## Read, not built (with why)

| Item | What the executable does | Why not built |
|---|---|---|
| `GetWalkSpeed` (eval 0x59C860) | calls 0x884DC0: with an animation data at +0x68 of the actor's controller and its flag 0x1000 clear, the float at +0x38 of that data; with the flag set, virtual +0x38 of the actor's movement object; with no data, virtual +0x458 of the actor | the first wave's answer (speed of the walk animation) is the first case; the other two need the race's movement data, 0 plugin uses |
| `ShowDialogSubtitles` | exec 0x5B5D40 sets the byte 0x11F122D (0 or 1); 0x9367F5 (in the subtitle chooser 0x935F60) shows a line of a speaker that is not the player's conversation partner (`player+0x208`) only when it is set | the port shows every line by the `Subtitle` setting and distance; it has no partner filter to widen |
| `GetActorAggroRadiusViolated` (eval 0x5A4F30) | the actor's process (0x8D8520), virtual +0x20C, then 0x41CA90, answers 1 when that is 14 | the port has no aggression radius state |
| `IsContinuingPackagePCNear` (eval 0x5A1A40) | 0x8A69D0 on the actor | not traced further, 0 plugin uses |
| `SetPCYoung` (0x5DA980), `SetPCToddler` | `Player::SetYoung` (0x969820): writes the byte at player +0x7C5 and, if it changed, rebuilds the 3D (0x8B78C0, 0x8D3FA0) | the child body is a model change of the player (renderer work); Fallout 3 only, 7+2 scripts |
| `Reset3DState` (exec 0x5D3AE0) | rebuilds the reference's 3D (0x55F970) and clears two form flags (0x422AA0, 0x422C20) | the port has no 3D reset; one New Vegas script (re-arming bear traps) uses it 12 times |
| `GetDLCCount` (exec 0x5DF930) | counts the nodes of the list at +0xC of the singleton made at 0xAF26C0; `IsDLCInstalled` (0x5DFA20) compares package names in it | what fills the list (the content package enumeration) was not found; the engine still counts the add-on plugins it loaded |
| `SetSize`, `SetRigidBodyMass` | collision capsule size, rigid body mass | the port's characters collide with their model's box and objects have no mass |
| `GetAnimAction` block, reload, stagger, dodge, wait-lower | numbers 7, 9, 10, 11, 12 | nothing plays those animations; no plugin compares with them (see the top) |
| entry point 27 (activate; `Cannibal`, `RoboticsExpert`, `MisterSandman`, `GhastlyScavenger`) | applied at 0x9406A9 in 0x93E840 as an "add activate choice" | a choice in the crosshair prompt and its key handling is interface work |
| entry points 61 to 66 (unarmed power attacks) | applied in 0x948310 (0x3D forward, 0x3E back, 0x3F crouched, 0x40 counter, 0x41 left, 0x42 right), one perk each | the port has no power attack or its direction |
| `SetClass` (0x5D06A0), `MatchRace` (0x5DA810), `AgeRace` (0x5DA690) | change the base record of a character (class, race) | rebuilding a live character's body; stand-ins as before |
| `PickIdle` (0x5CB0C0), `LoopGroup` (0x5C11F0), `SkipAnim` (0x5C1710) | idle tree walk with conditions, loop a group a counted number of times, skip an animation | animation work beyond this package; `LoopGroup` and `SkipAnim` have 0 plugin uses, `PickIdle` 2 |

## What was run

Unit tests (`components-tests --gtest_filter='FalloutScript*'`): 1692 tests, all pass (the new ones: `iniInteger` and `goreDisabledBy` in
`testworldrules.cpp`, `knockedAnswer` and `KnockTracker` (3) in `testhitreactions.cpp`, the spine commands in `testactorstatus.cpp`,
`lockBroken` in `testlockpick.cpp`, the loading menu in `testworldcommands.cpp`). `openfallout-tests`: 837 of 837 pass. The Python
checks `test_round.py` (63 passed) and `test_coverage_dashboard.py` (18) pass. `falloutstart.lua` was checked with `luac -p` only.

Hidden rounds (New Vegas, newest save of the profile, `scripts/openfallout/round/rounds/m16-tail2-nv.json` with
`scenarios/m16_tail2.lua`: the two placed gangers of the camp taken to open ground, the answers read through `core.actorStatus.call` and
`core.worldCommands`):

- First run, PASS in 96 s, 0 errors. `GetKnockedState` of a ganger: 0 before, 1 for the first 1.0 s after `PushActorAway`, 0 from 1.3 s on (the
  knock lasts `fKnockbackTime`, 1 s); `GetForceHitReaction` 0. `IsGoreDisabled` 0 (the game's ini has no such setting: the default),
  `IsNoHumanGoreExe` 0, `IsWin32` 1 (no pad), `EnableLoadingMenu 1` then `IsLoadingMenuEnabled` 1, and 0 after `EnableLoadingMenu 0`.
  The spine of a standing ganger gave up-parts of 0.019 (Y) and 0.009 (Z): both above 0, so both commands answered 1. For a standing body
  the entries are near 0 and their sign is noise, so that is not a proof of the rule (it shows the bone is found and the commands read it).
- Second run (a check of mine failed, the rest is as above). `SetPackDuration 1` on a ganger whose package was of type 12 changed its
  package to one of type 5 between the first and the second second, while the other ganger (no duration) kept its package for the
  5 s: the package with a duration was ended, as the rule says (my first check wanted a stage past the list, which a package that is
  replaced at once never shows; corrected in the scenario). The spine parts of a "killed" ganger did not change: the `Kill` of the round tool had
  no reference (`KillActor on -`), so nobody died.
- Third and fourth run, PASS (92 s and 83 s, 0 errors): the same answers, the package check passes (g1 12 to 5, g2 4 to 4), and with the
  reference given the ganger dies (`KillActor on 0x11629a3`) and the spine's up parts, taken to the world, go from 0.018 and 0.009 standing to
  -0.9998 and -0.000004 lying: `IsFacingUp` 0 and `IsLeftUp` 0 for the corpse (it lies with the bone's Y axis pointing down; it was 1 and 1 standing,
  where the entries are near 0 and their sign is noise). So the bone is read and follows the body; which way a character must lie for 1 was
  not tried (the sign convention rests on the executable's entry [2][1]).
- The New Vegas autoplay of the new game (`m16-tail2-autoplay-nv`, the autoplay tool playing, the scenario waiting): the intro ran through
  to VMQ01 stage 100 ("intro over") at 775 s of real time with 0 errors and 0 scenario errors on a busy computer (swap in use, the new game took
  293 s to begin). The runner's verdict is FAIL only because the autoplay ends the engine when the intro is over before the scenario's
  last step ("scenario NOT finished"), as in the world package's autoplay round.

Not run: Fallout 3 (no placed-character ids were looked up; the profile has a save), the credits after `ExitGame` (a round that leaves the
game to the title screen has no such path), a character that lies face up for `IsFacingUp`, `bDisableAllGore` set to 1 (the game's
ini is read-only for rounds; the 1-test is a unit test), the lock break count, and the pad setting.

## Not done

All the rows of the table above, the end question of the credits, the proof of the credits and of `IsFacingUp` on real data, and the
writer of the lock break count.
