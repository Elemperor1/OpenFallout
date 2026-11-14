# M16 notes (damage: breaking things, losing limbs)

What the `damage` work package of M16 added, and what it rests on. Every rule and number below was read from the executables
(FalloutNV.exe unless a line says Fallout3.exe; addresses given) or from the game data. Nothing of the games' text or assets is kept
in the repository.

## What was missing

- A record with destruction data (`DEST`, `DSTD`, `DMDL`, `DMDT`, `DMDS`, `DSTF`) was skipped by the loaders of most record kinds, a blow
  on a placed object did nothing, `DamageObject` and `ClearDestruction` did not exist, `GetDestructionStage` answered -1 (a stand-in).
- `IsLimbGone` answered 0 (a stand-in), a kill never took a limb off, `GetCauseofDeath` answered a fixed value, and the commands listed under
  "Other commands" below were missing or stand-ins.

## Destruction (the rule, `components/falloutscript/destruction.*`)

`BGSDestructibleObjectForm::Damage` is at 0x475B20. `DamageObject` (exec 0x5D4B90) reaches it through the vtable slot 0x144 of the
reference (0x579220). The data:

| Part | Content |
|---|---|
| `DEST` | health (i32), the number of stages, flags |
| `DSTD` (one per stage) | health percent (byte), self damage per second (byte), index of the model, damage stage (byte), flags (1 cap the damage here, 2 disable, 4 destroy), explosion form, debris form, debris count |
| `DMDL`, `DMDT`, `DMDS` | the model of the stage, its texture hashes, its texture set |

- Health is kept in the extra data type 0x56 (ExtraHealth). A thing that has no extra data has its full health, the `DEST` health.
- For a character the function reads the actor value Health (the getter at vtable 8 of the value owner at +0xA4, with the number 0x10) as
  the health after the blow, and writes its result (including a health stopped by a cap stage) only into ExtraHealth (0x5D43C0, 0x41B6E0):
  none of its calls sets an actor value. So a cap stage does not keep a character alive that the blow has taken to 0 health; the port
  does the same (the death comes from the actor value), and `GetDestructionStage` of such a character follows the capped health.
- Whether a stage has taken a thing out of the world is told from its health and its stages (`DestructionRule::disables`), so a loaded game
  knows it without a saved mark (ClearDestruction after a load puts the thing back).
- The percentage is the low byte of `ceil(health * 100 / max)`: a float32 intermediate, and the maximum is widened without a sign (the
  ceiling is the helper at 0x476B20). A health above the most wraps around. A thing that has some health left is never counted as 0.
- A stage is crossed by a blow when `percent < before && percent >= after`. The stage's flags then act: 1 stops the health at the stage's
  own percentage, 2 disables the reference, 4 marks it destroyed. A health that reaches 0 marks it destroyed too.
- `GetDestructionStage` (exec 0x5D9AA0, evaluator 0x5A4C60, stage function 0x477430) answers the number of the first stage whose percentage
  is below the thing's, the number of stages when none is, -1 for a thing that has not been hurt.
- `ClearDestruction` (exec 0x5DA260, reset at 0x477D10) forgets the health and the stages: the thing is whole, not destroyed, enabled again.
- A stage's self damage is applied by the updater at 0x4772F0: the thing hurts itself by that much a second (it burns down through the
  following stages).
- Held state: `Destruction` (health, stage, destroyed flag, self damage, per reference) is saved in the section `destruction` (version 1).

What the host does with an outcome (`apps/openfallout/mwscript/falloutdamage.cpp`): the model of a crossed stage replaces the model of the
reference (`OFClass::ModelOverrides`, consulted by `getModel` of the ESM4 classes; the reference is taken out of the scene and put back so the
new model shows), a stage with the disable flag disables the reference, and a loaded game shows the state again. What a stage names as
explosion and debris is logged as "not made" (see the list at the end).

Records that take the data: activator, movable static, container, creature, character, door, talking activator, furniture, terminal.
`FalloutScripts::strike` (blows on actors) feeds the health of a character to the same rule (`destructionActorHealth`), and a shot at a
world object (`fire`, the object branch) damages it by the weapon's damage.

## Dismemberment (`components/falloutscript/dismember.*`)

The evaluator of the messy death is 0x8B43A0, the function that applies a cut 0x8B4D10, the roll 0x646E50. It is called from 0x899CB0
(at 0x89C199 and 0x89C1E6) only when the actor's damage function (vtable 0x338) answered "died", and, when the hit limb has a condition actor
value, only when that value is 0 or less.

