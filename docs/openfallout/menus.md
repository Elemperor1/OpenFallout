# The game's own menus

The games ship their user interface as data: 53 menu files in New Vegas and 38 in Fallout 3 (`menus/*.xml` and the
prefabs they include, inside `Fallout - Misc.bsa`), nine bitmap fonts (`.fnt` plus a DDS picture), and a texture atlas
(`interface/interfaceshared.tai`). OpenFallout draws these files instead of rebuilding each screen by hand. What a button
does is still code; what a screen looks like, where its parts are and what it says come from the files.

`components/falloutmenu` is the part that needs no engine. It is tested on its own (`apps/components_tests/falloutmenu`).

| File | What it does |
| --- | --- |
| `xml.*` | A reader for the menu files, which are not well-formed XML: stray closing tags, `&` signs, comments inside tags. |
| `menufile.*` | Loads a menu: the `<include>` prefabs, the `&entity;` words (menu codes, justify values, system colours, pad buttons), the `&-sSetting;` texts, the tile tree. |
| `menu.*` | Evaluates a menu: every trait is a list of steps on one running value (`copy`, `add`, `mul`, `gt`, `onlyif`, ...) that reads other tiles, the screen, or the globals; then lays the tiles out, makes the draw list and finds the tile under the pointer. |
| `font.*` | Reads a `.fnt` and lays a text out with its glyph widths, justification and wrapping. |
| `atlas.*` | Reads the `.tai` list of pieces of `InterfaceShared*.dds`. |

## Showing a menu

| Part | What it does |
| --- | --- |
| `components/falloutmenu/openmenu.*` | Opens a menu from the data (menu file, fonts from the INI, the atlas, texture sizes) for a window of some size, turns the draw list into pictures in window pixels (a character cut from the font texture, an atlas piece, a repeated strip, a picture cut to a window), and plays the pointer into it. |
| `components/falloutmenu/listbox.*` | What the games' code does for `list_box.xml`: a row is a tile made from the menu's template, given its traits, its height and place; the scrollbar is told how many items there are; the highlight follows the pointer. |
| `apps/openfallout/mwlua/menubindings.cpp` | `core.menus.open(file, width, height, pad)` gives a menu object to Lua: its pictures, traits, clicks, lists, scrollbars. |
| `files/data/scripts/omw/falloutmenu.lua` | Draws the pictures as widgets, hands the pointer to the menu, and calls the script's handler for the tile that was clicked. `onClick(name, f)`, `set(tile, trait, value)`, `io(trait, value)`, `list(listTile, template)`, `onDrag`, `batch`. |
| `falloutquantity.lua`, `falloutitemcards.lua` | The "how many?" menu and the item cards, used by the screens that move or list items. |

What the host (the script) must do for a menu is what the games' code did: write the values the file leaves empty ("set
in code": a text's `string`, the `_Value` of a card, whether a tile is `visible`), the traits of the menu root that the file
reads through `io()` (`_DialogVisible`, `_ShowingText`, ...), and the cumulative counters the pointer drives. The file reads
these from the pointer, and the renderer sets them:

- `mouseover` is 1 on the tile under the pointer. `clicked` and `wheelmoved` only go up (`wheelmoved` goes down for a turn
  of the wheel away from the player); scrollbars and arrows subtract the counters. `dragstartx/y` is -1 while nothing is
  dragged; `dragx/dragy` are the pointer's place in the parent while dragging.
- A tile that has `target` is what the pointer hits. A tile with `clips` is cut to the nearest ancestor with a `clipwindow`,
  and so are its children.
- A scrollbar keeps its value when its marker is let go and when the wheel turns (the files only give the drag).

Screens drawn from their file so far: the hours to wait or sleep (`sleep_wait_menu.xml`), the dialogue
(`dialog/dialog_menu.xml`), taking things from a container and putting them back (`container_menu.xml`), "how many?"
(`quantity_menu.xml`), the bench (`recipe_menu.xml`) and the items, stats and data pages of the Pip-Boy
(`main/inventory_menu.xml`, `main/stats_menu.xml`, `main/map_menu.xml`). The
older hand-built widgets of each stay as a fallback for when the file cannot be opened, and are removed once the screen
has been played.

