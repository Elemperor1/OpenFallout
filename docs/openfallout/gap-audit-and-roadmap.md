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
| Plugin dispatch | Present | `apps/openmw/mwworld/esmloader.cpp` sends `Tes4` files to `ESMStore::loadESM4`; FormID remapping via `updateModIndices` | Multi-master FormID remapping exists. TTW needs FO3 and FNV masters loaded together (inferred); never tried. |
| Archives | Present | BSA versions 0x67 (TES4), 0x68 (FO3) and 0x69 (SSE) in `components/bsa/compressedbsafile.hpp:55-57`; BA2 in `components/bsa/ba2*.cpp` | Never checked against real FO3/FNV archives. A search for "invalidat" in `components/vfs`, `components/bsa`, `components/files` and `engine.cpp` finds nothing, so archive invalidation is not handled. |
| Launcher content list | Partial | `components/contentselector/model/contentmodel.cpp:460` reads Tes4 headers | Launcher and install wizard text and defaults are Morrowind-specific (`apps/wizard`, `apps/launcher`). |
| `esmtool` | Missing for ESM4 | `apps/esmtool/esmtool.cpp:437` prints "Printing raw TES4 file is not supported" | No tool to dump or census a Fallout plugin. |

### 2. Records

`components/esm4` has 79 `load*.hpp` headers. 77 of them declare a record type (the other two are `GRUP` and `TES4`). Of those 77, **39 are stored** in `ESMStore` and **38 are parsed and then discarded**. The stored list is the `ESM4::` entries of `StoreTuple` in `apps/openmw/mwworld/esmstore.hpp:130-156`; a record without a store is skipped after parsing (`components/esm4/readerutils.hpp:29`, `esmstore.cpp:268-295`).

Parsed but not stored (a loader exists, the engine never sees the data):

- Game logic: `QUST`, `DIAL`, `INFO`, `SCPT`, `PACK`, `GLOB`, `GMST`, `FLST`, `CLAS`
- Items and world: `KEYM`, `NOTE`, `TACT`, `REGN`, `PGRE`, `PWAT`, `ANIO`, `GRAS`, `ROAD`
- Presentation: `MUSC`, `MSET`, `ALOC`, `ASPC`, `LGTM`, `EYES`, `IDLE`, `IDLM`
- Navigation and physics data: `NAVI`, `NAVM`, `PGRD`, `BPTD`, `DOBJ`
- Probably not used by FO3/FNV (inferred): `APPA`, `MATO`, `SBSP`, `SCRL`, `SGST`, `SLGM`, `CLFM`

Fallout record types with **no parser at all** (inferred from public format references, not checked against game files): `WTHR`, `CLMT`, `WATR`, `FACT`, `SPEL`, `ENCH`, `MGEF`, `PERK`, `AVIF`, `PROJ`, `EXPL`, `IPCT`, `IPDS`, `EFSH`, `CSTY`, `LSCR`, `IMGS`, `IMAD`, `ECZN`, `MESG`, `RGDL`, `VTYP`, `RADS`, `DEBR`, `CAMS`, `CPTH`, `ADDN`, plus the New Vegas additions `CHAL`, `REPU`, `CSNO`, `CHIP`, `CMNY`, `CCRD`, `RCPE`, `RCCT`, `DEHY`, `HUNG`, `SLPD`.

Other record-level notes:

- `components/esm4/loadscpt.cpp:74` keeps the script *source* text (`SCTX`). The compiled bytecode (`SCDA`) is skipped (`loadscpt.cpp:79-110`, the read is inside `#if 0`). The comment on `ScriptDefinition::scriptSource` (`script.hpp:374`) calls it "compiled source", which is wrong.
- `components/esm4/script.hpp` already names 250 condition function indices (`FUN_*`), the table that dialogue and quest conditions use.
- Known parser gaps are marked in the code: `loadrace.cpp` has 14 FIXME/TODO, `loadnavm.cpp` 16, `loadarma.cpp` 17 (several say "FIXME ... FO3/FONV"), `loadlvli.cpp:104` guesses a flag bit.

### 3. Rendering and world

