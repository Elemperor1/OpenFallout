# Measuring performance on a Mac

This page says how to measure frame time, cell-load time, memory and CPU time of OpenFallout on a Mac, on a fixed set of
scenes, so that two measurements taken a month apart (or before and after a change) can be compared. It promises no
numbers: it is the method, and the first baseline below is a record of what the method gave when the page was written.

Everything here runs the game in a window in the background: nothing goes full screen, nothing takes the keyboard focus
or the pointer, nothing presses keys, and each run has its own configuration and user-data folders, so the settings and
saves of whoever uses the Mac are never read or written.

## The fixed scenes

One interior, one exterior and one busy exterior (many characters near the start point) of each game, named by the
arguments that start them. Use these and nothing else when numbers are to be compared.

| scene | game master | start arguments |
|---|---|---|
| `nv-interior` | `FalloutNV.esm` | `--start GSDocMitchellHouse` |
| `nv-exterior` | `FalloutNV.esm` | `--start WastelandNV:6,15` |
| `nv-busy` | `FalloutNV.esm` | `--start WastelandNV:-18,0` |
| `fo3-interior` | `Fallout3.esm` | `--start Vault101d` |
| `fo3-exterior` | `Fallout3.esm` | `--start Wasteland:18,24` |
| `fo3-busy` | `Fallout3.esm` | `--start MegatonWorld:-1,-6` |

Every scene is started with `--skip-menu --no-grab --start <cell> --content <master>` and the frame-time probe
(`scripts/openfallout/perfprobe`, see its README) with a configuration of three phases: `settle` 20 s after the load (a
screenshot at the end, to check what was drawn), `stand` 30 s standing still, `turn` 40 s for one full turn on the spot
(a screenshot at each quarter). The probe quits the game after the turn. Only the game master is loaded (no add-ons), and
the settings are the engine's defaults with a 1280x720 window, `vsync mode = 0` and `framerate limit = 0`, so that the
frame time is the engine's and not the display's.

## One command: `mac_measure.py`

```
scripts/openfallout/mac_measure.py scenes
scripts/openfallout/mac_measure.py run all --engine <build>/OpenFallout.app/Contents/MacOS/openfallout --repeat 3 --out <results>
scripts/openfallout/mac_measure.py run nv-busy --engine <...> --capture time
scripts/openfallout/mac_measure.py compare <results>/<before> <results>/<after>
```

`run` starts the engine once per scene and repeat (one engine at a time, 10 s apart) with `SDL_MAC_BACKGROUND_APP=1`
(the app never becomes frontmost) and, for each run, a folder `<scene>-<n>/` with its own `config/` (the settings above
and an `openfallout.cfg` of one line, `replace=config`), `userdata/` and the probe's configuration. The game data folders
come from the round profile (`~/.config/openfallout-rounds.json`, see `scripts/openfallout/round/README.md`) or from
`--nv-data` / `--fo3-data`. While the engine runs, the WindowServer heartbeat of the round guards
(`scripts/openfallout/round/roundguard.py`) ends it if the window system stops answering. Each run writes `result.json`:

- `phases`: frame times (ms) of the probe's phases: `frames`, `mean`, `p50`, `p95`, `p99`, `max`, `over33`, `over100`,
  `fps`. The `stand` and `turn` phases are the frame-time numbers; `settle` still includes the streaming after the load.
- `cell_load_s`: seconds from the log line `Starting a new game` to the probe's first frame in the scene (the start cell
  and the cells around it loaded). `startup_s`: from the first line of the log to that frame.
- `memory`: `footprint_mib` (physical footprint at the end of `stand`) and `peak_mib` (the largest `phys_footprint_peak`
  seen), both from `footprint --noCategories -p <pid>`, the same number Xcode's memory gauge and Activity Monitor's
  "Memory" column show.
