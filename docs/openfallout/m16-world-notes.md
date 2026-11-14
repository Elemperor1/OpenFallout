# M16 "world": the commands about the world, the machine and the odd corners

This is the package of about 100 script commands that are about the world itself: line of sight and cells, the sky and
the clock, the maths, the machine the game runs on, the screens some commands open and a few corners that are left over
from the casinos and the debug menus. The rules are in `components/falloutscript/worldmath.*`, `worldrules.*` and
`worldcommands.*` (pure, with a host interface that the tests fake), the engine side is
`apps/openfallout/mwscript/falloutworld.cpp` (`WorldHost`). Every address is of the retail executables
(`FalloutNV.exe`, `Fallout3.exe`); where the two differ the Fallout 3 address is given after the New Vegas one. The
executable wins over the GECK wiki wherever they differ.

A condition that nobody answers is 0, and a quest that waits for it never goes on, so the cheap answers were made exact
first. The state that has to outlast a save (tables of switches) is kept in `Switches`, which the game's save already
writes (keys `World.<Table>.<hex>`); no new section was added to the save.

## The maths (`worldmath.cpp`)

`Sin` (exec 0x5DC0F0, Fallout 3 0x52FF60), `Cos` (0x5DC1B0, 0x530030), `Tan` (0x5DC290, 0x530100), `Sqrt` (0x5DC350,
0x5301A0) and `Log` (0x5DC3B0, 0x530200).

- The arguments are 32 bit floats. An angle is in degrees: it is multiplied by the double 0.01745329238474369
  (0x1023128) and made a float before the library's float function.
- A second argument that is not 0 asks for the inverse (arc sine, arc cosine, arc tangent). The arc sine and cosine
  take the value into -1..1 first; the library's double function is called and its result kept as a float, then
  multiplied by the double 57.2957763671875 (0x102F248) to give degrees.
- `Log value base`: the quotient of two natural logarithms, each taken in double precision and kept as a float; a base
  of 1 gives 0; a call that leaves the base out uses the float 2.7182817 (0x103D018).
- `Sqrt` is the float square root; a negative value is NaN.

## Line of sight and cells (`worldrules.cpp`, `falloutworld.cpp`)

- `GetLineOfSight` (exec 0x5C1CE0, eval 0x59C990; Fallout 3 0x523370, 0x515850).
  - The player as viewer: the target has to be in the view of the camera, else 0 and no ray is cast. Then up to three
    rays from the camera, at 0.75, 0.5 and 0.25 of the target's height (the doubles at 0x101DE30, 0x1011588 and
    0x10290B0), the first that reaches it gives 1. Else the actor's own test, which for the player holds the near
    distance only.
  - Any other viewer (the function of the actor is 0x88B880): a dead or unloaded target is not seen; a target within 2
    units (the double at 0x1011590) is seen whatever lies between; the view cone is asked (the bearing minus the
    facing, strictly below half of `fDetectionViewCone`, 190 in the files of both games, 0x88C570); with
    `SetAllVisible` (below) that is enough; else the rays from the viewer's eyes, three of them when the target is the
    player or the viewer's own combat target (0x1016264, 0x1016248, 0x101622C) and otherwise the first only. The code
    path that asks the actor with a flag of 0 never casts a ray: it reads cached detection bytes (not built here: the
    rays are cast every time).
  - The engine side takes the camera's view and projection matrices for the frustum (the target's feet, middle and
    top), a ray cast of the physics world for each ray, and shortens the ray's end by the length the ray travels inside the
    target's own box (`World::depthInBox`), so that the ray's hit on the target itself is not taken for a wall. That
    tolerance is mine; the executable ignores the target in its ray cast. The box is built from the half extents the world
    gives for the target (the physics ones for a character, those of the model for anything else), upright and axis aligned
    around the reference's position with its feet at it. The offset and the turn of the target's own collision shape are
    not used (the world interface does not give them), so for a target whose shape is turned or off centre the box, and with
    it the tolerance, is off by that difference. Not run on real data except as below.
- `SetAllVisible` is a switch the rule reads.
- `GetInCellParam` (exec 0x5C7590, eval 0x59EF60; Fallout 3 0x5253A0, 0x5100C0): `_strnicmp` of the name of the cell
  the reference is in against the name of the asked cell, over the length of the asked name: a prefix test, not an
  identity test. The name is the full name of the cell record (an exterior cell has none).