| Area | Status | Evidence | Gap |
|---|---|---|---|
| NIF meshes (FO3/FNV) | Present | `components/nif/niffile.hpp:31,35` (version 20.2.0.7, Bethesda version 34); shader properties `BSShaderPPLighting`/`NoLighting` handled in `components/nifosg/nifloader.cpp:2573,2587,2836` | Visual fidelity of FO3 shaders is unverified. |
| Statics, doors, activators, containers, lights | Present | classes registered at `apps/openmw/mwclass/classes.cpp:78-103` | Needs real-data check. |
| Exterior terrain | Present | `components/esmterrain/storage.cpp:92,365,389` (ESM4 land, layers, blendmaps) | Not verified on FO3/FNV worldspaces. |
| Distant objects | Present | `apps/openmw/mwrender/objectpaging.cpp:24-26` includes ESM4 activator, container and door types | Static LOD files (`.btr`/`.bto`) not handled (inferred). |
| Interior lighting and fog | Partial | `apps/openmw/mwworld/cell.cpp:62` "TODO: use ESM4::Lighting fog parameters"; fog density is hard-coded to 1 | Fallout fog and light-template behaviour. |
| Weather, sky, climate | Missing | no `WTHR`/`CLMT` parser | The Capital Wasteland and Mojave skies. |
| Water | Partial | height taken from cell or worldspace (`cell.cpp:57-72`); no `WATR` parser | Water appearance and effects. |
| Interior/exterior navigation data | Parsed only | `NAVM`/`NAVI` not stored | OpenMW builds its own navmesh at runtime, so this may not be needed (inferred). |

### 4. Actors

| Area | Status | Evidence | Gap |
|---|---|---|---|
| NPC appearance | Partial | `apps/openmw/mwrender/esm4npcanimation.cpp` (189 lines) assembles race body parts, armor addons and head parts; line 44 notes "no easy way to distinguish TES5 and FO3" | No FaceGen morphs (the file never mentions them) and it never adds an animation source or `.kf` file. |
| NPC collision | Missing | `apps/openmw/mwclass/esm4npc.hpp:45-50`: `insertObjectPhysics` body is commented out | NPCs have no physics body. |
| Creatures | Partial | `ESM4Named<ESM4::Creature>` generic class (`classes.cpp:83`) | Same as NPCs. |
| Statics collision | Hack | `components/nifbullet/bulletnifloader.cpp:134` "FIXME: hack, using rendered geometry instead of Bethesda Havok data" | Havok shapes are never read for collision. |
| Animation | Missing | no ESM4 hits in `apps/openmw/mwmechanics` | Idle, locomotion, combat and VATS animation (`.kf`). |

### 5. Gameplay systems

A search for `ESM4` in `apps/openmw/mwmechanics`, `mwdialogue`, `mwscript`, `mwinput`, `mwstate` and `mwphysics` returns nothing. In `mwgui` it finds three files (`bookwindow.cpp`, `scrollwindow.cpp`, `console.cpp`).

| System | Status | Evidence | Gap |
|---|---|---|---|
| Script execution | Missing | script source kept, bytecode skipped; the existing script runner is the Morrowind one (`apps/openmw/mwscript`) | The whole FO3/FNV script language (ObScript) and its engine functions. |
| Quests and dialogue | Parsed only | `QUST`/`DIAL`/`INFO` parsed, not stored | Topics, conditions, result scripts, voice, UI. |
| Stats, perks, skills | Missing | no `AVIF`/`PERK`/`FACT` parsers | Actor values, leveling, S.P.E.C.I.A.L. |
| Combat, weapons, VATS | Missing | weapons are stored as items only | Everything. |
| AI packages | Parsed only | `PACK` parsed, not stored | Schedules, follow, sandbox, travel (companions depend on this). |
| Item use and activation | Partial | `esm4base.hpp:140` says activation "can be handled in Lua"; `ESM4Terminal` exposes text to Lua (`apps/openmw/mwlua/types/terminal.cpp`) | Lua is the only gameplay hook today. |
| Lua API coverage | Partial | 17 ESM4 object types in `files/lua_api/openmw/types.lua`; none for NPC, creature, container, furniture or tree | Needed before Lua can drive actors or containers. |
| Audio | Partial | `SOUN`/`SNDR` buffers (`apps/openmw/mwsound/soundbuffer.cpp:111-113,202-212`) | Music (`MUSC` parsed only), voice and lip files. |
| Save and load | Missing | `apps/openmw/mwworld/cellstore.cpp:151-156`: "TODO: Implement loading/saving of REFR4 and ACHR4"; ESM4 references are skipped on save | No save game can round-trip Fallout state. |
| New game | Unverified | `World::startNewGame` (`worldimp.cpp:266`) skips character generation when a start cell is given (`:297-310`), but the cell lookup (`findInteriorPosition`, `:2641`) searches for Morrowind marker names, and `ESMStore::checkPlayer` (`esmstore.cpp:801`, used when loading saves) expects a Morrowind player, race and class | Whether a Fallout-only content list starts at all was not tried. |