### The tabline

A menu file that has a tabline (`tabline.xml`) leaves the buttons to the game's code: it keeps the template
(`TabButtonTemplate`) and the code makes one button for each tab, gives it its text and a `listindex`, and says which one
is current in the line's `_CurrentTab`. `Shown:tabs(line, template, labels, picked, onPick, options)` of `falloutmenu.lua` does
that; `options.values` gives the `_CurrentTab` of each tab when the code leaves a number out (the data page has no local
map, so its tabs are 1 to 4 and the file's expressions for the lists still say which one shows).
A button is as wide as its text and the room the template leaves around it; the buttons are spread over the width of the
line with the same gap between them, which is also `_LeftLineLength`, the length of the lines the file draws from one
button to the next and to the brackets at the ends (the lines meet the brackets exactly with this gap, which is the
reason to think the game spreads them so). The Pip-Boy page row (STAT, INV, DATA) is not in any menu file: the games have
it in the Pip-Boy device, which is a model, and `falloutpipboy.lua` keeps a plain row of its own at the top of the screen
for it.

The pages of the Pip-Boy (`main/inventory_menu.xml`, `main/stats_menu.xml`, `main/map_menu.xml`; the repair, item mod and
reputation menus have the same id) are laid out in a box of 1024 by 768 design units, which is the background picture
`Interface\Shared\Background\pipboy.dds` stretched over it. A script opens them with the box
(`menu.open(file, { box = menu.PIPBOY_BOX })`) and the renderer draws the box as large as the window lets it be (the window
height at 16:9) in the middle of the window, below the 40 units at the top (the units of `ui.screenSize()`) that the row of
the pages keeps, with the pointer in the box's own pixels. Every other menu is laid out for the whole window.

### The items page of the Pip-Boy

`falloutpipboyitems.lua` lists what the player carries in the list of `inventory_menu.xml` (the marker of what is worn is
the file's), puts the weight, health, resistance (Fallout 3) or threshold (New Vegas) and caps in the cards of the
headline, and the cards of the thing the pointer is on in the file's `item_stats_display`. A click on a thing uses it
(puts it on, takes it off, eats it, reads it); the keyring is a row at the end of the miscellaneous things and opens the
keys, the Cancel button closes it. Keys: up and down and the pages move the pick, Enter uses it, D drops it (a stack of more
than `iInventoryAskQuantityAt` asks how many on `quantity_menu.xml`), `[` and `]` go to the tab before and after. Not there
yet: the picture of the thing (the games show its model), the hot keys, repair and mods (their buttons are hidden).

### The stats page of the Pip-Boy

`falloutpipboystats.lua` fills `main/stats_menu.xml`: the headline (level, health, action points, experience; the menu reads
them from `user5` to `user8` of its root), the five tabs along the bottom (the file draws them; the page is the root's
`user0`, 0 to 4, which a click on a tab sets), the lists of S.P.E.C.I.A.L., skills and perks (the template is
`stats_list_template`; the number on the right is `user1` of the row), and the description of the row that is picked or under
the pointer, which the file shows under the picture of the thing (`stats_icon`, made visible while there is a description).
The status page puts the pictures of the parts of the body in the Vault Boy (the games choose them in code: `head`, `torso`,
`left_arm` ... under `Interface\Stats`, with `_broken` for a crippled part, which the engine reports in `core.hud.status()`) and
gives each part's meter 1, or 0 when it is crippled. Keys: up and down and the pages pick a row, `[` and `]` go to the tab before
and after. Not there yet: the pictures of the skills, perks and attributes (the games choose models and textures in code), how
hurt each part is (the engine does not tell), the effects, the hardcore needs (their buttons are hidden), the healing buttons, and
the lines of the general page.

### The data page of the Pip-Boy

`falloutpipboydata.lua` fills `main/map_menu.xml`. The headline has the place the player is in (the name the engine gives the
cell, or the worldspace's) and the date and time of the world written as the executables do (`10.22.81, 21:41`: month, day, two digits of the year, then
the 24-hour clock; no AM or PM); the tabs are World Map, Quests, Misc (Notes in Fallout 3) and Radio with the
numbers 1 to 4 the file gives them, their words from the settings the executables name (`sWorldMapTabText`, `sQuestsTabText`,
`sMiscTabText` or `sNotesTabText`, `sCommsTabText`), written as the setting holds them (the executables' code for the tab bar
changes no case) (tab 0, the local map, is not there: the games draw it from a render of the cell the player is in,
and nothing here can).

*World map.* The picture is the file's `MM_WorldMap_ParentImage` (the file names the Fallout 3 picture; the picture of the
worldspace record, `ICON`, is the picture: both executables take it from the record and from nothing else, so New Vegas
shows its own `wasteland_nv_1024_no_map` and each expansion its own; the file's name stands in only for a record without
one). It is a hot rectangle that names a file, which the renderer now draws as
a picture (the hacking and character menus have such tiles too). The places the player has found are tiles made from the
file's `MapMarkerTemplate` as children of the picture, with their position in `_x` and `_y` as a part of the picture (0 to 1);
a place is put on the picture by the rectangle of cells the worldspace's `MNAM` gives it, as both executables do (the left edge
is the north-west cell times 4096, the right edge is the cell after the south-east one, the top is the cell after the
north-west one, the bottom is the south-east cell times 4096, and the rows go up to the north; the usable width and height
of `MNAM` are the size of the picture in the file's units, `filewidth` and `fileheight`, 2048 for the wasteland), and the
player's arrow is put the same way. On the world map tab the picture has a margin of 13/128 of its width on each side, so a point
is at `u * 0.796875 + 0.1015625` of the picture (the executables remap it for that tab only). The picture is
placed with the file's own numbers (the window of the map is 855 by 500 units, its middle 427.5 by 250 is where the picture
can be moved to at most, the box in the middle names the place under it) and starts centred on the player at the file's own
size (a magnification of 1.0, the file's default; the executables keep none between two openings).
The pointer drags the picture and the wheel zooms it (the file's `draggable` and `wheelable` tiles; the page does the
arithmetic, since the file leaves it to code): a turn of the wheel multiplies the magnification by 1.1 (up) or divides it
by 1.1 (down) once, however many notches the turn is, between `fWorldMapMinZoom` and `fWorldMapMaxZoom` (0.75 and 5.0 in
both executables, and no plugin of either game changes them). The widths the file leaves to the code (`user0` of the picture for the places, `user1`
for the quest markers, `user2` for the arrow) are each a pair of settings read from the executables: the width runs from the least to the
most as the magnification runs from its least to its most limit (places `fWorldMapMarkerMinSize` and `fWorldMapMarkerMaxSize`, 20 and 50;
quest markers 30 and 60; the arrow 40 and 70); a click on a place travels there, a place that cannot be travelled to (found,
not visited) only says its name. Enter travels to the place the box names. The picture of a place is the games' own table of fourteen
(`Interface\Icons\World Map\icon_map_*`: city, settlement, encampment, natural landmark, cave, factory, monument, military,
office, ruins town, ruins urban, ruins sewer, metro, vault, by the marker's type 1 to 14, in that order in both executables)
and the picture for a place that is not yet travelled to is `icon_map_undiscovered`; a type outside the table has the dot of a
waypoint. The arrow that turns to where the player faces is not done: the arrow does not turn.

*Quests.* The list of the file with its empty squares, the objectives of the quest that is picked in the list beside it (the
square of an objective that is done is filled and its text dim), as the engine's `core.pipboy.quests()` has them; a quest that
is done is not listed. The file's buttons (Show Location, Place Marker, Make Active) are hidden: nothing here knows where the
objectives of a quest are on the map, or which quest is the active one. The place the file puts the objectives in depends on
those buttons, so the page sets it to where the file has it with a button shown. *Notes and radio* are the file's lists with no
rows: the engine does not have the notes or the stations of the player.

The page needs `core.interact.mapInfo()` (the worldspace, its picture and the rectangle of cells, where the player stands and
faces, the name of the place, the date) and the `space` and `type` of each place of `core.interact.map()`.

### Making the character

`falloutchargen.lua` draws the screens that the engine's character-making (`core.chargen`) opens, one after the other. The
name is entered on `dialog/texteditmenu.xml` (the prompt, the name with a cursor, OK); the points of S.P.E.C.I.A.L. and the
tags of the skills are on `chargen/char_gen_menu.xml`, whose list takes the file's two row templates: `CGM_AttributeTemplate`
(the name, the value and the two arrows, which show by the file's own rules: the left one while the value is above its least,
the right one while points are left and the value is below its most) and `CGM_SelectItemTemplate` (the name, the value and
the square of a skill that is tagged). The file asks the code for the title, the text of the selection, the counter and the
numbers `_CurrPoints` and `_MaxPoints` (the points given so far and all that can be given), and Done is shown dim until they
are equal, as the file says. A click on a row picks it and shows its description; the arrows give and take a point; Reset
goes back to what the screen started with. The keys: up and down pick a row, left and right give and take a point (space
tags), Enter is Done. The top of the list is where the file leaves it to the code: 105 units down on the S.P.E.C.I.A.L. page
and 65 on the skills page (the numbers each page sets in both executables). The title is a format of the settings
`sAttributesTitle` ("ATTRIBUTES: %d/%d Points Assigned") and `sSkillsTitle` ("SKILLS:  %d/%d Selected", two spaces), not
changed in case: the points given and all that can be given (on the S.P.E.C.I.A.L. page the points are the values above their
least, 1, and all that can be given is what the script command allows less 7; on the skills page the tags and their
limit). The counter under the
description is read from the executables: the setting `sAttributesCount` ("Distribute %d Point", S.P.E.C.I.A.L.) or
`sSkillsCount` ("Tag %d Skill") is a format for the points that are left, with an "s" after it unless that is one, and the
counter is hidden when none are left; the number is the same whatever `flagA` of the script command (the points to add on
top of the values the character has) does, since the screen counts what is left. The picture of the selection (the file hides
it) is the `ICON` string of the actor value's own record (`SPECIAL_Strength.dds` and the others under
`Interface\Icons\PipboyImages\S.P.E.C.I.A.L`, `Skills_*.dds` under `Interface\Icons\Stats`; New Vegas's record for
Throwing names `skills_survival.dds`), taken with its description and name from the same record. The sex and race screen
(`chargen/race_sex_menu.xml`) is a rendered head with sliders and has nothing to show here, and keeps the stand-in widgets,
as does any screen whose file the install lacks.

*The traits (New Vegas).* New Vegas opens `trait_menu.xml` for them (the only script of `FalloutNV.esm` that shows a trait
menu, `VCG04ActivatorScript`, calls `ShowTraitMenu` after `ShowRaceMenu`; `trait_select_menu.xml` is the one that no script
opens) and Fallout 3 has no traits. The list takes the file's `LUM_PerkTemplate` (the name and a square on each trait that is
picked); a click picks a trait or takes the pick back, and one past the most does nothing (no message, as in the executable);
Reset takes all back; Continue is lit whatever the number picked, none included (the file and the code check nothing). The
description is the perk's own and the picture its `ICON`. The title is `sTraitMenuTitleText` ("CHOOSE UP TO %d TRAITS", or
`sTraitMenuTitleTextSingular` "CHOOSE %d TRAIT" when the most is one) with the most that can be picked, the counter
`sTraitMenuCounter` ("%d TRAITS LEFT", or the singular "%d TRAIT LEFT" for exactly one) with the picks that are left; the
counter is never hidden. The executable's own limit is `iTraitMenuMaxNumTraits` (3 by default; the engine keeps its limit of
two) and its list is every perk with the trait flag in the order the data handler has them (the engine lists them by name),
with the conditions of the perk checked for the player (the engine does not check them).

