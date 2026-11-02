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
cell, or the worldspace's) and the time and date of the world; the tabs are WORLD MAP, QUESTS, MISC (NOTES in Fallout 3) and RADIO with the
numbers 1 to 4 the file gives them, their words from the settings the executables name (`sWorldMapTabText`, `sQuestsTabText`,
`sMiscTabText` or `sNotesTabText`, `sCommsTabText`) (tab 0, the local map, is not there: the games draw it from a render of the cell the player is in,
and nothing here can).

*World map.* The picture is the file's `MM_WorldMap_ParentImage` (the file names the Fallout 3 picture; the picture of the
worldspace record, `ICON`, is the picture: both executables take it from the record and from nothing else, so New Vegas
shows its own `wasteland_nv_1024_no_map` and each expansion its own; the file's name stands in only for a record without
one). It is a hot rectangle that names a file, which the renderer now draws as
a picture (the hacking and character menus have such tiles too). The places the player has found are tiles made from the
file's `MapMarkerTemplate` as children of the picture, with their position in `_x` and `_y` as a part of the picture (0 to 1);
a place is put on the picture by the rectangle of cells the worldspace's `MNAM` gives it, as both executables do (the left edge
is the north-west cell times 4096, the right edge is the cell after the south-east one, the top is the cell after the
north-west one, the bottom is the south-east cell times 4096; the usable width and height of `MNAM` only stretch the picture,
and the rows go up to the north), and the player's arrow is put the same way. The picture is
placed with the file's own numbers (the window of the map is 855 by 500 units, its middle 427.5 by 250 is where the picture
can be moved to at most, the box in the middle names the place under it) and starts centred on the player at half size.
The pointer drags the picture and the wheel zooms it (the file's `draggable` and `wheelable` tiles; the page does the
arithmetic, since the file leaves it to code); a click on a place travels there, a place that cannot be travelled to (found,
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
- The look of the headline of the data page (the order of the time and the date, the 12 hour clock), whether the tab words are
  drawn in capitals (the page does, as the other tabs of the Pip-Boy; the executables' own code for it was not read),
  the zoom limits and the size of the places on the map (the executables keep them in settings whose default values are not
  read yet), and the remap of the position (`u * 0.796875 + 0.1015625`) that the executables apply for one mode of the map
  object, which is not the world map as far as is known.
- The words `&xitem;`, `&xlist;`, `&xboxhint;`, `&first;`, `&last;`, `&prev;`, `&next;` are in some menu files but not in
  the executable's table. The loader leaves them as text.

Game files are never part of the repository. The menu files are read from the player's own install at run time.
