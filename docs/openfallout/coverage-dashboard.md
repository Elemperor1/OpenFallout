# Coverage dashboard

A map of how much of what the games define the engine has, in the manner of a decompilation progress map: one box for each library, one square for each unit in it, green where the unit is done. `scripts/openfallout/coverage_dashboard.py` makes it from the source tree and from the tracker in `docs/openfallout/coverage/tracker`, so the picture follows the code and the notes. It shows two measures side by side, **Read** and **Implemented**, and the two headline cards on the page are buttons that choose which of them colours the map.

```
scripts/openfallout/coverage_dashboard.py --html coverage.html            # a page that opens in a browser
scripts/openfallout/coverage_dashboard.py --html page.html --fragment     # the same without html/head/body, for an artifact
scripts/openfallout/coverage_dashboard.py --json coverage.json            # the numbers
scripts/openfallout/test_coverage_dashboard.py                            # the checks of the script
```

It needs Python 3 and nothing else. It prints the table of numbers when it runs. The page is `scripts/openfallout/coverage_dashboard.html`, a template that the script puts the data into.

## What is counted

A **unit** is one thing the games define and the engine has to answer to. There are six kinds. Their lists come from tables that are already in the tree, except the menus and the perk entry points, whose lists the tracker holds (`docs/openfallout/coverage/tracker`, whose README says how it is kept):

| Family | Unit | Where the list comes from | Implemented when |
|---|---|---|---|
| Script commands | One command of the tables read out of the Fallout 3 and New Vegas executables (615) | `components/falloutscript/vanillacommands.inc` | The engine registers a handler for it: `setHandler("Name", ...)` or `setHandler(0xCODE, ...)` in `components/falloutscript` or `apps/openfallout/mwscript`, or a name in the two arrays that `switches.cpp` registers in a loop |
| Script extender functions | One function of a plugin table read out of the plugin's DLL (xNVSE core, JIP LN, JohnnyGuitar, ShowOff, kNVSE, lStewieAl's Tweaks, TTW; 2,363) | `docs/openfallout/coverage/extender-tables/*.csv` | The code is in `extendercommands.cpp` and has a handler. The slots a plugin reserves and names `EmptyCommand` or `NONE` are not functions and are left out |
| Record types | One record type of the two base plugins (107) | The census table "Every record type in the two base plugins" in `docs/openfallout/census-results.md` | The loader's record has a store in `apps/openfallout/mwworld/esmstore.hpp` |
| Menus | One menu file of the games, `menus/*.xml` (121: 52 screens and 69 blocks that the screens share) | `tracker/menus.csv` | Written in the table: the game's own menu file is drawn (done), or one of the engine's own Lua screens does the job (stand-in) |
| Perk entry points | One entry point of the editor's table (74 in New Vegas, 37 of them in Fallout 3; ten that nothing calls are left out, 64 counted) | `tracker/perk-entry-points.csv` | Some code applies it by a name of `EntryPoint` in `components/falloutscript/perks.hpp` |
| Animation groups | One group of the tables of the executables (245 in New Vegas, 183 of them in Fallout 3) | The tables in `components/falloutscript/animgroups.cpp` | `apps/openfallout/mwrender/falloutanimation.cpp` picks animation files by the group's name |

### Implemented

Each unit is in one of three states:

- **Done**: the engine has it, as above.
- **Stand-in**: the engine takes it in and does nothing with it, or answers a fixed value. These are the commands in the `ignored` and `switchCommands` arrays of `switches.cpp` (the source says what is not done), the commands in `docs/openfallout/coverage/stand-ins.csv`, and any other handler that only returns a number (the script lists them as stand-ins until somebody looks and puts them in the csv as `stand-in` or `exact`). A record type is a stand-in when its loader exists and nothing keeps the record. A menu is a stand-in when one of the engine's own Lua screens does its job but the game's menu file is not drawn.
- **Not done**: no handler (the engine reports the command once and carries on), no entry in the command table for an extender code, no loader for a record type, no screen for a menu, no code that applies a perk entry point or picks an animation group.

The Implemented card is the share of units that are done. Stand-ins are shown but not counted as done.

### Read

A unit is **read** when somebody has found out what it does, or for a menu how it is laid out, from the executables or the games' data files, and the read is on record. The record is a line of `tracker/reads.csv`, a `read` in the menu or perk table, or, for a record type, a loader that parses it. Each needs its evidence (a report of a Mac round and the address, or the file of the game) and the script refuses a read without one. The name and code from a table do not count, because every unit has them. The Read card is the share of units that are read.

