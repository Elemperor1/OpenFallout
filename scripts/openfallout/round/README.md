# Rounds

A round is a scripted check of a build on the real game that does not need anybody's computer: the engine runs with
`--headless` (its window is created hidden and never shown, it takes no keyboard focus, grabs no pointer, makes no sound), a
scenario file drives the game from inside (teleports, camera, waits, screenshots, probes) and `round.py` writes a report.
Nothing presses a key or moves the pointer, so the person at the computer keeps working. A round that took an hour of
hand-driven game starts (a build, then one start for each question) is one command of a few minutes: every question of the
round is one step of the scenario, the questions that need different settings are `variants` that start side by side.

```
scripts/openfallout/round/round.py profile --engine <build>/openfallout    once per computer: writes ~/.config/openfallout-rounds.json
scripts/openfallout/round/round.py build --ref origin/<branch>              incremental build in one persistent folder
scripts/openfallout/round/round.py run rounds/m13-mountains.json --parallel 3
```

## What a run does

1. Makes a folder `<out>/<time>-<name>/<variant>/` with its own `config/` (a `settings.cfg` made of the defaults of
   `round.py`, the round file's `settings` and the variant's, and an empty `openfallout.cfg`), its own `userdata/` (a **copy** of
   the save) and a `data/` folder that holds the scenario. The player's own saves, settings and log are never written: the
   engine is started with `--config` and `--user-data` pointing there, and reads the player's own `settings.cfg` only as the
   defaults under the run's.
2. Starts the engine at low priority (`taskpolicy -b` on macOS, `nice -n 10` elsewhere) with `OPENFALLOUT_HEADLESS=1`, the
   game's data folders and plugins of the profile, `--skip-menu --headless --no-grab`, `--load-savegame <the copy>` (or
   `--start <cell>`), and the content file `round.omwscripts` (the scenario player).
3. On macOS it asks `lsappinfo` once a second which application is frontmost: a run in which the engine ever was fails, because
   that is what takes the keyboard of the person at the computer.
4. Stops the engine when `limit` seconds are over, then writes `report.md` and `report.json` (the verdict, exit status, the `ROUND`
   lines of the log, the first ten distinct warnings and errors with their counts, the focus count) and copies the screenshots
   to `shots/<n>-<name>.jpg` (JPEG, quality 85; the engine numbers its files in the order they were taken and the scenario says
   which is which). A `summary.md` in the folder of the round has one row for each variant.

The report holds numbers, record ids and the names the scenario gave, never a text of the game.

## The profile

