# Test coverage targets

Where the Codecov numbers stand, what to aim for in each part of the code, and which gaps to test first. Measured on master at 665183a (2026-11-08) from Codecov's public API and from the gcovr traces in the CI artifact `ubuntu-coverage` of the same commit. How the reports are produced is in [CI reporting](ci-reporting.md).

## Short answer

Aim for 90% on the Fallout rules and 80% on the file loaders the games depend on, measured per component, and gate only new code under `components/`. One overall number cannot work: Codecov showed 13.72% for master, and it would still read about 24% if both tiers hit their targets, because 74% of the counted lines are engine, rendering, GUI and editor code that unit tests do not reach.

Codecov counts a line with an untaken branch as partial, which is not covered. gcovr reports the exception exit that the compiler adds to every call that may throw as such a branch, and almost no test takes it. `CI/test_reports.py` therefore passes `--exclude-throw-branches`, which drops these branches whether or not a test took them (the lines of a `catch` block still count) and should lift the total from 13.7% to about 19% and the rules from 63% to 87% with no new tests (calculated from the traces; the first master run after the change shows the real figures). gcovr applies the flag while it reads the gcov data, so it has to be on the per-suite run; adding it to the merge of saved traces changes nothing.

## Where coverage stood

Codecov for master at 665183a: 13.72%, with 27,561 lines hit, 14,811 partial and 158,400 missed of 200,772. By component the engine was at 1.77%, shared libraries 35.05%, the editor 0.61%, and the launcher, wizard and tools 0.00%. By flag, `components` 11.54%, `openfallout` 3.18% and `editor` 0.29%. The gcovr traces count 215,054 lines against Codecov's 200,772; the difference was not investigated.

Modules grouped into tiers by how they can be tested. "Codecov metric" counts a line as covered only when every branch on it was taken, which is how Codecov computes its percentage. These figures are from before the exception-branch flag.

| Tier | What it holds | Lines | Share | Line coverage | Codecov metric | Without exception branches |
| --- | --- | --- | --- | --- | --- | --- |
| A. Formats and loaders | `components/` esm, esm3, esm4, nif, bsa, vfs, files, detournavigator, lua, compiler and the rest of the pure data code | 42,095 | 19.6% | 60.5% | 41.2% | 55.2% |
| B. Fallout rules | `components/falloutscript`, `components/falloutmenu` | 14,059 | 6.5% | 94.4% | 63.2% | 87.1% |
| C. Engine | `apps/openfallout/mw*` | 98,219 | 45.7% | 2.8% | 1.8% | 2.6% |
| D. Rendering, GUI and audio helpers | sceneutil, nifosg, resource, shader, fx, terrain, widgets, sdlutil and similar | 22,701 | 10.6% | 15.3% | 9.5% | 12.7% |
| E. Editor, launcher, wizard, tools | `apps/opencs`, launcher, wizard, esmtool and the other tools | 37,980 | 17.7% | 0.8% | 0.5% | 0.7% |
| All | | 215,054 | 100% | 21.0% | 14.1% | 19.1% |

| Module | Lines | Line coverage | Codecov metric | Without exception branches |
| --- | --- | --- | --- | --- |
| `falloutscript` | 11,832 | 94.8% | 63.5% | 88.2% |
| `falloutmenu` | 2,227 | 92.4% | 61.4% | 81.5% |
| `esm4` | 11,797 | 66.3% | 47.1% | 62.0% |
| `esm3` | 7,258 | 64.2% | 45.2% | 57.9% |
| `esm` | 827 | 68.3% | 50.5% | 63.8% |
| `vfs` | 631 | 78.0% | 56.9% | 72.6% |
| `files` | 992 | 72.4% | 42.0% | 64.6% |
| `detournavigator` | 3,733 | 77.6% | 55.8% | 72.4% |
| `bsa` | 816 | 30.6% | 16.7% | 26.1% |
| `nif` | 4,523 | 22.0% | 13.1% | 18.3% |
| `nifosg` | 3,112 | 16.9% | 9.4% | 12.2% |
| `apps/openfallout/mwscript` | 13,688 | 1.2% | 0.3% | 1.1% |
| `apps/openfallout/mwmechanics` | 13,848 | 0.7% | 0.6% | 0.7% |
| `apps/openfallout/mwworld` | 11,046 | 9.8% | 6.9% | 9.0% |
| `apps/openfallout/mwrender` | 12,889 | 5.7% | 3.5% | 5.2% |
| `apps/openfallout/mwgui` | 20,251 | 0.1% | 0.1% | 0.1% |
| `apps/openfallout/mwlua` | 13,335 | 0.4% | 0.3% | 0.4% |
| `apps/opencs` | 29,819 | 1.0% | 0.6% | 0.9% |