Settings (GMST values, the same in both games' master files): iCombatExplodePartChance 75, iCombatDismemberPartChance 50,
iMessTargetedLimbExplodeBonus 0, iMessCrippledLimbExplodeBonus 25, iMessIntactLimbExplodeBonus 25, iMessIntactLimbDismemberChance 15,
iMessTorsoExplodeChance 10, fExplodeLimbRemovalDelay 0.03. The INI setting iActorsDismemberedPerFrame (registered at 0xFA87D0, default 2,
counter at 0x11E077C) limits the cuts a frame (`Dismemberment::takeSlot`). bDisableAllGore (the byte at 0x11DF7F8) is not modelled.

Body part data (`BPND`): flags at +0x60 (1 severable, 8 explodable, 0x40 absolute chance), the chance at +0x65. The condition actor values
of the limbs are 25 to 31 (`0x66EC10(4, i) = 0x19 + i`), the torso's is 26. 0x5E50F0 finds a part by its type, 0x5E5130 by its actor value.

- Roll (0x646E50): `r = rand() % 100`. The burst limit is `chance + explodeBonus` when the part's chance is absolute, else
  `trunc((iCombatExplodePartChance + explodeBonus) * chance * 0.01)`. The tear limit is `dismemberBonus + iCombatDismemberPartChance`. The burst is
  tested first.
- The weapon's On Hit (DNAM, read by 0x8A16B0) overrides the result for the hit limb: 1 a tear only, 2 a burst only, 3 neither.
- Bloody Mess (actor value 55) path: the hit part has the targeted bonus (a certain burst gives 100); a hit torso bursts if certain or by
  iMessTorsoExplodeChance; the other limbs (25 + i): a crippled one has the crippled bonus, a whole one comes in when certain, or when the torso
  burst, or when `random <= iMessIntactLimbDismemberChance`; at most 7 entries.
- Without it: only a hit part that is crippled (and either a certain burst or not the torso) or the parts of an explosion's source.
- `forced = explodable && (hit.flag 0x40 || (absolute && chance >= 100))`.
- Applying an entry: skip a limb that is gone already; roll (the weapon's override only for the hit limb); the part that goes is the first of
  the types 0 to 14 with the entry's actor value; a burst on a part that is not explodable becomes a tear; skip when it is neither severable
  nor explodable.

Cause of death (0x89C0B1): 3 by default; by the form type of the source: weapon 2, 0x2F 5, projectile 1, 0x3D/0x3F/0x40/0x69 1,
0x3E/0x51 0, 0x52 4; an explosion as the source gives 0. It is stored at +0x10 of the actor's extra data (writer 0x437730, getter
0x5730D0), the killer at +0xC (0x600AB0) and read at +0x14 by `IsKillerObject` (0x825C00). The limb mask is set by 0x430410 and read by
0x573090 (`IsLimbGone`).

State per actor (section `dismember`, version 1, 28 bytes an entry): form, limb mask (u16), burst mask (u16), cause (i32), killer, critical
stage (i32), the hour of death (f64).

## Other commands

| Command | Address | What it does here |
|---|---|---|
| `FireWeapon` | exec 0x5DA570 | the player fires through 0x87AA40 (action 0x20), a character through 0x523150; a form that is no weapon is a script error. Here: a one-attack scripted order and the same `fire` the characters use, at the one it fights |
| `PushActorAway` | exec 0x5D6B60 | the calling reference is the origin (vtable 0x1F4 gives its position), the parameter must be an actor; force is `0x646580(agility of the actor, number)`, then the process's vtable 0x418. Here the force of M14's `HitReactions::knockbackForce` and the same throw as a blow's |
| `AttachAshPile` | exec 0x5DB870 | puts a reference at the position of the character. Without a form: the default ash pile (see decisions) |
| `KillAllActors` | exec 0x5BE4F0 | every actor of the high process list (0x11E0E80) gets 0x89D900(actor, killer, 0.0), Kill. Here: every character and creature of the loaded cells but the player, each killed by `Actors::kill` (an essential one falls instead) |
| `ResetFallDamageTimer` | exec 0x5D10A0 | reads `[process + 0x544]` only: nothing to do |
| `GetForceHitReaction` | eval 0x5A5B30 | the byte at actor +0x1B1: stand-in 0 |
| `GetKnockedState` | eval 0x5A08C0 | 1 for the process knock states 3 and 4: stand-in 0 |
| `IsWeaponSkillType` | eval 0x5A0AB0 | the weapon is the reference's base when it is a WEAP; no weapon and skill 45 answers 1; else the weapon's skill actor value is compared |
| `IsWeaponInList` | eval 0x5A05E0 | the weapon (or the default Fists) is in the form list |
| `GetTimeDead` | eval 0x5A3A30 | `floor(gameDays * 24) - [process + 0x69C]`: the hour of death is saved |
| `GetConcussed` | eval 0x5A5020 | an active effect of archetype 33 (the gate byte at 0x11DBE50 is not modelled) |
| `SetCriticalStage` | exec 0x5DC010, apply 0x8A1A70 | the stage at actor +0x10C: 0 none, 1 goo start, 2 goo end, 3 disintegrate start, 4 disintegrate end |
| `IsInCriticalStage` | eval 0x5A2910 | compares that stage |
| `IsKillerObject` | 0x825C00 | the killer stored at +0x14 |
| `GetVATSValue` | eval 0x594E40 | an 18-case jump table at 0x5952BC over the V.A.T.S. state at 0x11F2250: structure only, not implemented |

## What ran

- Unit tests: `testdestruction.cpp` (23 tests: percent, stage, crossing, caps, flags, self damage, save), `testdismember.cpp` (30 tests: the
  roll at its edges, the plan of the messy death, cuts, the weapon's overrides, the per-frame slots, cause of death, save), the changed
  `testobjectstate.cpp`. `components-tests` 6523 passed, with the 15 failures that were there before (NifOsg, MiscToEulerAnglesZYX);
  `openfallout-tests` 836 passed.
- Hidden round `m16-damage` (New Vegas, `scripts/openfallout/round/scenarios/m16_damage.lua`): a placed car (325 health, six stages, a model
  of the wreck) went from stage -1 through 1, 3, 5 to 6 by blows of 30, 40, 60 and 200 and the self damage of the burning stages; the
  screenshots show the whole car, then the wreck with the door off, then the whole car again after `ClearDestruction`. Eight kills of two
  gangers at the head, torso, an arm and a leg took a limb in four of them, the mask and `IsLimbGone` agree, the cause of death was 1.
  `FireWeapon` fired the pistol and hit the player. `PushActorAway` moved a character 272 units. `SetCriticalStage` and `IsInCriticalStage` agree.
  `AttachAshPile` put a reference at the character. `KillAllActors` killed 27 of 27 characters of the loaded cells and not the player.
- Hidden round `m16-damage-fo3` (Fallout 3, start Wasteland:0,0) on the same checks with a placed electric box (50 health, activator, four
  stages) and two raiders: a first blow of 15 took the box to stage 2 (the model of the wreck at stage 2 replaced the sparking box, as the
  screenshots show), further blows to stage 4 and health 0, and `ClearDestruction` back to -1. Three of eight kills took a limb off (head,
  an arm, a leg), the cause of death was 1, `FireWeapon` hit the player, `PushActorAway` moved a raider 105 units, the critical stage and the
  ash pile worked as in New Vegas, `KillAllActors` killed 27 of 33 characters of the loaded cells (6 were dead already) and not the player. The
  round passed with 2 error lines: two landscape textures that are missing from the data at start-up, before any phase of the round.

## What a round could not show

- The part that is cut off is not hidden in the model (that needs the skin partitions of the body mesh handled in the renderer), and a
  critical stage does not change the body.
- The ash pile was placed, but the screenshot did not show it (the camera looked the other way).
- The explosion and the debris of a stage, mines, and the V.A.T.S. values have no effect in the port yet.

## Decisions and limits

- `AttachAshPile` without a form takes DefaultAshPile1 (local number 0x1B of the first master): the files hold two activators of that name
  (0x1B and 0x22) but the default-objects record has none, and the executable's default was not found. A choice, not from the executable.
- `mayBeOrdered` no longer excludes the player, so the player can hold a one-attack order (`FireWeapon` by the player).
- `KillAllActors` leaves the player out. Whether the executable's high process list holds the player was not checked.

## Not done

- No explosions and no debris of stages (they are logged as "not made" up to 10 times), no mines, `DetonatePlacedExplosives`, `DisableAllMines`,
  `PlaceAtReticle`, `ObjectUnderReticle`, `GetPlantedExplosive`, grabbed references (`IsPlayerGrabbedRef`, `GetPlayerGrabbedRef`),
  `EssentialDeathReload`, `ForceTakeCover`, `GetThreatRatio`.
- `GetVATSValue` and `SetVATSTarget` are not implemented. `GetKnockedState` and `GetForceHitReaction` are stand-ins returning 0.
- The BSDamageStage nodes of a model are not applied for a destruction stage without a model of its own (the damage stage is only logged).
- `bDisableAllGore`, the gore-less creature kinds, and the checks at 0x87F3D0 and vtable 0x1A0 in the apply function are not modelled. The
  list of the parts of an explosion's source (0x9B1720) comes only from the host and is not filled.
- Causes of death 4 and 5 (hazard and poison) are not produced.

## Review fixes

- The cause of death and the killing weapon are kept for every kill: `goreOfKill` records the cause before the gore guards, and the death queue
  carries the weapon of the killing blow (actors section version 2; version 1 saves load with no weapon), so a kill challenge asks the weapon
  of the blow and not the one in the hands when the death is told.
- `GetTimeDead` and the hour of death count the world's whole days times 24 plus the hour of the day (the executable floors the days
  passed, a number with a fraction there, times 24: 0x867E30).
- `ClearDestruction` after a load: a stage that disabled a thing is told from its health and stages (`DestructionRule::disables`), the model of
  a stage is given to an object made by a script when the load makes it (`adoptObject`), and the clear's work (model, mark) is done in the queued
  action, in the order of the blows before it. A blow and a clear change the state and the destroyed mark under one lock.
- `FireWeapon` puts a standing `UseWeapon` order aside for the shot and gives it back after it.