### The level-up screen

`falloutlevelup.lua` fills `levelup_menu.xml` (a module of `falloutpipboy.lua`, which opens it when the engine hands out a
level and shows the stand-in widgets only when the file is not in the install). Read from both executables (the creator,
the click handler and the list fill of `LevelUpMenu`):

- The title is `sLevelUpTitleText` ("WELCOME TO LEVEL %d") with the player's level, set once when the menu is made; the perk
  page keeps it.
- The counter is `sLevelUpSkillCounter` (one point left, no number in it) or `sLevelUpSkillCounterPl` (one `%d`) on the skills
  page and `sLevelUpPerkCounter` / `sLevelUpPerkCounterPl` on the perk page, hidden when nothing is left. The file's own
  `_CurrPoints` and `_MaxPoints` (the points given and all that can be given) decide when Continue is lit.
- The skills are the actor values 32 to 45 in that order (Barter to Unarmed), without any whose record has the flag 0x2000
  (the executable's own test; the games list 13 skills, Big Guns and Survival each being in one game only). A row has its
  base value, `_ExtraValue` (what perks, chems and the like add), `_AddedValue` (the points given on this screen) and the
  value as a whole number between 0 and 100 in `_DisplayString`. The points are `trunc(0.5 * Intelligence + 10)` changed by
  the perk entry point 10 (`iLevelUpSkillPointsBase`, `iLevelUp01Mult` and the others have no reader in New Vegas), and are
  limited to what the skills have room for under 100. The right arrow gives a point at once to the skill (the executable
  writes the actor value on the click), the left one takes back a point given on this screen; the arrows show by the file's
  expressions. Reset takes back every point given on the page.
- The selection is the actor value's own `ICON` and `DESC`; on the weapon skills (Big Guns, Energy Weapons, Explosives,
  Melee, Small Guns, Unarmed) the corner badge `stats_icon_badge` shows `weap_skill_icon_*.dds` (New Vegas's file only).
- The perk page comes when the level gives a perk. New Vegas tests `level % iLevelsPerPerk == 0` (2 by default); Fallout 3
  has no such setting and opens the page whenever at least one perk qualifies when the menu opens, in practice every level
  (the engine decides this as it did before; it was not changed). The list is the perks that are playable, not traits, below
  their last rank and whose conditions the player meets; with `bHideUnavailablePerks` at its default 0, perks the level has
  not reached are listed too, with the text dim (`_TextAlpha` 128 against 255) and a click on one is ignored (a row that is
  picked is put back whatever its alpha). The rows are sorted by that alpha (open first), then by the level the perk asks
  for, then by name. A multi-rank perk the player already has shows the rank it would be picked at in brackets ("%s (%d)",
  the current rank plus one). One perk is picked: a click on the picked row puts it back.
- The text under a perk is built by one function of the executables: "R: L 6" (`sRequirementsText`, `sLevelAbbrev` and the
  level, only when the perk asks for more than level 1; otherwise just "R: "), then a line for each condition that is a
  `GetPermanentActorValue` (the actor value's name, an operator and a number: a space and the number as it is, except for
  "greater" (the number less one), "less" (" < ") and "less or equal" (" < " and the number plus one); skipped when the
  number is 0, the value is Karma or has no name) or a `HasPerk` (the perk's name and the rank when it is above 0),
  each followed by ", " when another condition comes (an "or"-ed condition by " `sOr` " instead), the first one preceded by
  ", " when the level was written; the other kinds of conditions write nothing but count as written, and when nothing was
  written the text is "--". Then a new line, "K: n" (`sRanksText` and the number of ranks), a blank line, and the
  description. The picture is the perk's `ICON`.
- Continue goes to the page after, Back to the page before (shown from the second page on), Done on the last page adds the
  perk (at its next rank) and closes the screen. The code of Continue checks no points: only the file's own expression
  (`_CurrPoints == _MaxPoints`) lights it, and the screen sets those numbers. When the level itself opened the screen, the Pip-Boy
  window closes with it, as the menu of the games does.
- The keys: up and down light a row, left and right (or space) give or take back a point or pick the perk, Enter is Continue
  or Done, Backspace is Back, R is Reset.

Not done: the sounds, and everything on this page is checked in the offline harness only (not in the running game).

## The words of the labels

The menu files take the words of their labels from game settings (`<string> &-sCancel; </string>`). The games keep most of
those words in their executables, and the plugins only hold the settings that were changed from them, so reading the plugins
gives nothing for OK, Cancel, Take All, the titles and the names of the cards: the first run on the real data showed those
labels blank (the number of pieces of each menu with no words matched the count the engine gave exactly). `gameSettingText`
now answers from the plugins when a record has the text, and otherwise from `components/falloutscript/settingtext.cpp`, a
table of the port's own wording for the 120-odd labels the screens drawn from the files use (not a copy of the executables'
table; a game in another language gets its words from its plugin). The card, caravan and roulette games' words are not in it.

## What was read from the executable

- The design space is 960 high and `960 * width / height` wide; it is drawn scaled by `height / 960`.
- Tile types, trait names and ids, expression steps, operand sources and the `&entity;` table come from `FalloutNV.exe`.
- The system colours are numbered HUDMain 1, HUDAlt 2, Terminal 3, Pipboy 4, MainMenu 5, System 6 (none is 0, a tile's
  own colour is -1); their red, green and blue come from the `iSystemColor` settings in the INI.
- A tile's position is the sum of its ancestors' positions. A text tile's `x` is the point it is justified to (left end,
  centre or right end).
