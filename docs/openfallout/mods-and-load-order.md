# Mods, load order and Tale of Two Wastelands

This is what OpenFallout does with more than one plugin, and where it follows the games and where it does not. What the
games do was read from `FalloutNV.exe` and `Fallout3.exe` (Mac round 8A), not taken from a wiki.

## What loads, and in which order

The plugins to load are named with `--content` (or `content=` in `openfallout.cfg`), one for each plugin, and
optionally with `--plugins-file` (below). Whatever order they are named in, they load in this order:

1. Every plugin whose TES4 header flags it as a master loads before every plugin that does not. The flag in the header
   decides, not the extension: an `.esp` with the flag loads with the masters, an `.esm` without it with the others.
2. A plugin loads after every master it lists. The games do not check this (a master that loads late then replaces what
   the plugin changed); OpenFallout moves the master up instead.
3. Within the masters and within the other plugins, the order is the order they were named in. `--plugin-order time`
   uses the rule the games have instead: oldest file first. Files written at the same time keep the order they were
   named in. (The games put the one their file search found later first; that depends on the file system.)

The log says what moved, in lines that start with `Load order:`.

A master that a plugin lists and that was not named is added when a data directory has it, as the games do (they
activate the masters of every active plugin). One that no data directory has stops the start with a message that
names each plugin and each master that is missing; so do two plugins that list each other as masters, a plugin named
twice, and more than 255 plugins. 255 is the room a form ID has: its top byte names the plugin, and `0xFF` is the byte
of the objects the game makes while it runs.

When a plugin that is not a master was made for another version of a master (the size of the master's file is not the
size in the plugin's header), the log warns and the start goes on, as in the games.

## `--plugins-file`

`--plugins-file <path>` reads more plugins from a file in the format of the `Plugins.txt` of the games: one file name
to a line, case does not matter, lines that start with `#` and blank lines are skipped, a UTF-8 byte order mark is
skipped. The newer format marks the active plugins with a leading `*` and lists the others too; where any line has the
mark, only the marked ones count. The names are added after those given with `--content` (a plugin named in both is
loaded once). A name that is not in any data directory is skipped with a warning, as the games skip it.

The games themselves find their `Plugins.txt` in `%LOCALAPPDATA%\FalloutNV\` (or `Fallout3\`), or in the current
directory when `bUseMyGamesDirectory` is 0 in `Fallout.ini`. OpenFallout reads only the file it is given.

## Not done

- The games also activate plugins from `sTestFile1` to `sTestFile10` in `Fallout.ini` and, New Vegas only, from a
  `<plugin>.NAM` file next to the plugin. OpenFallout loads what it is told to.
- A `DLCList.txt` is not read: neither game's executable reads it.
