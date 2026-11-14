# M16 spawn2 notes (the level the cells' own actors are rolled for, and the rest of the spawn rules)

Second wave of the `spawn` package. A rule below was **read in the executable** (FalloutNV.exe; the addresses are given; Fallout3.exe
has the same commands and was not read function by function), **seen in a running game** (a hidden round, "What was run"), or
**chosen because nothing says it** (marked so). The games' own text is never quoted here.

## What the executable does with a levelled actor that a cell places

- **A placed reference whose base is a levelled character or creature is not allowed.** The initialisation of every reference
  (0x55D760, the function that also reads the location and the scale) checks the type of the base form (0x2C levelled creature, 0x2D
  levelled character) and logs an "invalid base object" message for it (0x55D7F8 to 0x55D80B). Both games' plugins agree: counted over
  Fallout 3 with its five add-ons and New Vegas with its ten, **0 placed characters or creatures have a levelled list as their base**
  (`esmtool references`: no LVLN or LVLC row). So the "placed leveled actors" of the package brief are the characters and creatures
  whose *template* is a levelled list (TPLT). Counted on the references that cells place: New Vegas 7681 characters and creatures, 4285 of
  them with a levelled template (1989 LVLN, 2296 LVLC), 2304 with a levelled list in their inventory; Fallout 3 7650, 4116
  (1369 LVLN, 2747 LVLC), 1458.
- **When.** At the initialisation above: the actor's base data (+0x30 of the base record) says whether its template is a levelled
  list (0x47CDB0 and 0x47CDD0: the template pointer at +0x24 has form type 0x2C or 0x2D), and the actor has no
  "levelled creature" extra data yet (0x56AFC0, extra type 0x2E), 0x47CE10 resolves it (called at 0x55DC8A). Once: the extra data (the
  original base at +0xC, the resolved one at +0x10; 0x4311F0, 0x421750, 0x4216F0) stays with the reference and the save keeps it.
- **The level (0x488350, 0x567E10, 0x567D20, 0x526190, 0x4214F0).** The list is rolled for `scaled = max(trunc(level * multiplier), 1)`,
  where `level` is 0x567E10 of the actor with the "rotate" flag on, and the multiplier is the one of the level modifier extra data
  of the reference (0x4214F0 reads extra type 0x1E: a float and the modifier; no extra data gives 1 and 0). 0x567E10 takes the
  zone of the reference (0x567D20: the reference's own encounter zone extra data, else its cell's zone, else the worldspace's); with a
  zone it is the zone's level (0x567EC0: the zone's own level word at +0x2C, or 0x526190), without one the level of the player
  (0x87F9F0); then the counter of 0x11CA420 goes up by one for every call and starts again at minus `iCalcLevelAdjustDown` once it is above
  `iCalcLevelAdjustUp` (defaults 1 and 1, in no file), and the result is not below 1. The multipliers are the fLeveledActorMult
  settings (0.5, 0.75, 1.25, 2.0 for the modifiers 0 to 3, in the files of both games). The first wave already had all of this for
  `PlaceLeveledActorAtMe`; the cells' actors go through the same functions.
- **The plugin data.** The reference's level modifier is the subrecord XLCM (4 bytes, 0 to 3: New Vegas 305 references, Fallout 3
  4296) and its own zone is XEZN (16 and 414 references). The zones of the cells and worldspaces are the ones the first wave read.
- **The inventory.** The levelled lists in an actor's inventory are rolled when its container data is made (0x4D1440, called at
  0x55E129 right after the template): for a character or creature (vtable +0x100 true) at the *actor's own level* (0x87F9F0, the PC
  level multiplier rule of `levelFromConfig`), for any other owner at 0x567E10 of the owner (so a container's lists use the zone level
  and move the counter). The engine rolls the lists of an actor's inventory and worn armor with the same level as the template (see Not
  done).
- **When a cell resets** (0x550C60, called at 0x54BD23): every actor that has the extra data with an original base that differs from
  the current base, and that the respawn check says is due (0x881C90), goes back to the original base (0x575690), loses the extra data
  (0x56B020 with two zeros) and resolves its template again (0x47CE10) for the level of the player now.

## What the code does now

- `ESM4::ActorCharacter`/`ActorCreature` keep `XEZN` (`mEncounterZone`) and `XLCM` (`mLevelModifier`, -1 for none).
- `FalloutScript::placementZone(own, cell, world)` (0x567D20) and `SpawnLevels::placedLevel(reference, baseLevel, levelMod,
  leveledTemplate, level, storedMod)`: finds the roll a reference has, or makes it once: a template that is a levelled list is rolled
  for `baseLevel` turned by the counter, times the modifier's multiplier; any other actor for the level of the player as it is (no
  counter step, no multiplier). The roll lives in the same table as the placements of `PlaceLeveledActorAtMe` and is therefore in the
  save (section `spawn`, unchanged format).
