# Running OpenFallout on your own copy of the games

OpenFallout plays Fallout 3, Fallout: New Vegas and Tale of Two Wastelands from the files of a copy of the games that you own. It includes none of their content. This page says how to point it at your files on macOS, Windows and Linux, how to start a new game, and which settings matter for the frame rate and the picture. What works and what does not yet is in the [roadmap](gap-audit-and-roadmap.md).

## What it needs

- The `Data` folder of the game: the plugins (`Fallout3.esm`, `FalloutNV.esm`, the DLC `.esm` files) and the archives (`Fallout - Meshes.bsa` and the others). A copy from Steam, from GOG (an installed copy, or the files unpacked from the offline installer), from the discs, or a copy of the folder from another computer all work. Nothing needs to be installed through Wine: the files are read as they are.
- For Tale of Two Wastelands, the output folder of its installer (the merged `FalloutNV.esm` is in it) and the `Data` folder of New Vegas. See [mods and load order](mods-and-load-order.md).

## Pointing it at the game

With nothing set, OpenFallout looks for the games where they are usually installed, and says so in its log (`Found Fallout: New Vegas in ...`):

- Steam on every system: the library folders that Steam lists, and the folders of `steamapps/common` with "Fallout" in their name. On Windows, Steam's folder comes from the registry; on Linux and macOS, from the usual places (`~/.steam/steam`, `~/.local/share/Steam`, the Flatpak and Snap ones, `~/Library/Application Support/Steam`).
- GOG, Bethesda's installer, Epic and hand copies: the folders with "Fallout" in their name under `GOG Games`, `Program Files (x86)`, `Program Files`, `Games` and a few others on each hard drive on Windows; under `~/Games` and `~/Applications` on macOS and Linux; and inside the Windows drives of Wine, CrossOver, Bottles, Lutris and Heroic (the `drive_c` folders of their prefixes).
- If both games are found, New Vegas is used. The search only runs when you have named no plugin and no data directory of yours has `Fallout3.esm` or `FalloutNV.esm`.

To use a folder of your own, or when the search finds nothing (the log then says so), name it. Either on the command line:

    openfallout --data "/path/to/Fallout New Vegas/Data"

or once and for all in `openfallout.cfg` in your configuration folder, one line for each data directory (later ones win over earlier ones):

    data="/path/to/Fallout New Vegas/Data"

The configuration folder is `~/.config/openfallout` on Linux, `~/Library/Preferences/openfallout` on macOS and `Documents\My Games\OpenFallout` on Windows. The settings file `settings.cfg` and the log `openfallout.log` are in the same folder. Saves and screenshots go to the data folder instead: `~/.local/share/openfallout` on Linux, `~/Library/Application Support/openfallout` on macOS, the same folder as the configuration on Windows.

Which plugins load: if you name none, the ones the games make active (the base master named in `Fallout_default.ini`, the plugins with a `.NAM` file next to them, and the ones in a `Plugins.txt` given with `--plugins-file`), with their masters. Name plugins with `--content` (or `content=` in `openfallout.cfg`) to choose them yourself. Tale of Two Wastelands: `data="<Data folder of New Vegas>"`, then `data="<output folder of the installer>"`. The rules, one by one, are in [mods and load order](mods-and-load-order.md).

The launcher (`openfallout-launcher`) has a Data Files page that writes these lines for you. It comes from OpenMW and was made for Morrowind; the project has not tried it on the Fallout games, so the command line and `openfallout.cfg` are the way that is known to work.

## Starting a game

    openfallout --skip-menu --new-game

starts a new game of the game you pointed it at: its opening scene, the making of the character and the first quest. Without these options the main menu opens, and *New Game* does the same. `--start <cell>` (for example `--start WastelandNV:-18,0`, a worldspace and the cell's grid position, or the name of an interior cell) opens a cell directly, which is what the project's own test runs do. `--load-savegame <file>` loads a save.

## Frame rate and picture

The settings are in `settings.cfg` in the configuration folder. The ones that matter most:

| Setting | What it does |
|---|---|
| `[Camera] viewing distance` | How far the world is drawn, in game units (one cell is 4096). The default is 7168, so distant hills are lost in fog. |
| `[Terrain] distant terrain = true` | Draws the land beyond the loaded cells. Off by default, because it costs a lot of frame rate and memory. |
| `[Video] force vertex buffer objects = true` | Draws all meshes from vertex buffers instead of OpenGL display lists. Display lists are slow to play back on some drivers (macOS). |
| `[Video] vsync mode`, `framerate limit` | As in any game. |
| `[Video] window mode`, `resolution x`, `resolution y` | The window. |

Trees are drawn as crossed cards with the leaf image of their record, because the games' SpeedTree files are not read.

## When something goes wrong

Look at `openfallout.log` in the configuration folder first: it lists the data directories, the plugins and the archives it found, in the order they were used (`Archive order`, `Load order`), and every file it could not read. A bug report with that file attached is the most useful one: <https://github.com/Elemperor1/OpenFallout/issues>. Do not attach game files.
