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
- A box or a text that the pointer is over also puts `mouseover` and `clicked` on the hotrects around it that have `target` (a
  tab button is a hotrect with a text box in it, and the file reads the hotrect). An image or a hotrect that has `target`
  is over alone: the arrows of a toggle or a meter show when exactly one of the row, an arrow and the value is over (the
  file adds their `mouseover` and compares the sum with 1), so a row that stayed over under an arrow would hide both and
  the click would fall on the value (found in the running game, round 12AC).
- A letter typed in a menu clicks the tile its file names for it: the root has `_PCButton_<letter>` traits whose value is a
  tile name (`stats_menu.xml`: S stimpak, A Rad-Away, X Rad-X, R repair, E doctor's bag; `inventory_menu.xml`: R repair, E
  cancel, X mod; `barter_menu.xml`: A Accept, X Exit; `trait_menu.xml`: R Reset, A Continue, E Back; `sleep_wait_menu.xml`: W
  wait, E cancel; and so on). The game's interface manager (FalloutNV.exe 0x70e2b1-0x70e3d2, round 12AG) clicks the tile if it
  is visible and takes the pointer. `Shown:pcButton(symbol)` does that; the rest, level-up and (to come) barter screens use
  it. The other screens still have only the keys of the port.
- A tile that has `target` is what the pointer hits. A tile with `clips` is cut to the nearest ancestor with a `clipwindow`,
  and so are its children. This is the port's rule, made for the lists; the executable applies the rectangle to a tile whose own
  `clips` is set and the files copy `clips` from the parent into each child where they want it (`horiz_floating_scroll.xml`; Mac
  round 12BJ B1), so the text in a row that does not ask is not cut in the game. Whether the text of the rows of the other lists
  (the dialogue's scrolled lines, for one) is cut there was not looked at; a screen that must not cut sets `clips` to 0 on its
  rows (the V.A.T.S. queue does).
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
than `iInventoryAskQuantityAt` asks how many on `quantity_menu.xml`), `[` and `]` go to the tab before and after. A weapon
and a piece of armor have a condition (tenths of a percent of their health, 1000 is full): copies with another condition are
rows of their own (the best first; the row id carries the condition after a `~`), the cards show the condition as the file's
bar (`ConditionInfo`, its meter reads `user5`, 0 to 1, under the word of `sInventoryCondition`, with no number: the executable
sets none; the pip image on the bar is fixed by `user6`, 1 for a weapon at three quarters and 0 for armor at a half, the places
where their effects start, Mac round 12AX R4), and use, drop, take and barter work on the copies of that row. The numbers of a
card are those of the copy at its condition (a weapon's damage by the curve below, armor's rating and threshold). The cards of
armor follow the executables' filler (Mac round 12BE B1, `FalloutNV.exe` 0x708958): in New Vegas the resistance card is shown
when its number is not 0 and the threshold card when its number is not 0, and when both are 0 the resistance card shows two
dashes and nothing else; the file puts the two cards in the same place, so the one piece of the game's data that has both shows
them on top of each other in the game too. Fallout 3 (`Fallout3.exe` 0x61BFA6) has the resistance card only and shows 0. Both
numbers are whole (`%d`); the "DT 6.0" of the headline above the list is a different tile with one decimal.

What condition does (Mac rounds 12AW S12 and 12AX R4, `FalloutNV.exe`; `components/falloutscript/wear.*`, `equipment.cpp`):
only the player's things wear. A gun that is fired loses `fDamageToWeaponValue` points of its health (0.2) once for a shot,
whether or not it hits, changed by the effects of its ammunition that are of the condition kind (the shot of a shotgun is one
shot); a melee weapon or one of the hands loses the same when a swing reaches a victim; the hands with no weapon, a thrown
weapon and a weapon that is not playable or embedded lose nothing. A blow at the player wears one piece of its armor (the head for a blow at a place 1 or 2, else the upper body) by
`fDamageToArmorPercentage` (0.35) of two things (Mac round 12BA A, `FalloutNV.exe` 0x9B5A30): the share of the blow that the
resistance took (`D0 * a`, `D0` the blow as it met the armor, `a` the resistance of all that is worn, at most 0.85), and the smaller
of the threshold of all that is worn and what is left of the blow after both, less `fMinDamMultiplier` (0.2) of the blow
(`min(T, D2 - 0.2 * D0)`, which can be negative, and then the armor does not wear). So armor that has only a threshold wears (a
blow of 22 against a threshold of 10 takes 2.66 health, a blow of 12 nothing), armor with neither never does, and the
amount is not split between the pieces: the body armor is worn by the threshold of the helmet too. In V.A.T.S. the amount is
times `fVATSPlayerDamageMult`. A piece gives its whole rating (and threshold) times the factor of its condition, to the next whole
number up, and the card prints that number. A weapon that
is below three quarters of its health does the damage of its record times one less two thirds of what is missing from three
quarters (a half: 0.8325, nothing: 0.4975); armor below a half gives a half and its condition of its rating and threshold. The
player is told (`sWeaponLowCond`, `sArmorLowCond`, with the sound `WPNBreak` and four seconds) when an item goes below a quarter;
a weapon that reaches nothing is put away with `sWeaponBreak`, armor stays on with no message, and a thing with no health does
not go on (`sCantEquipBrokenItem`). The weapon commands `GetWeaponHealthPerc`, `SetWeaponHealthPerc` (lowering goes through the
wear, raising stops a point short of the most) and `ModWeaponHealthPerc` work on the weapon in the hands, `AddItemHealthPercent`
gives copies at a fraction of their health. Fallout 3 wears the same way with other numbers (Mac round 12AY R4, `Fallout3.exe` 0x582640, 0x583A40, 0x582720): a shot
(or a swing that reaches) takes the damage of the weapon's record times a number by the kind of weapon (`fDamageToWeaponGunMult`
0.03 for types 3, 5 and 6, `...EnergyMult` 0.04 for 4 and 7, `...LauncherMult` 0.06 for 8 and 9, `...MeleeMult` 0.05 for 0 to 2;
a weapon whose flags2 has bit 7 uses its own number, DNAM byte 84; a thing that is thrown or laid, 1.0), the ammunition has no
effect on it, a weapon does `fDamageGunWeapCondBase + Mult * condition` of its damage (0.66 + 0.34 c, melee 0.5 + 0.5 c, from the
settings, with no knee), armor gives `fArmorRatingBase + (fArmorRatingMax - fArmorRatingBase) * c` of its rating (0.2 + 0.8 c; a
broken piece still gives a fifth), the rating of a piece is rounded, there is no threshold, and the player gets no message at a
quarter (only `sWeaponBreak` with `WPNBreak` when a weapon breaks). Armor wears by its rating only, as there is no threshold (`Fallout3.exe` 0x7C2990: `fDamageToArmorPercentage` of the blow
as it met the armor times the resistance, the player as the only victim; armor with no rating never wears). Not done: the effects of the condition of the weapon on jams (the exe's tables:
reloads jam below half, a shot never in New Vegas), a weapon hit at the place 14 (the hit damage, times
`fVATSDamageToWeaponMult` in V.A.T.S.), the weapon an NPC drops when it breaks, and the health of the things in the lists of the
records (COED) and of the ones that are placed.
Not there yet: the picture of the thing (the games show its model) and mods (their button is hidden). The Repair button (key R) is
there (the repair screen below). A number key held shows the wheel of the hot keys, and a click puts the picked thing on the lit
slot (see the hot keys below).

### Repair: the screen of the Pip-Boy and the repair services (Mac rounds 12AW S12 and 12AX R1 and R2, FalloutNV.exe 0x7B5400 to 0x7B8B30)

