# M16 spawn notes (things that come into the world, leave it and come back)

What the M16 `spawn` work package changed, and what it rests on. A rule below was **read in the executable** (FalloutNV.exe;
the addresses are given; Fallout3.exe has the same commands under the same codes and was not read function by function), **seen in a
running game** (a hidden round, see "What was run"), or **chosen because nothing says it** (marked so). The games' own text is never
quoted here.

## What was wrong

- Leveled lists were rolled only for the armor that characters wear, with the level fixed at 5 (`sDefaultLevel`). Nothing rolled a
  list of characters or creatures for a character that a script made, and `AddItemToLeveledList` was a command that did nothing
  (it was one of the silent stand-ins in `switches.cpp`, as was `ResetInventory`).
- `PlaceLeveledActorAtMe` (354 calls in the plugins), `RemoveAllTypedItems` (295, New Vegas), `Exists` (143 conditions), `RemoveMe`
  (134), `DropMe` (58), `PurgeCellBuffers` (32), `AddItemToLeveledList` (22), `ResetInventory` (21), `GetInCellParam` (65),
  `SetItemValue` (8), `Drop` (4), `SetActorFullName` (36), `SetCellFullName` (7) and the others of the package had no handler.
- A cell that was left and loaded again never reset: a character that died stayed dead, a container that was emptied stayed empty,
  whatever the clock said.

## The rules (components/falloutscript and components/esm4, no engine types)

### How a levelled list chooses (0x487D00, 0x487F70)

`ESM4::chooseLevelledEntries` is a port of the group walk of 0x487D00:

- A level below 1 counts as 1 (0x487F70).
- The chance of nothing is rolled first, and it is one byte: a chance that comes from a global variable (`LVLG`) is cut to a whole
  number and then to its low byte (`levelledChanceFromGlobal`: 7.5 gives 7, a negative number wraps).
- A list that uses all gives every entry that names an item, whatever its level.
- A list that calculates from all levels, or that a placement gave a level modifier above 0, chooses among the entries at or below the
  level (its level does not go above that of its highest entry; the settings `iLev*LevelDifferenceMax` allow a difference, they are in
  no file of either game, so the default is 0). Any other list chooses among the entries of the highest level that is not above the level.
- The walk goes in the order of the list and stops at the first entry above the level, so a list that is not sorted chooses from what
  comes before it. An entry of level 0 never starts the group (no entry of either game has level 0 or count 0). An entry that names
  no item ends in "nothing".
- The level of a character comes from `levelFromConfig` (ActorBaseData::GetLevel, 0x47DED0): the number in its ACBS, or the level of
  the player times it in thousandths with the fraction cut; a minimum above 0 wins over a maximum above 0.

### Entries that scripts add (0x487A60, 0x5DE700, 0x5DE670)

`AddItemToLeveledList list, item, level, count, health`, `AddNPCToLeveledList list, actor, level` and `AddCreatureToLeveledList` put an
entry into the list: it goes in front of the first entry whose level is above its own (`insertLevelledEntry`), so a list that is
not sorted gets it there too. A count of 0 adds nothing (0x5DE700); the two commands for characters and creatures add one (both are
0x5DE670, told apart by the kind of list). A list or an entry of the wrong kind does nothing (chosen: the executable checks the form
type; the kinds that the host accepts as items are armor, weapons, ammunition, aid, miscellaneous items, books, ingredients, item
mods, keys, notes and clothes).

`ESM4::setLevelledAdditions` is the one hook: every roll of a list, wherever it is made (the armor a character wears, the inventory, a
template of a character, `PlaceLeveledActorAtMe`), asks `levelledEntriesOf` for the entries, which are the record's entries with the
script's entries put in. `FalloutScript::LevelledLists` keeps the additions (they are in the save, section `spawn`).

### The level a placement is rolled for (0x5D9810, 0x526190, 0x567E10, 0x4214F0, 0x488350)

`PlaceLeveledActorAtMe base, levelMod, zone` makes a character or creature at the place of the calling reference.

