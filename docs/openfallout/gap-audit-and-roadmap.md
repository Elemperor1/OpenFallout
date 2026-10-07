# OpenFallout: gap audit and roadmap

Audited at commit `f90f239d` (OpenMW 0.52.0 mirror, master of 2026-10-03).

## Goal

Turn this tree into an open-source engine that plays Fallout 3, Fallout: New Vegas and Tale of Two Wastelands (TTW) from the user's own game files. The target is complete, faithful play: quests, combat, dialogue, companions, exploration, saves and meaningful mod compatibility.

## How to read this document

- **Status words describe what the source shows, not what runs.** No Fallout game files were available when this was written, so nothing below was run against real FO3 or FNV data. Each row says *present*, *partial*, *parsed only* or *missing*, with a `file:line` pointer you can check.
- **Claims marked (inferred)** come from general knowledge of the Fallout formats or from reading around the code, not from a direct check. Treat them as questions to confirm.
- The record counts in section 2 come from a method described in the appendix, which you can rerun.

## Starting point

The repository is OpenMW's tree. It has no OpenFallout-specific commits, branches, issues or planning notes. The README, CI and docs still describe OpenMW and Morrowind.

OpenMW already ships a Bethesda-generation (ESM4) layer written for Oblivion and Skyrim content, with some FO3/FNV awareness. That layer is the foundation, and it is thin where gameplay begins.

## Audit

### 1. Loading content

| Area | Status | Evidence | Gap for FO3/FNV/TTW |
|---|---|---|---|
| ESM4 plugin reader | Present | `components/esm4/reader.cpp` (zlib record decompression at `:35`, `:80`); FO3/FNV version constants `components/esm/common.hpp:27-34` | Reads headers, groups and compressed records; per-record fidelity varies (see section 2). |
| Plugin dispatch | Present | `apps/openfallout/mwworld/esmloader.cpp` sends `Tes4` files to `ESMStore::loadESM4`; FormID remapping via `updateModIndices` | Multi-master FormID remapping exists. TTW needs FO3 and FNV masters loaded together (inferred); never tried. |
| Archives | Present | BSA versions 0x67 (TES4), 0x68 (FO3) and 0x69 (SSE) in `components/bsa/compressedbsafile.hpp:55-57`; BA2 in `components/bsa/ba2*.cpp` | Never checked against real FO3/FNV archives. A search for "invalidat" in `components/vfs`, `components/bsa`, `components/files` and `engine.cpp` finds nothing, so archive invalidation is not handled. |
| Launcher content list | Partial | `components/contentselector/model/contentmodel.cpp:460` reads Tes4 headers | Launcher and install wizard text and defaults are Morrowind-specific (`apps/wizard`, `apps/launcher`). |
| `esmtool` | Missing for ESM4 | `apps/esmtool/esmtool.cpp:437` prints "Printing raw TES4 file is not supported" | No tool to dump or census a Fallout plugin. |

### 2. Records

`components/esm4` has 79 `load*.hpp` headers. 77 of them declare a record type (the other two are `GRUP` and `TES4`). Of those 77, **39 are stored** in `ESMStore` and **38 are parsed and then discarded**. The stored list is the `ESM4::` entries of `StoreTuple` in `apps/openfallout/mwworld/esmstore.hpp:130-156`; a record without a store is skipped after parsing (`components/esm4/readerutils.hpp:29`, `esmstore.cpp:268-295`).

Parsed but not stored (a loader exists, the engine never sees the data):

- Game logic: `QUST`, `DIAL`, `INFO`, `SCPT`, `PACK`, `GLOB`, `GMST`, `FLST`, `CLAS`
- Items and world: `KEYM`, `NOTE`, `TACT`, `REGN`, `PGRE`, `PWAT`, `ANIO`, `GRAS`, `ROAD`
- Presentation: `MUSC`, `MSET`, `ALOC`, `ASPC`, `LGTM`, `EYES`, `IDLE`, `IDLM`
- Navigation and physics data: `NAVI`, `NAVM`, `PGRD`, `BPTD`, `DOBJ`
- Probably not used by FO3/FNV (inferred): `APPA`, `MATO`, `SBSP`, `SCRL`, `SGST`, `SLGM`, `CLFM`