- `machine_before` / `machine_after`: chip, memory, macOS version and build, load averages and the number of compiler and
  linker processes running; `commit` (`--commit`, else the commit of the checkout the script is in), `version` (the
  engine's version line), `exit_status`, `stopped` (why the runner ended the engine, if it did).

`compare BEFORE AFTER` takes two result folders (all `result.json` under each) or files and prints a Markdown table of the
medians per scene and metric with the change in percent. A change smaller than the spread between repeats of one side
is noise: look at the repeats of each side before believing a few percent.

## Instruments captures with `xctrace`

`run ... --capture KIND` attaches `xcrun xctrace record` to the engine when the `stand` phase begins and records the 70 s
of `stand` and `turn` into `<run>/<KIND>.trace`:

| `--capture` | template | what it shows |
|---|---|---|
| `time` | Time Profiler | CPU time per thread and function |
| `alloc` | Allocations | heap allocations made while attached (not those of the load) |
| `game` | Game Performance | frame intervals, thread states, hitches |
| `metal` | Metal System Trace | the GPU work of the OpenGL-on-Metal layer, display intervals |

The command it runs, to use by hand on any running engine:

```
xcrun xctrace record --template 'Time Profiler' --attach <pid> --time-limit 70s --output time.trace --no-prompt
xcrun xctrace record --template 'Allocations'   --attach <pid> --time-limit 70s --output alloc.trace --no-prompt
xcrun xctrace record --template 'Game Performance' --attach <pid> --time-limit 70s --output game.trace --no-prompt
```

To record allocations from the very start (the load included), launch the engine under `xctrace` instead of attaching:
`xcrun xctrace record --template Allocations --output alloc.trace --env SDL_MAC_BACKGROUND_APP=1 --launch -- <engine>
<the scene's arguments>`. `mac_measure.py` does not do that, because a launched engine gets no terminal and shows a
dialog instead of a log line when it fails to start.

A trace adds its own cost: never compare the frame times of a run with a capture to those of a run without one.

### Comparing two traces

Open both in Instruments (`open time.trace`), select the same window of time (the whole `stand` phase), and compare the
call tree with "Invert Call Tree" and "Hide System Libraries" on, by the heaviest functions of the main thread. For a
textual comparison, export the tables and diff them:

```
xcrun xctrace export --input time.trace --toc
xcrun xctrace export --input time.trace --xpath '/trace-toc/run[@number="1"]/data/table[@schema="time-profile"]' > time.xml
```

## The performance HUD

The Metal performance HUD (frame rate, frame time graph, GPU time and memory) is switched on with the environment
variable `MTL_HUD_ENABLED=1`: `mac_measure.py run --hud` sets it for its runs, and the launch helper sets it with
`--metal-hud` when the game is started the way a player starts it:

```
scripts/openfallout/mac_launch.py --metal-hud -- --skip-menu --no-grab --start WastelandNV:-18,0 --content FalloutNV.esm
```

The HUD draws into the game's picture, so it is for looking at, not for numbers that are compared.

## Game Mode and the two ways of starting

`mac_measure.py` starts the engine binary directly, as the probe and autoplay tools do: that is the way that gives a pty, a
log and a pid at once, but macOS keeps Game Mode off for it (see [playing.md](playing.md), "On a Mac: start it through
`open`"). A measurement of what a player gets starts the packaged app through `mac_launch.py` (Game Mode can turn on, in
full screen only); attach the captures above by the pid of the running `openfallout` (`pgrep -n openfallout`). Say which
of the two ways a number was taken with.

## Before measuring

- Close what is heavy and wait until no build is running: numbers taken next to a compiler are noise (the result records
  the load and the number of compiler processes so that this can be checked afterwards).
- Use a Release build, the same build options both times, and the same game data.
- Run each scene three times (`--repeat 3`) and compare medians.
- Write down the commit, the macOS version and the chip with the numbers (`result.json` has them).

## Baseline

BASELINE-PLACEHOLDER