At the first run of the tracker (2026-11-01, commit `b285617` and the tracker on top of it): all 3,515 units, 217 read (6.17%) and 374 implemented (10.64%, 96 more are stand-ins). The reads are the 107 record types (their loaders), 58 commands that the Mac reports of M10 name with the address of the function that does the work, and 52 of the 64 perk entry points (Mac round 4A). No menu, extender function or animation group has a read on record: the rounds that read them wrote prose, not table rows, and the menu files have not been parsed by the engine yet. These are the numbers to move.

Three views of the same numbers: **All libraries**, **The games** (everything but the extender functions, which is what Fallout 3 and New Vegas need), and **Tale of Two Wastelands** (the games plus the 139 extender codes that its scripts call, from `ttw-extension-codes.csv`). The extender plugins hold 2,363 functions and the games only 1,152 units, so the first view is mostly extender functions that no vanilla game needs.

## What the numbers do not say

- **Done means built, not checked.** A handler that exists counts, whether or not anybody compared it with the games. `gap-audit-and-roadmap.md` and the gap list say which built things nobody has verified.
- **Read is only what is on record.** The engine implements 251 commands and only 58 have a read on record; the rest were built from reads that were never written down as rows, or from other sources. The tracker is where to write them. A read is also not a proof: it says that someone looked and where, not that they were right.
- **A record type is read when the engine parses it**, which is a weaker thing than reading what each field means.
- **The animation groups are measured by name.** The engine picks Fallout animation files by name for idles, walking and running, drawing, aiming, attacking and recoil (`falloutanimation.cpp`); the variants (up, down, iron sights), reloads, jams, dodges, jumps and deaths are not picked by the engine, though a script can still play a group by name. Which state or weapon picks which group has not been traced in the executable yet (Mac round 10B, part B).
- **A menu is done when the game's own menu file is drawn**, as the menu milestone (M12) does it, with the buttons' logic in code. Since M12 drew them, 73 of the 121 files are done (2026-11-08: 37 screens and 31 blocks that those screens include, and some of them are still not seen in the running game). The stand-ins that remain are the engine's own screens that approximate a menu on Morrowind's widgets (the title screen and the settings of `start_menu.xml`, the VATS screen). A block (`prefabs/*`) is done when a screen that is done includes it; the ones left are included only by screens that the executable never loads or that are not drawn yet.
- **The perk entry point count leaves out ten dead ones** (5, 7, 13, 16, 18, 19, 45, 49, 50, 60: no call site and no data in the executable). Two more, 3 in New Vegas and 24 in Fallout 3, are dead in one game only and stay in.
- **The topics are for reading.** The games' command table has no categories. `COMMAND_TOPICS` in the script sorts the 615 commands into libraries by words in their names (first rule that matches), `ANIMATION_TOPICS` does the same for the animation groups, and `RECORD_GROUPS` sorts record types by hand. Moving a unit to another box changes no total.
- **Not counted yet**: the games' interface textures, NIF block types and the functions of the executables themselves. The tree has no table of them to count against. To add a family, add a reader for its list and a rule for done to `build()`, and give it an entry in `FAMILIES` in the page.

## Keeping it right

- A new handler needs nothing: the script finds `setHandler` calls. A handler written in some other way (a loop over a table, as in `switches.cpp`) is not found and its commands show as not done; add a reader like `read_listed_commands` for it.
- A command that is accepted and does nothing goes in `ignored` in `switches.cpp`, with its reason, or in `stand-ins.csv`.
- The script lists the handlers whose names are in no command table (`unknownHandlers`); the test accepts only the extender ones the engine adds itself, which catches a mistyped name.
- When a loader or a store is added, nothing changes in the script; the record type turns green when `Store<ESM4::Name>` is in `esmstore.hpp`.
- A read goes into the tracker, not into the script: add the line to `tracker/reads.csv` (or the `read` column of the menu or perk table) with its evidence. The test checks that every line matches a unit.
- When the menu milestone draws a menu from its file, change its `implemented` to `done` in `tracker/menus.csv` (and `read` to `read` with where the file was read). The test checks that a stand-in names a screen that exists.
- The extender tables are copies of the files the Mac read from the DLLs (`docs/openfallout/coverage/extender-tables/README.md` says how). A new version of a plugin means a new table.