- `IsInDangerousWater` (eval 0x5A36B0): an actor (virtual +0x100 of the reference) whose in-water flag (+0x14C of the
  actor) is set and whose water type has bit 0 of the flag byte of the water record (0x580060: the byte at +0x71, the
  FNAM flags, "causes damage"). The water type is that of the actor's cell, else of its worldspace; the engine stands
  its feet below the water level of the cell in for the flag, and does not look for the type the executable falls back
  on (0x11CA53C).
- `IsWaterObject` (eval 0x5A45C0): a virtual call at +0x140 of the reference, which the executable's own log calls
  "IsWaterActivator". The virtual itself was not resolved: the engine answers 1 for a reference whose base is an
  activator that has a water type (a sink, a toilet, a fountain), which is what the name says.
- `GetIsObjectType` and `GetIsFormType`: the number is the index of the executable's table of record signatures
  (FalloutNV.exe 0x1187004, 12 bytes an entry; Fallout3.exe 0xF4A74C); Fallout 3 has the numbers up to 0x66, New Vegas
  adds 0x67 to 0x78. The type is that of the base record of the reference.
- `GetParentRef` answers the reference the enable state of the asked one follows (the XESP of its record).
- `GetStartingPos`, `GetStartingAngle` and `SetAtStart`: the pose the record gives a reference, or (for a reference
  that was made while the game runs) the pose it had when first asked; `SetAtStart` rewrites it from the pose the
  reference has now (0x5C0BF0). Angles are given in degrees.
- `GetDefaultOpen`, `SetDefaultOpen` (0x5CEBF0: bit 8 of the open-state extra is opened by default, bit 4 is open now):
  a door that is set open by default opens at once and one that is set closed closes (the engine's `OpenStates`).
- `ModScale` (0x5C1C40): the scale after the call is the scale now plus the argument, taken between 0.5 and 2, then
  rounded to two decimals and kept between 0.01 and 10 (0x5C1C10, 0x567490). `Rotate` grows an angle by degrees a
  second for the seconds of the frame, kept from 0 to a full turn.
- `PersistentRefCount` counts the references of the loaded plugins that carry the persistent flag (0x400).
- `IsWin32` (0x5A3870): 1, unless a pad of the Xbox 360 kind is connected and `bDisable360Controller` does not turn
  it off. The engine side asks SDL for a game controller; the setting is not read (always "not off").
- `EnableNavMesh` and `DisableNavMesh` take the form id as hexadecimal text (a `%x` scan in the executable) and keep
  the switch; `SetLastExtDoorActivated`, `RemapWaterType` (0x580680: the same type takes the remap away),
  `TriggerLODApocalypse` and `SetImageSpace` keep their value.

## The sky and the clock

- `GetIsCurrentWeather` (0x59EBB0) looks at the weather being shown only. `GetCurrentWeatherPercent` (0x59EB60) is the
  field the executables call the percent of the weather (sky + 0xF4): 0 at the start of a change, 1 when none goes on.
  `IsCloudy` (0x59E7F0) and `IsPleasant` (0x59E700) are not yes or no but the share of the change that is done when
  the weather shown has the classification bit (0x02 cloudy, 0x01 pleasant) plus the share left when the weather it
  comes from has it. `IsSnowing` (0x59EA10): the weather shown is snow (0x08) and the change is past where its
  precipitation begins (the byte over 255 times 0.999), or the weather it comes from is snow and the change has not
  got to where its precipitation ends (0.001 plus the byte over 255 times 0.999). `GetWindSpeed` (0x5A1880): the wind
  byte over 255 of the weather shown, blended with that of the weather it comes from by the share of the change
  (0x63C490).
- The host reads the engine's weather manager: its current weather is the one that is left while it changes to the
  next, and its transition factor the share of it that is left, so the percent is 1 minus the factor.
- `SetWeather` sets the climate's weather from now on (or holds it over the climate's when the second argument is not
  0) through the host's existing forced-weather call. `IsTimePassing` is true while a rest or a wait has hours left.
  `SetGlobalTimeMultiplier` sets the simulation time scale of the world (1 when the call gives none).

## The machine and the screens

