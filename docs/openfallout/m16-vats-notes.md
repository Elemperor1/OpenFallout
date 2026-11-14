# M16 vats: V.A.T.S. values, mines and explosives, the reticle

Work package `vats` of M16. What the executables do is written with the address in FalloutNV.exe (Fallout 3 was not read
again; the commands exist in both and the port takes the same rules). What is a choice of the port says so. Nothing here
is game text or an asset: numbers, ids, record signatures and setting names only.

## Where the code is

| What | File |
| --- | --- |
| GetVATSValue rules, target marks, side angles, free area and visible distance, target height, threat | `components/falloutscript/vatsvalue.hpp/.cpp` |
| Mines and laid explosives: rules, state, save | `components/falloutscript/placedexplosives.hpp/.cpp` |
| Commands of V.A.T.S., the reticle, the threat, the stand-ins | `apps/openfallout/mwscript/falloutvatscommands.cpp` |
| Laying, the per-frame update and the explosion | `apps/openfallout/mwscript/falloutmines.cpp` |
| State of V.A.T.S. for the values (`vatsParts`, `vatsAttack`, `setVatsOpen`) | `apps/openfallout/mwscript/falloutvats.cpp` |
| Tests | `apps/components_tests/falloutscript/testvatsvalue.cpp`, `testplacedexplosives.cpp` |
| Rounds | `scripts/openfallout/round/scenarios/m16_vats.lua`, `m16_vats_fo3.lua`, `rounds/m16-vats.json`, `m16-vats-fo3.json` |

The edits to the shared files are small: `falloutscripts.hpp` (declarations and four members), `falloutscripts.cpp` (two
calls in the constructor, one call in the frame update, one hook in `fire`, the reset of the new state), `falloutsave.cpp`
(two save sections: `explosives` and `vatsmarks`).

## GetVATSValue (eval 0x594E40)

A jump table of 18 cases at 0x5952BC over the state of V.A.T.S. at 0x11F2250. The mode is the word at +8: 2 and 3 read the
data of the screen (the VATSMenu singleton at 0x11DB0D4, flag +0x118, data +0x11C), 4 reads the first action of the playback
list; any other mode, no action, or an index above 17 gives 0.

The action record: +0 the kind, byte +4 "the attack hits", bytes +6, +7 and +0x24, the target reference at +0xC, the body
part at +0x10 (an actor value 25 to 31, or -1), and behind +0x14 the hit data: the weapon at +0x30, what it shoots at +0x50,
the flags at +0x58.

| Index | Answer |
| --- | --- |
| 0 | the weapon is that form |
| 1 | the weapon is in that form list |
| 2 | the target's base is that form (0 without a target) |
| 3 | the target's base is in that list |
| 4 | the distance to the target (the only number that is not 0 or 1) |
| 5 | the body part is that number |
| 6 | the kind of the action is that number |
| 7 | the attack hits |
| 8, 11, 12, 13, 14 | the flag 0x04, 0x10, 0x40, 0x20, 0x80 of the hit data |
| 9, 10 | what it shoots is that form, or is in that list |
| 15 | the animation type of the weapon is that number (no weapon: true only for 0) |
| 16 | byte +6 or byte +0x24 |
| 17 | byte +7 |

What the code does (all of it tested in `testvatsvalue.cpp`): the table above, exactly. What is a choice:

* The names of the flags. The executable only tests the bits. The names `Fatal` (0x10), `LimbGone` (0x20), `LimbBurst`
  (0x40), `LimbCrippled` (0x80) come from what the camera paths (CPTH) of both games' files ask with each index (a path for
  a killing blow asks index 11, one for a limb that is gone 13, a crippled limb 14, the bloody-mess paths 12 and 13). That
  is inference, not a read.
