# Mods, load order and Tale of Two Wastelands

This is what OpenFallout does with more than one plugin, and where it follows the games and where it does not. What the
games do was read from `FalloutNV.exe` and `Fallout3.exe` (Mac round 8A), not taken from a wiki.

## What loads, and in which order

The plugins to load are named with `--content` (or `content=` in `openfallout.cfg`), one for each plugin, and
optionally with `--plugins-file` (below). Whatever order they are named in, they load in this order:

1. Every plugin whose TES4 header flags it as a master loads before every plugin that does not. The flag in the header
   decides, not the extension: an `.esp` with the flag loads with the masters, an `.esm` without it with the others.
2. A plugin loads after every master it lists. The games do this for plugins flagged as masters (each is pulled behind
   the masters it lists, even when those were written later); a plugin that is not flagged and lists a master that
   sorts after it keeps its place there, and the master then replaces what the plugin changed. OpenFallout moves the
   master up for every plugin.
3. Within the masters and within the other plugins, the order is the order they were named in. `--plugin-order time`
   uses the rule the games have instead: oldest file first. Files written at the same time keep the order they were
   named in. (The games put the one their file search found later first; that depends on the file system.)

The log says what moved, in lines that start with `Load order:`.

A master that a plugin lists and that was not named is added when a data directory has it, as the games do (they
activate the masters of every active plugin). One that no data directory has stops the start with a message that
names each plugin and each master that is missing; so do two plugins that list each other as masters, a plugin named
twice, and more content files than fit: 255 is the room a form ID has (its top byte names the file, and `0xFF` is the
byte of the objects the game makes while it runs), and OpenFallout's built-in script file takes the first place, so 254
plugins and other content files fit. Only `.esm` and `.esp` files are plugins (the games have no `.esl`).

