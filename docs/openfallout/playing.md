# Running OpenFallout on your own copy of the games

OpenFallout plays Fallout 3, Fallout: New Vegas and Tale of Two Wastelands from the files of a copy of the games that you own. It includes none of their content. This page says how to point it at your files on macOS, Windows and Linux, how to start a new game, and which settings matter for the frame rate and the picture. What works and what does not yet is in the [roadmap](gap-audit-and-roadmap.md).

## What it needs

- The `Data` folder of the game: the plugins (`Fallout3.esm`, `FalloutNV.esm`, the DLC `.esm` files) and the archives (`Fallout - Meshes.bsa` and the others). A copy from Steam, from GOG (an installed copy, or the files unpacked from the offline installer), from the discs, or a copy of the folder from another computer all work. Nothing needs to be installed through Wine: the files are read as they are.
- For Tale of Two Wastelands, the output folder of its installer (the merged `FalloutNV.esm` is in it) and the `Data` folder of New Vegas. See [mods and load order](mods-and-load-order.md).

## Pointing it at the game

With nothing set, OpenFallout looks for the games where they are usually installed, and says so in its log (`Found Fallout: New Vegas in ...`):

- Steam on every system: the library folders that Steam lists, and the folders of `steamapps/common` with "Fallout" in their name. On Windows, Steam's folder comes from the registry; on Linux and macOS, from the usual places (`~/.steam/steam`, `~/.local/share/Steam`, the Flatpak and Snap ones, `~/Library/Application Support/Steam`).
- GOG, Bethesda's installer, Epic and hand copies: the folders with "Fallout" in their name under `GOG Games`, `Program Files (x86)`, `Program Files`, `Games` and a few others on each hard drive on Windows; under `~/Games` (also `~/Games/Heroic`) and `~/Applications` on macOS and Linux; and inside the Windows drives of Wine, CrossOver, Bottles, Lutris and Heroic (the `drive_c` folders of their prefixes).
- If both games are found, New Vegas is used. The search only runs when you have named no plugin and no data directory of yours has `Fallout3.esm` or `FalloutNV.esm`.

To use a folder of your own, or when the search finds nothing (the log then says so), name it. Either on the command line:

    openfallout --data "/path/to/Fallout New Vegas/Data"

or once and for all in `openfallout.cfg` in your configuration folder, one line for each data directory (later ones win over earlier ones):

    data="/path/to/Fallout New Vegas/Data"

The configuration folder is `~/.config/openfallout` on Linux (`$XDG_CONFIG_HOME/openfallout` if you have set that variable), `~/Library/Preferences/openfallout` on macOS and `Documents\My Games\OpenFallout` on Windows. The settings file `settings.cfg` and the log `openfallout.log` are in the same folder. Saves and screenshots go to the data folder instead: `~/.local/share/openfallout` on Linux (`$XDG_DATA_HOME/openfallout`), `~/Library/Application Support/openfallout` on macOS, the same folder as the configuration on Windows.

Which plugins load: if you name none, the ones the games make active (the base master named in `Fallout_default.ini`, the plugins with a `.NAM` file next to them, and the ones in a `Plugins.txt` given with `--plugins-file`), with their masters. Name plugins with `--content` (or `content=` in `openfallout.cfg`) to choose them yourself. Tale of Two Wastelands: `data="<Data folder of New Vegas>"`, then `data="<output folder of the installer>"`. The rules, one by one, are in [mods and load order](mods-and-load-order.md).

The launcher (`openfallout-launcher`) has a Data Files page that writes these lines for you (add the Data folder, then pick `Fallout3.esm` or `FalloutNV.esm` as the game file; adding the folder alone leaves no game file chosen). It comes from OpenMW and was made for Morrowind; the project has not tried it on the Fallout games, so the command line and `openfallout.cfg` are the way that is known to work.

## Starting a game

    openfallout --skip-menu --new-game