* When the state exists in the port. The executable has the state while the game plays V.A.T.S.; the port has it in two
  places: `vatsParts` sets the selecting mode (2) with the target and the part while it works out the chance of each part, so
  that a perk whose condition asks GetVATSValue (the entry point 8 of the hit chance) sees the part that is aimed at, and puts
  back what was there; `vatsAttack` sets the playback mode (4) before the blow (so the perk conditions that the blow asks
  about see this attack, not the one before it) and again after it, with the hit, the kill (0x10) and the cripple (0x80) of the
  result. `setVatsOpen(false)` clears it. The flags 0x04, 0x20 and 0x40 are never set (nothing in the port says
  the blow took a limb off at that point), so values 8, 12, 13 answer 0.
* The kind of the action by the weapon. The executable's kinds are numbers of its own; the port writes 0 to 8 for a weapon of
  the animation types 0 to 8, 0x10 for a launcher, 0x11 for a grenade and 0x13 for a mine; the kind of an attack the player
  queued is 0x12 in the selecting mode (0x9CA2C0 writes it with the target and the part).

## SetVATSTarget (exec 0x5DAAE0), GetVatsTargetHeight (eval 0x5A25F0)

SetVATSTarget keeps a mark in bit 0x04000000 of the flags of the reference, which turns the answer of the record round: the
mark is set when what the script asks differs from what the record says and cleared when they agree (the record's flag is
bit 0 of its destruction header, read at 0x576100). The port keeps the set of marked references (`TargetMarks`, saved in the
section `vatsmarks`) and `vatsCanTarget` asks it. Characters and creatures count as targetable by record, so
`SetVATSTarget 0` marks one and `SetVATSTarget 1` clears the mark. The port has no V.A.T.S. target that is not a character.

GetVatsTargetHeight is 0 unless the mode is 4, the kind is an attack (the kinds 0 to 8, 0x10, 0x11 and 0x13 to 0x15, a table of
one byte for each kind at 0x66DE14) and there is a target; then it is the world height of the node of the part that is aimed at
(the target's own height when the part has no node) minus the player's. The port takes the height of the middle of the target's
box for the node (a choice: a node of a skeleton is not asked for from a script), and the player's height is its position.

## The areas around an actor (eval 0x8BD830 and 0x8BDBD0)

GetVATS{Front,Back,Left,Right}AreaFree: a ray from the actor's position, raised by the setting fVATSSmartCameraCheckHeight (64
in the files' defaults; the read of the setting is at 0x8BD893), along the heading turned by 0 (front), 1.5708 (right,
0x101FF38), 3.14159 (back, 0x102B3C8) or 4.71239 (left, 0x101FF34); 10000 units long (0x1022958). The answer is the distance
to the first thing it meets; the float maximum (0x1016970) when it meets nothing. The port casts the ray with the same
physics query the line of sight of the detection uses (the world, the heightmap and doors).

GetVATS...TargetVisible steps the start of the ray out along the side, one step at a time (fVATSSmartCameraCheckStepDistance,
at most fVATSSmartCameraCheckStepCount steps; 64 and 5 in the defaults), and answers the distance of the last step from which
the target is still seen; the first blocked step ends it. The port looks at the middle of the target as the same height as
the start.

The optional argument of the area commands (an object) was not read. The port ignores it.

## Mines and laid explosives

A laid explosive is a projectile (PROJ) in the world, the executable's GrenadeProjectile. What the record says decides:

| PROJ DATA flag | Meaning |
| --- | --- |
| 0x0004 alt trigger | with a proximity above 0 (+0x7C of the record, read at 0x45CD80) a proximity mine; with none it counts the record's timer (+0x80, 0x644790) down from the start |
| 0x0020 can be disabled | what DisableAllMines and a disarm can switch off |
| 0x0400 detonates | goes off when a script says so (DetonatePlacedExplosives) |

Rules read in the executable, all in `placedexplosives.cpp` and tested in `testplacedexplosives.cpp`:

* Is it a mine (the virtual function at +0x318 of the class, 0x9BDF80): the record detonates, or has the alt trigger and a
  proximity above 0.