When a plugin that is not a master was made for another version of a master (the size of the master's file is not the
size in the plugin's header), the log warns and the start goes on, as in the games.

## Which plugins load

Name the plugins with `--content` (one for each) and those load, with their masters. Name none and OpenFallout does
what the games do: the files of the Data folder that are *active* load. The games find the active ones in four places,
and so does OpenFallout (the games' `Data` folder is the data directories together; for a name that is in several,
the last directory counts):

1. The base master: the values of `sTestFile1` to `sTestFile10` in the `[General]` section of the ini file, or, where
   the ini names none, `FalloutNV.esm` (`Fallout3.esm` where there is no such file).
2. The names in a `Plugins.txt` (`--plugins-file`, below).
3. Every plugin that has a file of the same name with the extension `.NAM` next to it (New Vegas does this; the files
   are empty). This is how Tale of Two Wastelands activates its plugins.
4. The masters of all of these, wherever the data directories have them.

The ini files are `Fallout_default.ini`, `Fallout.ini` and `FalloutPrefs.ini` in the folder above each data directory
(where the games ship `Fallout_default.ini`), then the ones given with `--game-ini`, later ones over earlier ones.
Without any plugin named, the order is the games' own: `--plugin-order time` (below) unless `--plugin-order list` says
otherwise. With the Tale of Two Wastelands output folder as the only data directory, the rules give exactly the 14
plugins of the merged game (the four small packs have no `.NAM` file there, so they stay out).

## `--plugins-file`

`--plugins-file <path>` reads more plugins from a file in the format of the `Plugins.txt` of the games: one file name
to a line, case does not matter, lines that start with `#` and blank lines are skipped, a UTF-8 byte order mark is
skipped. The newer format marks the active plugins with a leading `*` and lists the others too; where any line has the
mark, only the marked ones count. Where plugins are named with `--content`, the names are added after them (a plugin
named in both is loaded once); where none is, they join the plugins the games make active. A name that is not in any
data directory is skipped with a warning, as the games skip it.

The games themselves find their `Plugins.txt` in `%LOCALAPPDATA%\FalloutNV\` (or `Fallout3\`), or in the current
directory when `bUseMyGamesDirectory` is 0 in `Fallout.ini`. OpenFallout reads only the file it is given.

## Archives

The games open BSA archives in this order, and so does OpenFallout (the names are looked up in the data directories):

1. The archives of `sArchiveList` in the `[Archive]` section of the ini files (the files of "Which plugins load"; the
   list is split at commas and the names are trimmed, which matters for the retail list of New Vegas that has two
   spaces in front of its last name). Where no ini names a list, the retail list of the game is used: Textures,
   Textures2, Meshes, Voices1, Sound and Misc for New Vegas, Textures, Meshes, Voices, Sound, MenuVoices and Misc for
   Fallout 3, all as `Fallout - <name>.bsa`. (The games fall back to a list of three archives, one of them
   Oblivion's, when the ini does not name one; no retail install lacks the list.) A listed archive that no data
   directory has is skipped.
2. The archives named with `--fallback-archive`. A name that is not in any data directory still stops the start.
3. `Update.bsa`, for New Vegas, if a data directory has one.
4. For every plugin that has at least one master: the archives whose name starts with the name of the plugin
   (`<plugin>*.bsa`), the ones that are not empty, in the order of the names. This is how `Fallout3 - Meshes.bsa` and
   the others of Tale of Two Wastelands, `TaleOfTwoWastelands - Main.bsa`, `YUPTTW - Main.bsa` and the archives of the
   DLC (`Anchorage - Main.bsa`, ...) and of a mod (`MyMod - Main.bsa`) come in, with no list that names them.

`bUseArchives=0` in the ini turns 1 and 3 off. A name is opened once, whatever its case.

**The order they are looked in.** The games keep one list of the archives and take the file from the first archive
that has it. Fallout 3 appends each archive to the end of the list, so the one opened first wins. New Vegas ranks an
archive by its name (the test is on the letters, and case counts): `Fallo` 0, `DeadM` 1, `Hones` 2, `Lones` 3, `OldWo`
4, `Updat` 5, any other 6. A rank 0 archive goes to the end; any other goes in front of the first archive in the list
that is one of the base game's or of Dead Money, or of Honest Hearts when its rank is above 1, of Lonesome Road above 2,
of Old World Blues above 3, and of Update above 3; where there is none, to the front. So a later DLC's archives win
over an earlier one's, `Update.bsa` sits between them, and the archives of a mod win over all of them. The rule is
the same for Tale of Two Wastelands (a game with `FalloutNV.esm` is New Vegas, with or without `Fallout3.esm`). The log
prints the list, with the one that wins first:

    Archive order (the first one that has a file wins): Anchorage - Main.bsa, ..., Fallout - Textures.bsa, ...

For the Tale of Two Wastelands output folder alone (it has no New Vegas originals, which are in the New Vegas Data
folder; name both folders as data directories for a full install) the list is the 28 archives that Mac round 8A
worked out from the rules; a unit test (`newVegasRanksTheArchivesOfTaleOfTwoWastelandsAsTheMacReadItInTheGame`) holds it.

The engine looks in its archives from the last registered to the first, so OpenFallout registers them in reverse.

## Loose files against archives

`bInvalidateOlderFiles` in the `[Archive]` section decides (default 1, as in the games). With 1, a file in a data
directory wins over an archive's file with the same path, whatever the age of either: in the games, the existence of
the loose file invalidates the archive's. With 0, the archive's file wins. Another loose file in an earlier data
directory loses to one in a later directory, as before.

`ArchiveInvalidation.txt` (the file `SInvalidationFile` names; read from a data directory when `bInvalidateOlderFiles`
is 1) hides archive files that have no loose copy. A line with a backslash is a directory: the files directly in that
directory of the archives are hidden. Any other line is a file name, hidden in every directory of the archives for
which the data directories have a directory of the same path. (The games compare name hashes; OpenFallout compares
the names, which differs only when two names have the same hash.) A hidden file counts as not found in that archive,
and the search goes on in the others and then in the loose files.

`--game-ini` reads the settings above like any other; the engine's own `--fallback-archive` stays for the archives
that no ini names.

**Deviations.** `--fallback-archive` used to give the archive named last the highest priority. Now the archives it names
go through the order above like the others: among archives of rank 0 (`Fallout - ...`) the one opened first wins, as in
the games, and among those of any other rank the one opened later wins, so the archive of a plugin that loads later is
looked in before that of an earlier plugin.

## Not done

- The games skip a plugin file of zero bytes; OpenFallout does not check the size of a plugin.
- A `DLCList.txt` is not read: neither game's executable reads it.
- Not emulated, because the executables compare with an uninitialised value or the effect was not found: the file-time
  compare of archives that have no name table, the type mask that filters archives by what they hold, `.override`
  marker files, `bCheckRuntimeCollisions`, and the `ArchiveInvalidation Invalidated!.bsa` trick (the file is only
  another archive here).
- A file's path in an archive is looked up with forward slashes lower-cased, as in the engine's VFS; the games hash the
  path with backslashes.
