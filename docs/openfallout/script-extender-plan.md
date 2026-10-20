# Script extenders: what Tale of Two Wastelands and mods need, and a plan

NVSE (for New Vegas) and FOSE (for Fallout 3) are programs that start the game and patch it so that scripts can do more:
they add script commands, parts of a richer script language, events, and a place for plugin DLLs that add commands of
their own. Many mods are written for them, and so is Tale of Two Wastelands (TTW) itself. This page says what the data
shows, what OpenFallout does with it today, and the work a later milestone would do, in the order of what it buys. M8's
part of that work is the census and the run-time count at the end of "What the engine does today"; nothing else here is
written yet.

Sources: Mac rounds 8B (parts E and D), 8B2, 8B3 and 8D (part D) on the 14 plugins of the TTW output folder (counts,
bytes, editor IDs, setting names and the source text of scripts, no game text; one row per code is in
`ttw-extension-codes.csv` next to this page, the Mac's longer notes are in the project's shared folder and not in the
repository), the executables of both games (neither mentions an extender: Mac rounds 4 and 8A), and the public source of
xNVSE, read for how the commands are numbered and nothing else (see "Where the names come from"). Every name of a
command below is the one the scripts' own source text gives the code (all 139 codes have one); the bytes agree with the
names (a statement or an expression, on an object or not, the shape of the arguments), but no extender file was
available to check them, and the meaning and the parameters of a plugin-range command are known only from its name.

## What an extender adds to a compiled script

A call of a command in a compiled script is `code(2) length(2) arguments`, with `1C reference(2)` in front when it is
called on an object, so a reader can step over a call it does not know. The commands of the games are `0x1000` and up
(615 in New Vegas, 545 in Fallout 3; `esmtool scriptcode --commands` decodes their arguments from the table that
`scripts/openfallout/dump_command_table.py` reads out of the executable). An extender adds four things without changing
that file format:

1. **Commands.** NVSE numbers its own from `0x1400`, leaving the room below that to the games. A plugin of NVSE asks for
   a base of its own from `0x2000` up and gets consecutive codes after it, so a code from `0x2000` has a name only if the
   plugin that owns its range is known.
2. **Parts of a language.** `Let`, `While`, `Loop`, `Break` and the call of a user function are commands of that range
   like any other; a call can also stand inside an expression (the token `X`, 0x58, then code, length and arguments,
   as in the games). Arguments are written in one of two ways. Most commands use the plain layout of the games: a count
   `(2)`, then items `n` + int32, `z` + float64, `r` + 16-bit index into the reference table of the script, or a string
   as a 16-bit length and the bytes. The commands that make the language use an expression layout: items with their own
   length (counting its two bytes) that hold tokens, `R` (0x52, a reference, 16-bit index), `V` (0x56, a variable:
   type byte, 16-bit index of the reference or 0 for a variable of this script, 16-bit variable index), `S` (0x53, a
   string with a 16-bit length) and `$` (0x24, a pair marker), and for `Let` the postfix tokens of an expression.
3. **User functions.** A script that has one block, `Begin Function {parameters}` ... `End`, is a function; block type 13
   in these files (the games' table has no row for it). The Begin carries `01 N {16-bit variable index, type byte} x N 00`
   for its N parameters, and the variables are in the script's own list (SLSD). A function is not run by the game: it is
   run by the call of it, or by an event.
4. **Events.** A command registers a function for a game event (hit, item added, item equipped, a menu closes); the
   extender calls it when the event happens.

A fifth thing is not a file format: the plugin DLLs patch the game's own code at fixed addresses, so they can not run
inside OpenFallout, and what they add has to be written again one command at a time or left out.

## What the Tale of Two Wastelands plugins use

(Mac 8B parts E and D, 8B2, 8B3.) The Fallout 3 files with their five add-ons, the New Vegas files with their five
add-ons and the four packs hold no call of a code that their own game's table lacks (M4 and 8B: 0 and 0), and flag
`0x4000` (partial forms) is on no record of any of the 18 files. All the extender use is in the two TTW files:

- 132 codes, 1080 calls in 327 scripts: 1065 calls in 314 scripts of `TaleOfTwoWastelands.esm`, 15 calls in 13 of
  `YUPTTW.esm`. 652 calls are statements, 428 stand in expressions. (The decoder lists the call of a statement such as
  `Let`, `While` or `Call`, but not a call that stands among the argument bytes of that statement. Mac round 8D part D
  scanned those too and found 17 more calls of 7 more codes: `ar_construct` `0x154F`, `ar_size` `0x1550`, `ar_null`
  `0x1562`, `GetLoadedTypeArray` `0x224E`, `GetTeammates` `0x26CE`, `GetPlayerPerks` `0x286B` and `TTW_GetPlugins`
  `0x3806`, 139 codes and 1097 calls in all. The CSV has the 7 with `decoder` = `misses`, and the language step has to
  read such a token in the expression bytes: `58 00 00 code(2) length(2) body`.) 52 scripts are user functions; 189 of the 327
  scripts are functions or have an extension call in an expression, the other 138 (250 calls) are plain statements.
- 54 codes (496 calls) are in the NVSE range `0x1400` to `0x1FFF`, 78 codes (584 calls) are from `0x2000`.
- 69 scripts whose editor ID starts with `TTW` hold 584 calls; the other 258 scripts (496 calls) are scripts of the
  original games that TTW or the YUP patch changed, among them 18 `INFO` result scripts. The plugin-range codes are
  not only TTW's own: 22 of the 78 codes are called only by changed original scripts (by their names, a
  counter in 27 bobblehead scripts, `0x31E5`; the traps of the New Vegas add-ons, `0x3CE0`, in 21; achievements), 13 by both kinds.