Fallout record types with **no parser at all** (inferred from public format references, not checked against game files): `WTHR`, `CLMT`, `WATR`, `SPEL`, `ENCH`, `MGEF`, `PERK`, `AVIF`, `PROJ`, `EXPL`, `IPCT`, `IPDS`, `EFSH`, `CSTY`, `LSCR`, `IMGS`, `IMAD`, `ECZN`, `MESG`, `RGDL`, `VTYP`, `RADS`, `DEBR`, `CAMS`, `CPTH`, `ADDN`, plus the New Vegas additions `CHAL`, `REPU`, `CSNO`, `CHIP`, `CMNY`, `CCRD`, `RCPE`, `RCCT`, `DEHY`, `HUNG`, `SLPD`. `FACT` had none either until M1 slice 2. All of them have a loader since M1 slices 3 to 5, and the census on the 20 real plugins shows no type without one (see M1 progress).

Other record-level notes:

- `components/esm4/loadscpt.cpp:74` keeps the script *source* text (`SCTX`). The compiled bytecode (`SCDA`) is skipped (`loadscpt.cpp:79-110`, the read is inside `#if 0`). The comment on `ScriptDefinition::scriptSource` (`script.hpp:374`) calls it "compiled source", which is wrong.
- `components/esm4/script.hpp` already names 250 condition function indices (`FUN_*`), the table that dialogue and quest conditions use.
- Known parser gaps are marked in the code: `loadrace.cpp` has 14 FIXME/TODO, `loadnavm.cpp` 16, `loadarma.cpp` 17 (several say "FIXME ... FO3/FONV"), `loadlvli.cpp:104` guesses a flag bit.

### 3. Rendering and world

