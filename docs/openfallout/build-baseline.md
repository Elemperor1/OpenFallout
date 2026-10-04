# Build baseline and plugin census

Measured on 2026-10-04 on the cloud container this project works in: Ubuntu 24.04, 4 cores, 15 GB of memory, GCC with the `mold` linker, Release build.

## Building

`scripts/openfallout/setup_ubuntu_build.sh` installs the packages and builds the two libraries Ubuntu does not provide in a usable version, then prints the CMake command. The steps it takes:

1. Install the distribution packages (Boost, FFmpeg, SDL2, OpenAL, Bullet, LuaJIT, OpenSceneGraph 3.6.5, yaml-cpp, ICU, SQLite, GoogleTest and others).
2. Build MyGUI 3.4.3 and Recast 1.6.0 from the source packages in the Ubuntu archive. The build requires MyGUI 3.4.3 and a CMake config for Recast, and Ubuntu 24.04 ships MyGUI 3.4.2 and a Recast without one. CMake would download both from GitHub; this container's network policy blocks those downloads, and the archive is allowed. Both tarballs are checked against the SHA-256 sums in their Ubuntu source package.
3. Configure with the Qt tools off and tests on:

```
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="<prefix>/mygui;<prefix>/recast" \
    -DBUILD_LAUNCHER=OFF -DBUILD_WIZARD=OFF -DBUILD_OPENCS=OFF \
    -DBUILD_COMPONENTS_TESTS=ON -DBUILD_OPENMW_TESTS=ON \
    -DOPENMW_USE_SYSTEM_RECASTNAVIGATION=ON -DOPENMW_USE_SYSTEM_GOOGLETEST=ON
```

## Result on unmodified master (`f90f239d`)

| Item | Result |
|---|---|
| Build | 973 steps, no errors, about 20 minutes on a cold ccache, 0.5 GB of build output |
| `components-tests` | 1,595 tests, all pass |
| `openmw-tests` | 529 tests, all pass |
| Binaries produced | `openmw`, `esmtool`, `bsatool`, `niftest`, `openmw-essimporter`, `openmw-iniimporter`, `openmw-navmeshtool`, `openmw-bulletobjecttool` and the two test programs |

With the census change and reader fixes from this document applied, the whole tree still builds and `components-tests` runs 1,602 tests (the seven new ones included), all passing; `openmw-tests` is unchanged at 529. The setup script was run from scratch into an empty prefix and its printed CMake command configured successfully.

Not covered:

- The launcher, installation wizard and OpenMW-CS are not built, so `openmw-cs-tests` is not run either. They need Qt 6.
- The engine itself was not started. That needs game data and a display.
- Nothing was run against real Fallout data.

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
