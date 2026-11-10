# The progress tracker

The coverage dashboard (`scripts/openfallout/coverage_dashboard.py`, rules in `docs/openfallout/coverage-dashboard.md`) shows two measures for each thing the games define: whether the engine **implements** it and whether what it does has been **read** from the executables or the game's data files. Where the source tree can say which, the script reads it from there. Where it cannot, the answer is written down here, in files that a round or a milestone edits by hand.

| File | What it holds | Who edits it |
|---|---|---|
| `reads.csv` | One line for each script command (or extender function, or animation group) whose behaviour has been read, with where: `family,name,evidence`. `family` is `command`, `extender` or `animation`; `name` is the name in the games' table. | A Mac round that reads something, or the thread that writes the report of it |
| `menus.csv` | The 121 menu files of the games (`menus/*.xml` in `Fallout - Misc.bsa`; 121 in New Vegas, 99 of them also in Fallout 3). `library` groups them for the page, `kind` is `screen` or `piece` (a block the screens share), `games` is `both` or `nv`. `read` is `listed` or `read`, `implemented` is `done`, `stand-in` or `todo`. | The menu milestone (M12) as it renders the games' own menu files |
| `perk-entry-points.csv` | The perk entry points of the editor's table (74 in New Vegas, the first 37 of them also in Fallout 3). `read` is `listed` or `read`. `dead` is `yes` for an entry point that no code of the executable calls and no perk uses; those are left out of the counts. | A round that reads an entry point; the engine's use of it comes from the source |
| `command-usage.csv` | How many places of the games call each script command: `code,name,state,fo3_calls,fo3_scripts,fo3_conditions,nv_calls,nv_scripts,nv_conditions,uses`, in order of uses (`state` is `done`, `stand-in` or `todo`, or `unknown` for a code that no table lists). Written by `scripts/openfallout/command_census.py` from the counts of `esmtool scriptcode` and `esmtool dialogue`; the page that reads it is `docs/openfallout/command-census.md`. It is not an input of the dashboard. | A round that runs the census again (after a milestone changes the handlers, or a plugin changes) |

What the script works out for itself, so that nobody has to keep it in step:

- **Script commands and extender functions**: implemented from the `setHandler` calls and the arrays of `switches.cpp`, as before.
- **Record types**: implemented from the record store in `esmstore.hpp`. A record type counts as **read** when a loader of `components/esm4` parses it.
- **Perk entry points**: implemented when some code of the engine applies the entry point by a name of `EntryPoint` in `components/falloutscript/perks.hpp` (the entry points that header does not name are not done).
- **Animation groups**: the list is the two tables of the executables in `components/falloutscript/animgroups.cpp` (245 names in New Vegas, 183 of them in Fallout 3). A group is implemented when `apps/openfallout/mwrender/falloutanimation.cpp` picks animation files by its name.

## What counts as read

A unit is **read** when somebody has found out what it does, or for a menu how it is laid out, from the executable or the game's data files, and written down where. The name and code from a table do not count: every unit has those. A read needs its evidence, a short text that says where: a report of a Mac round and the address in the executable, or the file of the game. The script refuses a row marked read with no evidence.

A unit the engine implements but that has no read on record is shown as implemented and not read. That is not an error; it is the list of the things to write down, or to check against the games.

## Adding a read

1. Find the unit in the table of its family (`reads.csv` for a command, a function or an animation group; the `read` column of `menus.csv` or `perk-entry-points.csv`).
2. Put `read` in the column, or add the line to `reads.csv`, with the evidence. Use the name exactly as the table spells it (the case does not matter).
3. Run `scripts/openfallout/test_coverage_dashboard.py`. It checks that every line of `reads.csv` matches a unit and that every read has its evidence.

## Where the lists came from

- `reads.csv` was seeded on 2026-11-01 from the reports of the Mac rounds in `/mnt/project-files/openfallout`: a command is there when a report names it and gives the address of its `exec` or `eval` function in the same line, and says what it does. A report that only gives an address (or says "not read" or "body not traced") was left out. The list is short because the rounds wrote their reads as prose, not as a table; it will grow as they are written down here.
- `menus.csv`: the file names of the `menus` folder of `Fallout - Misc.bsa` of New Vegas and of Fallout 3, listed read-only on the Mac (nothing was run, nothing was copied into the tree). The stand-ins are the menus that one of the engine's Lua screens in `files/data/scripts/omw` does the job of, as the header of each script says.
- `perk-entry-points.csv`: the names of the entry points in the editor's executables (`GECK.exe` of each game, read-only). The reads are the entry points that Mac round 4A (parts F1 and F2, 2026-10-15) traced in the executable of the game: the roles of the arguments and where each is applied. The dead ones are the entry points that round found with no call site and no data.