## Why one overall number is the wrong target

The total is fixed by what is in the denominator. Tiers C, D and E hold 158,900 of the 215,054 lines and sit near 3%. If tier A reached 80% and tier B 90%, the total would be about 24%, or 29% with the editor, launcher, wizard and tools ignored. To reach 80% overall, tiers C to E would need about 79% themselves, which means testing the renderer, the GUI and the audio output.

Patch coverage on glue distorts the picture too. PR 143 showed 38.40% patch coverage with 85 lines missing. 65 of them were in `apps/openfallout` (`falloutface.cpp` 51, `esm4npcanimation.cpp` 11, `esm4npc.cpp` 3), which is rendering code that unit tests cannot run. The other 20 were in `components/esm4/facegen.cpp`: 2 missed lines and 18 partials, most of them exception branches. A patch gate across the whole diff would punish the first kind of change and add nothing for the second.

## Targets

Targets apply to tiers A and B only, in the Codecov metric after the exception-branch flag (the last column above). The rules are already close, so they get the highest bar. The loaders get a staged climb, and the engine, rendering and editor get no target, only a number on the page.

| Module | Without exception branches | Target | Gate |
| --- | --- | --- | --- |
| `falloutscript` (rules) | 88.2% | 90% | no drop over 1 point; new code 90% |
| `falloutmenu` (menu XML) | 81.5% | 90% | no drop over 1 point; new code 90% |
| `esm4` (FO3 and NV records) | 62.0% | 80% | no drop over 1 point; new code 80% |
| `esm` | 63.8% | 80% | same |
| `esm3` (includes save records) | 57.9% | 70%, save records 85% | same |
| `nif` | 18.3% | 60%, then 75% | same |
| `bsa` without the Fallout 4 readers | about 53% | 80% | same |
| `vfs`, `files` | 72.6%, 64.6% | 85%, 80% | same |
| `detournavigator` (NAVM and navmesh) | 72.4% | 80% | same |
| `compiler`, `interpreter`, `lua` | 64.4%, 50.4%, 62.2% | 70% | same |
| Engine, rendering helpers (tiers C and D) | 2.6%, 12.7% | none | informational only |
| Editor, launcher, wizard, tools (tier E) | 0.7% | none | informational only |

- **Ratchet, not wall.** A component's project check fails only when it drops more than 1 point, so a big PR in tier C never fails on a tier A number. Raise a target once the component has stayed 5 points above it for a month.
- **New logic goes where it can be tested.** Engine glue stays informational because it needs a running game. The way to raise tier C's value is to move rules into `components/falloutscript`, as the M12 menus did, and keep the glue thin.
- **Take out what the games cannot use.** `codecov.yml` ignores the Fallout 4 archive readers (`components/bsa/ba2*`, 412 lines at 0%) and the two Morrowind-only importers (`apps/essimporter`, `apps/mwiniimporter`, 1,590 lines at 0%). They could also be removed from the build.

`codecov.yml` implements this with five groups of components (`rules`, `loaders`, `render_helpers`, `engine`, plus the editor, launcher and tools) and two patch statuses, one for `components/` and one for `apps/`. Every status is informational for now: Codecov shows the numbers and cannot turn a PR red. The one patch status of `components/` holds a single 80% target, which the 5% threshold lets pass down to 75%; it is a stand-in, because the table gives the rules 90% and the rendering helpers no target. To make a gate, first split that status by tier with patch statuses on the components (`rules` 90%, `loaders` 80%, nothing for `render_helpers`) and set the threshold to 0, so the target is the floor. For a component project check, leave `informational` in `component_management.default_rules`, because removing it there turns every component into a gate. Give the component its own `statuses` entry with its number from the table instead, and no `informational` line.

## Gaps to test first

The loaders are the weak spot, not the rules. SPECIAL, derived stats, repair and lockpick each had 12 or fewer missed lines, so the rules read from the executables are already well pinned. Missed-line counts come from the merged CI traces for 665183a, and the lists are the worst files per module, not every file.