The 54 core codes, by what they are (53 of the names agree with the order of the NVSE command table; the scripts spell
`0x14A0` `PrintC`, the short form of `PrintToConsole`):

| Group | Commands (calls) | Calls |
|---|---|---:|
| The language | Call 83, Let 29, While 25, Loop 25, Break 3 | 165 |
| Events | SetEventHandler 4 | 4 |
| The extender itself | GetNVSEVersion, GetNVSERevision, GetNVSEBeta 1 each, IsModLoaded 9, IsPluginInstalled 1, GetPluginVersion 4 | 17 |
| State of the game | GetGameRestarted 6, GetGameLoaded 4 | 10 |
| Form lists and arrays | ListGetCount 10, ListGetNthForm 18, ListGetFormIndex 4, ListAddForm 2, ar_Append 1 | 35 |
| Game and ini settings | SetNumericGameSetting 51, GetNumericGameSetting 2, SetNumericIniSetting 12, GetNumericIniSetting 1 | 66 |
| Console, keys, logic | PrintC 47, Print 2, IsKeyPressed 8, LogicalAnd 20, LogicalOr 11 | 88 |
| Objects and actors | GetEquippedObject 22, IsReference 13, SetPackageTargetReference 12, GetPerkRank 12, IsFormValid 10, Update3D 5, GetType 4, and 19 more commands of 1 to 3 calls | 111 |

The 78 plugin-range codes, by the high byte of the code: `0x22` 8 codes / 61 calls, `0x26` 8 / 47, `0x27` 23 / 148,
`0x28` 31 / 255, `0x31` 5 / 42, `0x38` 2 / 10, `0x3C` 1 / 21. The five most called are `0x2837` `ToggleHardcoreTracking`
(66 calls), `0x27EC` `IsSpellTargetAlt` (50), `0x288B` `SetGameHour` (39), `0x2829` `CastImmediate` (33) and `0x282C`
`GetHitAttacker` (28). Which plugin owns which range is only partly known: the scripts' text gives TTW's own for the
names that start with `TTW_` (`0x3800`, `0x3806` and `0x3808`, `TTW_EnableRadioFix`) and one comment says that
`EnableNavMeshAlt` (`0x26EE`) is a function of JIP LN, so the other `...Alt` names (`0x26ED`, `0x2606`, `0x2717`,
`0x2718`, `0x27EC`, `0x27E2`) are probably its too (Mac 8D part D4). The rest is not known (see below).

