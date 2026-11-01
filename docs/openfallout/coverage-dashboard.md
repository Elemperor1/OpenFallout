# Coverage dashboard

A map of how much of what the games define the engine has, in the manner of a decompilation progress map: one box for each library, one square for each function in it, green where the engine has it. `scripts/openfallout/coverage_dashboard.py` makes it from the source tree, so the picture follows the code.

```
scripts/openfallout/coverage_dashboard.py --html coverage.html            # a page that opens in a browser
scripts/openfallout/coverage_dashboard.py --html page.html --fragment     # the same without html/head/body, for an artifact
scripts/openfallout/coverage_dashboard.py --json coverage.json            # the numbers
scripts/openfallout/test_coverage_dashboard.py                            # the checks of the script
```

It needs Python 3 and nothing else. It prints the table of numbers when it runs. The page is `scripts/openfallout/coverage_dashboard.html`, a template that the script puts the data into.

## What is counted

A **unit** is one thing the games define and the engine has to answer to. There are three kinds, each read from a file in the tree, and every unit counted is already read (its name, code and arguments are known):

| Family | Unit | Where the list comes from | Done when |
|---|---|---|---|
| Script commands | One command of the tables read out of the Fallout 3 and New Vegas executables (615) | `components/falloutscript/vanillacommands.inc` | The engine registers a handler for it: `setHandler("Name", ...)` or `setHandler(0xCODE, ...)` in `components/falloutscript` or `apps/openfallout/mwscript`, or a name in the two arrays that `switches.cpp` registers in a loop |
| Script extender functions | One function of a plugin table read out of the plugin's DLL (xNVSE core, JIP LN, JohnnyGuitar, ShowOff, kNVSE, lStewieAl's Tweaks, TTW; 2,363) | `docs/openfallout/coverage/extender-tables/*.csv` | The code is in `extendercommands.cpp` and has a handler. The slots a plugin reserves and names `EmptyCommand` or `NONE` are not functions and are left out |
| Record types | One record type of the two base plugins (107) | The census table "Every record type in the two base plugins" in `docs/openfallout/census-results.md` | The loader's record has a store in `apps/openfallout/mwworld/esmstore.hpp` (the engine keeps it) |

Each unit is in one of three states:

- **Done**: the engine has it, as above.
- **Stand-in**: the engine takes it in and does nothing with it, or answers a fixed value. These are the commands in the `ignored` and `switchCommands` arrays of `switches.cpp` (the source says what is not done), the commands in `docs/openfallout/coverage/stand-ins.csv`, and any other handler that only returns a number (the script lists them as stand-ins until somebody looks and puts them in the csv as `stand-in` or `exact`). A record type is a stand-in when its loader exists and nothing keeps the record.
- **Not done**: no handler (the engine reports the command once and carries on), no entry in the command table for an extender code, or no loader for a record type.

The headline is the share of units that are done. Stand-ins are shown but not counted as done.

Three views of the same numbers: **All libraries**, **The games** (script commands and record types, which is what Fallout 3 and New Vegas need), and **Tale of Two Wastelands** (the games plus the 139 extender codes that its scripts call, from `ttw-extension-codes.csv`). The extender plugins hold 2,363 functions and the games only 722 units, so the first view is mostly extender functions that no vanilla game needs.

At commit `a4dc45d` (2026-11-01): all libraries 328 of 3,085 (10.63%), the games 321 of 722 (44.46%), Tale of Two Wastelands 328 of 861 (38.10%), of which the 139 extender codes 7 done and 5 stand-ins.

## What the numbers do not say

- **Done means built, not checked.** A handler that exists counts, whether or not anybody compared it with the games. `gap-audit-and-roadmap.md` and the gap list say which built things nobody has verified.
- **Read is not counted separately.** Every unit is read to the level of name, code and arguments, because that is where the lists come from. What a command does is known only for the ones that are done. A measure of how much of the executables themselves has been read (functions by address) needs a list of the executable's functions, which the tree does not have.
- **The topics are for reading.** The games' command table has no categories. `COMMAND_TOPICS` in the script sorts the 615 commands into libraries by words in their names (first rule that matches), and `RECORD_GROUPS` sorts record types by hand. Moving a command to another box changes no total.
- **Not counted yet**: the games' own menu layouts (`menus*.xml`) and interface textures, perk entry points, animation groups, NIF block types and the functions of the executables themselves. The tree has no table of them to count against. To add a family, add a reader for its list and a rule for done to `build()`, and give it an entry in `FAMILIES` in the page.

## Keeping it right

- A new handler needs nothing: the script finds `setHandler` calls. A handler written in some other way (a loop over a table, as in `switches.cpp`) is not found and its commands show as not done; add a reader like `read_listed_commands` for it.
- A command that is accepted and does nothing goes in `ignored` in `switches.cpp`, with its reason, or in `stand-ins.csv`.
- The script lists the handlers whose names are in no command table (`unknownHandlers`); the test accepts only the extender ones the engine adds itself, which catches a mistyped name.
- When a loader or a store is added, nothing changes in the script; the record type turns green when `Store<ESM4::Name>` is in `esmstore.hpp`.
- The extender tables are copies of the files the Mac read from the DLLs (`docs/openfallout/coverage/extender-tables/README.md` says how). A new version of a plugin means a new table.
