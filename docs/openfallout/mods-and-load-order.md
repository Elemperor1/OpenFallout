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
   in its `Data` folder hold the plugin's display name, but only that the file exists counts). This is how Tale of Two
   Wastelands activates its plugins.
4. The masters of all of these, wherever the data directories have them.

The ini files are `Fallout_default.ini`, `Fallout.ini` and `FalloutPrefs.ini` in the folder above each data directory
(where the games ship `Fallout_default.ini`), then the ones given with `--game-ini`, later ones over earlier ones. A key
that stands twice in a section of one file counts with its first value, which is what the Windows call the executables
read their ini files with gives (the executable of New Vegas imports `GetPrivateProfileStringA`). The retail
`Fallout_default.ini` of New Vegas needs it: it has `sTestFile1=FalloutNV.esm` near the top and `sTestFile1=` again
further down, and its `sTestFile2` is `FalloutNV_lang.esp`. With the last value counting, the base master would not
have been named at all (Mac round 8D part B).
Without any plugin named, the order is the games' own: `--plugin-order time` (below) unless `--plugin-order list` says
otherwise. With the Tale of Two Wastelands output folder as the only data directory, the rules give exactly the 14
plugins of the merged game (the four small packs have no `.NAM` file there, so they stay out).

`FalloutNV_lang.esp` is a placeholder of 56 bytes: the header of a plugin (no master, no record, no group, next object
id `0x800`) and nothing else, which the retail ini makes active through `sTestFile2`. The executable does the same:
the files `sTestFile1` to `sTestFile10` name are looked up in the list of plugins the scan of the `Data` folder built and
get the same active flag a `.NAM` file gives them, in the same list and so in the same order (the scan sorts masters
before the others, then by file time; nothing in the executables builds a name with `_lang`; Mac round 8D part B). It
loads last of the 19 plugins in an unnamed run of the Tale of Two Wastelands output with the New Vegas `Data`, changes
no record, and takes one of the 255 places.

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

## What a later plugin does to the records of an earlier one

A record with the form ID of an earlier plugin's record replaces it as a whole; nothing is merged field by field, and
the last plugin in the load order wins. That is the rule of the games (Mac round 8A), and the stores here work the same
way: they are keyed by form ID and a later record replaces the earlier one. For placed references (REFR, ACHR, ACRE)
that means everything the reference has is the later plugin's: the flags, the position, and the cell it stands in.

- A reference that a plugin flags as deleted (flag `0x20`) is gone: the cell does not list it. This holds when the
  record has its subrecords and when it has none.
- A reference flagged as initially disabled (`0x800`) is disabled; an override without the flag enables a reference
  the master disabled.
- A reference whose override stands in another cell is in that cell only: it leaves the cell the earlier plugin put it
  in. (The games load the children of a cell when the cell loads, and the exe reads did not settle what happens to an
  override of a child that is already there; here the last plugin wins in every case.) Tale of Two Wastelands has 406
  of these, all in its own file: 404 references of `PointLookout.esm` that it moves into cells of `Fallout3.esm`, and
  2 of `Fallout3.esm` (Mac round 8B part D3).
- A plugin's own records have the plugin's own index in their form ID (the top byte, the number of its masters); a
  reference of a mod to a record of its master has the master's index.
- A cell, worldspace or any other record a plugin includes whole replaces the earlier one the same way, so a mod that
  only wants to add a reference to a cell has to carry the cell record too, as the editor writes it.

`smoke --only mods` and `--only activation` check each of these with a synthetic mod, `OFTestMod.esp`.

## The birth scene of Tale of Two Wastelands (CG00)

The first Mac run of a New Game on the 14 plugins (round 8C part E) never left the quest `CG00` of the Vault 101 birth
scene: the stages went 5, 6, 8, 9, 10, 18, 20, 22, 38, 40, 42, 65, 70, 80 and stood there. Fallout 3 alone goes on to
85, 90, 100 and `CG01` in about 12 s. The scripts and records (round 8D part A) say what Tale of Two Wastelands changed
and why that meets a gap the engine could not cross:

- Stage 60 of `CG00` calls `TTW_ShowGeneProjector` (code `0x3800`, no arguments, 2 calls in the 14 plugins, the other
  in `Vault101ExitScript`) where Fallout 3 calls `ShowRaceMenu`. The name is the one the source text of the scripts
  has. The engine now knows it as a command and opens the same menu for the race and the face; the gene projector of the
  extender's own interface is not made.
- A new stage 70 (the traits) comes between the Dad's last line of the talk and stage 80: the two Dad responses that set
  80 in Fallout 3 (`0x0601F37F`, `0x0601F380`) now need a stage below 70 and set 70, and the timer of `CG00SCRIPT` sets
  80 and opens the traits menu about a second later. Between 70 and 80 none of the 18 responses of `CG00DadSpeech` can
  be said (the two that fit by stage are Say Once and used).