**What TTW asks of the extender.** The quest script of `TTWGenericHandler` sets three globals from the version commands
and `TTWGenericHandlerScript` tests `100 * version + 10 * revision + beta < 635`, so TTW wants NVSE 6.3.5 or later; it
asks `IsPluginInstalled "ttw_nvse"` (TTW's own NVSE plugin, the "custom NVSE plugin" its readme credits) and
`GetPluginVersion` of `JIP NVSE Plugin` (57.2), `JohnnyGuitarNVSE` (501), `lStewieAl's Tweaks` (941) and `ShowOffNVSE
Plugin` (180); it asks `IsModLoaded` for five mods that change the same things (New California, Mission Mojave, NVEC,
NVMIM, the YUP patch) and for the four packs. It writes 20 game settings (hacking word counts, pick-pocket chances, the
speech experience rewards, the VATS multipliers, karma for murder, the distance of map markers, the perk interval) and
two ini settings (the cool-down of the sound a hit makes in a conversation). It registers four event handlers with the
core command (`OnHit`, `OnActorEquip`, `OnAdd`, `OnHitWith`) and 19 more with plugin-range commands.

**What stops working without it** (inferred from the editor IDs of the 26 functions that are called and of the 22 that
are event handlers): the follower commands (fire, wait, follow all followers), switching the two wastelands on and off,
the hardcore mode, the perk changes (functions named Stay Frosty, Daddy's, Scoundrel, Thief, Lockpick Bobblehead and Covert Ops), the
ammunition press, the weapon exchange of the Sink, the fixes for melee, darts, incendiary ammunition and the Pyromaniac
perk, and the checks that TTW makes of the other mods and plugins.

## What the engine does today

- All but 7 of the 35941 scripts of the 14 plugins decode (the 7 are the M4 ones, with no extension call before the
  stop); a call of a code the table lacks is kept as its code and bytes.
- Such a call is reported once in the log (`A script calls the command 0x...., which is not in the table`), stepped over
  by its length, and has the value 0 in an expression. So the script goes on, and `If` or `Set` on the value of an
  extender function sees 0. The version test of TTW therefore reads `100 * 0 + 10 * 0 + 0 < 635`, which is true: what
  TTW's handler does then is for the real-data round to show.
- One code is known by name and works: `0x3800`, `TTW_ShowGeneProjector` (2 calls, no arguments, in the quest `CG00` and
  in `Vault101ExitScript`; the name is the one the scripts' source text has, Mac round 8D part A). The engine opens the
  menu for the race and the face for it, as for `ShowRaceMenu` where Fallout 3 calls that, and `smoke` and a unit test
  check it. `extendercommands.cpp` is where such a name is added; the other 138 codes are still steps over and counts.
- The 52 function blocks never run, because nothing calls them: the interpreter runs the blocks of the events it
  knows, and a function is not one.
- A condition (`CTDA`) that names a function the table lacks is reported once and has the value 0, compared as usual.
  The 14 plugins hold 201,884 conditions; three extender functions are among them, 22 conditions in all and only in the
  two TTW files (index `0x17EC`, the script code `0x27EC`, in 17 conditions of 11 records; `0x177C` in 2 conditions of
  one spell; `0x2807` in 3 of one perk). A condition's function index is the script code minus `0x1000`.
- M8 adds a count: the engine now counts how often scripts reach a code the table lacks and prints it with the other
  missing commands at the end of a run (`Commands that scripts reached and that are not implemented, with how often`),
  so a start of TTW says which of the 132 codes a play-through reaches. The count takes the conditions in too: a
  function that a record's condition names and the table lacks (the three above, and the two of New Vegas in "Not
  done") is counted whenever the condition is tested, so the list can hold a code that no script calls.

## The plan

The order follows what each step makes work, and what it needs from the engine.

1. **The identity of the extender, and honesty about it.** `GetNVSEVersion`, `GetNVSERevision`, `GetNVSEBeta`,
   `IsPluginInstalled`, `GetPluginVersion`, `IsModLoaded` (17 calls). `IsModLoaded` is a look at the content list and
   costs nothing. The other four decide what scripts believe about the engine, so they answer last, in the step in
   which what they promise is true: a script that sees version 6.3.5 will call every command of NVSE up to it. Until
   the language works, they answer as when nothing is installed (0, false), and the scripts that have a way around
   (most mods) take it.
2. **The language and the user functions** (the biggest step; 169 calls, the 52 functions, and the 428 calls that stand
   in expressions). An evaluator for the tokens `R`, `V`, `S`, `$`, numbers and the operators of an expression;
   `Call` with its parameters (the argument count always equals the parameters the function declares: 82 of 82), a
   frame for the variables of a function, and the return of a value; `Let`, `While` with its jump (the offset of the
   byte after the `Loop`, 25 of 25 pairs), `Loop`, `Break`; `SetEventHandler` with an event table. The engine has to
   raise `OnHit`, `OnActorEquip`, `OnAdd` and `OnHitWith`, which it does not yet. Real bytes are in the notes of the
   Mac rounds (the 50-byte `TTWStayFrostyFunction`, the call with two parameters, the 29 `Let` expressions), so every
   part gets a unit test from a real script. This step is what the plugin-range handlers (19 more registrations) also
   need.
3. **The plain core commands TTW uses** (the lists, the settings, the object and actor queries: about 300 calls). Each
   needs a fact of the engine: the form lists (`FLST`) are in the stores; the game settings need a store that scripts
   can write (a change of a setting must reach the code that reads it, such as the hacking game and the pick-pocket
   chance); the equipped object and its condition need the equipment and condition state of M5 and M6; `Update3D` and
   `SetPackageTargetReference` need the world.
4. **The plugin-range codes** (584 calls). The scripts give each a name, not the parameters, the meaning or the owner. Two ways: the DLLs
   themselves (dump the registered command table like the games' table is dumped, which gives names and parameters
   without copying any list), or the public documentation of each plugin (names and meanings, written again by hand).
   The first is the better one and needs Jacob (below). Order by use: `0x2837`, `0x27EC`, `0x288B`, `0x2829`,
   `0x282C`, then the bobblehead and trap counters.
5. **Mods beyond TTW.** The retail setup on the Mac lists JIP LN, JohnnyGuitar, ShowOff, kNVSE, lStewieAl's Tweaks and
   others; each of them is hundreds of commands. They are written one at a time as mods ask for them; the run-time
   count of the engine says which a given load order reaches. Native plugin DLLs, hooks on the game's code and the
   interface plugins (MCM and the like) are not planned.

**How each step is proven.** A unit test from the bytes of a real script; a synthetic script in the smoke test's
synthetic plugin; then a Mac round that starts TTW with nothing named and reads the count of missing codes (it must
fall), the message boxes and the log for the scripts of that step.

**Fallout 3 and FOSE.** The Fallout 3 files and their add-ons hold no FOSE command (0 codes, M4 and 8B), so FOSE only
matters for mods. It is built like NVSE (commands above the games' own, a plugin range, the same bytes for a call), so
the work of steps 1 to 3 serves it; its command set and numbering were not read for this page and are for the step that
needs them.

## What it needs from Jacob

Nothing blocks steps 1 to 3. Step 4 needs the extender files of a working TTW install, which only Jacob has: the NVSE
loader and `nvse_1_4.dll`, and the plugin DLLs TTW asks for (`ttw_nvse`, JIP LN, JohnnyGuitar, ShowOff, lStewieAl's
Tweaks, kNVSE, and any other plugin of the mod list). None is on the Mac (Mac 8A part E and 8B part E6), and a new
download is Jacob's to make. With them, the command tables dumped from the DLLs give the names, the parameters and the
owner of every range; without them the 78 codes have a name from the scripts and nothing else.

## Where the names come from

The names of the 54 core codes were first got by counting the order in which the public NVSE source registers its
commands from `0x1400`, each imported console command taking a code (for example `0x1400` to `0x1402` are the three
version commands, `0x1545` is the call of a user function). They match the shape of every call in the files (statement
or expression, on an object or not, the arguments), and the scripts' source text confirms 53 of them (Mac 8D part D).

Every code of the 14 plugins, the plugin-range ones too, has a name from the source text of the scripts (`SCTX`, kept
in the plugins): in 326 of the 327 scripts that call an extender code, the identifiers that are none of the games'
commands, keywords, variables, editor IDs or words of ordinary scripts are as many as the codes and stand in the same
order, so the i-th code is the i-th name. 40 codes are certain (a script with one code and one name), 17 follow by
elimination and 82 by the order alone (the rule held at 762 of 762 places where a code was already certain); 53 of
those 82 are called in one script only. `ttw-extension-codes.csv` has the class of each name. The project of that source has no licence file, so nothing of its code
or its tables is copied here or in the engine; a table of names that ships has to be dumped from the DLL, or written
by hand from the layouts above, and checked against the DLL when there is one. The names of the plugin-range codes are
the scripts' own.

## Not done

- Every step of the plan above. The engine knows one of the 139 codes, `0x3800`; it still treats the other 138 as commands it does not know.
- The meaning of the tokens `42`, `18`, `73` and `6e` in the `Let` expressions, and of the `58` token's arguments, was
  not decoded (the layout and the sample bytes are in the Mac notes).
- Which plugin owns which range from `0x2000`, beyond TTW's own `TTW_` names and the `...Alt` names of JIP LN.
- The three extender functions that conditions name (above) are not implemented, so those 22 conditions read 0.
- Not an extender matter, found on the way: `FalloutNV.esm` has 25 conditions with the function index `0x262` and 74
  with `0x264`, and the command table has no row for the codes `0x1262` and `0x1264`. `0x264` (`PlayerInRegion`) is
  implemented, and `0x262` (`GetCasinoWinningStage`) answers 0, the answer of a player who has not gambled (see
  `mods-and-load-order.md`).
- FOSE's own command set and numbering.