- `ExitGame` sets the global `PlayCredits` (2 when the player has the perk the executable names in New Vegas, 0xED568,
  and 1 otherwise; always 1 in Fallout 3) and goes back to the main menu through `StateManager::endGame`. What shows
  the credits when the menu opens is not wired (see "Not done").
- `GetDLCCount` (0x5DF930, New Vegas only) counts the nodes of a list held by a lazily made singleton (0xAF26C0, list at
  +0xC). What fills the list was not traced; the engine counts the loaded plugins whose names are those of the add-ons.
- `IsGoreDisabled`, `IsNoHumanGoreExe` (a fixed 0, 0x5DFDE0), `SetAudioMultithreading` (the executable only turns the
  threaded audio on when `bMultiThreadAudio` is set, which the default file leaves 0, 0xAD7230), `WhichServiceMenu`
  (never given a value: -1, 0x119F344).
- `GetPlayerControlsDisabled` (0x5CAF40): seven flags (movement, Pip-Boy, fighting, point of view, looking, the text
  under the crosshair, sneaking) with the bits 0x01, 0x04, 0x08, 0x10, 0x02, 0x20, 0x40 of the player's control byte
  (the labels at 0x118C608); a flag the call leaves out is 1; the answer is whether any chosen group is disabled.
  `IsPC1stPerson` asks the camera.
- `IsPlayerActionActive` and `GetLastPlayerAction` read the player's list of actions (the list at +0x60C of the player,
  0x963CE0, 0x963EB0, 0x963BF0): records of an id, the seconds left and a reference, newest first; raising an id that
  is in the list takes the old record away; every frame takes the frame's seconds off and drops the records that have
  none left except ids 5 and 8. Nothing in the engine raises an action yet, so both answer "none".
- `PlayBink` plays the video through the window manager when there is a window to play it in (the player of a video
  holds the frame until it is over, which a hidden window never gets to); otherwise it logs and goes on.
- `ForceTerminalBack` is the host's existing terminal back. `ShowComputersInterface` and `ShowHackingMiniGame` open the
  terminal of the reference. `ShowTutorialMenu` shows the message. `ShowAllMapMarkers` (0x5B6D40): the first flag shows
  or hides, the second makes the shown ones places to travel to, the third leaves alone the markers whose record flag
  0x04 ("show all hidden") is set; both are 1 when the call leaves them out.
- `ShowSleepWaitMenu` (0x5E0090): a first argument of 1 goes another way (0x969FA0, on an object of the player's that
  nothing here has) and does nothing; otherwise the screen that asks how long to sleep (second argument 1, 0x7054F0) or
  wait opens, for no bed in particular.
- `ShowLockpickMenu`, `ShowLockpickMenuDebug`, `ShowViewerStrings` and the three plain casino menus (0x5CF040, 0x5CF0F0,
  0x5CF1A0: the executable reads a casino that the command does not give) log once and do nothing.

## Blood on the screen and the shake of the interface

