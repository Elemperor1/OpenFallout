# Build baseline and plugin census

Measured on 2026-10-04 on the cloud container this project works in: Ubuntu 24.04, 4 cores, 15 GB of memory, GCC with the `mold` linker, Release build.

## Building

`scripts/openfallout/setup_ubuntu_build.sh` installs the packages (Qt 6 and libunshield included) and builds the two libraries Ubuntu does not provide in a usable version, then prints the CMake command. The steps it takes:

1. Install the distribution packages (Boost, FFmpeg, SDL2, OpenAL, Bullet, LuaJIT, OpenSceneGraph 3.6.5, yaml-cpp, ICU, SQLite, GoogleTest, Qt 6, libunshield and others).
2. Build MyGUI 3.4.3 and Recast 1.6.0 from the source packages in the Ubuntu archive. The build requires MyGUI 3.4.3 and a CMake config for Recast, and Ubuntu 24.04 ships MyGUI 3.4.2 and a Recast without one. CMake would download both from GitHub; this container's network policy blocks those downloads, and the archive is allowed. Both tarballs are checked against the SHA-256 sums in their Ubuntu source package.
3. Configure with the Qt tools and all tests on:

```
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="<prefix>/mygui;<prefix>/recast" \
    -DBUILD_LAUNCHER=ON -DBUILD_WIZARD=ON -DBUILD_OPENCS=ON -DBUILD_OPENCS_TESTS=ON \
    -DBUILD_COMPONENTS_TESTS=ON -DBUILD_OPENFALLOUT_TESTS=ON \
    -DOPENFALLOUT_USE_SYSTEM_RECASTNAVIGATION=ON -DOPENFALLOUT_USE_SYSTEM_GOOGLETEST=ON
```

## Result on unmodified master (`f90f239d`)

Program and test names in this section are the ones at that commit, before rename stage A (`openmw-tests` is now `openfallout-tests`, and so on).

| Item | Result |
|---|---|
| Build | 973 steps, no errors, about 20 minutes on a cold ccache, 0.5 GB of build output |
| `components-tests` | 1,595 tests, all pass |
| `openmw-tests` | 529 tests, all pass |
| Binaries produced | `openmw`, `esmtool`, `bsatool`, `niftest`, `openmw-essimporter`, `openmw-iniimporter`, `openmw-navmeshtool`, `openmw-bulletobjecttool` and the two test programs |

With the census change and reader fixes from this document applied, the whole tree still builds and `components-tests` runs 1,602 tests (the seven new ones included), all passing; `openmw-tests` is unchanged at 529. The setup script was run from scratch into an empty prefix and its printed CMake command configured successfully.

## Result after rename stage A

Measured on 2026-10-04 on the same container, from an empty build directory, with the Qt 6.4.2 tools, the benchmarks and the construction set tests switched on as well.

| Item | Result |
|---|---|
| Build | 1,300 steps, no errors |
| `components-tests` | 1,612 tests, all pass (10 more than above, the launcher settings tests that only build with Qt) |
| `openfallout-tests` | 529 tests, all pass |
| `openfallout-cs-tests` | 154 tests, all pass (run with `QT_QPA_PLATFORM=offscreen`) |
| Benchmarks | the three `openfallout_*_benchmark` programs ran to completion |
| Programs started | `openfallout --version` prints `OpenFallout version 0.52.0`, `openfallout`, `openfallout-launcher`, `openfallout-wizard` and `openfallout-cs` start without game data and write `openfallout.log`, `launcher.log`, `wizard.log` and `openfallout-cs.log` under `~/.config/openfallout`, the other tools print their `--help` with the new name |

Not covered:

- The engine was not run with game data or a display. That needs game files, and nothing was run against real Fallout data.
- The macOS and Windows packaging paths (bundle names, NSIS, the `.rc` and manifest files) were edited but not built here.
- GitHub Actions has never run in this repository, so no CI result exists for any commit.

## Result after rename stage B

Measured on 2026-10-04 on the same container, from an empty build directory, with the same options as stage A (the `OPENMW_*` options are now `OPENFALLOUT_*`).

| Item | Result |
|---|---|
| Build | 1,300 steps, no errors |
| `components-tests` | 1,612 tests, all pass |
| `openfallout-tests` | 529 tests, all pass |
| `openfallout-cs-tests` | 154 tests, all pass |
| Benchmarks and tools | the three benchmarks ran to completion, every program starts as at stage A |
| Startup smoke test | `scripts/openfallout/startup_smoke_test.py --build <build dir>` passes: 43 Lua scripts configured, 12 l10n files enabled, no Lua error |