1. The zone is the one the command names, else that of the cell of the reference (`XEZN` of the cell), else that of the worldspace.
   The level of a zone for a player of level P with the lowest level M and the flags F is (0x526190): P below M gives M, or P when the
   zone matches the player below its minimum (flag 2); P above M gives `M + trunc((P - M) * fLevelScalingMult)` (the setting is 1.0 in
   both games' files). A placement with no zone uses the level of the player.
2. The level is made one higher, one lower or not changed, in turn (0x567E10): a counter goes up by 1 for every placement; once it is
   above `iCalcLevelAdjustUp` it starts again at minus `iCalcLevelAdjustDown` (the two settings are in no file; the defaults are 1 and 1).
   The level is never below 1.
3. The level modifier is 0 to 3 (the rest is "none", which is the default 4 of the command): the level is multiplied with
   `fLeveledActorMultEasy`, `Medium`, `Hard` or `Boss` (0.5, 0.75, 1.25, 2.0 in both games' files; 0x4214F0), the fraction is cut off,
   and the result is not below 1 (0x488350).
4. The list rolls for that level with the modifier. The record it chooses is made (`placeAtMe` of the host), and the level and
   modifier are kept with the reference (`SpawnLevels`, in the save). The levelled lists in what it carries (its inventory, the armor it
   wears, a template that is a list) are rolled for the same level and modifier, from the first moment: the world's own number of the new object
   is guessed (the next number the world makes up) so that the equipment, which is chosen when the object is put in its cell, already knows it.

### When a cell resets and a character comes back (0x54E0AA, 0x881C90, 0x5D24A0, 0x5DAB90)

- A cell that is left (unloaded) keeps the game hour it was left at. When it loads again it resets if the whole hours that went by,
  as unsigned numbers, are above `iHoursToRespawnCell` (default 72: the setting is in no file), or if it was told to reset
  (`ResetInterior` sets the time of leaving to -1). A cell that was never left is new and does not reset. A zone that never resets
  (flag 1 of its record, or `SetZoneRespawns zone, 0`) keeps its cells as they are.
- A character that is dead comes back when more than the hours passed since it died and its zone does not forbid it (0x881C90); a living
  one is always up to be made new. Only a character whose record has the flag that makes it respawn (bit 8 of the first dword of its ACBS)
  comes back (chosen from the record layout; the executable's own test of the flag was not read). The flag is part of the base data, so a
  record that takes its base data from a template (`Template_UseBaseData`) is judged by the flag of the record at the end of its chain of
  templates, the same record that the essential flag is read from (`respawnsAsRecord`).
- What the host does for a character that comes back: alive again with its health (the same two steps as `ResurrectActor`), a new
  inventory (its levelled lists are rolled again with a different seed: `inventoryGeneration`), and a container with the respawn flag of
  its record (bit 2 of the container's data flags, chosen from the record layout) is filled again the same way.

### The rest of the package

- `Exists` is true for the calling reference unless it is the one the argument names (0x5A4210 is the condition; 0x5D7B80 the command).
- `RemoveMe` and `DropMe` act on one copy of the item whose script runs: it leaves its holder to the named reference or out of the
  game (`RemoveMe`, 0x5B53D0), or into the world next to the holder (`DropMe`, 0x5B5860). `Drop item, count` does the same for a
  stack (0x5B58D0). The executable's commands also stop the script (the interpreter here has no such mechanism; the rest of the
  script runs: see Not done).
- `RemoveAllTypedItems destination, retainOwnership, suppressMessage, formType, exceptions` moves every item of the reference that is of
  that form type (any when the type is below 0 or left out; 24 armor, 31 miscellaneous, 40 weapons, 41 ammunition, 47 aid, 25 books and
  29 ingredients are the numbers of the executable's form types) and not in the form list of the exceptions to the destination, or out
  of the game (0x5B55A0, 0x4CE380). The copies move with their condition.
- `ResetInventory` (0x5DAC60) asks the holder to rebuild its inventory from its record: what scripts did to it is forgotten and the
  lists in it are rolled again, and what the character wears is forgotten so that it is dressed from its record again (as for a
  character that comes back by respawn). `SetZoneRespawns zone, value` sets or clears the flag that says the zone never resets (value 0 sets it,
  0x5DAB90). `PurgeCellBuffers` (0x5B6CD0) drops memory the games keep: nothing is kept here, so nothing is done.
- `GetInCellParam cell, reference` is `GetInCell` for another reference (0x59EF60; exec 0x5C7590); `GetParentRef` gives the reference
  that enables the calling one (the enable parent of its record: 0x5CE630); `GetContainerInventoryCount` is the total of the items
  (0x5E0100; chosen: the count is the sum of the stacks); `SetItemValue` keeps a value that every trade uses instead of the one of the
  record (0x5D3E30); `SetActorFullName` and `SetCellFullName` keep the message that gives the name (0x5D1920, 0x5D1890; the world
  does not show it yet).

## Engine wiring (apps/openfallout)

- `mwscript/falloutspawn.cpp`: the adapter that gives the rules the world (the records of the lists, `exists`, `placeLeveledActor`,
  the holder of an item, `dropItem`), the hooks (`ESM4::setLevelledAdditions`, `ESM4::setSpawnRollProvider`), and the cell driver:
  `cellLoaded` and `cellUnloaded` feed `RespawnState`, and a death is told to it when the death queue is taken.
- The roll of an actor is looked up by its reference: the number scripts know a made actor by (0xFF and a counter), or the world's own
  number of it, which `makeObject` and `applySaved` register before the object is put in its cell.
- The actors' classes (`mwclass/esm4npc.cpp`, `esm4creature.cpp`) and the script host (`falloutscripts.cpp`, `falloutvats.cpp`) follow
  the template chains and roll the inventory and the worn armor with the placement's level and modifier, instead of the fixed 5. A
  character that the cell placed (not a script) still rolls at 5, as before: the cell resolves its list before anything knows a level.
- `mwworld/esmstore`: the encounter zone records (`ECZN`) are in the store now.
- Save: a new section `spawn` (version 1) holds the additions of the lists, the placements, the cell and death clocks, the zones' flags,
  the inventory generations, the item values and the names; a game saved before has none of them and starts empty.
- `mwlua/spawnbindings.cpp` is `core.spawn`, for the rounds: `run(command, reference, args)` calls a script command through the
  interpreter exactly as a script does, and `base`, `placement`, `items`, `count`, `dead`, `deathHour`, `generation`, `additions`
  and `respawns` read what the rules keep.

## What was run

Unit tests (all in `components-tests`, `openfallout-tests`; the full `components-tests` run has only the 15 known failures of
NifOsg and `MiscToEulerAnglesZYX`):

- `ESM4Levelled*`, `ESM4WornArmor*`, `ESM4CreatureModel*`: the group walk (level clamp, chance byte, use-all, all-levels, level
  modifier, difference, level 0 entries), the sorted insert, the additions overlay.
- `FalloutScriptSpawnRules*`: zone level, modifier, scaled level, the rotation of placement levels, respawn clocks (hours,
  forced reset, zone flags, actor due), save round trips of each state. `FalloutScriptLevelledLists*` and
  `FalloutScriptSpawnCommands*`: every handler of the package, driven by compiled script code against fake worlds.

Hidden real-data rounds (`scripts/openfallout/round/rounds/m16-spawn-fo3.json` and `m16-spawn-nv.json`, scenario
`scenarios/m16_spawn.lua`, which calls the plugins' commands through `core.spawn.run` and reads what the rules keep). Both PASS
with 0 scenario errors and the engine never frontmost (Fallout 3: 131 s from a new game started in the Capital Wasteland; New Vegas:
130 s from a play save, taken out of the doctor's house through its door first). What each phase showed:

- A, levels. Fallout 3 (player level 1), list EncRaiderGun, whose entries are at levels 1, 3 and 5: with no zone the placement was at 2
  and 2 (the level of the player and the rotation); a zone with lowest level 1: 1 and 1; lowest level 7: 7 and 7 (the logs of
  other spawns show 6 and 8 in turn); with the modifier easy 4, medium 4, hard 8 and boss 16 (those are the levels the list
  was rolled for, after the multiplier); a zone with lowest level 14: 13, and with boss 28. New Vegas, list EncVHDGreatKhan (entries at
  levels 1, 13, 16, 19, 21, 24, 27, 30, 33): zones with lowest levels 1, 10 and 22 gave 1, 10 and 21; easy 5, medium 6, hard 12, boss 22
  (and 44 in the 22 zone). The records follow the rule: at 10 and 12 only characters of the level 1 group were chosen (4 of 4); at 21
  only those of the level 21 group (2 of 2); with the boss modifier the list chooses among every entry at or below the level (level 22: one
  character of the level 13 group, one of the level 21 group; level 44: one of level 1, one of level 19). Every spawned
  character exists, carries its level (`placement`), and holds an inventory rolled for it (3 to 23 items in Fallout 3, 15 to 21 in New Vegas).
- B, added entries. Before the call, 12 spawns from a list of three characters (New Vegas, VarGoodspringsPowderGanger) chose only those
  three; after `AddNPCToLeveledList list, actor, 1` (a character of another list) the new character was chosen 6 times in 30
  spawns (Fallout 3: list EncRaiderRandom with a character that is in none of its lists: 11 times in 30). `additions` went from 0 to 1.
- C, `RemoveAllTypedItems` (New Vegas only: the command is not in Fallout 3's command table, the call is refused there and the round says so).
  A spawned character held six kinds of item. Type 40 (weapons) moved its one weapon to the player (the player had none before), type
  41 (ammunition) took its ammunition out of the game (the player's own stack of the same ammunition stayed at 40), type -1 emptied it.
  The same command on a placed container (37 kinds, 2002 items): type 47 (aid) took out 3 kinds and 5 items, then type 41 (ammunition)
  took out 18 kinds and 509 items; the other 16 kinds stayed (seen in a running game, New Vegas round m16-spawn-nv, PASS 129 s).
- C2, `Drop`. A holder of a stack of 8 (New Vegas) and 3 (Fallout 3) dropped it: the holder had 8 (3) and 0 afterwards; the stack is one
  object in the world next to the holder.
- D, `ResetInventory` (both games). One actor's inventory was rebuilt three times: the generation went 0, 1, 2 and the items differed between
  generations (some lists rolled differently: items appeared, disappeared and changed counts).
- E, respawn (both games). A spawned character with the respawn flag was killed with `KillActor`: dead, with a death hour (1 in the Fallout 3 new
  game, 24 in the New Vegas save). Two containers with the respawn flag were placed with `PlaceAtMe` and emptied (13 and 11 kinds of
  item to 0 in Fallout 3; 41 and 36 to 0 in New Vegas). The player went 40000 units away (the cell unloaded), the game clock was put 4 days on, and
  the player came back: the cell reset (game hour 96 in Fallout 3, 120 in New Vegas, over the 72 hours) and the engine logged
  "1 characters came back, 2 containers were filled again" (New Vegas). The character was alive again with inventory generation 1 and no
  death time; the containers held items again (Fallout 3: 13 kinds and 11 kinds, 34 items each; New Vegas: 41 kinds with 1639 items and 40
  kinds with 2069 items, the second rolled differently from its first 36 kinds), generation 1.
- First round on 2026-11-13 found that the player's reference as scripts know it is not 0x14 in this engine (`core.spawn.player()` gives it); a
  call on the wrong reference made nothing and said so in the log.
- Not run live: a game saved with the new section and loaded again (each state class has a save round trip test and the two lambdas
  of the section are written in the same order, but no hidden round saves and loads); `RemoveMe`, `DropMe` (they need an item whose script
  runs), `SetZoneRespawns`, `ResetInterior`, `SetItemValue`, `Exists`/`GetInCellParam` in a condition (the unit tests drive them).
  `Exists` ran as a command through `core.spawn.run` on every spawn (`exists=1`).

## Not done

- The zone's own timer (the runtime time stamp that the executable keeps for a zone) is not modelled: a zone only says "never".
- Flora and ingredient resets (the plants that grow back) are not modelled.
- `RemoveAllTypedItems`: the handling of equipped and quest items, of ownership and of the message that the executable adds when the
  destination is the player are chosen, not read.
- `PlaceAtMeHealthPercent` places the objects like `PlaceAtMe`; the health fraction (argument 1) is read and dropped. Applying it is not
  done: the placement interface (`ObjectWorld::placeAtMe`, the pending request of the host and its saved form) carries no health, and
  what the fraction would set differs by kind (the health of a character, the destruction state of a breakable object, the condition of
  an item that has no stack to hold it while it lies in the world).
- `GetIsLockBroken`, `DuplicateNPCStats` and `PlaceAtReticle` have no handler (optional in the package).
- A character that the cell places from a list rolls at the fixed level 5, as before: the cell resolves its list when it loads, before
  a level could be known.
- Merchants' lists are not rolled again (the barter menu keeps what a vendor had); the bodies of the dead are not cleaned up.
- A reference's own `XEZN` (a zone set on the reference) is ignored: the loader does not keep it.
- `RemoveMe` and `DropMe` return "stop the script" in the executable; the interpreter has no way to stop the calling script.
- The names of `SetActorFullName` and `SetCellFullName` are kept, not shown.
- A pending `PlaceAtMe` of a stack (a dropped item with a count) that was not yet made when the game was saved is made again with count 1.
- `Drop` and `DropMe` take the stacks out of the holder at once and the world makes the object at the start of the next frame. The count,
  the condition and the mods of a stack are part of the request from the moment it is published, and when no object can be made (the
  holder is not in a loaded cell by then, or the world refuses it) the stack goes back to the holder with its condition and mods. A
  request that was saved before it was made has no holder to give back to after a load (it is made again as above).