starts a new game of the game you pointed it at: its opening scene, the making of the character and the first quest. How far the engine plays from there is in the [roadmap](gap-audit-and-roadmap.md); it is not a finished game yet. Without these options the main menu opens, and *New Game* does the same. `--start <cell>` (for example `--start WastelandNV:-18,0`, a worldspace and the cell's grid position, or the name of an interior cell) opens a cell directly, which is what the project's own test runs do. `--load-savegame <file>` loads a save.

## Game controller

A controller that the operating system shows as a game controller (Xbox, PlayStation and Switch Pro pads, by cable or Bluetooth) is read through SDL, as in OpenMW, and `[Input] enable controller` is on by default. **None of this has been tried with a real pad yet**: the buttons below are what the code does and what the smoke test sends to the screens as events; a round with the pad in hand is still to be done. Each press that a Fallout screen acts on is logged as `Pad: screen=... button=... key=...` (the first 200 of a run), which is how to see what a pad did.

In the world (the bindings OpenMW gives a pad): left stick walks, right stick looks, right trigger attacks, left trigger jumps, A activates, X draws or puts away the weapon, a click of the left stick sneaks, a click of the right stick changes the view, B opens the Pip-Boy, Start opens the menu, RB waits, the Guide button saves quickly, and (new) LB opens V.A.T.S. Y does nothing in Fallout. These are the defaults: the bindings file that an earlier run saved in the configuration folder (`input_v3.xml`) keeps the buttons it has.

On a screen (the Pip-Boy, a conversation, V.A.T.S., a container, a bench, a lock, waiting): the left stick moves the pointer, A clicks what it is over and B leaves the screen (all three are the engine's). The right stick does not scroll a list; the D-pad, the bumpers, Y and X do what keys do:

| Screen | D-pad | LB / RB | Y | X |
|---|---|---|---|---|
| Pip-Boy | up and down move in the list, left and right change the page (STAT, INV, DATA) | a page up or down | use the thing picked (equip, eat, read) | drop it |
| Conversation | up and down pick a topic | a page of topics | say the topic picked; carry on while a character speaks | |
| V.A.T.S. | left and right pick the character, up and down the part | the character before or after | queue an attack on the part picked (the first press shows which part is picked) | attack: carry out the queue |
| Container | up and down pick a thing | a page | take it (the first press shows which is picked) | take everything |
| Bench | up and down pick a recipe, left and right a kind | a page | make it | |
| Lock | | | put in a bobby pin | |
| Waiting | left and right change the hours | | begin | |

The terminal (menus and the hacking game), the making of the character (the name still needs a keyboard) and the screen of a level up are used with the pointer and A only. The quests are on the DATA page of the Pip-Boy: the journal of Morrowind (the J key, and LB by the old default) opens nothing in Fallout now.

## Frame rate and picture

The settings are in `settings.cfg` in the configuration folder. The ones that matter most:

| Setting | What it does |
|---|---|
| `[Camera] viewing distance` | How far the world is drawn, in game units (one cell is 4096). The default is 7168, so distant hills are lost in fog. In a worldspace that has distant land files (next row) it is raised to `game lod distance`. |
| `[Terrain] game lod = true` | Draws the land and the buildings beyond the loaded cells from the level of detail files that Fallout 3 and New Vegas ship (`meshes/landscape/lod`). `game lod distance` (65536) says how far, `game lod objects = false` leaves out the buildings. Worldspaces without those files, and `game lod = false`, use `distant terrain`. |
| `[Terrain] distant terrain = true` | Draws the land beyond the loaded cells. Off by default, because it costs a lot of frame rate and memory. |
| `[Video] force vertex buffer objects` | On by default: all meshes are drawn from vertex buffers instead of OpenGL display lists, which are slow to play back on some drivers (on macOS the frame rate was 1.3 to 2.7 times higher). |
| `[Video] vsync mode`, `framerate limit` | As in any game. |
| `[Video] window mode`, `resolution x`, `resolution y` | The window. |

Trees are drawn as crossed cards with the leaf image of their record, because the games' SpeedTree files are not read.

## When something goes wrong

Look at `openfallout.log` in the configuration folder first: it lists the data directories, the plugins and the archives it found, in the order they were used (`Archive order`, `Load order`), and every file it could not read. A bug report with that file attached is the most useful one: <https://github.com/Elemperor1/OpenFallout/issues>. Do not attach game files.