- `TriggerScreenSplatter` (0x4DE8E0) makes a group of quads (count, duration, size and opacity multipliers);
  `TriggerScreenBlood` (0x4E0330, Fallout 3 0x492E70) the plain kind, which stays within `iBloodSplatterMaxCount`.
  The groups are timed in the frame (0x4DE600): in a fade mode other than 0 the alpha falls over the last `fade`
  seconds; in mode 0 the number is a threshold and the ramp runs from `fBloodSplatterFadeStart` of the group's time
  (the executable's own quirk, so the alpha can pass 1 before it falls). `SetScreenSplatterTexture` takes slots 0 to 2
  (0x4DEED0), `ClearScreenSplatter` (0x4DEFB0) clears the groups and sets the fade to 0 with mode 0. The defaults of
  the settings are the ones the executable has (0x11CF9A4 and its neighbours). Nothing is drawn: the interface has no
  splatter layer.
- `TriggerHUDShudder` (0x7F7880, 0x7F7970): x offset = sin(t * frequency) * amplitude * (1 - t / duration) for t
  milliseconds in; defaults 500 ms, 0.1 and 0.05 radians a millisecond. The offset is worked out every frame; the
  interface does not move by it.

## Odd corners

- `SetSecuritronExpression` (0x437F90): the two words joined by an underscore in the folder of the securitron
  textures; the shape is `Screen01:0`. Done through the host's texture swap.
- `SetPackDuration` (0x8F4020): the package is over when the game hours since it began, times 100, reach the duration
  (the duration is hundredths of an hour; 0 is no limit). The rule is written; the engine's packages do not read it.
- `HasBeenEaten`, `SetMinimalUse` (0x5DC5E0: the door's own record is changed, so every reference to that door is),
  `ToggleDemigodMode` (0x5B7000: both flags go to the opposite of the demigod one), `AddCardToPlayer`, `RedeemCode`,
  `IsCodeRedeemed`, `RevokeCode` (a code is kept by its name in lower case; these take text, not forms).
- Eleven commands are a dummy in the executables (`mov al, 1; ret`: 0x5D4A40, Fallout 3 0x52A5E0):
  `EnableLinkedPathPoints`, `DisableLinkedPathPoints`, `DumpMasterFileSeekData`, `StartMasterFileSeekData`,
  `PrintTrackedCells`, `EnableCellTracking`, `ClearTrackedCells`, `ShowClassMenu`, `ShowSurgeryMenu`, `TrapUpdate` and
  `SetCasinoCheatLevel`. They answer 0 and are listed as exact in `coverage/stand-ins.csv`.

## The tool for rounds

`core.worldCommands` (`apps/openfallout/mwlua/falloutworldbindings.cpp`, in Lua `r.core.worldCommands`) runs a command of
the script language by its name, the way a condition calls it: `run(name, reference, arguments)` queues the call for the
thread of the frame and gives a number; `result(number)` gives the answer once the frame has run it (nil before). A
reference and an argument are numbers, forms as text (`"0x..."` or `"FormId:0x..."`) or text; a text that is a form
also carries its number. Round `rounds/m16-world-sky.json` with `scenarios/m16_world_sky.lua` uses it.

## What was run

Unit tests: `components-tests --gtest_filter='FalloutScript*'` ran 1,377 tests and all passed (the three new files
`testworldmath.cpp`, `testworldrules.cpp` and `testworldcommands.cpp` hold 10 + 48 + 47 of them, and the map marker file has five new ones); the whole `components-tests` run has the 15 known failures (the NIF loader and the Euler angle tests) and
no other; `openfallout-tests` passed 836 of 836. `test_coverage_dashboard.py` passed (18 tests); `coverage_dashboard.py`
counts 345 commands done (278 before) and 54 stand-ins (35 before).

Round `m16-world-sky` (hidden, New Vegas, newest save of the play folder), first run: PASS in 84 s, 0 scenario errors,
0 errors in the log, the engine frontmost in 0 of 17 samples. It showed on real data: `Sin 30` 0.5, `Cos 60` 0.49999997,
`Tan 45` 1, `Sqrt 144` 12, `Log 8 2` 3, `Sin 0.5` with the inverse flag 29.999999 (degrees); `IsWin32` 1, `GetDLCCount` 9,
`IsPC1stPerson` 1, `WhichServiceMenu` -1, `PersistentRefCount` 20,345; the time scale 1, then 0.5 after
`SetGlobalTimeMultiplier 0.5`, then 1 again after the call with no argument; at the start of the sky the percent 1, pleasant
1, cloudy 0, snowing 0, wind 0.196 (50 of 255); after `SetWeather` of a weather record `GetIsCurrentWeather` of it
answered 1 and of the other 0, and after the second `SetWeather` the other way round. What that run did not show (the
scenario named the player by a Lua id, which is not a script form id, so every call on the player failed to parse):
the form types, the cell prefix test and the line of sight.

Second run of `m16-world-sky` (the player named by the number the scripts know it, 0x01000014; the first plugin of the
load order is index 1, so the base game's numbers carry it): PASS in 92 s, 0 errors. The player's base form is of type
0x2A: `GetIsObjectType 0x2A` 1, `GetIsFormType NPC_` 1, `GetIsFormType DOOR` 0 (a first attempt answered 0 for all of
these: the number the engine gives a record has one bit set that no letter has, which `signatureOfRecordType` now leaves
out). `GetInCellParam` of the player's own cell 1. `GetStartingPos X` of the player 2257.04, which is where it stands
(a reference that has no record places it keeps the pose it had when first asked). Line of sight with the camera aimed
at a character 604 units away: player to character 1, character to player 1; with the camera turned half a turn away:
player to character 0 (the frustum), character to player still 1. `IsWin32` answered 1 in the first run and 0 in the
second: the host asks SDL for a game controller, so the answer follows what was plugged in on the Mac; the cause was not
looked for. `SetWeather` is instant in this
engine (the engine's forced weather jumps), so the percent stays 1 and no value between 0 and 1 was seen on real data;
the blend is only in the unit tests.

Round `m16-world-misc` (hidden, New Vegas, newest save): PASS in 37 s, 0 scenario errors, the engine frontmost in 0 of 8
samples. Map markers: three marker references of the loaded cell group answered 0, after `ShowAllMapMarkers` with show 2
(visible and travel allowed), and 0 again after the call with hide. Codes: `IsCodeRedeemed` 0, `RedeemCode` 1,
`IsCodeRedeemed` 1, `RevokeCode` 1, `IsCodeRedeemed` 0. `GetPlayerControlsDisabled` with each of the seven groups chosen
alone answered 0 for movement, 1 for looking, 1 for the Pip-Boy, 0 for fighting, 0 for the point of view, 0 for the
rollover, 1 for sneaking (the chosen groups differ in their answers, which is the proof the mask is read per group; the
three disabled groups are what the loaded save recorded, not something the round set). `ShowSleepWaitMenu` opens the wait
screen: `IsTimePassing` stays 0 until the screen starts a wait, which nobody did. `GetStartingPos` X/Y of the player
2257.04/2318.22 and `GetStartingAngle` -149.13, `GetDefaultOpen`/`GetParentRef`/`IsWaterObject`/`IsInDangerousWater` of the
player 0, `IsWin32` 0 (a controller was plugged in). `PlayBink` of a file that does not exist logged one error line
("Failed to open video") and went on: the window of a hidden round counts as shown to the window manager, so the host
handed the name to the video player. The splatter, blood, fade, texture, clear and shudder commands answered 0 and
changed nothing on screen (the drawing is not done).

Round `m16-world-autoplay-nv` (hidden, New Vegas, new game, the autoplay tool of `scripts/openfallout/autoplay` playing, the
scenario only waiting): the regression check that the new commands do not break the intro. The intro ran through
quest VCG01 stages 1 to 115, 200, then VCG02 stage 5, VCG04 stage 0, and VMQ01 stages 10 and 50 (the autoplay's own
gates for VMQ01 worked), with 0 error lines in the log and 0 scenario errors but the time limit. The round's verdict is
FAIL for that reason alone: the new game needed 292 s before the scenario could begin (a busy computer, 9 GB of swap),
the scenario waits 660 s, and the limit is 700 s, so the runner stopped the engine at the limit with the game running
well. An earlier milestone measured the New Vegas autoplay at 324 s in all on a quiet computer.

## Not done

- `SetSize` (0xC708A0): it sets the size of the collision capsule of a character (2 for a size of 0 or less, at most
  half of the controller's 10); the rule is written, the engine's characters collide with the box of their model, which
  nothing resizes.
- `SetRigidBodyMass`, `Reset3DState`,
  `IsGoreDisabled` (the setting is not read), `SetPackDuration`: the commands are taken in; the engine side does
  nothing or answers 0.
- `IsFacingUp` and `IsLeftUp` (0x5A0710, 0x5A0800: the orientation of the spine bone of a character), `EnableLoadingMenu`
  (0x5DFBA0: shows or hides the loading menu through 0x457D70), `IsLoadingMenuEnabled` (0x5DFC10: a pointer at
  0x11DA0C0 is not null), `ResetPipboyManager` (0x5DB490), `SetCellImageSpace` and
  `SetLocationSpecificLoadScreensOnly` are read but not built.
- The credits after `ExitGame`: the global is set and the game goes to the main menu, but the menu script does not
  start the credits screen from it.
- Not drawn: the screen splatters, the shudder, image spaces, the water remap and the distant-object apocalypse (the
  state is kept, answered and saved).
- `ShowDialogSubtitles` keeps its switch; the conversation window does not read it. `AlwaysShowActorSubtitles` is read
  where the lines that characters say outside of a conversation choose their subtitles (the line is shown for a
  character that has it set, whatever the subtitle setting says).