* Age (0x9B4312 to 0x9B4388): a mine lives for fMineAgeMax (the executable's own default is 0, which is for ever), any other
  laid projectile for fGrenadeAgeMax (90 seconds); 0 means no limit. The update takes it away at that age without an
  explosion (0x9B41B0 and 0x9C3190).
* Radius (0x9C39E0): the proximity, times fMineExteriorRadiusMult (1.6) outside.
* Who sets it off (0x9C3930): not the one who laid it unless iProjectileMineShooterCanTrigger is above 0 (the default is 1: the
  player's own mine hurts the player); not a character in the faction that owns it; and, when a character laid it, only a
  character that the layer sees as neutral or an enemy (an ally or friend walks over it). The port asks
  `relationOfActors(owner, character)` for the last rule; the faction rule is not built (a faction member is told by the
  relation already).
* The player is looked at after the characters. It is rolled against a chance (the perk entry point 4 of 100, "Calculate Mine
  Explode Chance" in the executable's list; no perk of the port uses it yet) with `rand() % 100`; a roll that lets it pass is
  remembered (run-time flag 0x4000) and the mine does not ask again.
* Review fixes: a laid explosive keeps the place it lies in (the interior cell, or the worldspace outside) and only what is in that
  place sets it off or takes its blast (a mine outlives the cell it was laid in, and the cells of another place overlap it); a ghost
  takes no blast; what the player's charge takes off a character is the player's damage (the experience and the statistics of a kill
  follow it); a charge that is both a mine and a detonator goes off when told, whatever the proximity; a charge laid by an attack
  with the no-damage flag does no damage (explosives section version 2). PlaceAtReticle still puts the object in front of the
  player: the hit position of the reticle is known, but the placement path (`placeAtMe`) takes a reference, a distance and a
  direction, not a position.
* Follow-up fixes: every thing that is not a character and that the blast reaches hears of it as of a blow of a weapon (the
  last hit is noted, so its `OnHit` blocks run), a blast of an attack with the no-damage flag included; only the destruction damage
  stays conditional on the blast having damage. The ray that tells whether something stands in the way of the blast ends inside the
  box of such a thing (half extents of the world, standing on its feet), and what the ray hits within the length it travels inside
  that box is the thing and not a wall, so a large solid object does not keep the blast off itself (the world's ray cast has no
  reference to leave out). The box is upright and axis aligned, so for a thing that is turned or off centre the tolerance is off by
  that difference.
* Delay (0x9C3D1C to 0x9C3D5E): after it is set off a mine counts down fMinesDelayMin (0.2), and for the player, that plus the
  Explosives skill (floor, 0 to 100) times the record's timer over 100.
* Disarm (0x9C43E0): needs the can-be-disabled flag and not to be disabled yet; sets run-time flag 0x200; the player gets
  iMineDisarmExperience (5) when it was not silent and the mine had not been set off or would have been set off by the one
  who disarms it (0x9C4587). The port has no disarm command in the player's hands yet (see Not done); the rule is there and
  tested.
* DisableAllMines (exec 0x5DE110): every laid projectile for which the mine function says yes and that can be disabled is
  disabled, silently, with no experience. DetonatePlacedExplosives (exec 0x5DDF90): the projectiles that the actor laid whose
  record detonates get run-time flag 0x100000 and go off at the next update. The run-time flags are the word at +0xC8; the
  timer of a mine that has not been set off is the float maximum (0x10231B0).

How the port lays and updates them (a choice, no model of the projectile exists in the port):

* A weapon of the animation types land mine (11) or thrown mine (12) lays the projectile of its record 70 units in front of the
  attacker where it stands, instead of striking at a target; a thrown grenade (10) with no target lies 400 units ahead.
  A grenade thrown at a target (V.A.T.S.) keeps the old blow at the target. The projectile is not a reference of the world: it
  is a row in `Laid`, so it has no model, no light, no sound and cannot be picked up or seen.
* Each frame (`updateExplosives`) the rules run over the laid ones: the characters near are the loaded actors in the same kind
  of cell (exterior or interior) within the radius by their position, no line of sight check for the trigger.
* The explosion (`explodeLaid`) takes the explosion record (EXPL) of the PROJ: every character and the player in its radius,
  with a clear ray from just above the blast to the middle of the target (a gap of fExplosionLOSBuffer, 16 when the files have no
  value), loses the record's flat damage. The player's god mode is honoured. What the port does not do is what the executable
  does with the distance (a falloff), with armor and with the difficulty: none of these was read, so none is built, and
  the flat damage is a choice. The health goes down by the actor-value damage, the destruction rule sees it, the victim
  is told it was hit by the one who laid the mine and dies by the same path as a blow (`setDead`, the essential case falls).
  Objects in the radius with a clear ray get the same number through `Destruction::damage`, so a car or an electric box goes
  through its stages.
* A blast is a hit on every character it reaches, as a blow is: the victim hears of it (`OnHit`, the challenges) even when the attack
  was to do no damage, a blast of the player's goes through the law's blow handling (the assault crime, with the exception of a
  friend or ally that tolerates the hit), and a survivor and the owner are put in a fight. The weapon of the attack that laid the
  explosive is kept with it and named by the hit and by the death that the blast causes, so what is filtered by weapon sees them.
* State is saved in the section `explosives` (every laid projectile with its record, owner, place, age, timer, run-time flags,
  the character that set it off, whether its blast does no damage, and from version 3 the weapon that laid it; versions 1 and 2
  still load, with no weapon) and reset on load or new game.

## The reticle, grabbing, threat, cover, essential death

* ObjectUnderReticle: a ray from the camera along its look direction, up to the first number (the executable cuts the range at
  50000); the answer is the number of the object it hits, 0 for none. The optional second number was not read.
* PlaceAtReticle: makes the base at the place the camera ray meets the world. The port has `placeAtMe` only for a place next to
  a reference, so it puts the object in front of the player at the horizontal distance of the hit and at the player's height.
* IsPlayerGrabbedRef, GetPlayerGrabbedRef: the port has no grabbing, so they answer 0 (a stand-in).
* GetThreatRatio: the executable's threat of one actor is its level times its health, divided by one minus the share that the
  damage threshold stops (the threshold over 30, at most 0.99). The port answers the threat of the actor in the argument over
  the threat of the actor the command is called on. Which of the two is on top was not read: this is a choice.
* ForceTakeCover: accepted and does nothing (the AI has no cover); a stand-in.
* EssentialDeathReload: asks the state manager whether to go back to the last save (what the death menu does), on the thread
  of the frame.
* GetPlantedExplosive: answers 0 (a laid explosive of the port has no reference); the vtable function it calls on the actor's
  process (+0x388) was not read; a stand-in.

## What was run

Unit tests (`components-tests --gtest_filter='FalloutScript*'`: 1723 passed; the V.A.T.S. and explosives suites alone: 57;
`openfallout-tests`: 837 passed). `testvatsvalue.cpp` covers every index of GetVATSValue, the modes, the marks and their save,
the sides, the free area, the visible distance, the target height and the threat; `testplacedexplosives.cpp` covers the
settings, the mine test, the ages, who triggers, the delays, the player roll, the timed charge, the detonator, DisableAllMines,
the disarm experience, expiry, and the save (including the cause of a mine that is counting down and a cut-short save).

Hidden rounds (`round.sh`, New Vegas content, the scenario `m16_vats.lua`; the engine log is the evidence). The last run, on the
branch after the merge of `m16`, passed all 18 checks (`RESULT 18 passed, 0 failed`); the earlier runs found two faults, one in
the engine and one in the scenario, which are told below.

* Queries with V.A.T.S. closed: all 18 values 0. The four free areas answered 88, 819, 321 and 292 units (a run before the last);
  the visible distances 320 or 0 (a blocked step gives 0); GetThreatRatio 0.675; the grabbing queries 0; GetVatsTargetHeight 0.
* SetVATSTarget: a placed ganger 59 units away is a target; after `SetVATSTarget 0` it is not; after `SetVATSTarget 1` it is again.
* A V.A.T.S. attack with the bare hands on that ganger (6 parts, 95 % each; the head hit for 2.5): while it plays, the values
  that are not 0 are 4 (the distance, 59.45), 6 (the kind with the form 0), 7 (hits) and 15 (the kind of the weapon with 0, no
  weapon); 11 (fatal) is 0 as the attack was not fatal; 5 with the part's group (25) is 1; GetVatsTargetHeight is -32.15 (the
  middle of the target's box less the player's height). When the screen closes every value is 0 again.
* A frag mine (PROJ flags 0x66, proximity 100, timer 3, explosion damage 100 radius 192) laid with FireWeapon went off 0.15
  seconds after it was laid when the character under it came into its radius, "set off by" that character's reference; its
  health went from 42.5 to -57.5 and it died. The same death counts as a crime of the player (reputation and the law reacted),
  as any kill by the player does.
* DisableAllMines: a second mine, laid away from anyone and disabled, then a character put on it: no damage (45 to 45). (The scenario
  now lays the mine and disables it in one step, with no frame between them, so that the mine cannot be set off first. It still
  cannot see that the mine was armed when it was switched off: the host has no query for the laid mines.)
* Plastic explosive (PROJ flags 0x462, the detonate flag, no alt trigger): lay it, four seconds with a character beside it,
  nothing happens (health 45); `DetonatePlacedExplosives` at the player: 250 damage, radius 512, the character died, 64
  destructible objects in the radius took the damage through the destruction rule.
* The reticle commands answered (ObjectUnderReticle returned a reference number; PlaceAtReticle of form 0 answered 0).

The two faults found on the way: (1) `core.vats.canTarget`, `describe`, `parts` and `attack` took only the objects of local
scripts, so a call from a global script (a round scenario) got no target; fixed in `vatsbindings.cpp` by taking the common
base class. (2) The first scenario called DisableAllMines 0.3 seconds after it laid a mine under a character, and a mine goes
off after the shortest delay of 0.2 seconds; the scenario was wrong, the rule is as the executable has it.

Fallout 3 (`m16_vats_fo3.lua`, two placed raiders of cell -3,0, same checks without the three New Vegas commands: the command
table of Fallout 3 has no ObjectUnderReticle, PlaceAtReticle or DetonatePlacedExplosives): `RESULT 17 passed, 0 failed`. The raider
was a target at 51 units, had 6 parts (95 % each), the hands attack hit the head for 2.0 and the values while it played were
4 (50.9), 6, 7 and 15 (all 1 or the distance), GetVatsTargetHeight -9.6, GetThreatRatio 1.2; the frag mine killed the raider
under it (57.975 to -42.025) and a disabled mine left the second raider's health at 65. An earlier Fallout 3 run failed
on the scenario: it laid the disabled mine 330 units from a raider that walked up to the player and stood on it before
DisableAllMines ran, so the scenario now lays it 1500 units away.

## Not done, and why

* **The cut limb is not hidden.** A node-visibility route needs the limb's geometry apart from the body. The head and the hands
  could be hidden by marking biped slots covered in `falloutNpcParts`, but the arms and legs are part of the skinned
  upper-body and lower-body meshes; hiding them needs the renderer to split the mesh by bone partition, which is renderer
  work the package does not allow. Rebuilding the actor while it dies would also reset its animation. The rule that says a
  limb is gone is `IsLimbGone` of the damage package; what it shows is still the whole body.
* No grabbing (see above), no cover, no model for a laid mine, no explosion effect, light or sound, no flight of a thrown
  grenade, and no disarm in the player's hands: the rule and the experience are in `Laid::disable` but nothing calls it yet
  (the executable's disarm is the activate of the laid projectile, which needs it to be an object).
* The explosion's falloff, armor and difficulty, and the order of the damage of an explosion that hits many characters, were not
  found in the executable.
* The optional arguments of the area queries, of ObjectUnderReticle and of PlaceAtReticle were not read.
* The faction rule of the mine trigger is covered by the relation only.
* GetVATSValue flags 0x04, 0x20, 0x40 are never set, and the kinds of action are the port's own numbers.
* DetonatePlacedExplosives has a round only on New Vegas (the plastic explosive is the one weapon of either game whose
  projectile has the detonate flag: PROJ 0x130043, flags 0x462, no alt trigger); Fallout 3's files have none.