- `FalloutScripts::placedActorRoll` (mwscript/falloutspawn.cpp) is what the provider of `ESM4::setSpawnRollProvider` asks for a
  reference of a cell: the zone chain, `zoneLevel`, the player's level. Everything that rolled "level 5" for a placed actor (the
  template chain, the worn armor, the inventory, the creature body parts) goes through that provider and now gets the real roll.
- When a cell resets, the rolls of its living actors (and of dead ones that are due) are forgotten and the cached template data of the
  actor is dropped, so the next look rolls again for the player's level of that time (`spawnCellLoaded`).
- `SetActorFullName` gives the name to the **base record** (0x5D1920 copies the full name of the form that is named into the base of
  the actor: +0xD0 of the actor's base, and marks it changed) and `ESM4Npc::getName` / `ESM4Creature::getName` return it, so every
  reference of that record shows it (the crosshair and the Pip-Boy ask `getName`). The text is the full name of the named form (a
  message's title, a note, a miscellaneous item, a character, a creature). `SetCellFullName` is still only kept.
- Merchants (0x886360, in the update of an actor; the settings `iMerchantRespawnDay1` = 0 and `iMerchantRespawnDay2` = 3 are in no
  file): when the day changes and the whole days since the game began, modulo 7 (0x867EF0), is one of the two, a merchant that is up gets new
  stock: the exe asks the container of its merchandise (extra type 0x3C, the XMRC) for what `ResetInventory` asks (the virtual at
  +0x208, the one 0x5DAC60 calls). `RespawnState::merchantRestocks` says yes once per day per merchant;
  `FalloutScripts::updateSpawn` resets the inventory of the container of every loaded merchant. Chosen: the once-a-day bookkeeping
  (the exe keeps a flag on the actor, byte +0x175, and compares the day of its process with today) and that the days are not saved.
- Round tools: `core.spawn.template(reference)` (the records of the template chain for the roll the reference has), `quickSave()`,
  `itemValue(item)`, `zoneNeverResets(zone)`.

## Body cleanup (read, not built)

0x977130 (the update of the list of actors that are processed): a timer at +0x103BC counts down by the frame time; when it is up
the actors are counted, and the bodies that may be removed (dead, in a loaded cell, not the player's, not a quest item) are listed. With
`iRemoveExcessDeadCount` (15) bodies or more and `iRemoveExcessDeadTotalActorCount` (20) actors or more the **oldest** body is removed
(0x8FEB60 on its cell); in the "complex" case (0x525420 true and 25 or more actors) the limits are `iRemoveExcessDeadComplexCount` (3)
and `iRemoveExcessDeadComplexTotalActorCount` (20) and bodies are removed until fewer than the limit are left; the timer is then set to
`fRemoveExcessDeadTime` or `fRemoveExcessComplexDeadTime` (floats; no file of either game sets any of the five settings). Nothing here
removes a body when its cell resets; that is the revival of the first wave. Not built: it needs a list of the dead bodies of the
loaded cells with their death times and a way to delete a reference, and the effect (looting a body that is gone) wants a play test.

## What was run

Unit tests (`components-tests`, `openfallout-tests`; the whole `components-tests` run has only the 15 known failures of NifOsg and
`MiscToEulerAnglesZYX`): `FalloutScriptSpawnRulesTest` (zone of a placement, a template roll made once with the counter, the
modifier multiplier and the floor of 1, an actor without a levelled template, forget and save), `FalloutScriptRespawnTest` (the two
merchant days), `FalloutScriptSpawnCommandsTest` (SetActorFullName keys the base record), `ESM4ActorReferenceTest` (XEZN and XLCM kept,
and none when absent).

The loaded rounds take `"save": "latest"`, the quicksave that the placement round wrote, so they run right after it; the placement rounds must start from a save that has not visited their cells (a zone level that was locked or a placement that was stored by an earlier visit shows as it was kept).

Hidden real-data rounds (`rounds/m16-spawn2-placed-{nv,fo3}.json` with `scenarios/m16_spawn2_placed.lua`; `rounds/m16-spawn2-loaded-{nv,fo3}.json`
with `scenarios/m16_spawn2_loaded.lua`); all PASS, 0 scenario errors, the engine never frontmost. The scenario enters interior cells that
cells place levelled-template actors in, at different player levels, and reads what the engine rolled for each actor
(`core.spawn.placement` and `core.spawn.template`). Numbers only:

- New Vegas (the play save; player level set with `SetLevel`): the Hoover Dam power plant (zone lowest level 10) at player level 1:
  9 actors with a levelled template, levels 9..11 (the zone's level 10, one lower, none, one higher in turn), 9 different templates; a vault
  (zone 6) at player level 25: 9 actors at 24..26; a casino (zone 22) at player level 40: 15 actors at 39..41, 13 different templates. Back
  in the power plant at player level 40: 0 of 9 actors changed their level (the roll is kept with the reference). After `ResetInterior`,
  leaving and coming back at level 40: all 9 changed, now 39..41 (10 + 30 of the level above the zone's lowest, in turn). The cell log
  line says "145 actors resolve their template again". The vault's zone never resets, and its 9 actors kept 24..26 at player level 40.
  New Vegas has hardly any XLCM in interior cells, so all rolls had no modifier.
- Fallout 3 (a new game; the player level stayed at 20, the cap without the add-on that raises it): the Capitol building (zone 7; 34 of 36
  placed actors have XLCM) at player level 1: 7 actors, 3..16, by modifier: easy 3, medium 4, hard none, boss 14..16, none 7 (the multipliers
  0.5, 0.75, 1.25 and 2.0 of the zone level 7 turned by one); a metro centre (zone 5) at level 20: 38 actors, 9..25; a castle (zone 5) at 20:
  3 actors with easy 10 and boss 38..42. Back in the first cell: 0 of 7 changed; after `ResetInterior` and a visit elsewhere: 7 of 7
  changed (10..42 now, for level 20).
- **After the merge of the fo3 package** (a zone fixes its level the first time a cell that uses it comes into the game and keeps it: Fallout3.exe
  0x4BFBA0, New Vegas 0x526270; `SpawnLevels::lockedZoneLevel` is now the base level of the placed actors' rolls, as of `PlaceLeveledActorAtMe`),
  the New Vegas round was run again (PASS, 78.7 s): the same 9..11, 24..26 and 39..41 on the first visits, 0 of 9 changed on the second visit, and
  after `ResetInterior` 6 of 9 got another place in the turn of the counter, still 9..11: the power plant's zone had fixed level 10 on the first
  visit, so a reset re-rolls (new dice, new place in the turn) but for the zone's fixed level, not for the player's level of that time. The 39..41
  of the numbers above for the reset were from before the merge. The text above that says "for the level of the player now" holds for an actor
  without a zone only.
- Commands that were only unit tested before, now seen live: `RemoveMe` (the real script of a crafting item ran when the player got it and
  the copy was gone; and by hand 2 copies of an item to 1, both games), `DropMe` (New Vegas: an item with a script, 1 copy to 0), `SetItemValue`
  (15 to 777, 800 to 777), `SetZoneRespawns` (the record says "never resets" false, true after the command; Fallout 3 the other way),
  `ResetInterior` (above), `AddNPCToLeveledList` (additions 0 to 1).
- **Save and load.** The scenario calls `core.spawn.quickSave` (the save is made at the start of the next frame, as the quick save key does)
  after all of the above; a second round starts from that save and reads the same state: New Vegas 10 of 10 placed actors with the
  same (level, modifier, template) before and after, 1 added list entry, zone flag, item value 777; Fallout 3 7 of 7 identical, the added entry, the zone flag (changed by the command) and item value 777 the same. This is the first
  round that saves and loads the `spawn` section.
- The merchant rule ran: "day 0 restocks 1 merchants" was logged at the first frame of every New Vegas round (the save is at day 0, a restock
  day); nothing showed the new stock itself.

## Not done

- The inventory and the worn armor of a placed actor are rolled with the template's level (the zone's, turned and multiplied); the executable
  rolls them for the actor's own level (0x87F9F0), which only differs for an actor with the player-level multiplier (581 New Vegas and
  282 Fallout 3 placed actors) and for an actor with a modifier. `PlaceLeveledActorAtMe` keeps the first wave's rule.
- The roll of an actor of a cell is kept in the save table, which the executable's is too, but it is also kept for an actor whose cell was left
  without saving, where the executable resolves again; the executable's rule for an actor made while the cell loads (before the player's
  level is known) was not read.
- A reference whose base is a levelled list itself (none exist in the files) still goes through the cell loader's fixed level 5.
- A cell reset that gives an actor another template (the re-roll in `spawnCellLoaded`) clears its custom data and the cached fight traits
  after the cell has been inserted into the scene (`Scene::loadCell` builds the render and physics objects and the mind, and copies the
  package list, before `FalloutScripts::cellLoaded`), so the body, the collision shape and the packages of the actor stay those of the template
  it had until the cell is loaded again. A true fix rolls the template before the actor is inserted (a step in `insertCell`) or rebuilds the
  actor's scene objects and mind; neither is a local change. The name and the AI data it reads follow the new template.
- The merchants' days are not saved (a loaded game restocks again on the same day) and the stock itself was not looked at in the barter menu.
- Body cleanup (above), zone run-time timers, flora resets, the equipped and quest items of `RemoveAllTypedItems`, `PlaceAtMeHealthPercent`
  (the executable hands the percent to the placement worker 0x5C4B30; nothing here has a condition), `GetIsLockBroken`,
  `DuplicateNPCStats`, `PlaceAtReticle`, `SetCellFullName` (still only kept) and the names of the world that the HUD draws for a cell.
- `SetActorFullName` showing in the game was not seen live (the name is read by `getName`, which the crosshair and the Pip-Boy ask).
- `RemoveMe` and `DropMe` still do not stop the calling script.