The first build of stage B failed. The rename script skipped `extern/`, so the macros `OPENMW_FFMPEG_5_OR_GREATER` and `OPENMW_FFMPEG_CONST_WRITEPACKET` in `extern/osg-ffmpeg-videoplayer` kept their name while the code in `apps/` that uses them was renamed. An undefined macro counts as 0 in `#if`, so on a system with older FFmpeg the code would have compiled the old way without a word; on Ubuntu 24.04, which ships FFmpeg 6, it stopped the build. Every `OPENFALLOUT_*` macro used in an `#if` is now defined somewhere, which a script over the whole tree confirms, and `extern/oics` keeps its upstream `OPENMW CODE STARTS HERE` comments.

### The startup smoke test

The unit tests never load the Lua scripts and l10n files under `files/data`, so a module id or l10n context renamed on one side only would pass them and fail when the engine starts. `startup_smoke_test.py` closes that gap. It writes a small synthetic game file (skills, the six date globals, the game settings the engine reads, one race, one class and the player), starts `openfallout` on it under a virtual display with Mesa software rendering, lets the main menu run for 15 seconds, stops it with SIGTERM and reads the log. It fails on a `Can't start`, `Lua error` or `Fatal error` line, and when no scripts or l10n files were loaded. The game file holds no Bethesda data.

- The log of the stage B binary matches the stage A binary line for line (297 lines, after removing timestamps, paths and the revision; the stage A log was read with the `OMW*` l10n context names mapped to `OF*`). Only the order of two lines written by different threads differs.
- As a control, one `require('openfallout.ui')` in a copy of the resources was changed back to `openmw.ui`. The script fails and prints `Can't start Menu[scripts/omw/settings/menu.lua]; Lua error: module not found: openmw.ui` and the three scripts that depend on it.
- Limit: only scripts for the `MENU` context start, because the synthetic file has no cell to start a new game in. The `GLOBAL`, `PLAYER` and `NPC` scripts are listed in the log but not started. For them stage B relies on a static check that every `require` id in `files/data` and `files/data-mw` is either registered in C++ or a file in the virtual file system, and that every `l10n(...)` context and `#{Context:Key}` reference has a matching directory.

## Plugin census

`esmtool census <plugin>` reads a TES4-format plugin (the format Oblivion, Fallout 3, New Vegas and Skyrim use) and prints how many records of each type it holds, how many were read by a loader, how many have no loader and how many a loader rejected, with the failure of each rejected one. The table holds record types, counts and failure messages only, never record contents, so it can be shared without sharing game data. The lines above the table also show the file name as given on the command line, the format version and the master plugin names, so check those before sharing the output. Some loaders put record contents in their error messages (the LVLI, LVLC and LVLN loaders include the editor ID), so only the `Unknown subrecord` message, which names a loader and a four character subrecord code, is printed as it is. Any other loader failure is counted and printed as `loader error (message withheld, it may contain record contents)`; `esmtool dump` shows the full message.

Example, on a small synthetic plugin built for this check:

```
Record        Total     Parsed  No parser     Failed
ACTI              1          1          0          0
SPEL              1          0          1          0
STAT              5          3          0          2
WTHR              2          0          2          0
All               9          4          3          2

Failures in STAT records:
  1 x ESM4::STAT::load - Unknown subrecord QQQQ
  1 x ESM4::STAT::load - Unknown subrecord ZZZZ
```

What the columns mean:

- **Parsed** means the `components/esm4` loader accepted the record. It does not mean the engine keeps the data: many parsed records are discarded afterwards (see section 2 of the roadmap).
- **No parser** means no loader exists for the record type. That is the list of record types still to be written.
- **Failed** means a loader exists but threw, usually on a subrecord it does not know.

The exit status is 0 when the whole file was read and non-zero when reading stopped early or the file could not be opened. Records that fail do not stop the census.

## Reader fixes made while building the census

- **The last record of every plugin was never read.** `ReaderUtils::readItem` stopped when `hasMoreRecs()` was false right after reading a record header, but a record's data counts as read as soon as its header is, so that is always true for the final record. The engine's own plugin loading uses the same code, so it skipped the last record of every ESM4 plugin too. Found because the census test file ended on a record that was never counted. Now only a group header with nothing after it ends the read early.
- **A loader that threw part-way through a record left the reader inside it.** `Reader::skipRecordData` assumes every subrecord seen so far was read in full, because sizes are counted when a subrecord header is read. After an exception the next header was taken from the middle of the record. `Reader::skipFailedRecord` moves to the end of the record using the position saved when its header was read, and it also handles compressed records.

Both are covered by tests in `apps/components_tests/esm4/testcensus.cpp`. With the first fix removed, tests in that file fail because the last record of their synthetic plugins is never counted.