### 6. Tests and CI

- `apps/components_tests/esm4/includes.cpp` only checks that the headers compile. No parser has a behavioural test and no fixture data exists.
- BSA and NIF physics have unit tests (`apps/components_tests/bsa`, `.../nif`).
- `.github/workflows/push.yml` builds on Ubuntu and runs `components-tests`, `openfallout-tests` and `openfallout-cs-tests` (named `openmw-tests` and `openmw-cs-tests` before rename stage A). `.gitlab-ci.yml` and `CI/` are upstream leftovers.

## Recommended milestones

Ordered so each milestone unlocks the next. "Exit" is a check a person can run.

**M0: Feedback loop.** Get a clean build and test run in a known environment, because the rename (see "Rename to OpenFallout") cannot be checked without one. Add a plugin census tool that, on a user's own FO3/FNV/TTW files, reports per record type: count, parse failures and unknown subrecords. Add synthetic-record test helpers so parsers can have unit tests without game data. Then rename in the stages listed below.
*Exit:* CI green on this tree; census output committed for each game (counts only, no game content); the rename stages merged with the build green after each.
*Progress:* the build baseline and the census tool (`esmtool census`) are described in `build-baseline.md`.

**M1: Complete data.** Store the parsed-only records the engine needs, then add parsers for the missing record types in priority order: `GMST`/`GLOB`, `FACT`, `WTHR`/`CLMT`/`WATR`, `SPEL`/`ENCH`/`MGEF`/`PERK`/`AVIF`, then the rest. Stop skipping `SCDA` bytecode in `SCPT` and `INFO` records (decision 3).
*Exit:* census reports zero skipped records for all three games.

**M2: Walk the world.** Start in a chosen cell with a placeholder player, correct weather and sky, water, interior lighting and fog, collision for statics (real Havok data or an accepted substitute), NPC bodies with collision.
*Exit:* walk Megaton and the Goodsprings start without crashes or invisible walls.

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
| 3 | Script path | **Run the compiled bytecode (`SCDA`).** | The VM decodes `SCDA` directly, as the original engine does, so no compiler is needed. First step: stop skipping `SCDA` in `ESM4::Script` and `ESM4::DialogInfo` (`loadscpt.cpp:79-110`, `loadinfo.cpp:116-118`). `SCTX` source stays for tooling and debugging. Each engine function the scripts call needs its own implementation. |
| 4 | Test data | **Jacob can provide the files from their machine or upload them.** | See "Getting game data to the project". |
| 5 | Naming | **Everything becomes OpenFallout.** | See "Rename to OpenFallout". |

## Getting game data to the project

The census tool and every real-data check need the installed game folders, not the installers: `Fallout3.esm`, `FalloutNV.esm`, the Tale of Two Wastelands plugin, and all the `*.bsa` archives from each `Data` folder.

- **Preferred: a Remote Control session on Jacob's machine.** The project can already see Jacob's MacBook as a connected device. A session there runs the census and test tools against the installed files in place, and only counts and logs come back. Nothing needs uploading, which matters because the archives run to many gigabytes. Each folder is approved by Jacob before a session can use it.
- **Fallback: upload to the cloud.** Files attached to the project are copied under `/mnt/project-files/uploads/hearth/`. I have not checked size limits, and multi-gigabyte archives may not fit, so this suits a few small plugins at most.
- **If only installers exist**, they have to be unpacked first. Installer-only copies (for example offline GOG installers) can usually be unpacked with a tool such as `innoextract` on Jacob's machine.
- **Never commit game files.** They are copyrighted. Committed census output is counts only.

## Rename to OpenFallout

Sizing at commit `f90f239d`, excluding `extern/`:

- 1,206 tracked files mention "openmw" (5,720 lines). 1,010 paths contain "openmw", almost all under `apps/openmw/`.
- 10 executables are named `openmw*` (`openmw`, `openmw-cs`, `openmw-launcher`, `openmw-wizard`, `openmw-iniimporter`, `openmw-essimporter`, `openmw-navmeshtool`, `openmw-bulletobjecttool` and two test binaries), plus the `openmw-lib` and `openmw-cs-lib` libraries.
- Lua modules are named `openmw.*` (20 API files under `files/lua_api/openmw/`). `openmw.cfg` is mentioned 228 times. `OPENMW_*` guards and options are everywhere, and `OMWEngine` and `OMW*` symbols exist.
- 792 files use `MW*` namespaces (`MWGui`, `MWWorld`, `MWLua`, `MWMechanics` and others). `MW` stands for Morrowind.
- 206 files mention Morrowind outside `extern/` and the translations (launcher and wizard text, docs, data). Those belong with the new-game and launcher work in M2, not the rename.

The rename lands in stages, each one buildable and passing the tests before the next starts, so a break is easy to trace:

1. **Stage A, user-visible names.** Executable names, window titles, README and docs, config file and user-data directory names, packaging. Add an acknowledgement of OpenMW to the README.
2. **Stage B, interfaces.** Lua module ids (`openmw.*` to `openfallout.*`), `OPENMW_*` CMake options and header guards, `OMW*` symbols.
3. **Stage C, source layout.** `apps/openmw` to `apps/openfallout` using `git mv`, then `MW*` namespaces to `OF*` (for example `MWWorld` becomes `OFWorld`). `OF` is the default chosen here and is cheap to change before this stage starts.

Progress: stage A is done. The programs are `openfallout`, `openfallout-launcher`, `openfallout-wizard`, `openfallout-cs`, `openfallout-iniimporter`, `openfallout-essimporter`, `openfallout-navmeshtool`, `openfallout-bulletobjecttool`, `openfallout-tests`, `openfallout-cs-tests` and the three benchmarks. The CMake project, configuration and log files (`openfallout.cfg`, `openfallout.log`, `openfallout-cs.cfg`), settings directories (`~/.config/openfallout`), window titles, dialogs, translations, desktop and appdata entries, package and CI names, the README and `CONTRIBUTING.md` use the new name. Deliberately left for stage B or C: the `openmw-lib`, `openmw-cs-lib` and `openmw-navmeshtool-lib` libraries and the `openmw_add_executable` macro, Lua module ids and `OPENMW_*` names, `Role_OpenMW*` and `Version::getOpenmwVersionDescription` identifiers, the `OpenMW 0.48.0` and `OpenMW 0.52.0` names in the "save is too old" message (they name real OpenMW releases that can still read such a save), icon and logo artwork (`openmw.png`, `openmw.ico`, `openmw.icns`, `openmw_project_logo.webm` and the matching resource ids), `apps/openmw` and the `MW*` namespaces, and the OpenMW-owned documentation text under `docs/source`, which still describes the Morrowind engine.

Not renamed: `AUTHORS.md`, `LICENSE` and the copyright notices in source files. GPLv3 requires keeping them, and the OpenMW contributors wrote most of this code. This is a plain reading of the licence, not legal advice.

## Appendix: how the counts were produced

- **Stored vs parsed-only.** For each `components/esm4/load*.hpp` that declares `sRecordId`, find the enclosing top-level struct and check whether `Store<ESM4::Name>` appears in `apps/openmw/mwworld/esmstore.hpp`. Result: 77 headers, 39 stored, 38 parsed-only. `loadgrup.hpp` and `loadtes4.hpp` declare no record id. The tuple also holds `Store<ESM4::ActorCreature>`, which has no header of its own, so the count is per header, not per store.
- **Record types with no parser.** Absence of a matching `components/esm4/load*.cpp` file. The list of Fallout record types is from memory of public format references, so a name on it may be wrong.
- **Gameplay-side ESM4 usage.** `rg -il esm4 apps/openmw/mwmechanics apps/openmw/mwdialogue apps/openmw/mwscript apps/openmw/mwinput apps/openmw/mwstate apps/openmw/mwphysics` returns nothing.