| # | Gap | Worst files (missed lines) | How to test |
| --- | --- | --- | --- |
| 1 | ESM4 reader and the records behind visible bugs, about 1,500 lines | `reader.cpp` 237 (55% covered), `loadrace.cpp` 295 (8%), `loadnavi.cpp` 128 (0%), `loadland.cpp` 103 (0%), `loadammo.cpp` 102 (0%), `loadnpc.cpp` 98, `loadmset.cpp` 96 (0%), `loadrefr.cpp` 95, `loadarmo.cpp` 92, `loadcell.cpp` 78, `loadarma.cpp` 71 (0%), `loadcrea.cpp` 71, `loadbook.cpp` 69 (0%) | One record per type from the synthetic plugin builder (`apps/components_tests/esm4/syntheticplugin.hpp`), read back, every field asserted. Race records feed the face and skin work, LAND is terrain. |
| 2 | NIF records, about 3,000 lines in `nif/` plus the OSG loader | `controller.cpp` 608 (0%), `physics.cpp` 582, `particle.cpp` 472 (0%), `node.cpp` 432, `data.cpp` 410, `property.cpp` 397 (0%), `extra.cpp` 125 (0%); `nifosg/nifloader.cpp` 1,486 of 1,910 | Table-driven: one tiny synthetic NIF per record type, as `testphysics.cpp` and `testanimsurvey.cpp` already build them in memory. |
| 3 | Save format | `esm3/savedgame.cpp` 52 (0%), `activespells.cpp` 119, `aipackage.cpp` 54, `inventorystate.cpp` 61, `aisequence.cpp` 143, `objectstate.cpp` 58; `falloutscript/savecharacters.cpp` 42; engine side `mwscript/falloutsave.cpp` 824 (0%) | Write-then-read round trips for each record the Fallout save uses, before the format ships, so later changes break a test instead of old saves. |
| 4 | FO3 and NV archives | `bsa/compressedbsafile.cpp` 130 of 209, `vfs/manager.cpp` 41 of 74, `vfs/bsaarchive.hpp` 30 of 59 | Small archive built in memory, as `testbsafile.cpp` does for the uncompressed case. |
| 5 | Rules still open | `caravan.cpp` 70, `equipment.cpp` 51 (armor), `itemuse.cpp` 29, `interaction.cpp` 23 (80%), `perks.cpp` 21, `quests.cpp` 19, `pipboy.cpp` 18 (82%), plus about 1,000 lines with a condition never taken both ways | After the flag change, open each file in Codecov's line view and add the missing side of each condition. |
| 6 | Menu XML | `menu.cpp` 59, `openmenu.cpp` 40, `richtext.cpp` 19, `menufile.cpp` 17, `listbox.cpp` 10, `xml.cpp` 10 | Small. Add with the next menu change. |
| 7 | Fallout glue with logic, about 9,200 lines | `mwscript/falloutscripts.cpp` 3,865 (3.6%), `mwmechanics/falloutactors.cpp` 1,083, `falloutinteraction.cpp` 936, `falloutsave.cpp` 824, `falloutpipboy.cpp` 498, `falloutlook.cpp` 370, `falloutcasino.cpp` 354, `falloutcaravan.cpp` 320 | Not with unit tests. Move logic into `components/falloutscript` and cover the thin remainder with a headless smoke run. |
| 8 | Lua and Python | 45 `fallout*.lua` scripts, 19,455 lines, are not measured by Codecov. The Python tests under `scripts/openfallout/` did not run in CI before the Changes job started running them | They run in CI now. Measure Lua line coverage later through lupa, if the Lua keeps growing rules. |

## Rollout

1. **Plumbing, in one PR:** the exception-branch flag, the Python tests in CI, the new `codecov.yml` with every check informational, and this note.
2. **Tests for the top three gaps** (ESM4 records, NIF records, save round trips), added with whatever feature work touches them rather than as a separate push. Re-read the per-module numbers after each batch.
3. **Tighten.** After a couple of weeks of informational patch checks that look sane, split the patch status of `components/` by tier, as described above, and drop `informational` from it. Then give each component that is over its target its own `statuses` entry with its number, leaving the default informational.
4. **Optional: a headless smoke run under coverage** as a fourth flag, so the engine glue gets a number. The repo already has a synthetic Fallout plugin and startup smoke scripts under `scripts/openfallout/`. How much glue a smoke run would reach, and what it would cost in CI minutes, has not been measured.