| Area | Status | Evidence | Gap |
|---|---|---|---|
| NIF meshes (FO3/FNV) | Present | `components/nif/niffile.hpp:31,35` (version 20.2.0.7, Bethesda version 34); shader properties `BSShaderPPLighting`/`NoLighting` handled in `components/nifosg/nifloader.cpp:2573,2587,2836` | Visual fidelity of FO3 shaders is unverified. |
| Statics, doors, activators, containers, lights | Present | classes registered at `apps/openfallout/mwclass/classes.cpp:78-103` | Needs real-data check. |
| Exterior terrain | Present | `components/esmterrain/storage.cpp:92,365,389` (ESM4 land, layers, blendmaps) | Not verified on FO3/FNV worldspaces. |
| Distant objects | Present | `apps/openfallout/mwrender/objectpaging.cpp:24-26` includes ESM4 activator, container and door types | Static LOD files (`.btr`/`.bto`) not handled (inferred). |
| Interior lighting and fog | Partial | The cell colours (ambient, sun, fog) from `XCLL` already reached the renderer (`apps/openfallout/mwworld/cell.cpp`, `renderingmanager.cpp:configureAmbient`). The fog range (`Fog Near`, `Fog Far`) is now used as the fog distances (`fogmanager.cpp`), and the lighting template of a cell (`LTMP` and the `LNAM` inherit flags, record `LGTM`, stored since this slice) fills the values the cell inherits. Checked on a synthetic plugin. | Sun direction (`Directional Rotation`), directional fade, fog power and clip distance are read but not used; the fog is linear. Not seen on real Fallout data: whether the colours and distances look right in the real cells, and interiors with no `XCLL` (the old density of 1 still applies there). |
| Weather, sky, climate | Partial | `WTHR` and `CLMT` are in the store. A Fallout exterior cell has the weather of the climate of its cell or worldspace: the sky, fog, ambient and sun colours at sunrise, day, sunset and night, the fog distances, the wind and the sun glare come from `NAM0`, `FNAM` and `DATA` (`Weather::Weather(…, const ESM4::Weather&, …)` in `weather.cpp`), and the weather is chosen by the `WLST` chances of the climate. | The sky itself: dome, clouds, stars and sun and moon textures (the sky meshes of Morrowind are missing and show the error marker), rain, snow and thunder, the times of sunrise and sunset of the climate (`TNAM`; the engine keeps the Morrowind ones), the globals of the weather list change while the game runs (a weather that needs one is chosen when the global of its `GLOB` record is not 0, nothing sets it yet; that is M4), high noon and midnight colours of New Vegas, interior sky. The `NAM0` layout and the scale of the transition delta in `DATA` (255 steps up to 0.25 per second, as in xEdit's definitions of newer games) are from format references and are not checked on real game data. |
| Water | Partial | height taken from cell or worldspace (`cell.cpp:57-72`); no `WATR` parser | Water appearance and effects. |
| Interior/exterior navigation data | Parsed only | `NAVM`/`NAVI` not stored | OpenMW builds its own navmesh at runtime, so this may not be needed (inferred). |

### 4. Actors

| Area | Status | Evidence | Gap |
|---|---|---|---|
| NPC appearance | Partial | `apps/openfallout/mwrender/esm4npcanimation.cpp` (189 lines) assembles race body parts, armor addons and head parts; line 44 notes "no easy way to distinguish TES5 and FO3" | No FaceGen morphs (the file never mentions them) and it never adds an animation source or `.kf` file. |
| NPC collision | Missing | `apps/openfallout/mwclass/esm4npc.hpp:45-50`: `insertObjectPhysics` body is commented out | NPCs have no physics body. |
| Creatures | Partial | `ESM4Named<ESM4::Creature>` generic class (`classes.cpp:83`) | Same as NPCs. |
| Statics collision | Hack | `components/nifbullet/bulletnifloader.cpp:134` "FIXME: hack, using rendered geometry instead of Bethesda Havok data" | Havok shapes are never read for collision. |
| Animation | Missing | no ESM4 hits in `apps/openfallout/mwmechanics` | Idle, locomotion, combat and VATS animation (`.kf`). |

### 5. Gameplay systems

A search for `ESM4` in `apps/openfallout/mwmechanics`, `mwdialogue`, `mwscript`, `mwinput`, `mwstate` and `mwphysics` returns nothing. In `mwgui` it finds three files (`bookwindow.cpp`, `scrollwindow.cpp`, `console.cpp`).

| System | Status | Evidence | Gap |
|---|---|---|---|
| Script execution | Missing | script source kept, bytecode skipped; the existing script runner is the Morrowind one (`apps/openfallout/mwscript`) | The whole FO3/FNV script language (ObScript) and its engine functions. |
| Quests and dialogue | Parsed only | `QUST`/`DIAL`/`INFO` parsed, not stored | Topics, conditions, result scripts, voice, UI. |
| Stats, perks, skills | Missing | no `AVIF`/`PERK` parsers; `FACT` is parsed, not stored | Actor values, leveling, S.P.E.C.I.A.L. |
| Combat, weapons, VATS | Missing | weapons are stored as items only | Everything. |
| AI packages | Parsed only | `PACK` parsed, not stored | Schedules, follow, sandbox, travel (companions depend on this). |
| Item use and activation | Partial | `esm4base.hpp:140` says activation "can be handled in Lua"; `ESM4Terminal` exposes text to Lua (`apps/openfallout/mwlua/types/terminal.cpp`) | Lua is the only gameplay hook today. |
| Lua API coverage | Partial | 17 ESM4 object types in `files/lua_api/openmw/types.lua`; none for NPC, creature, container, furniture or tree | Needed before Lua can drive actors or containers. |
| Audio | Partial | `SOUN`/`SNDR` buffers (`apps/openfallout/mwsound/soundbuffer.cpp:111-113,202-212`) | Music (`MUSC` parsed only), voice and lip files. |
| Save and load | Missing | `apps/openfallout/mwworld/cellstore.cpp:151-156`: "TODO: Implement loading/saving of REFR4 and ACHR4"; ESM4 references are skipped on save | No save game can round-trip Fallout state. |
| New game | Partial | A Fallout-only content list starts in a chosen interior cell: `World::startNewGame` (`worldimp.cpp:266`) skips character generation when a start cell is given, `findInteriorPosition` looks for the ESM4 statics `COCMarkerHeading` and `XMarkerHeading` and for door destinations, and `insertPlaceholderRecords` (`apps/openfallout/mwworld/placeholderrecords.cpp`) supplies the player, race, class, skills, magic effects, clock globals and game settings that only Morrowind format content holds. `--start <Worldspace>:<x>,<y>` starts in an exterior cell of a worldspace (`worldspacestart.cpp`, `findExteriorPosition`; the cell comes from `WorldModel::getExterior`, which already loaded ESM4 exterior cells and their `LAND` terrain). Checked on a synthetic plugin (`build-baseline.md`, "The Fallout start smoke test"). | Not tried on real Fallout 3, New Vegas or Tale of Two Wastelands data. The Fallout character creation and the real player record are not done; the placeholders keep Morrowind names. An exterior start has Morrowind's sky meshes missing (see the weather row) and no water (no `WATR` in the store), and the start position of an exterior cell is its `COCMarkerHeading` or `XMarkerHeading` when it has one and its centre when it has not (the same order as the interior start and the `x,y` form), which is not checked on real data. |

### 6. Tests and CI

- `apps/components_tests/esm4/includes.cpp` only checks that the headers compile. No parser has a behavioural test and no fixture data exists.
- BSA and NIF physics have unit tests (`apps/components_tests/bsa`, `.../nif`).
- `.github/workflows/push.yml` builds on Ubuntu and runs `components-tests`, `openfallout-tests` and `openfallout-cs-tests` (named `openmw-tests` and `openmw-cs-tests` before rename stage A). `.gitlab-ci.yml` and `CI/` are upstream leftovers.

## Recommended milestones

Ordered so each milestone unlocks the next. "Exit" is a check a person can run.

**M0: Feedback loop.** Get a clean build and test run in a known environment, because the rename (see "Rename to OpenFallout") cannot be checked without one. Add a plugin census tool that, on a user's own FO3/FNV/TTW files, reports per record type: count, parse failures and unknown subrecords. Add synthetic-record test helpers so parsers can have unit tests without game data. Then rename in the stages listed below.
*Exit:* CI green on this tree; census output committed for each game (counts only, no game content); the rename stages merged with the build green after each.
*Progress:* the build baseline and the census tool (`esmtool census`) are described in `build-baseline.md`. The census has run on Fallout 3, New Vegas and their add-ons (16 plugins) and the counts are in `census-results.md`: no record failed to load, and 42 record types have no loader (28 in Fallout 3, all 42 in New Vegas). The census also ran on the four Tale of Two Wastelands plugins, produced by a community installer on the same Mac (see "Getting game data to the project"): no unparsed record type is new, and 17 `NPC_` records in three of the plugins fail to load (three of them hit unknown subrecords, `DLVT` twice and `LSNA` once, and 14 are loader errors that are not yet explained). The installer's output was not hash-checked against the official installer, so some of the failures may come from the installer and not from TTW's data. GitHub Actions runs on the rename branches: the Ubuntu, Windows, macOS arm64 and macOS Intel jobs pass. The Intel job used to hang in its ccache step and now skips ccache (see `build-baseline.md`).

**M1: Complete data.** Store the parsed-only records the engine needs, then add parsers for the missing record types in priority order: `GMST`/`GLOB`, `FACT`, `WTHR`/`CLMT`/`WATR`, `SPEL`/`ENCH`/`MGEF`/`PERK`/`AVIF`, then the rest. Stop skipping `SCDA` bytecode in `SCPT` and `INFO` records (decision 3).
*Exit:* census reports zero skipped records and zero failed records for all three games.
*Starting point:* the 2026-10-04 census (`census-results.md`) lists the 42 types with no loader, with the roadmap order in a column. The 14 types only New Vegas has (such as casino, Caravan, recipes, reputation and challenges) fall under "the rest". The missing types hold 0.7% of the records in the 16 plugins, so M1 means covering many record types, not many records. The 17 `NPC_` records from the Tale of Two Wastelands run that fail to load (3 unknown subrecords and 14 loader errors) belong here too. First check whether they also fail on files made by the official TTW installer, and only then change the `NPC_` loader.
*Progress:* the first slice, script bytecode (decision 3, step 1), is implemented in PR #7. `SCPT`, `INFO` and `QUST` records keep their `SCDA` bytecode and their `SCRO`/`SCRV` reference table in one new `ESM4::ScriptDefinition`; `INFO` keeps its begin and end scripts apart; `QUST` now has stages and log entries, each with its own script; and the census counts the scripts and checks their headers (`census-results.md`, "Scripts held by records"). On the 16 installer plugins the compiled size matches the bytecode in every script, the reference count matches in all but one, and the variable-count check flags none (it flags 13 scripts in two TTW plugins). Still open in M1: the 42 types with no loader, the 17 failing `NPC_` records, and other script holders: `TERM` menu items and `PACK` begin, end and change events. Nothing runs the bytecode yet; that is M4. The second slice is `FACT`, the first of the missing types in the roadmap order (`GMST` and `GLOB` already have loaders): `ESM4::Faction` keeps the relations to other factions and races with the New Vegas group combat reaction, the flags, the ranks with their male and female titles, and the New Vegas reputation. It is parsed, not stored in `ESMStore`, like the other game-logic records. The census on the 20 real plugins (2026-10-06, rerun on the final head `0070f8bd`) parses all 2,528 `FACT` records with none failing, and no other cell of the main tables moved (`census-results.md`, "Faction records"). The real files have the 12-byte `XNAM` everywhere, in Fallout 3 too, so the 8-byte layout the format references give for Fallout 3 turned out to be Oblivion's. The third slice starts with a way to look before writing: `esmtool survey` lists, for the record types it is asked about (all of them by default), which sub-records they hold, how many, at which data sizes and in which order, and with `--failed` only for the records a loader rejects. It prints codes and numbers, never record contents, so it can run on game files that must not leave the machine; the loaders of this slice were written from its output on the 20 plugins (FO3, NV and TTW). It showed what the 17 failing `NPC_` records have in common: all three face generation sub-records (`FGGS`, `FGGA`, `FGTS`) are empty, and the loader read 200, 120 and 200 bytes into them, which ran into the next sub-records. `ESM4::RecordReader` is the new shared way to write a loader: it rejects a sub-record that runs past its record, a size no game uses, bytes that no sub-record accounts for and a file that ends inside a field, with the record's code in the message. The fourth and fifth slices finish the list. Slice 3 (PR #11) holds the loaders for 22 types: `WTHR`, `CLMT`, `WATR`, `AVIF`, `PROJ`, `EXPL`, `IPCT`, `IPDS`, `EFSH`, `CSTY`, `LSCR`, `LSCT`, `IMGS`, `IMAD`, `ECZN`, `RGDL`, `VTYP`, `DEBR`, `CAMS`, `CPTH`, `ADDN` and `MICN`, and the `NPC_` fix: an empty `FGGS`, `FGGA` or `FGTS` now reads as no coefficients, which makes all 17 failing records load. Slice 4 (PR #12) adds the 19 that were left: `SPEL`, `ENCH`, `MGEF`, `PERK`, `MESG`, `RADS`, `DEHY`, `HUNG`, `SLPD`, and the New Vegas types `AMEF`, `CCRD`, `CDCK`, `CHAL`, `CHIP`, `CMNY`, `CSNO`, `RCCT`, `RCPE` and `REPU`; the effect lists of `SPEL` and `ENCH` share one reader, and the destruction sub-records of the records that have them (`CHIP`, for one) another. Slice 5 (also PR #12) reads the scripts of `TERM` menu items and of `PACK` begin, end and change events into the same `ESM4::ScriptDefinition` and counts them in the census, which completes decision 3, step 1. `esmtool` uses the new loaders only for Fallout 3 and New Vegas files (`Reader::isFalloutFile()`, decided from the header version and the form version of the file's own `TES4` record), because Skyrim has records of the same types with other sub-records. A zero form ID read through `RecordReader` stays zero, so a null reference is not turned into a reference to the file that holds it. The loaders were compared with the sub-record lists of the Fallout 3 and New Vegas format references, which added the optional model, icon, sound and destruction sub-records that none of the 20 plugins has, and made the loaders strict about what those hold: model alternate textures are decoded (a texture set that is a form ID), perk entry data is read by the type of the entry, an effect without `EFID` starts an effect of its own, a menu item of a terminal starts where its sub-records start over, a condition may leave out its run on and its reference (20 and 24 bytes), and the structs that the references let end after any member (effect shader, image space and water data) are accepted at each of those sizes. The form IDs inside the blocks that are kept as bytes (projectile, explosion and effect shader data) are adjusted too. Form IDs in every loader of this milestone are adjusted to the load order and a null one stays null, which tests check by loading plugins as the third of the load order. Two things are left as they were: the parameters of conditions (which of them are form IDs depends on the function, `FIXME` in `script.hpp`) and the last eight bytes of `CHAL` data, which mean something else for each type of challenge (its type, threshold, flags and interval are decoded, and the value of a reputation is a float). *Status of the exit, 2026-10-06:* the census on the 20 real plugins (FO3, NV and TTW) reports no record without a loader and no failed record, in every plugin, on `f5f45ebf` and again on the last head of PR #12 (`census-results.md`, "Loaders of M1 slices 3 to 5"). The first sentence of M1, storing the parsed-only records in `ESMStore`, is deliberately not done: nothing reads these records yet, and each one should be stored when the milestone that uses it needs it (weather and water in M2, spells, perks and effects when the game rules need them), with the loader's fields as they are.

**M2: Walk the world.** Start in a chosen cell with a placeholder player, correct weather and sky, water, interior lighting and fog, collision for statics (real Havok data or an accepted substitute), NPC bodies with collision.
*Exit:* walk Megaton and the Goodsprings start without crashes or invisible walls.
*Progress:* the first slice, starting in a chosen cell with a placeholder player, is merged (PR 13). `openfallout --content Fallout3.esm --skip-menu --start <CellEditorId>` (without `--new-game`, which loads the Morrowind starting cell) starts a new game in that cell. When the content list holds no player record, the world makes placeholder records (player, race, class, 27 skills, 143 inert magic effects, 15 globals, every default game setting; the game setting tables moved from the Construction Set to `components/esm3/defaultgmsts.*` for this), skips the Morrowind `main` global script when there is none, and uses a bare skeleton (`files/data/meshes/placeholder_skeleton.nif`, installed in `resources/vfs-fallback`, which the engine adds below every archive and data directory) because the base skeleton files that Morrowind supplies are missing. The skeleton needs its `Bounding Box` node, without which the player has no collision size and falls through the floor, and a `Head` node at eye height, without which the first person camera stays at the world origin and the view is empty (found on the first run on New Vegas data: a flat colour with the interface on top). A synthetic plugin and a Lua walk test check it end to end: the player stands on the floor, walks and is stopped by a wall. The second slice, interior light and fog, is in a pull request: the fog distances of a cell (`Fog Near` and `Fog Far` of `XCLL`) are the fog distances of the renderer instead of a constant density, and a cell takes the values that its inherit flags name from its lighting template (`LGTM`, now loaded into the store). The synthetic plugin has such a template and the smoke test reads the resolved lighting from the log line "Cell lighting:". The ambient, sun and fog colours were already used; the sun direction, fog power and clip distance of the cell are not. The third slice, starting in an exterior cell, is in a pull request: `--start WastelandNV:<x>,<y>` (a worldspace editor id, a colon, two grid coordinates) loads that cell and its neighbours with their `LAND` terrain and collision, and the player starts at the entry marker of the cell, or at its centre when the cell has none. The synthetic plugin gains a worldspace with a flat `LAND` record and the smoke test runs both starts. The fourth slice, weather, is in a pull request: `WTHR` and `CLMT` are loaded into the store, an exterior cell takes the climate of its cell (`XCCM`) or of its worldspace (`CNAM`, or the one of the parent worldspace when the `PNAM` flags say it uses that) as its region (an interior cell that shows the sky has only the climate it names itself), and an exterior cell with no climate at all has no region, so the weather stays as it was, as for a Morrowind cell without a region, and the weather manager builds a weather from each `WTHR` record that has colours and a region weather from each climate (the `WLST` chances, scaled to add up to 100, for the weathers that exist). The colours and fog distances of the weather, which are by time of day, drive the fog and the clear colour of the renderer, and the player starts in the weather of the region at once, not in a transition from Morrowind's clear weather. A weather that needs a global (`WLST`) is left out while the global of its `GLOB` record (now in the store) is 0, and the others share its chance. The save game keeps the weathers of Fallout by id: `WeatherState` has optional extra sub-records for them (`CWTX`, `NWTX`, `QWTX`, `RGNX`, `RGNF` with `RGNG`) after the index records of Morrowind's ten, so saves of Morrowind content are unchanged. The engine logs the weather it chose in a "Weather:" line, and the smoke test checks that line and that the colour of the screen where nothing is drawn is the fog colour of the weather. Not done in M2 yet: any check of the light, fog, exterior start and weather on real game data, the sky (dome, clouds, stars, sun), rain and thunder, water, collision from Havok data, NPC bodies.

**M3: Living actors.** Skeleton animation, locomotion, FaceGen, equipment, creatures, AI packages, companion follow.
*Exit:* NPCs idle, walk their schedules and visibly wear their gear.

**M4: Scripts and quests.** A C++ virtual machine that decodes and runs the compiled `SCDA` bytecode (decision 3), plus quest stages, global variables and the engine function set the base games use.
*Exit:* the first quest of each game can be completed from a clean start.

**M5: Dialogue.** Topic selection, conditions (the `FUN_*` table), result scripts, voice and lip sync, dialogue UI.

**M6: Combat and character.** Actor values, perks, damage, VATS, inventory, crafting, Pip-Boy.

**M7: Saves.** A save format for ESM4 references, script and quest state, with a version number from day one.

**M8: Mods and TTW.** Load order, loose-file priority, archive invalidation, TTW multi-master loading, and a plan for the script-extender functions (NVSE/FOSE) that many mods use.

**M9: Fidelity, performance, portability.**

## Decisions

Answered by Jacob on 2026-10-04, after reading the first version of this audit.

| # | Question | Decision | What follows |
|---|---|---|---|
| 1 | Upstream relationship | **Diverge.** | No more merging OpenMW master, so shared code (`mwworld`, `mwrender`, `components`) can change freely. Upstream fixes would have to be ported by hand. The GPLv3 licence and the OpenMW contributor credits stay (see the rename plan). |
| 2 | Where gameplay lives | **Whatever gives the best quality and performance.** | Default adopted: C++ for fidelity-critical and per-frame systems (script VM, AI, combat, physics, animation, dialogue conditions, saves). Lua stays for UI and extension hooks, which activation and terminals already use. Revisit a system if profiling says otherwise. |
| 3 | Script path | **Run the compiled bytecode (`SCDA`).** | The VM decodes `SCDA` directly, as the original engine does, so no compiler is needed. First step: stop skipping `SCDA` in `ESM4::Script` and `ESM4::DialogInfo` (`loadscpt.cpp:79-110`, `loadinfo.cpp:116-118`). `SCTX` source stays for tooling and debugging. Each engine function the scripts call needs its own implementation. Step 1 is done for `SCPT`, `INFO` and `QUST` in PR #7 and for `TERM` menu items and `PACK` events in PR #12 (see M1 progress). |
| 4 | Test data | **Jacob can provide the files from their machine or upload them.** | See "Getting game data to the project". |
| 5 | Naming | **Everything becomes OpenFallout.** | See "Rename to OpenFallout". |

## Getting game data to the project

The census tool and every real-data check need the installed game folders, not the installers: `Fallout3.esm`, `FalloutNV.esm`, the Tale of Two Wastelands plugin, and all the `*.bsa` archives from each `Data` folder.

- **Preferred: a Remote Control session on Jacob's machine.** The project can already see Jacob's MacBook as a connected device. A session there runs the census and test tools against the installed files in place, and only counts and logs come back. Nothing needs uploading, which matters because the archives run to many gigabytes. Each folder is approved by Jacob before a session can use it.
- **Fallback: upload to the cloud.** Files attached to the project are copied under `/mnt/project-files/uploads/hearth/`. I have not checked size limits, and multi-gigabyte archives may not fit, so this suits a few small plugins at most.
- **If only installers exist**, they have to be unpacked first. Installer-only copies (for example offline GOG installers) can usually be unpacked with a tool such as `innoextract` on Jacob's machine. This worked for New Vegas on 2026-10-04: the census ran on plugins unpacked from the GOG installer.
- **Tale of Two Wastelands has to be built by its installer.** TTW is not a download of plugins: the installer reads the user's own Fallout 3 and New Vegas folders and writes the patched plugins and archives. On 2026-10-05 the official Windows installer (v3.4) ran under Homebrew Wine 11 on the Mac for about 30 minutes with no visible progress and was stopped; it was not shown to be hung and may only have been slow. The community Linux installer (`SulfurNitride/TTW_Linux_Installer`, commit `936b0ae`) was read first, then built natively for arm64 and finished in about 3 minutes. Keep both game folders read-only while it runs.
- **Never commit game files.** They are copyrighted. Committed census output is counts only.

## Rename to OpenFallout

Sizing at commit `f90f239d`, excluding `extern/`:

- 1,206 tracked files mention "openmw" (5,720 lines). 1,010 paths contain "openmw", almost all under `apps/openfallout/`.
- 10 executables are named `openmw*` (`openmw`, `openmw-cs`, `openmw-launcher`, `openmw-wizard`, `openmw-iniimporter`, `openmw-essimporter`, `openmw-navmeshtool`, `openmw-bulletobjecttool` and two test binaries), plus the `openmw-lib` and `openmw-cs-lib` libraries.
- Lua modules are named `openmw.*` (20 API files under `files/lua_api/openmw/`). `openmw.cfg` is mentioned 228 times. `OPENMW_*` guards and options are everywhere, and `OMWEngine` and `OMW*` symbols exist.
- 792 files use `MW*` namespaces (`MWGui`, `MWWorld`, `MWLua`, `MWMechanics` and others). `MW` stands for Morrowind.
- 206 files mention Morrowind outside `extern/` and the translations (launcher and wizard text, docs, data). Those belong with the new-game and launcher work in M2, not the rename.

The rename lands in stages, each one buildable and passing the tests before the next starts, so a break is easy to trace:

1. **Stage A, user-visible names.** Executable names, window titles, README and docs, config file and user-data directory names, packaging. Add an acknowledgement of OpenMW to the README.
2. **Stage B, interfaces.** Lua module ids (`openmw.*` to `openfallout.*`), `OPENMW_*` CMake options and header guards, `OMW*` symbols.
3. **Stage C, source layout.** `apps/openmw` to `apps/openfallout` using `git mv`, then `MW*` namespaces to `OF*` (for example `MWWorld` becomes `OFWorld`). `OF` is the default chosen here and is cheap to change before this stage starts.
4. **Stage D, what stages A to C leave (proposed; Jacob agreed to keep reading the old `.omw*` extensions).** GLSL names (`omw_*`, `OMW_*`), virtual file system and asset directories (`scripts/omw`, `openmw.png` and the other artwork), the `omw.*` settings keys, the `OMWInputBindings` section in `player_storage.bin` (kept at stage B so saved bindings survive; renaming it needs a migration), the `OMW_Generated_*` record ids, and the file extensions `.omwgame`, `.omwaddon`, `.omwscripts` and `.omwsave`. Several of these are formats other tools read and write, so each needs its own decision on whether to keep reading the old name.

Progress: stage A is done. The programs are `openfallout`, `openfallout-launcher`, `openfallout-wizard`, `openfallout-cs`, `openfallout-iniimporter`, `openfallout-essimporter`, `openfallout-navmeshtool`, `openfallout-bulletobjecttool`, `openfallout-tests`, `openfallout-cs-tests` and the three benchmarks. The CMake project, configuration and log files (`openfallout.cfg`, `openfallout.log`, `openfallout-cs.cfg`), settings directories (`~/.config/openfallout`), window titles, dialogs, translations, desktop and appdata entries, package and CI names, the README and `CONTRIBUTING.md` use the new name. Deliberately left for stage B or C: the `openmw-lib`, `openmw-cs-lib` and `openmw-navmeshtool-lib` libraries and the `openmw_add_executable` macro, Lua module ids and `OPENMW_*` names, `Role_OpenMW*` and `Version::getOpenmwVersionDescription` identifiers, the `OpenMW 0.48.0` and `OpenMW 0.52.0` names in the "save is too old" message (they name real OpenMW releases that can still read such a save), icon and logo artwork (`openmw.png`, `openmw.ico`, `openmw.icns`, `openmw_project_logo.webm` and the matching resource ids), `apps/openmw` and the `MW*` namespaces, and the OpenMW-owned documentation text under `docs/source`, which still describes the Morrowind engine.

Progress, stage B: done. Lua module ids are `openfallout.*` and `openfallout_aux.*` (the API files are under `files/lua_api/openfallout/`, the auxiliary modules under `files/data/openfallout_aux/`), the CMake options and variables are `OPENFALLOUT_*`, the internal libraries are `openfallout-lib`, `openfallout-cs-lib` and `openfallout-navmeshtool-lib`, the macros are `openfallout_add_executable` and `add_openfallout_dir` (`cmake/OpenFalloutMacros.cmake`), header guards, preprocessor macros and environment variables use `OPENFALLOUT_`, and the `OMW*` l10n contexts and Lua event names (`OMWEngine`, `OMWShaders`, `OMWConsoleEval` and the others) are `OF*`. A Lua mod written for OpenMW has to change its `require` ids to run here, and its `l10n` contexts if it used the engine's. Verification, including the startup smoke test that loads the built-in Lua scripts, is in `build-baseline.md`. Deliberately left: `apps/openmw` and the `MW*` and `OMW` namespaces (stage C), and everything in stage D above.

Progress, stage C: done. The engine sources are in `apps/openfallout` and their unit tests in `apps/openfallout_tests`. The C++ namespaces are `OFWorld`, `OFBase`, `OFMechanics`, `OFGui`, `OFRender`, `OFLua`, `OFPhysics`, `OFDialogue`, `OFSound`, `OFState`, `OFClass`, `OFInput` and `OFScript`, and `OMW` is `OF` (so `OMW::Engine` is `OF::Engine`). The `MWScript` name is kept where it means the Morrowind script language, as in the Lua API documentation. Deliberately left for stage D: the lower-case directory names under `apps/openfallout` (`mwworld`, `mwgui` and the rest) and `apps/mwiniimporter`, widget and skin names (`MWSkill`, `MWList`, `MW_Button` and the rest, which layout files refer to by name), `MWShadowTechnique`, the `MWUI` Lua interface that mods call, and the `BUILD_MWINIIMPORTER` option.

Not renamed: `AUTHORS.md`, `LICENSE` and the copyright notices in source files. GPLv3 requires keeping them, and the OpenMW contributors wrote most of this code. This is a plain reading of the licence, not legal advice.

## Appendix: how the counts were produced

- **Stored vs parsed-only.** For each `components/esm4/load*.hpp` that declares `sRecordId`, find the enclosing top-level struct and check whether `Store<ESM4::Name>` appears in `apps/openfallout/mwworld/esmstore.hpp`. Result: 77 headers, 39 stored, 38 parsed-only. `loadgrup.hpp` and `loadtes4.hpp` declare no record id. The tuple also holds `Store<ESM4::ActorCreature>`, which has no header of its own, so the count is per header, not per store.
- **Record types with no parser.** Absence of a matching `components/esm4/load*.cpp` file. The list of Fallout record types is from memory of public format references, so a name on it may be wrong.
- **Gameplay-side ESM4 usage.** `rg -il esm4 apps/openfallout/mwmechanics apps/openfallout/mwdialogue apps/openfallout/mwscript apps/openfallout/mwinput apps/openfallout/mwstate apps/openfallout/mwphysics` returns nothing.