The Repair button of the items page (key R, shown for a weapon or a piece of armor that is below full health and has something to
repair it with) opens `repair_menu.xml` (`falloutpipboyrepair.lua`, in the box of the Pip-Boy). The list has the copy first, with
the file's brackets round it, and under it each thing the player carries that can repair it: a thing the item's repair list
(`REPL`) names (the lists of the games hold the item itself, so another copy of it repairs it), and, for a weapon, any playable
weapon with the same skill and way of being held, and for armor any playable piece of the same weight class (the general flag 0x80
heavy, 0x08 medium, else light) and the same equipment type (`ETYP`: body or head wear), when the player has the perk of the entry
point 48 (Jury Rigging in the games' words; `FalloutNV.exe` 0x47BB50, Mac round 12AY R4); a quest item is not offered, and the copy that is repaired is not its own donor. The worn ones come first among equals,
the weakest first. The pointer on a thing shows the health the copy would have
(the cards of the right: the health bar, `+N%` and the gain of damage, threshold or resistance), the copy itself says to select
an item (`sSelectItemToRepair`). A click repairs: the player's Repair skill (actor value 39) gives the new health of the copy
as `max(A, B) + 10 * fRepairMin + 10 * (fRepairMax - fRepairMin) * skill / 100 + 0.05 * min(A, B)` percent, A and B being the
health of the two, capped at the full health; the thing is used up, the statistic "items repaired" (0x22) goes up, the sound is
`UIRepairWeapon`, and the screen returns to the items when no other thing is left or the copy has 99 percent. Keys: the arrows
and the pages pick a row, Enter repairs, E is Cancel.

The repair services (`falloutrepairservices.lua`, `repair_services_menu.xml`) are what a conversation opens with `ShowRepairMenu`
(the result script of a topic): the list is the weapons and armor the player carries that the character can mend. The character
repairs to `T = SkillBase + (SkillMax - SkillBase) * skill / 100` percent (the settings of the Repair Services; New Vegas
`SkillMax` is 10 in the plugin) at most; a row asks `ceil(CostMult * value * (T - health) / 10)` caps when that is above a half
(else it asks nothing and cannot be bought), and a copy that is already as good as the character can make it has the words of
`sCantRepairPastMax` on its card. A quest item, a thing the player cannot use and armor with neither a rating nor a threshold are
not listed; the rows that can be paid come first, the healthiest first, then the others, the weakest first. The button over the list
repairs all rows, one payment, and is on only while the player can pay the whole sum; a row that the player cannot pay is dimmed
and makes the sound of a refusal. When the screen is left the character says a line of the topic of subtype `RepairExit`. The skill is the character's own Repair, read
once. The engine's rule that the menu keeps the count of the list's children makes the file's expression for an empty list show
over rows (`_num_filtered` is set to the row count), so the script sets the words of an empty list itself.

The pad works through the Pip-Boy's own handler on the items page (the D-pad moves the row, the bumpers page, Y is Enter) and, for the repair services, through the screen's own. Not there: the pulse of the choose text (the executable's alpha animation is a guess); the brackets when the list is
scrolled are placed by the script from the row's place each frame (an approximation of the executable); the mods of the things that repair; the repair filter, the hover and the sort of Fallout 3 are the New Vegas ones (the formulas are the same in the
Fallout 3 settings, the rest was not compared); the skill of the vendor includes its buffs (a guess); Escape and Tab do what the
generic menu does.

### Item mods: the item mod screen (Mac rounds 12AX R3 and R3b, FalloutNV.exe 0x783310 to 0x784A40; New Vegas only)

The Mod button of the items page (key X, shown for a weapon of the player that has a mod to take or one installed) opens
`item_mod_menu.xml` (`falloutpipboymods.lua`, in the box of the Pip-Boy). A weapon record names up to three mods (`WMI1` to
`WMI3`) and what each does (the `DNAM` effect numbers and values); the list has the mods the copy holds first (dim, with the
file's marker) and then one row for each unit of those mods in the player's pack that goes into a free slot of the copy. A click
installs at once (there is no apply button on the PC): one copy of the mod item is used up, the statistic 0x21 ("weapons
modified") goes up, the sound is `UIItemGunsSmallUp` and the screen stays open. A weapon that is worn comes off while the screen
is open and goes on again when it closes (Cancel, key E). The pointer on a row changes the words under the list (the mod's
description) and the picture; the card of the weapon shows its health and the damage of a shot with its mods.

The effects are worked out when the weapon is used, from the numbers of the slots in the weapon record: effect 1 adds to the
damage (before the 1.3 of effect 12 when that is there), 2 the clip size, 3 takes from the spread (never below none), 4 changes the
weight, 10 the maximum condition (the health of the copy goes up with it), and the value of a weapon with mods adds the value of each mod item (rounded up to
a tenth). The names of weapons with mods get a `+` (`%s+`, `%s+ (%d)`). The copy is a whole of its own for the engine: two
copies with other mods are different rows and different stacks, in the items page, in barter, in the repair screens and in
the saves (the mods are kept with the condition in the key of the copy, so older saves read as copies with none). Fallout 3 has
no mods, so the screen and the Mod button are New Vegas only.

The pad works through the Pip-Boy's handler, as for the repair screen. Not there: the cards of the file that would compare a mod with the weapon (the executable never fills them); the rate of
fire (effect 8) and the effects 5, 6, 9, 11, 14 and 16 (they are in the records but their use was not read); the models of the
weapon per mask of mods (the engine shows the weapon's one model); the bump of the maximum condition for a copy that is whole
was read as one point less than the effect says, which is the executable's way but unverified on screen; two copies that a
merged stack of the executable counts as one are not told apart; the health that wear takes is counted on the base health of
the weapon, not the one with a mod of maximum condition; the things that repair a weapon do not carry their mods.

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
Throwing names `skills_survival.dds`), taken with its description and name from the same record. The sex, race and face screen
(`chargen/race_sex_menu.xml`) is drawn from its file too (see the section after the Vigor Tester), and a screen whose file
the install lacks keeps the stand-in widgets.

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

*The Vigor Tester and the SPECIAL book (Mac rounds 12AW S5 and S6 and 12AY R1, FalloutNV.exe 0x7915F0 and 0x7C5FC0).* When a
script asks for SPECIAL with `ShowLoveTesterMenu` (menu 1074; `VCG01VigorTesterSCRIPT`, 40 points, at Doc Mitchell's) or
`ShowSPECIALBookMenu` (menu 1060; Fallout 3's `CG01SpecialBookSCRIPT`, 40 points), the screen is not a menu file: the files
`love_tester_menu.xml` and `specialbookmenu.xml` hold a hot rectangle over the whole window (id 5, which takes every click) and a
few images the pad is tied to, and everything that is seen is a model that the executable turns page by page with its sequences:
`Meshes\architecture\Goodsprings\NV_VitoMaticVigorTester_Activate.NIF` with its `_Cabinet.NIF`, or `Meshes\Terminals\Babybook02.NIF`.
`falloutspecialscreen.lua` draws them on the scene of `core.menuScene` (`keepShapes`: the loader keeps the nodes of these files
apart, so that each button and digit can be named), with the camera `Surgery3DCamera` at (0, 0, 1) looking along -Z and a frustum
1.810 wide on its near plane (the tester) or 0.2925 (the book, a narrow lens) and as high as the window is; the tester's two
models are turned by the matrix of rows (-1, 0, 0), (0, -0.12533, 0.99212), (0, 0.99212, 0.12533) and put at (0, -85, -85) (-75 in a
window 640 wide or less), the book's at (0, 0, -60) at scale 0.05 with a quarter turn about X and the light of its file at
(-7.7, -139.9, 92.1). The rules are `components/falloutscript/specialscreen` (`core.specialScreen`, tested):

- The pages: 0 the cover (a button), 1 to 7 one attribute each, 8 the index of all seven with its Done button, and the book has a
  ninth with nothing on it. The tester opens on page 1 (its clip `Forward` plays and the page-turn sound comes with it; the cover is
  reached only by going back); the book opens on its cover. A turn plays the clip of the page it goes to (forward: Forward, Backward,
  Left, Right, FastForward, FastBackward, FastLeft, FastRight, and TurnLeft for the book's ninth; back: AimUp, Aim, Holster,
  JumpLand, JumpLoop, JumpStart, TurnRight, TurnLeft, and Attack2 and Attack1 first for the book) from its start, 1.3 s for the
  tester's Forward and 1.2667 for the others, 0.5 and 0.4667 for the book's. A page turns only when the clip has reached its end
  (the cover is always ready). The right trigger on the tester's last page is its exit.
- The points: the attributes come to the script's limit, each between 1 and 10; the screen writes the attribute of the player at
  once on each change (no cancel). A button to raise one gives the sound of a rise (`OBJBookSpecialNumber`) in the tester whenever
  the attribute is below 10, even with no point to give, and a key gives nothing then; the book sounds only when something
  changed, and with the same sound for lowering (the tester's is `OBJBookSpecialNumberDown`). Done is allowed when the total is
  the limit exactly (the tester then plays `OBJBookSpecialNumberFinished`, the book is silent); else `UIActivateNothing` and the
  screen stays. Nothing else closes it: Escape opens the pause menu over it, Tab does nothing, a click that misses does nothing.
- The pictures are textures put on named shapes: the points left as two digits (`Textures\terminals\BBNumber0..10.dds`, the first
  digit the tens, 9 at most, 0 when the points are over), the value of each attribute on the index and (book) on its page, the
  book's message by the points left (`english\BBMessage03` for more than one, `04` for one, `02` for none), and the lamps of the
  tester's cabinet (`P1_PointVal_NN_GLOW:4/:5` shown while the value reaches NN). The buttons to lower and raise show only while
  they can act. Keys: Left and Right turn the pages, Up and Down raise and lower (the tester also takes + = and - _), A leaves; a
  pad's right and left raise and lower, up and down move the index cursor, the shoulders turn pages, X leaves.
- A click casts a ray from the camera through the pointer (`core.menuScene.pick`) at the buttons of the page; the first it meets
  does what its button does.

Not verified in the running game (written from the executables only): the names of the shapes of the index (`Index_<Attribute>`
with the attribute in full is a guess), which shapes the book's messages and digits sit on, the light of the tester (the games' is
a point light at the scene's origin; a light from the camera stands for it), the book's turn about X, the faces of the buttons
(`BBRTOn`, `BBLTOff` and the others are left as the model has them), and the mode of the pad's index cursor (not wired). A game
that lacks the model files or the menu file keeps the stand-in of the points of SPECIAL.

### The sex, race and face screen (Mac rounds 12AW S7 and 12AY R2, FalloutNV.exe 0x7AC730; `falloutfacescreen.lua`)

`ShowRaceMenu` (menu 1036; `VCG01` stage 36 and `VCG04ActivatorScript` in New Vegas, `CG00` and `Vault101ExitScript` in Fallout 3)
opens the screen of the sex, race, hair and face; `ShowBarberMenu` (mode 2) and `ShowPlasticSurgeonMenu` (mode 3) open it
again for the hair and the face with the sex and race fixed (the mode is the one value of the screen's page, `page.values[1]`;
a new character has none). The executable edits the live record of the player in place and nothing is copied back at the end;
here `core.chargen` keeps what the player picked (`FalloutScripts::playerLook` is that, and else what the record of the player
says) and puts it in the save (the section `chargen`: the sex, the race and the look).

The file holds the panel (340 by 500 units, its title and rows above the `_top_bound` 80 and below the `_bot_bound` 336 are
hidden by the file's own `_position` traits), the Back and Next buttons, two scroll arrows and the two templates the code makes
the rows from: `RSM_list_item_template` (a row of 36 units: a name and a mark for the one that is picked) and
`RSM_slider_option_template` (64 units: a label, a bar between two arrows, a marker, the number beside the label). The rows
of the lists are children of the panel; the menu's own `user0` is the number of the list that shows, and each row's `user0` is
the list it is in. The lists (twenty, in the executable's order): 0 sex (two rows), 1 race (a row for each playable race),
2 face (the preset slider, the link `Customize`, the row `Randomize`, the age slider), 3 hair (links: hair style, hair colour,
facial hair for a man), 4 customize (links: shape, tone, eye colour), 5 hair style (a row for each hair of the race and sex),
6 hair colour (a preset slider 0 to 15, whose colours are the settings `iHairColor00` to `iHairColor15` and else the
executable's own, and R, G and B sliders 0 to 255), 7 facial hair, 8 eye colour, 9 shape (nine links),
10 to 18 the shape sliders (general, forehead, brow, eyes, nose, mouth, cheeks, jaw, chin) and 19 the tone sliders. A link row
is its list's name followed by ` >`. The first four lists are the tabs (`1. Sex`, `2. Race`, `3. Face`, `4. Hair`, numbered only
for a new character); Next goes to the next tab and is `Done` on the last, which asks `sRSMConfirmDone` followed by the word for what is being done (`sRSMCharacter` for a new
character, `sRSMHair` at the barber, `sRSMFace` at the surgeon: by the mode, not the tab) and a question mark in a box that
lists No first and Yes second; Back and Escape go up a list, and Shift with Left or Right changes the tab. A face slider shows the control's value times ten, cut towards zero (-35 to 35, the INI value of both games, 3.5; the
controls 21 and 23 of `facegen\si.ctl` run -20 to 20, the controls 49 and 53 -35 to 30) and writes nothing beside its
label; moving the slider by its arrows is one step, by the halves of its bar a jump of a quarter of its range (an eighth for
the colours). The sliders are the controls of `facegen\si.ctl` (read by `readFaceControlFile`, which takes only the exact layout
of the games' file: 56 shape, 26 asymmetric and 33 tone controls and the fan controls): a slider's value is the dot product of
its direction with the numbers of the whole face (the race's for the sex plus the character's own, which are what the record
keeps), and moving it adds the difference along the direction (`FaceControls`; the sum is kept as a float after every term, as
the executable does). The directions are not at right angles to each other, so the other sliders move with it; the slider that
was moved keeps its number and the others read the face again (the executable leaves the active slider out, so it never shows
a step nearer zero). The age slider (1 to 10) stands for 15, 20, 26, 31, 37, 42, 48, 53, 59 and 64 years and moves the face along
the two age directions of the file (`FaceAge`): the age in years for the shape, and for the tone the same shift, so that the tone
stays as much older or younger than the shape as it was; the second number of the age directions only the random face redraws.
The age slider and the preset slider both open at 1 whatever the face is. Randomize (after the box `sRSMConfirmRandomize`, with
No first and Yes second; a face that Randomize or the age slider just made is not asked about again, until a sex, race, hair,
eyes, facial hair or preset click) gives each shape and tone coefficient a normal number times `fNPCGeneticVariation` (0.5) times
1.5 and zeroes the asymmetric ones, then settles the fan controls (`FaceAge::settleRandom`: the shape keeps the age the draw gave
it, the tone stays as much older or younger as before, the second number is drawn again between -2 and 2); the hair, eyes and
colour stay. A preset is a character of the data of the same sex and race (`Is CharGen Face Preset`, or all of the
race when none has it), by name; choosing one puts its hair, eyes, colour, head parts and face on. A click on the sex or race
takes the head parts off, puts the first preset on and changes a hair or eyes the new kind does not have.

The file puts the panel where the executable's device has its screen (x 930, y 550 of a texture larger than the window); here the
panel is at the right edge of the window and in its middle (the window is 960 units high). Rows are made again whenever a list
is shown (a click on a link waits for the next frame, the row it is on is in the click's way) and their values are written to
the existing tiles after every change, so a drag of the marker (the file's `draggable` is set on it) keeps its tile.

The head the player turns is a scene of the menus (`core.menuScene`, the `FacePreview` of `mwrender`): the character is made
from the look that is picked (`core.menuScene.updateHead`, again a short time after each change), the camera's frustum is cut so
that the head stands in the middle of the file's hotrect `RSM_Face_Grab` (an off-centre frustum: the middle of the picture is
not the camera's axis), a drag over the hotrect turns the head about its vertical axis and the wheel moves the camera nearer
or farther (`setHeadView`); what the executable has for that camera (Mac round 12BF B2) is a half angle of 11.25 degrees (0.15 of the 75
degrees of `fDefaultFOV`), a near plane of 5, a drag of one whole turn over the 680 units of the hotrect (`2 pi / 680` radians per unit, no
limit, the angle wraps) and a wheel notch of an eighth of the zoom (eight notches from end to end; the start is at 0.7 of the way); the
screen's script has the angle, the plane, the turn and the notch now and keeps its own start; the camera is 55 units from the pivot that the head turns about at the nearest and 135 at the farthest (with
the player's origin 5 units off the pivot), and it aims at 0.84 and 0.80 of the height of the player (110.9 and 105.6 units, the head bone is at
111) instead of 6.5 above the bone, but the picture of the executable
is laid on the screen differently from this one, so the port keeps its own 70 to 170 and the aim it was fitted with. The barber and the plastic surgeon have the one tab they start at (the hair, the face), where Next
reads `Done`. The head was seen in the running game in Mac round 12BC: it is made again 0.17 seconds after each change, a drag
of 200 pixels turned it by a quarter of a turn, and a wheel event is one step of the zoom. That round showed the first view
too close (the camera aimed at the bone `Bip01 Head`, the base of the skull, so the top of the hair was cut by the window):
the head now starts thirty degrees short of facing the viewer (the executable's start, Mac round 12BB D and E), turns all the way
round, and is seen from 120 units (the zoom runs from 70 to 170). Round 12BD found three things: the head started seen from the
back (the camera at yaw 0 looks at the head's right side and the face is seen a quarter of a turn before that, at -pi/2, not
after it; a drag to the right now turns the face towards the panel, one full turn was about 650 pixels at 1080 lines, and is now the executable's 680 units, 765 pixels there), the head
sat 39 pixels below the middle of the file's `RSM_Face_Grab` (the camera now aims 6.5 units above the bone, from 8) and at the
nearest zoom it was taller than that hotrect (800 pixels against 698; the nearest is now 70 units). The tone sliders
move the colour of the face with the texture morph of the head's `.egt` file (`FREGT003`: 50 modes of a scale and three 256 by 256
pictures of signed bytes; Mac round 12BE B3): the base mod is the sum of coefficient times scale times byte, taken in whole
numbers as `BSFaceGenModel` at 0x65B410 takes it (the coefficient and the scale times 256 cut towards zero, the sum divided by
65536, over the first 30 modes at most: the setting `uiEGTClampSize`, which no INI file has), which is how many of the 256 steps of each colour the face moves there; the mod keeps half of it above mid grey (the
executable hands the sum to 0x64CEB0 with the range -255 to 255 and the factor 0.5) and the skin shader doubles it, so it goes
where the editor's `facemods` texture goes, to `tintSkinImage`. It is made only for a face that has no `facemods` texture of
the editor (any character whose files are not in the archives, and always the player, record 7, whose face the game makes
itself, and the head the face screen shows, which is a character of the data with the numbers of the player's look on top). The
rows of the pictures are turned over by the port (`sToneRowsFlipped` in `falloutface.cpp`): the executable does not reorder them (the rows
of the file go to the rows of its texture as they are, top row first, as Direct3D has them; Mac round 12BF B3), and an image of OSG starts at
its bottom row. The need for it was found in the running game: the log says, for the first six
characters that have a texture of the editor, how far the picture made from their coefficients is from it, with the rows as
they are and turned over (`FaceGen tone check`), and for six characters the mean difference was 1.3 to 1.8 steps turned over, 3.7 to 5.5
as they are and 7 to 40 with no coefficients; with the rows as they are the head of the face screen showed a smear across the nose and jaw at every
slider position, and with them turned over it did not. Moving the twelve tone sliders from one end to the other takes the head from a dark
red-brown through natural skin to a pale grey-white. The skin of the body, the hands and the skin that shows from clothes has
a tone the same way (Mac round 12BG B1: the games build one texture for the three from the `.egt` file of the body's model, a file of 32 by 32
pictures that are one flat colour for a human or a child and vary for a ghoul or an old person, with the coefficients of FGTS and the
formula of the head, and take a `bodymods` texture of the editor, which is one flat colour of 8 by 8, when the archives hold one for the
character; the games' skin shaders for the head, the hands and the body are the same, so the tone is added to the skin picture alike):
a character with no `bodymods` texture, and the player, gets it made from the `.egt` of its body model (the human body of its sex when that
has none), the hands take its average. Not read in the executable: which sampler the tone texture lands on in the shader. The face
screen has no hair length or head size slider in the games' executable (Mac round 12BG C1: the sliders are the shape and tone
coefficients, and the hair length of a character is a morph of its hair model that no menu moves), so there is none here.

The rows of the hair, eyes and facial hair lists follow the executable's own lists of the records, which put every new record
at the head: the newest first (a plugin after the master puts its hairs and eyes before the master's), not sorted and not in
the race's order. A hair is a row when it is playable, the race lists it and the sex can wear it (the second flag bit must be
clear for a man, the third for a woman); eyes the same with the race's eyes; the facial hair rows (a man only) are every playable
head part, whatever the race. The lit facial hair row is the character's first part in its list, which is the last the record
names; a click on it takes it off and a click on another takes all the character's parts away (the eyebrows too) and puts that one
on. The presets are sorted by name byte by byte (capitals first). A hair that the new sex or race cannot have is replaced by the
race's own for the sex, and only when the race has none by the first of its hairs that fits (Mac round 12BB C1 and C2).

The glyphs on the ends of a slider's bar are `<` and `>` (the menu's `user1` and `user2`, read from the executable in Mac round
12BB F). Escape opens the pause menu over the screen, as the executable does (Mac round 12BB F; the screen says every frame that
it keeps Escape, `core.chargen.claimEscape`, and the window manager pushes the main menu over the mode of the interface instead
of leaving it; the pad's B and the Back button go up one list). The pad (`falloutfacescreen.lua`, `module.pad`): the D-pad moves the lit row up and down and a slider left and right, the bumpers change the list (they stand for Shift and the arrows), Y is Enter; A is the engine's click where its pointer is, and B goes up one list. The jump of a bar by the triggers is not wired. Not done: the Pip-Boy still has no pause
menu over it (Escape closes it), the hair's length (a vertex morph of the hair model by the record's length) and the head's size
by the executable's formulas (Mac round 12BB D and E), and the real-game check of the rows' order (it is read from the
executable's loading code).

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

### Message boxes

`falloutmessage.lua` draws the message boxes of the scripts (a `ShowMessage` with buttons, and the box that names a
reputation) on `message_menu.xml`. The engine keeps the queue and the answer (`core.messageBox`: `current()` gives the
box that is open with its `title`, `body`, `buttons`, `timed` and `time`; `press(n)` is the button picked; `claim(true)`,
said every frame, tells the engine that the script draws the boxes, so that it makes no window of its own, and
`claim(false)` gives them back, which the script does when the file is not in the install). Read from `FalloutNV.exe`
(`MessageMenu`, round 12Q):

- The file's six tiles are found by their numbers (the box 0, title 1, icon 2, words 3, the list of buttons 4, the
  background 5). The code sets: the box's width, the title's words and whether it is shown (not when the message has no
  name), the words, the icon (shown only when the message has a picture, 100 by 100; the words move over by the file's 120),
  the list's width and height, and the justify of the title and the words.
- The width of the box is the widest of three: the title's words in font 6 plus 100, the wrapped words in font 2 plus twice
  `_horbuf` (40), and the widest button plus 80; never more than `_MaxMenuWidth` (700) and then never less than
  `_MinMenuWidth` (200). The words wrap at `_MaxMenuWidth` less twice `_horbuf` (and less 120 with an icon), as the file says.
  The text of a text tile reports at least the room it is wrapped in, so the width of the longest line is measured with
  `menu:measure(font, text, wrap)`.
- The title and the words are both centred (justify 2) when the words are one line and both to the left (1) when they are
  more. A box with no name has no title, and its title tile (not shown) is still one line high, because the executable lays an empty
  text out as one space (Mac round 12BF B1: the code at 0xA12FB0 swaps it, and the layout runs whether the tile is shown or
  not; the message menu only hides the title without setting a string), so the words start as far down as in a box with a title
  (`MM_MessageText` is at the title's y, plus its height, plus `_verbuf` (20), less 5). The same goes for every text tile of
  every menu file: with no characters it is one line high and one space wide (the port's line is the file's line height, a
  few units less than the executable's, 31 against 32 in font 2 and 34 against 38 in font 6, for lines with characters too).
- The buttons are the rows of `MM_ButtonTemplate`, in the order of the record, one click id for all (7); the list is as
  high as the rows are together, up to the file's 420, and scrolls beyond that. The file's own scrollbar rule counts every
  row as one step of 26 units, which shows the bar for a box that fits, so the script says when it is shown.
- A message with the box flag and no button has one button, "OK" (the literal of the executable, in capitals, not the
  word of the setting `sOk`; the engine adds it), and stays until it is picked: the display time of a message (`TNAM`) is
  the one of the notification in the corner of the screen (a message without the box flag), and the box of `ShowMessage`
  is made with the time 0, which closes nothing (round 12AE). A button whose conditions fail is not there at all in the
  game (`ShowMessage`, 0x5b4630, gives it a one-space text, the box's creator leaves that out of the words and the button
  builder makes no button for an empty slot: it is not shown, cannot be clicked or reached by keys; the buttons after it
  keep their original numbers, so the answer a script reads has gaps, as the port's does); if every button fails, the
  first one becomes the literal "OK". Boxes queue one after the other and a new one does not replace the open one.
- Keys, as the code of the box reads them (FalloutNV.exe, round 12AG: `MessageMenu` handler 0x7aa110, creator 0x7a8e60):
  the box's own key code is only this: a letter picks a button, and the key the OS reports as the menu's accept (value 1,
  the control 0x1c, not named) picks the LAST button except in a box of `ShowMessage` (kind 0x17), which ignores it, as it
  ignores Escape (Escape gives value 0). The code takes the one character after the first `&` of each button's words,
  appends it to one string of letters (a button with no `&` adds none) and erases the `&` from the shown words; a typed
  letter is upper-cased and the place it has in that string is the NUMBER OF THE BUTTON it picks, so a button without a
  `&` ahead of one that has it moves the letters up by one button (kept as it is, `hotkeysOf` in `falloutmessage.lua`). No button
  of either game's plugins has an `&` (none in 1567 New Vegas and 672 Fallout 3 messages, round 12AH), so the letters are for
  mods only.
  Space is taken by the box and does nothing there (`HandleClick(-1, ...)` is a no-op). Enter, the arrows and the pad do
  not come from the box: they go to the interface manager's generic code (round 12AH A4, `0x70f6e0`). No button is lit when
  the box opens, and the file has no `xdefault`, `xup` or the like, so the arrow keys do nothing at all in the game's box; Enter
  clicks the selected tile or, with none selected, the one under the pointer, and with neither nothing happens (the root is no
  target). The script keeps the arrows as its own addition (they light a button, the pad's D-pad uses them), and Enter clicks
  the button that is lit (a hover lights it) and does nothing otherwise; Space does nothing. The pad: the engine's
  pad handling steps aside while a box that the script draws is open, so the script takes the pad itself: D-pad up and
  down move the lit button, A picks it, B does nothing (as Escape; not seen in the running game yet).
  Fallout 3's `ShowMessage` (0x53dd40) and message menu (key handler 0x6694d0, creator 0x6698e0) are the same code with
  other addresses (round 12AG, A2): the same flag bit, 10 slots, one-space blank for a failed button, 'OK' when none passes,
  kind 0x17, the same three key rules and `&` handling; the only difference is five button tiles instead of six.
- The background of the box is the file's `solid_black.dds` tile with the alpha `_background_fill_alpha` (32 in `globals.xml`),
  and when a menu is already open under the box (the Pip-Boy, the dialogue, a container, barter...) the code writes its
  alpha as the setting `fPopUpBackgroundOpacity` times 255 (the INI of both games has 0.87, the executable's own default is
  1.0; FalloutNV.exe 0x7a8d10, round 12AG); the brightness is never written. The script does the same over an open screen,
  with 0.87, since the port reads no INI. Seen on the Mac (round 12AH): over the Pip-Boy the page text shows through at 13 to 17 %
  of its brightness, and over the plain world the box is about 10 % darker (the file's alpha of 32).
- The box is modal, as the games' (`stackingtype` is `no_click_past`): a widget the size of the screen under it takes the
  pointer, and the Pip-Boy, level-up, dialogue, container, crafting, VATS, lock and character-making screens leave the
  keys, the pad and the wheel to the box while it is up (`I.FalloutMessage.isOpen()`, true for a box in the engine's own
  window too, which takes only the arrows, Enter and Space). The engine's own actions for the keys
  and buttons (Escape, which would close the screen under the box, the inventory key) are not carried out either while a
  box that the script draws is open (`FalloutScripts::luaMessageOpen`); the script still gets the keys. A question that a
  screen asks (`I.FalloutMessage.ask`, as barter does when it is left with things in its piles) has no box of the engine, so
  the script says it to the engine every frame it is open (`core.messageBox.setQuestion`; a second without it and it is
  gone), and the engine counts it as a box that the script draws: Escape and B do not close the screen under it.
- A box is drawn only when it is the script's (`current().lua`): a box that opened before the script said it draws them, or
  after its claim ran out, has the window manager's own window and is left to it.

Not done: the icon (the records that hold the pictures, `MICN`, are not in the engine's store), the message's own sound,
and everything in this section is checked in the offline harness only (not in the running game). The sounds of the box
that are done: `UIPopUpMessageWindow` when it opens, `UIMenuOK` when a button is clicked with the pointer, Enter or a
letter, and `UIMenuFocus` when an arrow key lights another button. The text under `onlyif` of the file counts a
text with characters as true (`onlyif` on the words' `string` keeps the buttons below the words).

### Terminals and the game of hacking

`falloutterminal.lua` draws the menu of a terminal on `computers_menu.xml` and `falloutterminalhack.lua` draws the game of
hacking on `hacking_menu.xml`; `falloutinteraction.lua` owns the screen and keeps its widgets as the stand-in for an install
that lacks either file. What a terminal holds, what a line does and every rule of the game of hacking are the engine's
(`core.interact`, `components/falloutscript/hacking.*`, `FalloutScripts::openTerminal` and the calls after it). Read from
`FalloutNV.exe` in rounds 12R, 12S, 12T and 12U (Fallout 3 only the constants: the same strings, buffer, rows and states).

The menu of a terminal:

- A terminal whose record is not marked as unlocked (the TERM flag 0x02) types an introduction at every visit, also after a
  hack: four lines (the setting `sHackingIntro01`, `> ` and `sComputersLogon`, `sHackingHeader2`, `> ` and as many stars as
  the letters of a word of its game of hacking), at the rate of `iHackingInputRate`, then two seconds, then the menu; a click
  on the picture brings the menu at once. The reference override of that flag (a byte of the reference's extra data) is not
  read: the record's flag decides.
- The menu types the two header lines, the terminal's name, the words of the record (wrapped at 600) with the rule under
  them, and the lines (`> ` and the words, in the order of the record; a line whose conditions fail is not there) at
  `iComputersDisplayRateMenus` characters a second. A `> Back` line, the last, is there only below the first menu. A menu below
  the first is rebuilt in place: the header stays, the words and the lines are typed again.
- A line with result words puts them in the line under the menu (`> ` and the words, typed) for
  `iComputersResultDisplayTimeout` seconds; the list stays. A line with a note of text (and the note not yet in the
  inventory, for its add flag) shows the note in the display zone, a page of the zone's `wraplines` (16) at a time at
  `iComputersDisplayRateNotes`; a click on the zone, or the key that cancels, turns the page and the last one closes the note.
  A note that is a sound, an image or a voice has its own screens in the game and is only added here.
- The only key the game's menu knows is the one that cancels (its code 10): it turns or closes a note, goes back a menu, or
  leaves from the first. Here Backspace does that; Escape closes the whole screen (the engine's own key), and Up, Down and
  Enter are the port's, the game picks by the pointer.

The game of hacking (`Hacking::Puzzle`):

- A closed terminal (not unlocked, no password note carried) starts it; in New Vegas only when the Science of the player is
  at least the `fHackingMinSkill*` setting of the level (0, 25, 50, 75, 100; Fallout3.exe has no such setting, so Fallout 3
  and Tale of Two Wastelands ask nothing): below it the player is told by `sHackIneligible` (as a line of the HUD here; the
  game uses a pop-up with a picture) and no screen opens. A terminal of difficulty 5 asks for a key and a
  terminal that locked the player out before shows the locked message (`sHackingLockout3` and `4`) until the player leaves.
- The dump is 408 characters in 34 rows of 12, two columns of 17, each row with its address (`0x` and four digits; the first is
  random and each row is 12 more). Each character is one of the 30 symbols; the words (the dictionary file, only the length of
  the terminal's level: 4 and 2 for each level, 1 more when the terminal's base form id is odd, never more than 12) are laid
  in with two non-letter characters on each side. How many words: `iHackingMinWords` to `iHackingMaxWords` (5 to 20), more for
  a lower Science (Science and the perk entry point for hacking, against the level's setting). The picker keeps words that have
  something in common, and the attempts are 4 or what the worst case of the best guesser needs.
- A word that is chosen costs an attempt unless it is the password; the line of the log says how many letters are where they
  are in the password (`>3/7 correct.`); a word can be chosen again and costs again. A pair of brackets of one kind in the
  same row with no letter between them takes a word out (a dud) or, once per board with a chance of one in four, gives the
  attempts back; it is spent after that, and with one word left it does nothing. A character that is no part of a word or a
  pair is a wrong guess with 0 in common.
- The last attempt locks the terminal: the screen slides up and closes by itself, the terminal stays locked until a script or
  the console opens it (once more for a player with the perk entry point 20) and the next board waits
  `iHackingRetryMilliseconds` from the closing of the screen. The counters of the lock-outs are not saved yet.
- A right guess unlocks the terminal for good, gives the experience of `iXPRewardHackComputer*` of the level, and hands over to
  the menu of the terminal in the same visit.

The screen: the pointer on a word, a pair or a character lights it (its text in inverse video, over two rows when a word is
broken at the end of a row) and types it after the prompt of the log; a click chooses it. The log keeps every line (the
older ones scroll up and off by the file's own rule). The header line with the attempts has a block for each; the warning
line (`sHackingWarning`) replaces the second header line when one is left.

Not done: the sounds (the humming loop, a key sound for each typed character, the right and wrong password sounds: the names
are known and the screen plays none), the keyboard and the pad on the dump (the game moves a cursor of one character with the
arrows, a word is one step), the position of the whole screen (the file centres nothing; the depth rect is put in the middle of
the window here), the pause after a right guess (half a second here, the game waits 250 ms and then fades), and everything is
checked in the offline harness only, not in the running game.

### The pause menu

`falloutstart.lua` (a script of the menu context, so it runs while the engine's menu is open, which the scripts of the
player do not) draws the menu that opens with Escape in a running game on `options/start_menu.xml`. The game's code
(class `StartMenu`, menu id 0x3F5, `FalloutNV.exe` round 12V) does not use a file for each of the title screen, the pause
menu and the lists of saves: they are one file, and the code shows a part of it by fading its containers in and out (it
sets their `_alpha`, and the file shows a tile when that is more than 0), so the script sets `_alpha` and `_enabled` of
`main_container` (the list), `saveload_container` (the saves, with the panel of the one that is lit above it),
`confirm_container` (a question and its choices) and `main_back_button`.

- The list: Continue (there is no Resume item: Continue and Escape do the same), Save (only when the game can be saved:
  `menu.canSave()`), Load (only when there is a save), Settings, Quit (which asks). The game has a Help item and, in the
  pause menu, an item for the title screen that are not here, and the title screen itself is still the engine's: no content
  is loaded there, so the words of the game (game settings) are not there either.
- The list of saves: the newest first; the row for a new save when saving (`sNewSave n/1000 sSavesRemaining`, from the
  template `lb_newsave_template`, so a row of a list can name its template: `_template`), the rows of the saves
  (`lb_saveload_template`: `#0003` and the place, or the abbreviation of the quick or the automatic save). The panel
  shows the name, the level and the time played of the save that is lit. Writing over a save and loading one ask first.
  With a pad connected the menu opens as the file's `&xbox;` layout (the list 20 units further in, the buttons with their
  glyphs), and then the file shows the Delete button (`main_defaults_button`, the Y button; its `_target` is the code's
  "a save is lit", and with a PC layout the file never shows it, because its `_line_alpha` and `target` both ask for
  `io()` user5, "using a 360 controller"): it asks `sConfirmDelete` and takes the save away on Yes (Mac round 12BH part 8
  names the question and the three states of the game's confirm box; round 12BI part D corrects it: 0x200 is load, 0x400
  save, 0x800 delete, 0x100 Continue and 0x80 new game). A Yes on delete removes the `.fos` of the save and nothing else
  (no picture file: the picture is inside the save), and there is no key that deletes. The game orders the list, and
  decides which save Continue loads, by the last write time of the file (the newest first), as the port does by the time
  the save was made. The title screen, which is the engine's own here, has the rules the file's list does not: its Continue
  and Load rows exist only when a save does, and Continue there asks `sConfirmContinue` first (the pause menu's Continue
  asks nothing). The save's picture and its version line are not here.
- The engine's side: `menu.mainMenuOpen`, `menu.claimMainMenu(drawn, escape)`, `menu.resume`, `menu.canSave`,
  `menu.openSettings`, `menu.settingsOpen`, `menu.inputTaken` (`openfallout.menu`). While the console, a
  message box or a dialog of the engine's own windows (the one that tells that a save failed, which is modal and has the
  buttons of the pad first) has the input, the script leaves the keys and the buttons of the pad alone: the keys typed in
  the console reach the menu's scripts as well, and so does the key that closes it, so the engine takes who had the key or
  the button when it came, before it acts on it (`inputTaken` in the handler). The script says every frame that it draws
  the menu; while it does the engine's own buttons are not shown, and a script that fails (or an install without the file) gives the menu
  back within a second. In a screen below the list Escape and the B button are the script's (they go back one step); in the
  list the engine closes the menu with them. Settings opens the game's own settings screens (below).
- Where the question's container is: the file says its `x`, `y` and `width` are "set in code", but round 12AC found no code
  in `StartMenu` that sets a position or a size for the container or for the panel of the lit save (only the strings, the
  fades, the user traits, and one `y` shift of a tile on a wide screen), so the port puts the container at the list's side, in
  the middle of the window's height (an estimate, as the question's own tile sits 20 units to the left of it, right-aligned).
  The panel of the lit save goes with the list of saves (it is hidden while the question is shown: the first run in the
  game, round 12AB, drew the question over its play-time line). Not read yet, so estimated: which tile is lit first (the
  first row), and the format of the time
  played (days, hours and minutes as `000.05.33`). The darkening of the paused world is the port's (the game draws its own
  picture there). The sounds are the file's: a tile's `clicksound` and `mouseoversound` (see "Sounds of the menu files"), played
by the menu module; the game's own sounds for the back step were not found. Nothing was seen in the running game.

### The settings screens

Settings in the pause menu shows `settings_container` (the four submenus, as the game's list does) and, for the one that is
picked, `options_container` (nine rows at a time, scrolled) with a toggle or a meter for each item, from the templates
`lb_toggle_template` and `lb_meter_template` of the file (a row names its template with `_template`; the code of the game
gives them their value: `user0` is the word of a toggle or the position of a meter, `user3` the number of positions, and
`user2` which of the two arrows can be used; the centre of the values is `user1` to `user4` of the menu, one for each
submenu). `falloutsettingsitems.lua` holds the items. What the code of the game builds, read in rounds 12W, 12Y and 12Z
(`FalloutNV.exe`): the items of each submenu in order, the kind (a toggle with 2 to 5 words, a meter of 16 positions, 26 for
the volumes), the words of a toggle (built from the last to the first, so index 0 is the last word built), what each index
stores, and the start of each item (the compiled default of its INI setting). A change is made the moment a row changes
(the game's handlers apply when their flag is set, which they are when a row changes), so the engine takes it at once
(`menu.setOption`, which tells the parts of the engine that read settings) and the settings are written to the user's file
once, when the screens are left (`menu.saveOptions`).

- Gameplay: Kill Cam (None, Player, Cinematic), Hardcore, Difficulty (Very Easy to Very Hard, normal is the third),
  Save on Rest, Save on Wait, Save on Travel, True Iron Sights. Display: Brightness, HUD Opacity, HUD Color and Pip-Boy
  Color (Green, Blue, Amber, White), Texture Size (Large, Medium, Small), the nine fades (actor, item, object, grass,
  shadow, light, specularity, object LOD, tree LOD), Crosshair, Dialog and General Subtitles. Audio: master, music,
  footsteps, voice, effects and radio volume. Controls: Camera Pitch, Mouse Sensitivity, and the two items of the pad
  (Controller, Rumble) only while a pad is plugged in (`menu.controllerConnected()`, as the game leaves them out).
- Which of them the engine takes: the five volumes (the sound settings), Brightness (the video's gamma: the game's meter
  is a straight line from `fGammaMin` 1.4 at the first position to `fGammaMax` 0.6 at the last, and the engine's gamma is
  the inverse of the game's), Mouse Sensitivity (the camera's sensitivity: the game's meter goes from 0.0005 to 0.0100 and
  starts at 0.002, the engine's value is the game's over what the game starts from, so 1 is where the game starts),
  Camera Pitch (the invert-Y setting), Crosshair, and Dialog Subtitles (the subtitles setting; the engine has one setting
  for both kinds). The other items are kept (in the permanent storage, `FalloutOptions`, so the screens show what was
  picked) and nothing in the engine reads them yet: Kill Cam, Hardcore, Difficulty, the three saves, True Iron Sights, HUD
  Opacity, the two colours, Texture Size, the nine fades, General Subtitles, Radio Volume, and the two items of the pad.
- The list of submenus ends with a row of the port's own, "Engine", that opens the engine's window of settings (the size
  and the mode of the window, the shaders, the mods): the game's screens do not have those. The Defaults button (it resets
  the bindings of the controls and does nothing in the other submenus, round 12W) is not shown, because the rows of the
  controls (rebinding) are not drawn yet. The rows that the game fills with the key or the button bound to a control, and
  the rebinding itself, are not here.
- Not read, so estimated: which row is lit first (the first), what a click on the value of a toggle does (it goes to the
  next word and, after the last, to the first; the arrows stop at the ends), where on a meter a click sets it (the
  position under the pointer), the pad's LB and RB (a fifth of a meter), whether the Hardcore row can be changed in a
  running game (the game disables it under conditions that were not decoded), and what the Fallout 3 screens differ in
  (not checked; the same file is drawn). Nothing was seen in the running game.

### Sounds of the menu files (round 12AH A5)

The games play a sound where a tile names one: the `clicksound` of a tile that is clicked (with the pointer, or by a key that
clicks it) and the `mouseoversound` of a tile the pointer or a key comes to. The words are the editor ids of sound records
(`UIMenuOK`, `UIMenuFocus`; the executable's own sounds like `ITMBottlecapsUp` and `UIPopUpMessageWindow` go the same way),
looked up once among the sound records (`FalloutScripts::soundByEditorId`) and played in 2D (`core.pipboy.playSoundNamed`,
the first file when the record names a folder). `falloutmenu.lua` plays them for every menu drawn from a file: on a click the
tile that was hit or the first above it that names one, on the pointer's move to a tile (once for the tile that owns the
sound, not for each box inside it). The list rows (`list_box_template.xml`) have `UIMenuOK` and `UIMenuFocus`, so every list
of every screen makes them; `text_box.xml` has `UIMenuFocus`; the quantity menu has the focus sound only (its clicks are silent).
A message box plays `UIPopUpMessageWindow` when it opens (the sound of the message when it has one is not read). Where a key
lights or clicks a row the screen plays the names itself (barter and the message box do; the other lists are silent for keys
so far). The Pip-Boy's own sounds (`UIPipBoyAccessUp`,
`UIPipBoyAccessDown`, the hum `UIPipBoyHumLP`, the light) and the container's (`UIItemTakeAll`) are not played yet. The sound
files were not heard in the running game.

### Keys of the Pip-Boy (round 12AH A3, FalloutNV.exe; Fallout 3's control table and defaults only)

- The key that opens and closes it is the control "Menu Mode" (Tab), the only one: a release before `fPlayerPipBoyLightTimer`
  has run out opens it, holding it longer toggles the Pip-Boy's light instead (sounds `UIPipBoyLightOn` and `UIPipBoyLightOff`);
  in the open Pip-Boy a Tab release closes it, unless a text field has the input. No control exists for the previous or next
  page. F1, F2 and F3 (raw keys) go to Stats, Inventory and Data: closed they open the Pip-Boy on that page, open they switch
  to it directly, and the key of the page that is up closes it. Escape does not close the Pip-Boy: it puts the pause menu on
  top (the Pip-Boy stays under it), and with the pause menu open it closes only that.
- The port's own differences: the engine's Tab binding opens and closes it at once (no light), Left and Right change the page, and
  Escape closes the screen (the game puts the pause menu over it; seen in play, Mac round 12AZ B6). F1, F2 and F3 are the game's:
  they open the Pip-Boy on Stats, Items and Data, switch to the page when it is open and shut it from the page that is up (the
  engine's own F1 and F2, the quick keys and the post-processing, are no longer bound by default: F2 showed a message box about the
  post-processor in play, Mac round 12AZ B). The 28 numbered controls and their defaults are in `mac-12ah-results.md` (Part A3).

### Trading (barter)

`barter_menu.xml` is drawn by `falloutbarter.lua`; the engine side is `core.pipboy.barter*` (`apps/openfallout/mwscript/falloutbarter.cpp`)
and the prices and checks are `FalloutScript::Barter` (`components/falloutscript/barter.*`), all as the Mac read them from
`FalloutNV.exe` in round 12AG (menu class `BarterMenu`, id 0x41d, `Create` 0x72d250, the deal 0x72fd10, the checks of the Accept
button 0x72f6f0, the price of one 0x72ed00).

- The only way in is the script command `ShowBarterMenu` (0x1173), which the response of a dialogue line calls (New Vegas: 90 lines in
  58 topics; Fallout 3: 157 in 41). The reference it is called on is the vendor; its optional integer is a percent from -100 to
  100. Nothing about the character's services (the AIDT bits) decides whether the line is there. The dialogue stays
  open under the menu and is back when the menu closes (inferred; the dialogue's code was not read).
- What the vendor sells is its own goods and those of its merchant container together, the counts added by form
  (`FalloutNV.exe` 0x46f310, round 12AH A1). The container is not read from the faction (the factions of these games have no
  vendor fields: no VEND, VENC or VENV in any plugin of either game): it is an extra data item of the vendor's reference
  (type 0x3c, `ExtraMerchantContainer`), which the placed reference's `XMRC` fills (86 records in New Vegas, 51 in Fallout 3,
  which the ESM4 loader reads now) and which the script commands `SetMerchantContainer` and `RemoveMerchantContainer` (0x11b7,
  0x11b8) give and take away (a section `merchants` of the save keeps them). A vendor with no container sells what it carries.
  The port leaves out what the vendor wears (the executable's loop over worn pieces at 0x46f4ad was not followed). The vendor's
  caps are the caps of the container and of the vendor together; they are never made up, and a vendor that has none pays
  nothing. Caps are not listed (they are the money), and neither is what is worth nothing. The player's side leaves out keys and
  the armor the files keep from the player, as the Pip-Boy does. A buy is taken from the container first, then from the
  vendor; what the player sells and pays goes to the container (the vendor without one).
- The price of one is `value * Base + Mult * (1 - Base) * value * c^Exp` with the settings `fItemConditionValueBase`, `Mult` and
  `Exp` (0, 0.0316 and 1.5) and c the condition over ten: 10 for a thing at full health (and for everything that has no
  condition), which makes the base value the value, less for a worn weapon or piece of armor (the rows of the lists are per
  condition, each with the price of its own). Then the factor of the side: `fBarterBuyBase + skill * fBarterBuyMult / 100` for what the player buys
  (1.55 at skill 0, 1.10 at 100) and `fBarterSellBase + skill * fBarterSellMult / 100` for what it sells (0.45 to 0.90), from
  the Barter skill read once when the menu opens. Then the percent of the script (a positive one is less to pay and more for
  what is sold), the perk entry point 0x11 (`EntryPoint::buyPrice`, with the vendor and the item as its context), and the
  rounding: above 1 up to a whole number, else up in tenths. What the player buys costs at least the base value and what it
  sells brings at most the base value. The settings the executable has and nothing reads (`fBarterHaggle*`, `fBarterDisp*`,
  `fBarterMin/Max`) are not used.
- The two lists are the player's things (left) and the vendor's (right), each with the six tabs of the container
  (everything, weapons, apparel, aid, misc, ammo; the aid tab holds books and ingredients too, the misc tab all the rest;
  round 12AH A2). The arrows over a list (and a click on its title) go to the next tab, and a tab with nothing to list is passed
  over in the same direction until one has something, or everything is back. The title over a list is the tab's word in
  capitals (the executable writes them so when the language is English), and over the vendor's list, while it shows
  everything, the vendor's own name. A click moves one thing into the pile of the deal; of a stack of more than
  `iInventoryAskQuantityAt` (5) the quantity prompt asks how many. A thing in a pile shows in the list of the other side,
  added after the rows of that side, with an empty square (`_IsBarterSelected`) and the count that is in the pile, at the price
  of its new side, and a click on it takes it out again; a row of its own side shows what is left of a stack that is partly in
  a pile; a worn thing has the filled square. Nothing changes hands until Accept.
- A row reads the name, with the count in brackets above one, and the price of ONE thing, as the executable writes it: two
  dashes for nothing, tenths below 1, a whole number from 1. The line total is not shown in the list. The caps of each side are
  written over its list as "caps, three spaces, number". The order of the rows is the order of the inventory model in the
  executable, which was not followed (no sort call was found), so the port lists them by name.
- In the middle the caps that would move are shown on the arrow (`BM_CapsFlow`; `_ArrowDirection` is 2, the file's second
  picture, when the player is paid and 1 when it pays; nothing is set before the first pick, when the flow is hidden), faint when the vendor has less than it owes or the player has too little to pay. Accept works only
  with something in a pile and, when the player pays, only with the caps for it; when the vendor owes it always works and the
  payout is cut at what the vendor has. Exit asks "sCancelBarter" first when a pile has something in it; Escape does nothing in
  that box. The letters of the file work: A is Accept and X is Exit.
- Accept moves the things (a worn one is taken off first), then the caps (the vendor's container gets what the player pays and
  the player what the vendor pays), adds the caps to the statistic "Barter Amount Traded" (id 0x27), and leaves the menu open with
  empty piles.
- The first key: with nothing picked, the first of the arrows (up, down, left or right, the same for all four) picks the first
  row of the first list that has rows, the player's (round 12AH A4: every list row has `xdefault` 1 and the player's list is
  first in the file; the barter file has no other `xdefault`); nothing else happens with that key. Enter on a picked row (or
  on the row under the pointer) is a click on it. The pad's pick of the vendor's list when the menu opens (the code
  selects its first row when a pad is in use) is not done, the port does not know whether a pad is in use.
- Which list has the pointer (round 12AI B8): the file's `_IsContainerListSelected` (default -1) is 0 for the player's list and 1
  for the vendor's. Two hotrects over a half of the menu each (`BM_Items_FocusRect` id 0 and `BM_Container_FocusRect` id 1, alpha 0,
  depth 5) are targets only while their list is not the selected one, and the file says "mousing over them switches the focus":
  the list below is under a rect that takes the pointer, so it cannot be hovered or clicked until the focus is switched. The
  port sets the trait to 0 when the menu opens, to the side the pointer has come to (the rect it is over; `mouseover` is read
  every frame) and to the side a key goes to (left or right, the first arrow). That the executable sets it the same way, and
  to 0 at the start, is inferred from the file and the keys; the code that does it was not read. The wrapping of names
  of 22 or more characters to a second line is the file's own (`wrapwidth` 300 on `ListItemText`); the prompt for a number
  of things takes no digit keys in the port (not known for the game).
- Fallout 3 (round 12AH A6, `Fallout3.exe` partly compared): the same menu class with another layout of fields and other
  click ids (Exit 14 and a row 18, where New Vegas has 19 and 23); the file is the same, drawn by tile names. The close and its
  BarterExit line, the cancel box, the quantity prompt, the arrow counters and the caps sounds were found the same; the price
  has the same shape with the same settings and perk entry point, though where the executable writes the factors from the skill
  was not found. Not compared: the commit, the conditions of Accept. Fallout 3 has no branch for the two New Vegas casino and
  caravan form types (they are not in the data the port reads).
- Sounds (round 12AH A5): the menu's own code makes one, at the end of the deal: `ITMBottlecapsUp` when the player was paid and
  `ITMBottlecapsDown` when it paid, none for 0 (the script plays them). The rows of the lists make the sounds of the shared list
  template, `UIMenuFocus` when the pointer or a key comes to a row and `UIMenuOK` when one is clicked; the buttons Accept and
  Exit, the arrows and the title have none, and there is no sound when the menu opens. The pointer's sounds are the menu
  module's (it plays what a tile's `mouseoversound` and `clicksound` name, see "Sounds of the menu files" below); the keys
  play the two names of the list template.
- Leaving: after a deal was made the vendor says a line of the dialogue topic of subtype BarterExit (the table of the engine's own
  topics, kind 5 index 4; the response whose conditions pass, said by the speaker, then the dialogue menu is refreshed); leaving
  without a deal says nothing, and nothing is said without a conversation open. The port says it the way a script's `Say` does
  (`closeBarter` finds the topic by the editor id "BarterExit" and has the vendor say it: voice and subtitle; the line is not put
  in the dialogue's own text, which the executable refreshes). Not seen in the running game.
- The picture: while a thing is picked `BM_ItemIcon` shows the record's inventory icon (the ICON of the weapon, armor, ammunition,
  aid, miscellaneous item, book, ingredient or mod, as `core.pipboy.barterRows` gives it in `icon`; armor takes the male
  one), as the executable sets the tile's `filename` from the form (0x4be200). The pictures of the items are not in the files the
  offline harness reads, so it was not drawn offline.
- Not done: the pad's B, which the engine uses to leave at once without the question. The pad's
  first pick (the vendor's list) was not made. Nothing was seen in the running game.

### Models in a menu, and the lockpicking screen

Three screens of the games draw a model of their own inside the menu: the lock and the bobby pin of `lockpick_menu.xml`
(`Meshes\Terminals\LockInterface01.NIF` and `BobbyPin01.NIF`), the model behind the pause menu and the item of the
Pip-Boy. The code of the games has no picture for them in the files: it draws a scene straight into the back buffer after
the world and before the 2D tiles (Mac round 12AJ A2, `FalloutNV.exe` 0x78E1C0 to 0x790F40). OpenFallout draws the scene
to a texture and shows it in a layer under the windows, so the tiles of the menu file are over it.

| Part | What it does |
| --- | --- |
| `apps/openfallout/mwrender/menuscene.*` | One scene: a render texture, a camera (frustum, place, direction), one ambient and one directional light, models by name with a place each, point lights that are children of a node of a model, and, for a model with sequences, `setSequenceTime` that puts it at a time of one of its `NiControllerSequence`s (the games set the time by hand for the lock and the pin). `placement` and `placementFromRows` turn the numbers of the games' files (angles about X, Y, Z in degrees, or the rows of a matrix) into the matrix of a node. |
| `apps/openfallout/mwgui/menuscenes.*` | The scenes by name; each texture is registered with MyGUI as `scene:<name>` (`RenderManager::registerTexture`) and stays registered, because a widget may still point at it. |
| `apps/openfallout/mwlua/menuscenebindings.cpp` | `core.menuScene`. Every call is queued (`LuaManager::addAction`) because the scripts run beside the frame; the calls are done in the order they were made, so a script can make a scene and fill it at once, and `ready(name)` says when the picture can be shown. |
| `files/data/scripts/omw/falloutscene.lua` | Makes a scene and, once it is ready, a widget in the layer `MenuScenes` (after the HUD, not hit by the pointer) that shows its texture. |

The rules of the lockpicking screen are `FalloutScript::Lockpick` (`components/falloutscript/lockpick.*`, tested in
`apps/components_tests/falloutscript/testlockpick.cpp`); `core.lockpick.new{class, skill, picks, meter}` gives the Lua side a game
to `step{seconds, move, turn}` and `force()`, with the events of each step ("enter", "move", "turn", "squeakA", "squeakB", "stop",
"tensionStart", "tensionEnd", "break", "unlock", "forceFailed", "outOfPicks") and what to draw (the sequence and time of the
lock and of the pin, the turn of the pin). Round 12AI A1 has the numbers: five lock classes (very easy to very hard) with a
sweet spot of 400, 200, 100, 50 and 25 menu units on a meter as wide as the safe area, six nested bands around its centre that let the
cylinder turn 90, 75, 60, 45, 30 and 15 degrees, a pin that lasts `1 + 0.01 * skill` seconds of strain, forcing at
`skill - gate + 10` per cent, one experience reward per lock, the skill that each class needs (0, 25, 50, 75, 100, and a key
only). The skill does not change the size of the sweet spot in either game's data.

`falloutlockpick.lua` is the screen: it opens `lockpick_menu.xml`, writes the rows the file leaves to the code (the skill, the pins,
the level word, the Force button's text with the chance), puts the scene together with the numbers of the code (the scene root 125
units ahead and turned so that the x of a model is to the right, its z up and its y away; the camera at the origin looking
along +X; a frustum of `tan(75 degrees * 0.15) * 0.75` at distance 1 and as high as the window is; the pin turned about the
depth axis by where it is on the meter), and each frame feeds the game with the mouse's movement along X in menu units (1:1 with
the cursor) and with the four movement keys. F and E are the letters the file ties to its buttons (Force and Exit), X on a pad forces,
B leaves; the pointer is hidden over the area the pin moves in. What happens outside the screen is the engine's
(`core.interact.lockOutcome`): the lock is opened with its experience, a pin is taken, the player is told that none is left, a
failed force breaks the lock for good. `falloutinteraction.lua` shows the older widgets (a chance per pin) when the screen cannot be
shown (no menu file, no scenes).

Seen in the running game (Mac rounds 12AK and 12AL, New Vegas, 1920 by 1080): the screen opens with the picture on the
screen after one frame; the intro lasts 1.32 seconds and the entering sound comes on its first frame, as the code of the game
does it (with no pin the player is told so at that frame and no sound plays); the pointer moves the pin one to one with
the menu units and reaches both ends of the meter; the cylinder turns, binds (the stop sound and the strain sound), breaks
the pin after `1 + 0.01 * skill` seconds of strain, a new pin comes in a second later and can be used two seconds after
that; the force button turns the cylinder 90 degrees in 0.98 seconds with the two squeaks at 30 and 60 degrees and the unlock
sound, closes the screen and opens the container (experience 30 for an easy lock); a failed force closes the screen and the lock
stays shut for good (every later try shows a box); E leaves; all the sounds resolve. The lock file's own `shadow` mesh darkens
the world round the disc (0.58 at its edge, none beyond 1.7 radii): that is the games' picture, not a dimming by the port.
The first two runs drew the disc and the pin black: the nodes of the lock's file that held the point lights are not kept by
the model loader, so the lights were put at the root of the model, behind the disc; the lights are now made under the nodes
that are kept, at the places the file gives them (the scene logs its nodes and where each light went).

Not seen yet: the lit lock and pin (after the fix above), the pointer at the edge of the screen (the games read relative
movement, the port the moves of the window's pointer), the pad's axes (inferred), the sound of strain (a loop in the games,
played once here), and the karma that a stolen lock costs. The pause menu's model (three flat quads, no motion) and the
Pip-Boy's item are not drawn yet.

### The head-up display (Mac rounds 12AL and 12AM, FalloutNV.exe)

`main/hud_main_menu.xml` is mostly empty containers (health, action points, messages, the sneak meter, the experience bar,
the radiation, the enemy bar, the limbs, the crosshair, the text under it): the code of the games finds them by name, puts them
in place from the size of the screen, makes their parts from the templates of `HUDTemplates.xml` and writes the numbers into
them every 200 milliseconds (`HUDMainMenu`, id 0x3EC, create 0x76BFE0, update 0x770430). `falloutmenuhud.lua` does the same with the
menu engine; `falloutstatus.lua` gathers what the display shows (`core.pipboy.stats`, `core.hud`, `core.combat.weapon`) and keeps
its older drawing for a data folder that lacks the file.

- The menu is 960 units high and as wide as the window (1280 wide when the window is taller than wide); the safe zone is 15 on
  each side (`iSafeZoneX`, `iSafeZoneY`: the INI is not read here), so a block keeps 2 x 15 from the edge. The health block sits at
  `x = 40`, `y = H - 157`, the action points block at `x = W - 386`, the radiation at the top right, the enemy bar in
  the middle above the health, the sneak meter at the top middle, the messages at `(25, 55)`, the experience bar at
  `(W - 420, H - 310)`, the limbs at the right (`W - 230`, `130`), the crosshair in the middle of the window.
- A bar is a row of whole tick marks (`hud_tick_mark.dds`, 8 wide): `round(_TotalWidth * share / _ImageWidth) * _ImageWidth`, at
  least one tick; the health bar grows from the left, the action points bar from the right, the enemy bar from its middle (in
  steps of two ticks, so the halves are whole).
- Messages are one at a time: a message is shown for the seconds it came with (2), fades in and out in 0.35 seconds, waits behind
  the one shown, and is dropped when it is the same as the one queued last. The icon is the one the message brings; a message
  without one gets the face of the Vault Boy of its kind (0 neutral, 1 very happy, 2 sad, 3 in pain: `Interface\Icons\Message
  Icons\`, the first named `glow_message_vaultboy_neutral`; the script uses the first file of three spellings that exists, the
  names of the other faces were not read in full). A message may bring the name of a sound that plays as it comes on (the
  karma messages `UIKarmaUp` and `UIKarmaDown`, the worn weapon or armor `WPNBreak` with the sad face and four seconds), and
  a message with the front flag ends the one that is shown at once and comes next (the executable puts it at the head of the
  queue; where exactly was not followed; Mac round 12AV S4, 12AY R5). The callers choose the face (Mac round 12BA E: of 198 calls
  the kind is 0 at 178, very happy at 4 and sad at 16, and 59 refusals bring a sad picture of their own): the port gives the very
  happy face to a skill book's gain, to a payout of a casino and to a gain on the positive side of a reputation (the sad one on
  the negative side), and the sad face to the refusal to sleep or wait. Not there: the faces of the other refusals, the
  messages for the chems, challenges, a map marker or a radio signal, which need the systems behind them, the pictures of the
  locks (padlock, key) and the loss of a reputation.
- The compass is a strip of 360 degrees of the interface atlas (1023 texels) of which 345 texels, about 121 degrees, are
  shown. The executable shifts the texture of the strip by the heading over 360 less 0.15 (and adds 1 when that is below
  0); the menu engine scrolls a picture with the trait `_sourcex` (a window of the texture at its natural size from that
  texel on, going on at the left edge when it runs past the right one), and the script gives it the same number of texels
  (Mac round 12AN B8). A mark is drawn when 1.45 times its bearing is within 70 degrees of the front (a bearing within
  48.3 degrees), at x = (1.45 x bearing + 70) x 274 / 140 of the strip, so the marks stretch over 274 of its 345 texels and
  the one straight ahead stands at 137 (the player's mark is put there: its x was not read). They fade out towards the
  ends (read for the right end, where they are lit up to 1.6 x 137 and go dark at 274; the left end is taken to be the
  same). The marks of places (quest, landmark, characters) are made from the templates and put by their `_Heading`. Not
  there: the blink of the marks, the discovery of a place when the player comes within 1000 units, the north of the cell
  that is added to the heading, and the rule that picks the characters that show as ticks (at most 10 within 1500 units,
  3000 outside).
- When a window is open only the messages are shown, and nothing while the player is dead. The executable picks what the
  display shows by a mode of 17 bits, and a script that holds the player still (`DisablePlayerControls` on movement, Mac round
  12AM B5) puts it in mode 0xC: only the radiation, the experience bar, the messages, the subtitles and the quest reminder
  stay, and the health, the action points, the crosshair, the sneak meter, the limbs and the enemy bar are hidden. A
  script that disables rollover hides the text under the crosshair. `core.hud.status` has the disabled controls
  (`controlsOff`); `falloutstatus.lua` turns them into `scripted` and `rolloverOff`.
- The text under the crosshair (`Info`, Mac rounds 12AM B2 and 12AN B7) is a block 320 wide, centred, 110 above the lower
  safe edge: the word of what the player does with the thing (the file's hot rect, with the key that does it next to it, or the
  button of the pad: the file is opened for the one in use and made again when the player changes it), its name (with the
  number in brackets for a stack), a row of weight and value (a number of nothing is two dashes, a value under 1 has a
  decimal; the executable's builder, Mac round 12AO B4, shows it for armor, miscellaneous things, aid and item mods, for a
  weapon or ammunition only when it is playable, and not for a book, an ingredient or a key: the name alone), and a line for
  its lock. The words are the game's settings, string settings of the executables with no record in the plugins (the
  port's table, `settingtext.cpp`, holds the defaults, compared with the executables byte for byte in Mac round 12BB G1):
  `sTargetTypeTake` (also for a book that lies in the world: the executable says Take for a book, Read only for the
  Pip-Boy's prompt), `sTargetTypeOpen` (container), `sTargetTypeOpenDoor`, `sTargetTypeActivate`,
  `sTargetTypeSleep` and `sTargetTypeSit` (a piece of furniture with a marker of that kind switched on: the kinds are
  the port's reading of the marker numbers, 1 to 6 sleeping and 10 to 20 sitting), `sTargetTypeTalk` (a character that does
  not fight the player), `sPickpocket` (the same character while the player sneaks: the word is shown, but activating the
  character still talks to them, the pickpocket screen is not built), `sSearch` (a body), `sSteal` (a thing
  to take whose reference is owned by a character that is not the player, or by a faction the player is not in; the
  executable's test, 0x579690, is longer and was not read to its end, and the port does not ask the owner of the cell),
  `sPick` (a door that is locked when the player has no key for it) and `sCloseButton` (a door that stands open); a
  character that fights, and a creature that is alive, have no word. The lock line is, as the executable writes it,
  `sBroken` for a lock that forcing broke (the setting is a whole line with its brackets), else `sInaccessible` for a
  door whose reference has the inaccessible flag (also a whole line; the door has no verb; the words of both were compared
  with the executable in Mac round 12BB H), else for a locked thing `[sLocked - sHUDUseKey]` when the player carries the key, `[<class word>]` when only a key opens it, and
  `[sLocked - <class word>]` (the words `sLockLevelName*`). `core.hud.info` gives the engine's side, the
  object is the one of the focus ray (what the activation would hit). A thing with no name has no text. Not there:
  disarming a mine (`sDisarmMine`; the port has no placed mines to look at), the weight and value row for containers, doors and characters (the executable has none for them), and the colour
  that marks a thing that is owned (system colour 2 when taking it is theft or the target is a follower in some cases: the
  executable's rule is in B4). The key hint on the PC is the file's own `PCShortcutLabel` (see the rules of an expression
  below).
- Subtitles (`Subtitles`, Mac rounds 12AO B3 and 12AQ B3, FalloutNV.exe 0x774FD0, 0x7711F4 and 0x775230): the tile is 800 by
  110 and empty in the file; the code makes a centred text in it, 50 above the block of the text under the crosshair (the
  executable moves it up with that block when the block's text is taller; the port's block does not move). A line a character
  says outside of a conversation (the voice of the data, or the text alone when it has no voice) is an entry: it stays from the
  moment it was said for the length of its text times `fNoticeTextTimePerCharacter` (0.06 in the executable, 0.08 in the files),
  and is taken when subtitles are on (they are on by default, as New Vegas ships them), the line has no voice or a script forced it.
  The executable tests no distance when a line is taken (Mac round 12BB G4): a line of a speaker out of reach waits among the
  entries and is chosen if the player comes within 500 units before it is over. The text is the response alone: the executable puts no name before it. Every 200 ms the display drops the entries that are
  over, and when none is shown it chooses one: the entries in the order they were said, a forced one takes over whatever was
  chosen, any other is chosen when its speaker is within 500 units and nearer than the one chosen (and the one chosen is not
  the player's own line while this is another's: `kind` 0 is the player's, 2 another's). The tile is shown in the modes of the
  game and of a script that holds the player, not under a window. `falloutmenusubtitles.lua` keeps the entries,
  `core.hud.takeSubtitles` is the engine's side; the stand-in display prints a line as a message. There is no colour of a
  speaker (the three values the entry carries are the speaker's place). Not known: the voice's own length as the duration (the
  port has no length for a voice file, so a voiced line stays as long as its text takes).
- The region line (`Region_Location`, Mac rounds 12AO B3 and 12AQ B1, FalloutNV.exe 0x94CAE0, 0x7728F0 and 0x772C30): the
  tile is 100 by 100, 5 to the right of the health block and 75 above its top, and has two lines of text, left-justified,
  font 7, the second drawn over the first; the executable shows only the first (the one that is to show the name of a place
  is never given text: the discovery of a place is a message of the quest events, below). The line shows the name (the map
  name, `RDAT` type 4) of the region the player is in when the player's cell changes and the region is another than the one
  shown last: the region is the first of those of the cell (`XCLR`) that cover the player's place and have a worldspace of
  that place or none and are not deleted, which a later one replaces when it has a weather entry and the first has none, or
  its override is set and the first's is not, or the overrides are the same and its priority is higher. In a building the
  region is left as it was and nothing is shown; a place that no region covers, or a region with no name, shows nothing
  and is forgotten as the one shown last (coming back to the one before shows it again). What the executable does with the
  name (Mac round 12AV S1): a region name is only written to the line (and its hold flag cleared) when the line is not on the
  screen (alpha 0) or has the same text, and fades in from nothing over 2 s, the old text gone, only when the line is on the
  screen with another text; the line that normally fades in is the name of the world space (its `FULL`) that the player is
  placed in, at the end of every placement of the player (a door, fast travel, a script's MoveTo, the console's COC, a load, the
  push back at the border: the one caller 0x93CC9A of 0x772C30, Mac round 12AW S10; the argument is NULL for a building, so a
  building has no such line, and a new game of both games starts in a building), which is written with the hold flag set (so a
  region that comes in the same change of cell is overwritten by it); the name of the exterior cell itself is never used and starts its fade in once the tile is shown by the mode of the
  game (bit 0x20 of the mode mask) and the second line is idle. The fade out begins in the first update more than 4 s after the
  start of the fade in, from the alpha it has then, and takes 2 s; the text stays on the tile. The text tile is always visible
  and only its alpha is animated (target 255 times `fHudOpacity:Interface`; the port uses 255); the tile `Region_Location`
  follows the mode. `FalloutScript::RegionLine` is the rule for the regions, `FalloutScripts::takeRegionEvents` looks at the
  player's cell (a step to a cell next to the last in the same world space is not a placement; the port cannot tell the
  executable's placements apart further, and the push back at the border is not made), `core.hud.takeRegions` gives the rows
  `{ name, cell }` (`cell` is true for the name of the world space). The default of `fHudOpacity:Interface` is 1.0 (it is in no
  .ini of the games; Mac round 12AW S10), so the target alpha 255 is what the games have until the player changes the option.
  Not checked in `Fallout3.exe`: the rule for the name of the world space.
- The quest text (`QuestReminder`, Mac round 12AR C1, FalloutNV.exe 0x77A650, 0x77B430 and 0x77C170): the block is 400 by 200,
  15 in from the safe zone and 170 under its top edge; `QuestAdded` (80 down) and `QuestStages` (40 down) are in it.
  *An event* (a quest was added, completed or failed, a place was found: `kind` 0 to 3 of `core.hud.takeQuestText`) writes the
  heading (the word of the games' settings, font 7) over the name in upper case, font 8, a character to a tile; the character
  *k* begins to fade in 0.1 s x (*k* + 1) after the event began, takes 2 s to do it, stays 4 s from then and fades out in
  2 s. A name of 32 characters or more goes on a second line from the last space within the first 33; no more than 63
  characters are shown. The sound of the event is made when it begins (`UIPopUpQuestNew` for a quest added or a place found,
  `UIPopUpQuestComplete` for the others). *The lines of objectives* (`core.hud.takeObjectives`: the text of the objective that
  was shown, or the word of the settings in front of one that was completed) are taken together when no line is left; each has a box before it
  (filled for a completed one) and wraps at a third of the menu's width. The first four are looked after: the first fades in
  over 0.5 s; the second and the third come 1 s and 2 s later; the first stays 3 s after that and fades out (0.5 s), and the
  lines under it scroll up by its height (0.5 s). Nothing is taken from the queues until 2 s have passed since the last
  thing was told; the lines wait while an event is on the screen, and a quest that is completed takes the lines that are
  shown and those that wait away. `falloutmenuquests.lua` keeps the times, `falloutmenuhud.lua` draws them. Not checked
  in the game: which of the lines of one batch is at the top (the one told last is), the place of a line's box, and the
  sound of a line (one `UIQuestUpdate` for each); the reminder that repeats the objectives (`AutoDisplayObjectives`, off in
  the settings) is not made.
- Not drawn: the armour icons, the weapon's name and condition and the needs of hardcore mode; the weapon's rounds show the
  rounds carried, as the engine has no clip.
- **The breath and the air meter** (Mac rounds 12BI B and 12BM, FalloutNV.exe driver 0x889930, meter 0x77E4F0, Fallout3.exe the
  same rule at other addresses; `DerivedStats::swimBreath`, `FalloutScripts::updateBreath`, `falloutairmeter.cpp`). The air
  lasts `fActorSwimBreathBase` (10) plus `fActorSwimBreathMult` (0.5) seconds for each whole point of the player's **heal
  rate** (the actor value 15, not the endurance: it is the endurance's bonus, 0 below an endurance of 6, 5 for 6 to 8, 10 for 9
  and 10, `fAVDHealRateEndurance6Bonus` to `10Bonus`, so 10 to 15 seconds). With the head under the water (the point at 0.875 of
  the height is) the breath runs down one second for each second; with the head out it is full again at once. When it is gone
  the player loses `fActorSwimBreathDamage` (0.2) times its health (whole points) for each second, with the sound
  `NPCHumanDrowning` and the flash of a blow, and dies when the health is gone. The health counts in whole points, so the
  damage is none below one point and the player would stay at a fraction of a point for ever; the port kills a player with
  less than a point when the breath is out (the executable's call after the damage, 0x8A7D50, was not traced). Only the
  player is hurt by it. The
  executable asks for the magic effect of water breathing and for the actor's CanBreatheAir and CanBreatheWater; the port has
  none of them yet, so the breath runs down whenever the head is under. The engine's own drowning (20 seconds, 3 points a
  second) is off for the player. The meter is the tile `BreathMeter` of the file, the model `AirTimer01.NIF` (a ring of 16
  tick quads and a quad with an "O2", all one texture, `air_meter.dds`, 64 by 64), which the menu engine does not draw:
  `FalloutAirMeter` does, with a widget for each, the ticks turned 22.5 degrees apart about the ring's middle (tick 16 at the
  top, tick 1 just right of it). The model's origin is the tile's place, in the middle of the screen and 80 units under the
  sneak meter; a unit of the model is a unit of the display's 960 (not read: the code that projects the model into the tile's
  64 by 64 was not found). The ticks go from the one next to the top, clockwise, one for each sixteenth of the breath
  (`ceil(16 * share)` stay). Its alpha takes half a second to come when the head goes under and half a second to go once the
  head is out and a coming in is over; a window, the Pip-Boy and death hide it at once. Its colour is the tile's. Not seen in
  the running game yet.

### Books and scrolls (Mac round 12AM B4, FalloutNV.exe)

`book_menu.xml` is the screen of a book (two pages, the page numbers, the arrows, Take and Exit) and of a scroll (a
roll with a scrollbar). The file turns the pages itself: the number of the left page counts the clicks of the arrows
(`_pagenum`), and the text tiles ask for `pagenum`. What the code puts in the menu is three traits of the menu itself:
`user0` (a book, not a scroll), `user2` (the words) and `user3` (it can be taken); the page width (`user4`) and the font
(`user5`) the file has. `falloutbook.lua` does that and wires the buttons. **Nothing in play opens this screen** (Mac
round 12AV S3): the executable's only way to the function that makes it is the console (the command parser that also has
`use workbook`), a book that the player reads from the Pip-Boy has no screen and the Pip-Boy stays as it is, and a book that
lies in the world is picked up as any other item (the activation of a book is the generic one of an item that goes into
the pack). The port opens it from no place; the module stays for a console command.

The words are laid out by the menu engine (`components/falloutmenu/richtext.cpp`) for a text tile that says `ishtml`;
the executable's text engine is shared with the notes of the terminals:

- It is marked up text only when the string begins with `<`; else it is plain (broken at the new lines).
- The marks are `IMG` (`SRC`, `WIDTH`, `HEIGHT`), `DIV ALIGN=LEFT|CENTER|RIGHT`, `FONT FACE=<number>` and
  `COLOR=RRGGBB` with `/FONT`, `BR` (the text goes on one line down), `P` (two lines down) and `HR`: a new page when the
  tile has a `wraplimit`, two lines down when it has none. Other marks are left out. The default colour of the letters is
  (117, 59, 33). A setting name between `&` and `;` stands for the words of that game setting (the Lua side fills them).
- `wrapwidth` is the width the lines are broken at (at the last space; a word wider than that is broken where it fills
  the line), `wraplimit` the height of a page: a line stays on the page while the height used, the line and the
  `linegap` (added to the height of every line) fit in it. `pagenum` (from 0) says which page the tile draws, a number
  that is no page draws nothing, and the tile's `pagecount` trait says how many pages there are. The width and height of
  the tile are those of the page it shows.

Read in round 12AN B9: in marked up text a carriage return and a line feed are left out wherever they stand (only `BR` and
`P` break lines), a space and a tab are a space each and a run of them stays (a space that would overflow the line is dropped
and the line breaks at the last one; the port keeps no space at the start of a line, which was not read); in plain text a line
feed breaks the line, a carriage return and the other characters below 0x20 are left out and a tab is a space. `FONT FACE`
takes 1 to 8 (the font files `sFontFile_1` to `sFontFile_8`; a face given by name matches the name of the font object, whose
source was not found). `DIV ALIGN` takes LEFT, CENTER and RIGHT and the executable has no closing tag for it (the port ends
it at `/DIV`, and whether the executable applies it at all is not proven). `IMG` takes `SRC`, `WIDTH` and `HEIGHT` as whole
numbers; where the picture stands, its line height and the path rule were not found. The colour of plain text was not read.
The keys the screen has in the console case: Space closes it (the Escape key and the Pip-Boy key do not), a click on the exit
tile closes it and a click on the take tile (id 32) puts the book in the pack and closes it; the wheel turns the pages by two
(a scroll scrolls). The screen plays no sound of its own except the page turn from the menu sound table (opening and leaving
are silent).

**Reading a skill book** (the use function of the actor, `0x515040`, read in round 12AV S3; `ItemUse` does it): the reader must
be the player (for anyone else nothing happens); in combat the player is refused with the message `sCanNotReadBook` and the
book stays; the points are the setting `fBookPerkBonus` changed by the perk entry point 11 and are added to the skill for good;
the message is `sSkillIncreasedNum` with the name of the skill and the points as a whole number, the sound is
`UIItemGenericUp`, the Books Read count of the Pip-Boy goes up by one, and the book is removed at once. A book with no skill
(a plain book) does nothing when it is used, with no message and no count; only the sound of the item plays. A skill of 200
or more turns the book away with no message (the value is read as 100 at most, so this never happens in the port).

### The help manual (Mac round 12AN B3, FalloutNV.exe)

`tutorial_menu.xml` is the screen of the help pages: a title, the words of the page (a text tile that is marked up text
when the words begin with `<`), `Previous` and `Next` buttons, the number of the page and a scrollbar for a page that is
longer than the tile. The executable fills it from the form list of messages named `HelpManual` (`HelpManualXBox` when a
pad is in use); every message of the list is a page (its title and its description), and the pause menu has a Help
button that opens it. `falloutmanual.lua` does the same: `core.pipboy.helpManual(pad)` gives the pages (the form lists
are found by their editor id and the messages by their form id), `core.pipboy.helpMessage(id)` one message by its
editor id (a page that is shown alone, with no page number and no buttons to turn it). The start menu (the port's pause
menu) has a Help row after Settings when the data has a manual; Escape or Backspace (B on the pad) leaves, the
arrows and the pages (the sides of the pad) turn the pages, and the letters of the buttons work as they do in the menu.

Not there yet: the help hints that the game queues while the player plays (the first time a thing is met), and the keys of
the screen on the PC, which were not read (the arrows are a choice of the port).

### The companion wheel (Mac round 12AN B3, FalloutNV.exe; New Vegas only)

`companion_wheel_menu.xml` is the wheel of eight slices the player gets when it activates a companion (a character that
follows it as a teammate), where it would otherwise talk. The slices (angles clockwise from the top, 30 degrees each, rings
76 to 332 of a picture of 760 units): 0 be aggressive or passive, 1 use a stimpak, 2 wait here or follow me, 3 talk to, 4 back
up, 5 stay close or keep a distance, 6 open its inventory, 7 use ranged or melee weapons. The wheel keeps nothing of its
own: it reads four variables of the companion's script when it opens (`FollowerSwitchAggressive` is 1, `IsFollowingLong` is
1, `CombatStyleRanged` is 1, `Waiting` is 0), each click flips the flag it shows and makes the companion say a topic
(`FollowersTacticsCombatAggressive` or `...Passive`, `FollowersLetsGo` or `FollowersWait`, `FollowersTacticsDistanceLong` or
`...Default`, `FollowersTacticsCombatRanged` or `...Melee`, `FollowersTrade`), and the results of those responses (the plugin's
scripts) are what changes the companion. Slice 0 writes `FollowerSwitchAggressive` itself as well. A stimpak of the player is
used on the companion. The label of the slice under the pointer is the word of the setting (`sCWheelBeAggressive` and its
neighbours), and under the wheel the stimpaks and the companion's health, what it carries of what it can, or the weapon in its
hands. The sounds are `UIPopUpMessageGeneral` (open), `UIPipBoyScroll` (another slice) and `UIMenuOK` (a click).

`falloutcompanion.lua` draws it and lights the slice under the pointer (the left stick moves the pointer; the arrows and the
sides of the pad turn the lit slice, Enter and Y use it); the engine side is `core.pipboy.companion*`
(`mwscript/falloutcompanion.cpp`). The middle of the rings is taken as the middle of the picture, not as the file has it (55
units lower: the pad does not care and the executable's pointer test was not found). Not there yet: what Back Up does (the
executable calls a function that is not read), the fade the wheel closes with, the heal of a stimpak is what the item's effects
give, and whether the player says the topic or the companion (the companion says it here: the conditions of the lines are
about it).

### The hot keys (Mac round 12AN B2, FalloutNV.exe)

The number keys 1 to 8 are eight slots (`HotKeyWheel`, 0x701610; the control names are `sUActnHotKey1` and `3` to `8`), each
holding an item that the player put there. The second slot is not one of them: the wheel shows the ammunition swap
(`IM_AmmoSwap` of `prefabs/hotkeys.xml`) there, and the key changes the ammunition of the weapon in hand (not made). The
head-up display handles them every frame (0x77DA60), not while a menu is open, a script holds the player or the Pip-Boy is
switched off: a key that is let go before `fHotKeyDownTimer` (half a second) uses the item of the slot (a weapon or armor is put
on or, when it is on, taken off; an aid item is used; a stimpak at full health is refused with the word `sFullHealth` and the
sound `UIVATSInsufficientAP`); a key that is held longer shows the wheel of the eight slots with the held one lit
(`HK_SelectionBox`), and the picture and the name of the item (`%s (%d)` with the count when there is more than one) are on the
screen with it and stay, the wheel too, for `fHotKeyShowTimer` (a second and a half) after the last frame of the hold. A quick tap
shows neither the picture nor the name for any kind of item (Mac round 12BA G3: a press only loads them into the tiles; the tiles
are made visible when the key has been held longer than the timer), and a stimpak tapped at full health gives the message and is
not used (the key is let go, 0x701E80). All of it is in the file: the block `Hokeys` of
`hud_main_menu.xml` (300 by 300, over the action points) holds `template_hotkey_selector` (which includes the wheel of
`prefabs/hotkeys.xml`, with the big picture `hotkey_icon`) and a line of text for the name.

With a pad (0x701BD0, 0x77DA60) the D-pad chooses the slot, eight ways round the wheel: down is the slot 7, down and left 6, down
and right 8, up and left 1, up and right 3, left 4, right 5 (up alone would be the swap of ammunition, no slot). A new combination
has to stay more than 50 ms to replace the old one, except to or from none. The picture and the name come up at once, a direction
held longer than `fHotKeyDownTimer` shows the wheel, and letting go uses the item of the slot that was chosen (`padSlot` and
`stepPad` of `falloutmenuhotkeys.lua`).

`falloutmenuhotkeys.lua` keeps the timing of the keys (`I.FalloutHud` draws it through `falloutmenuhud.lua`; the keys are read
with `input.isActionPressed` of `QuickKey1` to `QuickKey8`, and the engine's own quick keys of Morrowind do nothing for
Fallout); `core.pipboy.hotKeys` and `assignHotKey` are the engine's side (`mwscript/fallouthotkeys.cpp`, the class
`HotKeys` of `components/falloutscript`, whose eight slots are saved in the section `hotkeys`). Only slots 0 and 2 to 7 take an
item (the second key is the swap of ammunition, with no `sUActnHotKey2`).

In the Pip-Boy (Mac round 12AO B2, FalloutNV.exe 0x781BA0 and 0x780140) the number key is held and the item clicked: a number key
that is pressed or held (not on the keyring) shows the file's wheel `IM_HotKeyWheel` with that slot lit (`_SelectedHotkey`, and
`_SelectedText` the name of what the slot holds, the picture in `IM_HotkeyIcon`, each slot's `_HotKeyIcon` and `_HotKeyAssigned`);
while it is up, a click on a thing (or Enter) puts it on the lit slot instead of using it; when no number key is down any more the
wheel goes (`falloutpipboyitems.lua`). Nothing is said and no sound is played for a thing put on a slot. Ammunition does nothing at
all; a thing that cannot be used, and a book, is refused with `sCantHotkeyItem` and the sound `UIVATSInsufficientAP`. A thing has
one key, so it moves from the one it was on. The key goes with the entry of the inventory, so a slot whose thing the player no
longer carries (dropped, sold, used up) is empty: the engine empties it when the slots are asked for.

Not read or not made: whether a bare number press with no click assigns in the same frame (not traced; here only the click does),
the executable's `sCantHotkeyBrokenItem` (this engine has no condition yet), what the executable does with the count of a thing
that is only reduced (here a slot is emptied only when none is left), the Hot Key button that only a player with a pad sees and the
pad's way to hold the wheel in the Pip-Boy (a direction held shows it, let go assigns: 0x701BD0; the button is the right bumper
there, which the page's pad handler gives to the page down key), and the tiles of the display that the executable hides while the
wheel is up on a pad. Seen only in the offline harness (`hotkeys.lua`, `hotpad.lua`, `itemwheel.lua`), not in the running game.

### The credits (Mac round 12AO B5, FalloutNV.exe 0x75FB90 and 0x75F9B8)

`options/credits_menu.xml` (id 0x417) is a full-screen backdrop (`background`, dark blue, its picture changes in turn) and an empty
tile `credits_rect` (at x 640, clipping) that holds the lines. The text is `DATA\<LANGUAGE>\CREDITS.TXT` (ENGLISH, GERMAN, FRENCH,
SPANISH or ITALIAN, from `sLanguage`), else `DATA\CREDITS.TXT` (the module asks the engine's file system for them from the data folder,
as `english\credits.txt` and `credits.txt`); a `CREDITSPATCH.TXT` in the same place replaces it when it is there;
the Wild Wasteland trait, a Ctrl key held or the global `PlayCredits` at 2 make it `CREDITSWACKY.TXT`. It is one buffer, not
pages: a line that is only a line end is a blank line, a line that begins with `*` is a comment, and every run of characters up
to a line end, `<`, `>` or `*` (255 at most) is a text tile of its own, put under the one before by the height of its font. The marks
(`<X:n>` `<X:+n>` `<X:-n>` across, `<Y:n>` `<Y:+n>` `<Y:-n>` down with `+` moving down, `<J:C>` `<J:R>` `<J:L>`, `<F:n>` the font,
`<C:r g b a>` in columns of four, `<I:wwwww hhhhh aaaa name>` a picture, `<B:name>` a background from `Interface\Credits`) set the
state; the units are those of the 1280 wide menu. The block starts 1020 down and goes up by `fCreditsScrollSpeed:Menu` (55) a
second; the backgrounds change with a slide time of the block's height over the speed over their number; it ends when the block
has left (the executable's limit was not read: it uses the size of the tile), or with the key of the menu. `falloutcredits.lua`
parses and shows it (`menu:create` makes the tiles, and only those near the window exist); it has no trigger yet. What starts the
credits in the games (the global `PlayCredits`, which the executable makes itself and which no record of the data names; a
command of that name is registered next to it) and the question that follows them (`sContinuePlayingAfterCredits`, Yes and No)
are the game's and not made. Not read: the end limit, what reads the accumulated height, the third number of a picture, which
`sFontFile_n` is which font slot, the order of the colour numbers (taken as red, green, blue, alpha), the control that speeds up
or hides the layer (0x3E9), and the music (none is played by the screen; the main menu starts `EndCredits.mp3`).
Seen only in the offline harness (`credits.lua`) with a made-up file, not in the running game.

### The casinos: the roulette table (Mac round 12AO B6 and B7, FalloutNV.exe 0x7BBE20 and 0x7BF670; New Vegas only)

The table games are opened by `ShowBlackJackMenuParams`, `ShowRouletteMenuParams` and `ShowSlotMachineMenuParams` (a casino record, the
least bet, the most, and what the player must have won before it may sit down; the scripts of the game give `1 200 0` and the
like). They look the casino (`CSNO`, now in the record store) up, make the player's entry for it the first time
(`components/falloutscript/casino.cpp`: the earnings of the player in each casino, saved in the section `casinos`) and say no with a
notice when the player has won what the casino will lose (`sGamblingBannedText`), has fewer chips than the least bet
(`sGamblingBrokeText`) or has not won the number asked for (`sGamblingMinWinText`). The chips are the casino's currency item; the
table counts them apart from the pack and they move only when the player gets up (`core.casino.close`, with the notice of what was
added or taken). The command returns 1 whether or not it is shown.

`roulette.cpp` holds the rules: the American wheel in the order of the executable (the colour of a pocket is the parity of its place
on the wheel), the bets (the 147 inside bets of the executable's table, 0x7B9C40: 38 numbers, 60 pairs, 15 threes, 22 squares, 11 lines of
six and the five numbers, and 12 outside: the three dozens, the two halves, even, odd, red, black and the three columns), the ladder of the amount (1 up to 10, 5 up to 40, 10 up to 100, 25 up to 500, then 100, the same steps down), at
most ten bets and the most for one spin (`maxTotal`, which is the maximum bet of the command), the pocket (uniform from 0 to 37, 37
being 00) and the pay (the stake and the multiplier on top for each bet that wins). Luck changes no number; above 5 it makes the
words of a win `sLuckyWinText`, below `sUnluckyLoseText`. The random numbers come from one engine for all casino games, seeded
once; the rules take the draw as a function so the tests choose the pocket.

`falloutroulette.lua` draws `roulette_menu.xml` (the info block and the five buttons, named as the file names them) and puts a
betting cloth in the middle of it. The game's own table is a 3D scene that the pointer picks bets from (`NV_Roulette-Points.NIF`);
that scene is not drawn, the cloth stands for it: the zeroes, the 36 numbers in three columns, the dozens, the even-money bets
and the columns bets, with the places on the lines (a split, a corner, a street, a six line, the five-number bet) found by how near
the pointer is to a line. A click, Place Bet (W) or A puts the amount on the place under the pointer, Remove Bet (F) takes the bet
of the place off, Finish Bets (S) spins, Increase and Decrease Bet are Q and E (the shoulders on the pad), Exit is R or B. Chips are
drawn on the places with their stakes, and the pocket that came up is written beside the cloth. The wheel is not drawn; the sounds are
the game's (`GAMERouletteValidBet`, `InvalidBet`, `IncreaseBet`, `DecreaseBet`, `Spin`, `Win`). `falloutroulettetable.lua`
is the cloth (cells, no screen).

`blackjack.cpp` holds the rules of a round (Mac rounds 12AO B6 and 12AQ B6, the state machine 0x734DB0): a shoe of the record's decks
(cards drawn at random; it starts again under five cards and, after a round, when the share of the record is left); the bet steps
(1 to 10, 5 to 40, 10 to 100, 25 to 400, 100 to 1500, then 500). The deal is the dealer's card, the player's, the dealer's second
(face down) and the player's second, Luck helping only with the two second cards; a 21 of the player or of the dealer on the first two
cards decides the round at once (both: a push) and the hole card is turned at once. Hit goes to the hand that is played; a hand that
reaches 21 or more (more than 21 is a bust) or its seventh card is finished. A Hit takes away Double Down, Split and Surrender.
Double Down is offered on any two cards when the chips cover a second bet (and, after a split, the second hand's bet too); it takes
one card and ends the hand, and it always works on the first hand, even when the second one is the one in play, as the executable has
it. Split (a pair of the same rank, once, needs the chips) gives each hand a second card without Luck's help, a hand that comes to 21
is a blackjack and finished (both: the round is paid at once, the dealer does not draw). The turn never moves to the other hand by
itself, not even after a bust: Switch Hands does it. Stay ends the player's turn for both hands at once (a second hand that was never
played counts with its two cards). Surrender (before any other move) gives up half the bet: int(bet / 2 + 0.51). The dealer draws
below 17, and on a 17 that holds an ace (any ace card, so a hard 17 with an ace too) unless the casino's flag says it stands, and not
at all when every hand is lost or given up; at most seven cards. A blackjack pays int(bet x ratio + 0.51) (hand two: + 0.5). Luck
above 5 helps the player's hits and doubles, below 5 the dealer's draws: as many cards are drawn as Luck is away from 5 and the one
that brings the hand nearest to 21 without going over is taken; which of them was used chooses the lucky and unlucky words of the
result. `falloutblackjack.lua` draws `black_jack_menu.xml` (the bet, the chips, the casino, the six buttons and the status text, named
as the file names them) and plain cards made of tiles in the middle of it in place of the 3D table: the dealer's above (the second
face down until the round is over), the player's hand or two hands below with their bets and a gold ground behind the hand that is
played. Before the deal the buttons are Deal (W), Increase Bet (E), Decrease Bet (Q), Max Bet (S) and Exit (R); in a round Hit (F),
Double Down (W), Split (E), Switch Hands (Q), Surrender (S) and Stay (R), each only when it is allowed. Leaving in the middle of a round
(Escape, or B on the pad, which the engine takes as leaving) loses what is on the table. Not read: the suit order of the cards (the
executable's suit number is the card's place divided by 13), the Luck rig's rules for low totals, and what the split code sums for
the second hand's 21 (it reads as the first hand's cards twice; this port tests each hand's own).

The winnings level (Mac round 12AQ B9): the entry of a casino also holds a level word. `GetCasinoWinningsLevel` and the condition
`GetCasinoWinningStage` (`0x262`) work out the highest of 1 to 4 for which the winnings are at least that quarter of the casino's
most (the record's `mMaxWinnings`, no rounding), 0 for a casino with no entry; `SetCasinoWinningsLevel` writes the level word and
the winnings that make it (level x a quarter of the most) for 0 to 4 and an entry that exists, and starts nothing. After a round of
blackjack the level word is raised to the highest quarter reached; when it went up the table closes (the result stays on the screen
for a moment) and, when the table closes, the casino's quest is started. A casino whose most is 0 keeps no limit here (the
executable would put it on level 4). When the table closes, chips that were won are added in every case; with the winnings at or over
the most the notice of the casino that will not play any more comes with them.

### The slot machine (Mac rounds 12AO B7, 12AQ B7, 12AR and 12AS E1, FalloutNV.exe 0x7C0A40 to 0x7C58F0; New Vegas only)

`components/falloutscript/slots.cpp` holds the rules: a strip of 28 stops made from the casino record's seven reel counts, a stop
drawn for each of the three reels (0 to 12 uniform with the full record; 13 only from the Luck rig), the class of the three stops
(three alike on a symbol pays the bet times 10, 20, 30, 40, 60 or 100 by symbol and 10 for the special one; two special symbols
five times, one special twice; a pair or nothing loses the bet) and the bet steps (+1 below 10, then 5, 10, 25 and 100 steps).
A pull adds one to the Pip-Boy statistic 0x2A ("Slots Games Played") and its net goes into the casino's winnings at once; the level
is looked at only after a pull that won, and a machine that has no chips left closes after saying so.

`falloutslots.lua` draws `slot_machine_menu.xml` (the bet, the chips, the casino and the buttons, named as the file names them) over
the machine that the executable draws behind the tiles: the model of the casino record's eighth `MODL` and three reel models, in a
`core.menuScene` made as the executable makes its scene (a root 175 units in front of the camera and 18 below its axis, the
executable's camera and frustum, the four point lights of the machine's file). The reels are not turned to a stop: each pull plays
the same `Forward` sequences of the reel files (the arm is pulled in 0.83 s, the reels start turning at 0.5 s and stop at 2.4, 3.3
and 4.2 s, each ending in the pose it started in), and the stop is made visible by putting the record's seven symbol pictures
(`ICON`, the order of the record) and the blank between them on the faces of the reel by their names (`Cylinder06:0` to `:13` for
the first reel, `Cylinder05` and `Cylinder04` for the others; `core.menuScene.setTexture`): the six faces in front when the arm is
back, the other eight when the last reel has stopped, and a reel is changed only when its stop is not the one it had. A pull that
wins plays the machine's `Backward` sequence (the coins fall, 3.2 s) with the sound of its class (`GAMESlotsWinSmall`, `Med`,
`Jackpot`); the payout list is the `Payout_In` and `Payout_Out` sequences of the machine (the board slides in over 0.67 s and waits
for any button, then slides out). The chips, the bet and the words of the result (`sYouWin` or `sLuckyWinText`, `sYouLose`) are shown
when the reels have stopped and the words go after a second; while the machine is turning no button does anything. The files of the
machine (`meshes/terminals/nv_slotmachine/` and the Dead Money one) are not run through the loader's optimizer, which would merge the
faces that share a picture and take their names away. Seen only in the offline harness with a made-up engine (`slots.lua`: the
calls to the scene and the menu over a pull that loses, one that wins and the payout list), not in the running game. Not read: what
the Luck rig does below Luck 5 (the unlucky words and the lose sound are not made), the lights of the Dead Money machine's file
(the four of the New Vegas files are used), and whether the earnings number belongs in the casino tile (it is hidden as on the
other tables).

Not read from the executable and made as stated: the number of places on the roulette cloth is 159 as in the executable. Not made:
the 3D table and wheel, the setting of what the executable keeps at +0x4010, the closing of the roulette table after a spin that
crosses a quarter (its closer was not read in detail). Seen only in the offline harness with a made-up engine (`roulette.lua`,
`roulettehit.lua`: every one of the 159 areas is reachable from some place on the cloth, `blackjack.lua`), not in the running game.

### Caravan (Mac round 12AS E3 and E4, FalloutNV.exe 0x73D650 to 0x752E20; New Vegas only)

`ShowCaravanMenu` (the reference it is called on is the opponent; the arguments are a `CDCK` deck record, the kind of opponent
0 or 1 and the share of its money the opponent opens with) opens a match: `openCaravan` in `falloutcaravan.cpp`. The records of the
game (`CCRD` cards, `CDCK` decks and `CMNY` caravan money) are in the record store now; the two numbers of a card's `INTV` are its
suit and its value (1 to 10, 12 Jack, 13 Queen, 14 King, 15 Joker; the files have no 11). The player's cards are the card items in
its pack, each card of a stack by itself, and the menu does not open for fewer than 30 (the executable's own message was not read,
so the notice is worded here). The opponent plays the cards of the deck record, not of its pack.

`components/falloutscript/caravan.cpp` holds the rules as the executable has them: three caravans of up to seven rows for each side (a
row is a number card with up to three face cards on it), the total of a caravan (a King doubles its row, the Queen of the last row
sets the suit and turns the direction), the placement test (0x74F6C0: in the start a number card on the first row of an own caravan
that has none; later a number card on the next row going the way of the caravan, against it only in the caravan's suit, never the
value of the row below; a face card on any row that has a number card; a Queen only on the last row), the Jack (removes its row and
what is on it), the Joker (removes every other row of the same value, of the same suit when it is on an Ace) and the end of the match
(0x752E20: a caravan is sold at 21 to 26; the player's caravan i faces the opponent's 5 - i, counting from 0; a side wins with two of the three pairs
decided in its favour and all three decided; a side with no cards left loses, the player's tested first). The opponent is one chooser
(0x74FDC0): each use of a card in the hand is given a number and the highest is made, the first of equal ones; the types 0 and 1 of
the game differ only in the start, the Joker and the price of throwing a caravan away. The Joker scoring of the type 3 and above, and
what the executable does with the Jack of the opponent's own second generator, are not read. `CaravanStake` and `settleCaravan` hold the
money: the opponent opens with the share of what it has, the player matches, may raise and the pot is the smaller bet; a side's money
is its caps and its caravan money (form type 0x74, read as the kind of `CMNY`) and what the loser pays is its caravan money item by
item in the order of the pack (here: of the forms) and then the rest in caps. Nothing moves until the match is over, and a match
that the player leaves is a forfeit, which is a loss.

`falloutcaravan.lua` draws the four screens from the files of the folder `Menus\Prefabs` (the executable's include handler reads
`<include src>` from there, `FalloutNV.exe` 0xA02D40, and the four files of the same names next to `caravan_menu.xml` have no
controls: the loader reads a file of the prefabs folder before one of the menu's own folder, Mac round 12AV S2). Every label is
a game setting, which the file names (`&-sName;`) and the executable writes itself: stake `sAutoMatchText`, `sRaiseText`,
`sAcceptText`, `sExit`, `sCurrentAnteText`, `sTotalFundsText`; deck `sAddText`, `sRemoveText`, `sPlayCaravanText`, `sAllCardsText`,
`sNavigateText`, a title `sCaravanDeckText` and the button of the right bumper written as `sRSMRandomize` and `sCaravanDeckText`
with a space; match `sSelectCardText`, `sDiscardCardText`, `sDiscardTrackText`, the give-up button `sCancel` (the file says
`sForfeitGameText`, the code writes `sBack` and then `sCancel` over it), `sAntePotText`, `sNetTotalText`, `sWinLossText`; result
`sCaravanYouWinText` or `sCaravanYouLoseText` and later `sCaravanPressAnyKeyText`, `sCaravanWinningsText`, `sCaravanLossesText`,
`sCaravanWinLossText`, `sCaravanBiggestAnteText`; the question `sQuitCaravanText` with `sYes` and `sNo`. The keys (the executable's key
handler, 0x749360, reads the letters in both cases; the files name the same letters on their buttons): stake W Auto-Match, A Raise,
F Accept, R Exit; deck W Add, A Remove, F Play Caravan, R leaves at once; match W Select Card, Q Discard Card, E Discard Caravan, R
Cancel; any key on the result closes it from the moment the words that ask for a key are up (Mac rounds 12AW S9 and 12AX R6:
the result screen is shown at once when the match ends, with "You win" or "You lose" and the rows, while the table plays a
sequence of 2.0 s; the words that ask for a key then take the place of those; keys before that do nothing, and a click on the
screen does nothing at any time, the executable has no tile there that takes one; the pad's B closes it a second after the
words are up). Escape is not handled by the Caravan code (the executable's menu never sees it; the pause menu opens on top, a
static reading). R on the match puts a picked card back and asks
nothing; with nothing picked it asks the one question, and Yes is a loss with the pot paid. The pad (the files' buttons, but A, which
the engine clicks with): the stake X Raise, Y Accept, B Exit; the deck the right bumper for the button of the deck, X Remove, Y Play
Caravan, the D-pad for the cursor (Add is A on the file); the match the bumpers for the triggers (Discard Card, Discard Caravan).
The deck has plain card tiles for the 3D strips of the game (the caravan deck above, all the cards below; click or the arrows to
move, the wheel to scroll, Enter to add or remove). The executable keeps one list of every card the player has, sorted by the
card's value, smallest first, and by nothing else (12AW S9; the screen's list is sorted so too), takes at most 108 cards into the
deck (adding is blocked at 108) and lets Play Caravan work from 30. The button of the deck, which says "Randomize" and the word of
the deck (key S, the right bumper), empties the deck and fills it with random cards without a repeat: the count is 30 for a
player with no more than 30 cards, else a number from 30 to the number of cards less one (12AX R6 settled it, the call
0x944460(30, count) with the top excluded; the executable does not cap it at 108, only adding a card is blocked there). The match is plain tiles for the 3D scene: the caravans as columns (the opponent's above
the player's, the player's n under the opponent's 7 - n), the hand of eight below, a card picked up by a click (or W) and put on a
caravan by a click on it, the totals coloured by sold, over or not yet; the file's counts of the corner show the pot, the net
caps won at Caravan in all and the matches won over lost (12AW S9: the pot of the corner is twice the stake of each side, the net
is the player's caps won less lost at Caravan, the matches won over lost as "%i/%i"; written once when the match starts), and the file's tiles for the totals over the caravans and the numbers of cards stay hidden. The
opponent moves after a second, its card lights the row it went on, and the result comes two seconds after the match ends. The
suit numbers of the card records are 1 hearts, 2 spades, 3 diamonds, 4 clubs and 5 the Joker, the values 1 to 10, 12 Jack, 13
Queen, 14 King and 15 Joker (no 11); the face of a card is the record's `TX00` texture
(`terminals\NV_PlayingCards\Deck<X>\card<X>_<suit letter>NN.dds`, five art sets B, G, L, T and U) and the back is `TX01`; the
code never works an image out from suit and value. The cards of the deck screen and of the hand are drawn with that texture (the
slot has the record's picture over its tile; `core.casino.caravan.cards()` and `board()` give it as `face`); the strips of the
caravans on the board have the words of the card (rank and suit letter) because the strips of the game are partly covered 3D cards.
Each match adds one to the Pip-Boy statistic "Caravan games won" (0x25) or
"lost" (0x26) and to the player's own totals (caps won, caps lost, matches won, matches lost, the biggest pot won), which the result
screen shows and the save keeps (`CaravanRecord`, the section `caravanrecord`). Seen only in the offline harness with a made-up
engine (`caravan.lua`), not in the running game. Not made: the 3D table, deck strips and hands (the scenes catalogued in 12AS
E2: `Meshes\Terminals\NV_Caravan\NV_Caravan-Table.NIF`, `-Deck.NIF` and `-Available.NIF`, the sequences `Card1..4_In/_Out`,
`Deck-Card1..5_Out`, `Deck8-Card1..8_Out`), the card backs (`TX01`) and the pictures on the board strips, the sounds, the text the executable writes into the title of the
opponent's ante on the stake (the file copies the string of its tile), and the Escape and the pad's B on the match, which leave
at once and forfeit without the question (the engine has no way yet for a screen of a script to keep them; the executable's B
puts a picked card back, else asks sQuitCaravanText, and its Escape opens the pause menu), and the pad's B a second after the result is up (the engine's B leaves at once).

### The loading screen (Mac round 12AR C2, FalloutNV.exe 0x788CC0 and the functions around it)

`loading_menu.xml` (one object in the executable, `[0x11DA0C0]`, with a thread of its own at about 20 Hz) has a full-screen image for the
last frame of the world (`loading_world_image`, id 12), two full-screen slides (`loading_tile_slide_01` and `_02`, ids 32 and 33,
black until a picture is put on them), a tip text and a black matte behind it for each slide (`tip_text_01` and `_02`, ids 34 and
35; `tip_matte_01` and `_02`, ids 36 and 37), the pinwheel (`loading_nif`, `Interface\Loading\LoadingAnim01.NIF`) and the groups
of text of the other kinds of load screen (objective, level progress, stats, plate), which the executable builds hidden.
The data of the games asks for the tips only: all 208 `LSCR` of New Vegas use the one `LSCT` of type 3. What the code does:

- The menu fades in over `menufade` (0.75 s) over the last frame of the world, which fades out as the menu comes (every tile's
  alpha is `p * 255`, the world image's `(1 - p) * 255`).
- It picks the pictures when the load begins (Mac round 12AY R3, 0x78A5A0): from the `LSCR` whose picture is for the main menu
  (record flag 0x400) when the load is from the main menu (20 of them), else `iMaxScreens:Loading` (4) in all, of which at most
  `iNumLocationSpecificScreens:Loading` (1) are for the place the load goes to and the rest are general ones (no `LNAM`
  locations), at random, none twice (the same file is the same picture). A picture is for a place when one of its `LNAM`
  entries is the cell (an interior), the world (an exterior), or a grid cell of the world that is the one the player arrives in.
  A load that goes nowhere in particular (a saved game, the main menu) takes general pictures only; when the pictures of the
  place and the general ones do not fill the four, the pass over the general ones takes what is left. An interior has pictures like
  an exterior has (the pictures of its cell, then general ones).
- The first picture is on the screen at once, on slide 1; every ten seconds (four from the main menu) the next picture is put on
  the other slide and cross-fades over the one before in 2.2 s (`p = clamp((now - stamp) * 0.00045)`; its tip and matte fade
  with it). The picture is `Textures\<ICON>` of the record and the tip its `DESC`.
- The tip is laid out by the `LSCT` (`DATA`: type at 0, x, y, width and height on a canvas of 1280 by 960 at 4, 8, 0xC and 0x10,
  the angle, the font, the colour as three floats and the justification at 0x14, 0x18, 0x1C and 0x28): the window is `left =
  max(x / 1280 * W, 15)`, `top = max(y / 960 * H, 15)`, `right = min(W - 15, left + width)`, `bottom = min(H - 15, top + height)` in the
  units of the menu, the text starts at the left, the middle or the right of it by the justification and wraps at its width, and
  the matte is 15 above it and 30 higher than it. New Vegas: font 6, 255/180/53, centred, 850 by 400 at (210, 100).
- There is no input and no least time; the screen goes when the load is done.

`components/falloutscript/loadscreens` has these rules (the window, the choice, the slide show; tested) and `loadingmenu` puts
them on the menu file; `mwgui/falloutloading` draws the pieces of the menu as widgets under the loading box of the engine, over
the frame the engine keeps of the world, and takes the pictures from the `LoadScreen` and `LoadScreenType` stores. A game
without load screens or without the menu file keeps the screens of the engine. Two things from the running game (Mac rounds
12BD C5 and 12BE C): the engine begins a loading screen for each step of a change of place (three within a second and a half for
one door, each with another picture and tip), where the game shows one for all of it, so a screen that begins within a second of
the end of another goes on with that one; and the widgets of the pieces are made again, with all after them, when one of them is
given another picture, because reused image boxes drew only a third to four fifths of the letters of a tip (every glyph is one
piece and one box; fresh boxes drew nearly all, 94 to 96 per cent, and the cause in MyGUI was not found).

The pinwheel (Mac round 12AY R3): the model is `Meshes\Interface\Circular Loading\loading01.nif` (tile `loading_pinwheel`, id 11;
not `LoadingAnim01.NIF`, whose tile 0 is static and only gives the duration and the sounds `UILoadScreenInitial` and
`MicroFiche01`). The tile's x and y are where the model's origin stands, in the units of the menu, and the scale is
`fLoadingWheelScale` (2); the centre of the wheel is the origin plus the scale times (-80.2141, +74.6074). The wheel turns
clockwise at 67.5 degrees a second and the ball counter-clockwise at 135, a loop of 5.3333 s. We draw it as three textures
(`load_roulette_bars`, `load_roulette_wheel`, `load_roulette_ball`) turned by widgets, with the numbers read from the model, not
the model itself. Not verified in the game: the folder of the textures (the code tries `Textures\Interface\Loading\`,
`\Circular Loading\`, `\Main\` and `\Interface\`), the layering (bars at the back, the wheel, the ball on top) and the
direction of the turn. Not done: the mode of a new game, which shows the wheel and no picture; the pictures of the other
kinds of load screen (objective, level progress, stats, plate). The box of the engine (the label and the progress bar) stays at
the bottom. Seen only in the offline harness and in tests, not in the running game.

### V.A.T.S. (Mac rounds 12BH and 12BI, FalloutNV.exe `VATSMenu` create 0x7E9200, click dispatch 0x7EECB0, layout 0x7F1840; `falloutvats.lua`)

`vats_menu.xml` is the whole screen; the file leaves three blocks empty (`HitPoints`, `ActionPoints`, `EnemyHealth`), has a template for the bracket of a body part (`body_part_percent`) and the list of the queue (`queued_actions`, rows from `queued_action`), and the code of the game does the rest. The menu is 960 units high, the safe zone 15 on each side, and `falloutvats.lua` makes these things:

- **Health** (360 by 127, left): the left bracket, a bar (`_OriginalX` 20, y 41) and the word `sHitPointsShort` right-justified at x 53, y 8. There is no number.
- **Action points** (386 by 127, right, x = W - 2 x 30 - 386 + 30): the right bracket at x -37, two bars at `_OriginalX` 326 that grow to the left, the word `sActionPointsShort` at x 315, y 8, the text `sVATSMessageLowAP` at x 270 (hidden until the points do not reach), and the rounds at x 315, y 80 (font 7). The bars are rows of whole ticks, as the HUD's. The points of the queued attacks are paid already; the bar in front is what the next attack leaves, the half-lit one behind it (alpha 128) what is left before it. When the points do not reach the next attack the bar in front shows its cost and the one behind nothing.
- **Enemy** (350 by 52, x = W/2 - 175, y = H - 2 x 30 - 88): the bracket, a bar that grows from its middle (`_OriginalX` 171, `_TotalWidth` 240) and the name centred at x 170, y 35, `systemcolor` 2 (hostile). The executable adds the health and the damage threshold under the name when the player has a perk (entry point 0x2F); that is not done, and a target that is not hostile is not told apart.
- **A bracket for each part** of the picked target (the slot of a bracket is the type of the part, 0 to 14: 0 torso, 1 and 2 head, 3 and 4 left arm, 5 and 6 right arm, 7 to 9 left leg, 10 to 12 right leg, 13 brain, 14 weapon; the place of the part in the target's list of parts, which is the number an attack names it by, can differ from its type, so the screen keys brackets and groups by type and queues by place): the template is made once for each slot, at the place the bone of the part has on the screen (`core.vats.partPosition`: the world position of the bone named by the part, projected to the menu's units from the view transform and the field of view of the camera, not from `camera.worldToViewportVector`, whose places are pixels of the video resolution while the menu's size is the interface's (Mac round 12BJ on a window at interface scale 2: 1920x1080 against 960x540, and every bracket off the screen); a body without that bone puts the part at a height by its type). The offset by part is the executable's (0x7F2874): the head 100 up when it is in the upper half and 100 down when it is in the lower, an arm, a leg or the weapon 100 to the side (away from the middle) and 50 up or down, the torso and the brain as they are. The bracket is kept inside the box from 130 to W - 180 across and from 130 to H - 236 down, and two brackets closer than 150 by 100 are pushed apart by half the overlap each (30 passes at most; the axis it overlaps least is taken, which the executable's code left open). The picture follows the side of the screen the bracket is on, not the limb: a bracket on the left half has the right-hand bracket picture (justify right) and one on the right half the left-hand one; the torso has the chest picture and the head the head picture. It shows the chance (at most 95, as `fVATSMaxChance`; a part with no chance shows 0), the condition of the part as a bar and, only for the parts of the picked group, its name; the group of a part is the actor value of its condition (byte 7 of the part's BPND: 25 head, 26 torso, 27 and 28 the arms, 29 and 30 the legs, 31 the brain), which parts of one body can share (a Handy's three arm parts are all 28), so a human's parts are each a group of their own (Mac round 12BJ B3; `core.vats.parts` gives it as `group`). A crippled part shows an empty bar and nothing else. The alpha of a bracket goes to 255 over 1.0 s for the picked group, 0.2 s for the others, and to 0 over 0.5 s when it is off the screen (Mac round 12BI A2 reads these as probable, not proven).
- **The queue** is the file's list: a row for each attack, "name of the target: name of the part", the newest at the bottom, and nothing to click or light. The text of a row (`ListItemText`, right-justified at x 15, 20 under the top of its row) is not cut at the list's rectangle in the game, because it has no `clips` of its own (Mac round 12BJ B1), so it lies mostly to the left of the list; the port's rows are set to `clips` 0 so that nothing cuts them. The row's height is the height of its text, whatever the text's y (B2). The file's formula for the y of a row puts the last row's top at the bottom of the list's rectangle, which would put its text over the action point text of the block below it; the formula counts the list's children, which this list has more of than the executable's, so the rows are placed by the port's rule instead, the last row's text ending at the bottom of the list's rectangle (the executable's own y was not worked out). The accept button takes no click (its `target` is 0) until there is something queued.

Input follows the executable (round 12BI A4 and A5): **A** and **D** (or the file's two arrows) pick the previous and next target, and play the sound of a refusal when there is only one; **W** and **S** or the wheel pick the part, one part for a press, in the order of the target's body part data (W or the wheel up the next, S or the wheel down the previous, round the ends, a part whose bracket is off the screen stepped over; Mac round 12BJ B3, the executable's loop at 0x7F57F7 and its keys at 0x7F3070); the pick starts on the first part of the torso's group, or the first part (the executable also starts on the head when it is less than 300 units from the player, which was not traced to the end), and a change of target picks the torso's group again; a click where no tile takes it is the attack control and queues an attack on the picked part, as does Space; **E** or Enter accepts, which leaves the screen (a fade of 0.25 s, `explorefade`) and carries the queue out, and **B** or the right button takes the last attack off the queue (its points come back) and leaves the screen when there is none. An attack that has no chance is refused with the message `sVATSMessageZeroChance` and the sound `UIVATSEnterFail`; one the points do not pay for with `UIVATSInsufficientAP`. Sounds: `UIVATSEnter` when the screen opens, `UIVATSMove` when an attack is queued, `UIMenuCancel` when one is taken off, `UIVATSExit` and, with a queue, `UIVATSReady` when the screen is left with Accept. With a pad the file is drawn in its pad layout (the hints for Select, Target and Body Part; the arrows are hidden) and the buttons stand for keys: Y queues, X accepts, the left bumper picks the part and the right one the target.

Not done: the camera of V.A.T.S. (the view is the player's own) and the slow motion, the special attacks (the two buttons of the file stay hidden; they need the weapon's cost and the unarmed skill), the bar of the weapon's condition, the compass, the row for a reload that follows a shot that empties the magazine, the bracket of the target's weapon, and the perk that shows the target's health and threshold. The world stands still while the screen is up (the screen is a window of the interface); the executable never slows time in the selection (Mac round 12BJ B4: no caller of the time-scale setter in the opening or the selection, and the menu's `user15` 0.25 is never read; only the playback scales time), so a plain pause is the reading the port keeps. The attacks of a queue come 0.7 s apart.

### The rules of an expression with a text in it (Mac round 12AO B1, FalloutNV.exe 0xA09410)

A trait that holds a text is the number 0 in a number (no parse, no length). `and`, `or`, `not`, `onlyif` and `onlyifnot` look at
the string of their operand first: a text counts as true whatever its number (the engine takes a text of no characters as none:
the executable's reading of an empty one was not found); a text that stands first, as a bare `copy`, is the number 0 and so
false. `eq`, `neq`, `gt` and the others compare numbers only, so two texts are always equal and a text equals a number only when
that is 0; there is no comparison of strings. This is what makes the hint of the PC key of a button show: the file's
`PCShortcutLabel` is visible when `not(_Has360Controller) and (_PCButtonText of its parent)`, so a button whose `_PCButtonText` has
text (`&-sPCMenuHintA;`, the setting's words: the executable's defaults are the letter and a closing bracket, for R, A, E, S, X, F,
W, Q and RMB) shows its letter next to it while no pad is in use. `_Has360Controller` is written by the executable when the menus
are set up and again when a pad is plugged or unplugged.

## The words of the labels

The menu files take the words of their labels from game settings (`<string> &-sCancel; </string>`). The games keep most of
those words in their executables, and the plugins only hold the settings that were changed from them, so reading the plugins
gives nothing for OK, Cancel, Take All, the titles and the names of the cards: the first run on the real data showed those
labels blank (the number of pieces of each menu with no words matched the count the engine gave exactly). `gameSettingText`
now answers in this order: a plugin that has a record with the text (a game in another language); the default text that
the player's own executable holds; the port's own wording in `components/falloutscript/settingtext.cpp`, a table of about
175 labels that the screens drawn from the files use (not a copy of the executables' table; it is only the answer for an
install whose executable is not found, and for the offline tests).

The executable is read once at start-up (`FalloutScripts::loadExecutableTexts`, `components/falloutscript/exesettings.*`): the
file `FalloutNV.exe` or `Fallout3.exe` next to the `Data` directory (or in it), the one of the game that is played first.
Every string setting is built by a static initialiser, and a plain scan of the image finds them without a disassembler,
by two layouts that were read in round 12AC: the code `68 <text> 68 <name> B9 <object> E8` (or `... 8B C8+n E8`: push the
address of the default text, push the address of the name, load `ecx`, call the constructor), and, in Fallout3.exe for most
of the settings, a table in the data of records of three addresses (text, name, table of functions). Round 12AC counted
1319 string settings in FalloutNV.exe and 1371 in Fallout3.exe this way, and every one of the 55 names the settings
screens use was found in New Vegas (49 in Fallout 3). The texts are converted to UTF-8 from the code page that is set for the data of the game (`encoding`; Windows-1252 by default); nothing of them
is kept in the repository or the user's files, and the line `Settings texts: N read from <path>` in the log says that it
worked. Before that (round 12AB, first run of the settings screens) every label of them was drawn as the name of its
setting, since the table had none of them and the plugins hold none. The card, caravan and roulette games' words are
found the same way.

## What was read from the executable

- The design space is 960 high and `960 * width / height` wide; it is drawn scaled by `height / 960`.
- Tile types, trait names and ids, expression steps, operand sources and the `&entity;` table come from `FalloutNV.exe`.
- The system colours are numbered HUDMain 1, HUDAlt 2, Terminal 3, Pipboy 4, MainMenu 5, System 6 (none is 0, a tile's
  own colour is -1); their red, green and blue come from the `iSystemColor` settings in the INI.
- A tile's position is the sum of its ancestors' positions. A text tile's `x` is the point it is justified to (left end,
  centre or right end).
- A hot rectangle that names a file (`filename`) is a picture that takes the pointer: the picture of the world map, the
  cursor of the hacking menu (seen in the files, not yet in the running game).
- A hotrect with no file is a plain colour of its own alpha: the executable gives every picture and hotrect a quad (a
  texture on it when a file is named) and none to a rectangle, menu or window (FalloutNV.exe tile factory 0xa00c20, round
  12AG, A6); each tile draws with its OWN alpha, never multiplied by its parent's (a rectangle's alpha is only a value
  that the tiles in it copy). The port draws a hotrect that says an `alpha` above 0 and has a size, as the 7 by 7 pixel
  `solid.dds` of the shared atlas stretched, in the tile's colour times its brightness; the files turn a hotrect off with
  `alpha 0`. The bar behind the lit row of a list is one (`list_box.xml`: its alpha is the list's `_fill_alpha`, 40, and the
  terminals set 224 and turn the row's text dark, which made the lit row of a terminal vanish in the running game before
  the bars were drawn); so are the fill of a lit tab of the Pip-Boy and the band behind a lit button of the start menu.
  Not as the executable has it: it reads a hotrect with no alpha as fully opaque (default 255), which cannot be so for the
  containers of the files (`SWM_MainRect` of the wait menu is 860 by 400 and says none), so the port draws only the ones
  that say one. Colour in the executable: brightness 0 or more gives the system colour (the nearest tile above that names
  one, else 1) times brightness/255 and ignores the tile's red, green and blue; a negative brightness (`uselocalcolor`)
  gives red, green and blue. The port's `colorOf` still uses the tile's red, green and blue when no system colour is named.
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