- A hot rectangle that names a file (`filename`) is a picture that takes the pointer: the picture of the world map, the
  cursor of the hacking menu (seen in the files, not yet in the running game).
- A hotrect that only copies its child's `x` and `y` does not move that child a second time (the Pip-Boy tabs would end up
  off the screen otherwise). This is the rule that matches the design; it has not been seen in the running game yet.

## What is not known yet

- How the games place the Pip-Boy background (1024 by 768 design units) on the screen of the device; the box is centred and
  scaled to the window here, which is a choice of the port and not read from the games.
- How the games wrap a word that is wider than the room a text has: the Pip-Boy's status buttons ("CND", "RAD") are given
  less room than their three letters take, and are one line in the games, so a line is broken at a space only and such a
  word stays whole (the port's rule, matched to that).
- The meaning of the byte `&widescreen;` reads, and of the traits `wraplimit`, `wraplines`, `ishtml`, `cropx`, `cropy`,
  `repeat*`, `explorefade` and `menufade`.
- Whether the font or the file draws the tab words of the data page in capitals (the executables' code for the tab bar does
  not change their case; New Vegas was read, Fallout 3 was not), what turns the arrow of the player on the map (the code sets
  its angle to minus the player's heading, minus 0.3, minus the cell's north offset inside; the page has no turning picture
  yet), the local map tab (zoom 0.1 to 0.9, places 20 to 75, a render of the cell), and the headline's place text outside: the
  games ask the exterior cell at the player's place for a form by its extra data (a positional lookup; which subrecord of the
  cell that is, and whether the name written is the form's map name, its editor id or its full name, was not read), and when
  there is none they write the cell's own name; the regions are scanned (the highest priority wins, an override beats one that
  is not) only when no cell is there, and the worldspace's name comes last. The page writes the cell's name, else the
  worldspace's.
- The words `&xitem;`, `&xlist;`, `&xboxhint;`, `&first;`, `&last;`, `&prev;`, `&next;` are in some menu files but not in
  the executable's table. The loader leaves them as text.

Game files are never part of the repository. The menu files are read from the player's own install at run time.