`round.py profile --engine <path of the openfallout program>` looks for the games under `~/Games` (`Fallout New Vegas`,
`Fallout 3`, with or without `/Data`), and writes the machine's profile: `engine`, `resources` (for a macOS bundle:
`Contents/Resources/resources`), `games` (`data` folders and `content` plugins of each of `nv`, `fo3`, `ttw`; `content` is empty by default, which loads the plugins the selected game makes active, the same as the game does, so a save finds the plugins it was made with; the `content=` lines of the player's own configuration are never used, they may name the plugins of the other game), `saves` (the
folder that `"save": "latest"` takes the newest file from: the engine's own user-data folder unless `profile --saves <folder>` says
where the game is played, and `profile` warns when the folder holds no save), `out` and, if wanted, `settings` (a settings file laid under the
round's). Edit it by hand for Tale of Two Wastelands (`data` = the New Vegas folder, then the output of its installer).
`round.py check <round file>` prints what a run would use without starting anything.

## The round file (JSON)

| key | meaning |
|---|---|
| `name` | name of the round (the folder of the results) |
| `game` | `nv`, `fo3` or `ttw`: a key of the profile's `games` |
| `scenario` | the Lua file of the scenario, relative to the round file or to this folder |
| `save` | a file, or `"latest"`; without it `start` (a cell for `--start`, with `new_game` for `--new-game`) |
| `settings` | `{"Section": {"key": "value"}}` for every variant, for example `{"Terrain": {"game lod": "true"}}` |
| `resolution`, `framerate` | the size of the window (so of the screenshot) and the frame rate limit (defaults 1280x720 and 30) |
| `limit` | seconds after which the engine is stopped (300) |
| `variants` | a list of `{"name", "settings"}`: one engine start each; `--parallel N` runs N at a time, `--variant name` only that one |
| `extra_args` | more arguments for the engine |
| `allow_missing_content` | accept a save that wants a game plugin (`.esm`, `.esp`) the run does not load; without it such a round fails (a missing `.omwscripts`, such as the plugin of a play session, is a note) |

## The scenario (Lua)

`scripts/roundscenario.lua` returns a function that is given `r` and runs as a coroutine, or a table `{ run = function(r) ...
end, limit = <seconds>, delay = <seconds before the first step> }`. Steps that wait (`wait`, `waitFor`, `shot` and all that ask
the player script) must be called from the function itself, not from inside a `pcall`. Every step logs a line that starts with
`ROUND`; a step that fails (a teleport to a place that is not there, a quest that does not exist) logs a `ROUND error ...` line and the
scenario goes on, so that the report shows everything that went wrong, but the round fails; an error in the scenario's own code
ends it (`ROUND error scenario`) and makes the round fail too. A screenshot that was asked for and never written fails the round
as well.

| step | does |
|---|---|
| `r.wait(seconds)`, `r.waitFor(fn, timeout, label)` | real seconds go by, or until `fn()` is true |
| `r.teleport{ pos = {x, y, z}, yaw, onGround, cell, cellId, worldspace, label }` | takes the player to a place (the cell of the position in the worldspace of the cell the player is in, or `cell` by name, `cellId`) |
| `r.look(yaw, pitch)`, `r.camera('FirstPerson')` | the camera (radians; a positive pitch looks down, as in the rest of the Lua API) |
| `r.shot(name[, false])` | an engine screenshot (what the game draws, the same as the screenshot key); it first waits (`r.settle`) until no message box has been open for 6 s, because the add-ons of a save open their notices one after the other when a place has loaded; `false` takes the picture at once |
| `r.settle([quiet[, timeout]])` | waits until no message box has been open for `quiet` seconds (6), at most `timeout` seconds (40); a timeout is a note (`ROUND settle timeout`), and the `shot` line then says `box=<number>` if one is open |
| `r.eye(label)`, `r.ground(label, {{dx, dy}...})`, `r.actors(label, radius, count)`, `r.fps(label, seconds)` | log the player's place and camera, the height of the ground under the eye and offsets, the characters near the player (record id, distance, place), the frame times |
| `r.hour(h)`, `r.global(name[, value])`, `r.quest(name[, stage])`, `r.speed(scale)` | the game's clock, a global, a quest, the speed of the world |
| `r.object(form[, label])`, `r.activate(form)` | what the world says about an object (`"FormId:0x..."` or `{file, id}`), activate it |
| `r.boxes(false)` | a message box that the game opens covers the picture and nobody clicks it in a round, so it is answered with its first button after a second (`ROUND box` lines name its number, form and buttons, never its text; a `shot` line says `box=<number>` when one is open as the picture is taken); `r.boxes(false)` leaves the boxes open, `r.boxes(true)` answers again |
| `r.log(...)` | a note |
| `r.world`, `r.core`, `r.util`, `r.player()`, `r.lookup(form)` | the Lua modules and helpers, for steps of your own |

`scenarios/smoke.lua` is the smallest scenario; `scenarios/m13_mountains.lua` with `rounds/m13-mountains.json` is the check of the
distant land of New Vegas in seven variants of `game lod debug`. Its places are in the Mojave and the save can be anywhere:
the scenario takes the worldspace from the cell of a reference that lives there (`anchor` lines say which one was found and which
worldspace it names; `eye` lines name the worldspace the pictures were taken in). It stops with a `ROUND error` when no such
reference is found and the save is in an interior.

## What it cannot do

- It sees what the engine draws, not what the player's eye would: for a look at a screen that a pointer or a key opens (the
  Pip-Boy, a conversation) the scenario must open it through the game's own scripts or events, the way `autoplay` does.
- Sound is off unless `OPENFALLOUT_HEADLESS_SOUND=1`; nobody can hear a round, so sound is checked by its log lines.
- Software and hardware drawing differ: on a computer without a GPU (a Linux box under Xvfb and Mesa) the frame times mean
  nothing, and a driver-specific fault (a texture format) may not show.
- The first time on a Mac, whether a hidden window really stays off the desktop and never takes the focus is what the focus
  count of the report says; a report with `frontmost in 0 of N samples` is the proof, anything else is a failed round.

## Building

`round.py build --ref origin/<branch>` keeps one worktree and one build folder (next to the repository: `round-worktree`,
`round-build`), checks the ref out in it and builds only the `openfallout` target at low priority with ccache, so a change of a few
files rebuilds those files and not the whole engine. The first run configures with `-DCMAKE_BUILD_TYPE=Release` (more
arguments of the configuration with `--cmake-arg`); later runs reuse the folder.

## On a Mac

```
cd <the repository>
python3 scripts/openfallout/round/round.py build --repo . --ref origin/<branch> \
    --worktree ../round-worktree --build-dir ../round-build --cmake-arg ...    # the arguments of your last configuration
python3 scripts/openfallout/round/round.py profile --engine ../round-build/OpenFallout.app/Contents/MacOS/openfallout
python3 scripts/openfallout/round/round.py run scripts/openfallout/round/rounds/mac-focus-proof.json --parallel 1
```

An existing worktree and build folder can be given to `build` (`--worktree`, `--build-dir`): it checks the ref out in the worktree and
builds what changed. `rounds/mac-focus-proof.json` starts the smoke scenario three times, each with another way of keeping the
application off the desktop (`hidden`: only the hidden window, `accessory` and `prohibited`: also the activation policy that
`OPENFALLOUT_HEADLESS_POLICY` sets, which makes the application one that cannot be made the frontmost). Its summary table has the
focus column: `0/N` is what a headless run must show. While it runs, use the computer as usual; if a window appears, a Dock icon
bounces or a key goes missing, say which variant it was (they run one after the other, about a minute each).