- The script of the Dad calls `SayTo` every frame while his `talking` is 0 and sets `talking` to 1 after it; a block
  `Begin SayToDone CG00DadSpeech` sets it back to 0. In the executable `SayTo` (`0x5C9100`) raises that block on the
  speaker's script at once, inside the call, when no response of the topic can be said (`0x8DC596`, in `SayTopic`; the
  block runs on the next pass over the script's blocks); when a line is said it is raised at the end of the line, after
  the end script of the response (`0x936076`), and a second `SayTo` while a line is running cuts it. So the Dad's script
  keeps cycling through the gap and says the next line as soon as stage 80 is set. The engine ignored the empty call, so
  `talking` stayed 1 for good.

The engine now tells the script of a speaker that has nothing to say, in the next frame, that it is done (the frame after,
so that the block that made the call has ended first, as the executable's flag does), and the log says when a topic that
had a line has none, or the other way round. `smoke --only dialogue` checks it with a topic whose only response is for
another voice.

Other facts of the executable that the engine does not follow yet: Say Once is kept for the response itself, for every
speaker (the engine keeps it for each character); a speaker that is not in the high process level, or a player in some
state, takes an instant path that runs the begin and end scripts and raises the block back to back, and raises nothing when
no response fits; the begin script of a response runs when the response is picked, before the voice starts, and the end
script when the line is over (the engine does both).

## Regions: `PlayerInRegion` and `IsPlayerInRegion`

`FalloutNV.esm` has 74 conditions that ask `PlayerInRegion` (the function number `0x264`; the command table has no
command of the code `0x1264`) and a script command `IsPlayerInRegion` (`0x1260`) that asks the same thing. Both read the
list of regions of the player, which the executable makes in one place (Mac round 8D part C): whenever the player is put
into a cell (`SetParentCell`, `0x94CAE0`), not when the player moves inside it. The candidates are the regions that the
exterior cell lists (its `XCLR`); the player is in one of them when its position is inside one of the areas of the
region (`REGN` `RPLI`, then `RPLD`: the corners of a polygon as floats) and the region is of the worldspace of the
player or of none. A building has no list, so every region reads 0 there, and the object the condition runs on is never
looked at. OpenFallout reads the areas of the `REGN` records (`ESM4::Region`) and answers both from the exterior cell of
the player: the region is on its `XCLR` list, the worldspace matches, and the player is inside an area (an even-odd test
of the x and y). The one difference: the games work the list out once, with the position the player is put at, so a
player who walks out of an area of the cell still reads as inside until the next cell is entered; OpenFallout looks at
the position of the player when the condition or the command is asked. The parameter of the function is a form, so the
loader gives it the load order index of its plugin like the other forms of a condition (the table of the parameters of
conditions has the entries `610` and `612`, which the command table cannot give).

Also in this part of the table, `GetCasinoWinningStage` (the condition `0x262`) and `GetCasinoWinningsLevel` (`0x125C`)
tell the highest of 1 to 4 for which the winnings of the player in that casino reach that quarter of the casino's limit,
and 0 for a casino that the player has no record in; the record is made only by the menus of the slot machines,
blackjack and roulette (`ShowSlotMachineMenu`, `ShowBlackJackMenu`, `ShowRouletteMenu`), and `SetCasinoWinningsLevel`
changes a record that exists and makes none. OpenFallout has none of those menus, so the player never has a record: the
condition and the command answer 0 and `SetCasinoWinningsLevel` does nothing.
`smoke --only exterior` and `--only interior` check the region, from the command and from a condition of a log entry.

## Not done

- The games skip a plugin file of zero bytes; OpenFallout does not check the size of a plugin.
- A `DLCList.txt` is not read: neither game's executable reads it.
- Not emulated, because the executables compare with an uninitialised value or the effect was not found: the file-time
  compare of archives that have no name table, the type mask that filters archives by what they hold, `.override`
  marker files, `bCheckRuntimeCollisions`, and the `ArchiveInvalidation Invalidated!.bsa` trick (the file is only
  another archive here).
- A file's path in an archive is looked up with forward slashes lower-cased, as in the engine's VFS; the games hash the
  path with backslashes.
- Partial forms: the games read a record flagged `0x4000` in a plugin that is not a master and lists no masters as an
  addition to an existing form (it loads into it, and is dropped when there is no such form). OpenFallout loads such a
  record like any other, as a whole. No record of the 14 plugins of Tale of Two Wastelands, of its four packs or of
  the 16 retail ESMs carries the flag (Mac round 8B part D1), so this matters for other mods only.
- The casino of New Vegas: the menus of the slot machines, blackjack and roulette, and the winnings the player has in
  each casino (the answers above are the ones of a player who never gambled).
- `PlayerInRegion` works the list out when it is asked, not when the player enters a cell (see above). The parts of the
  executable that were not traced: what the per-frame check at `0x947B10` and the state at `player+0x7AC` do when they
  put the player into a cell again.
