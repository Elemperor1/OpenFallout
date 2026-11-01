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

## What was read from the executable

- The design space is 960 high and `960 * width / height` wide; it is drawn scaled by `height / 960`.
- Tile types, trait names and ids, expression steps, operand sources and the `&entity;` table come from `FalloutNV.exe`.
- The system colours are numbered HUDMain 1, HUDAlt 2, Terminal 3, Pipboy 4, MainMenu 5, System 6 (none is 0, a tile's
  own colour is -1); their red, green and blue come from the `iSystemColor` settings in the INI.
- A tile's position is the sum of its ancestors' positions. A text tile's `x` is the point it is justified to (left end,
  centre or right end).
- A hotrect that only copies its child's `x` and `y` does not move that child a second time (the Pip-Boy tabs would end up
  off the screen otherwise). This is the rule that matches the design; it has not been seen in the running game yet.

## What is not known yet

- How the Pip-Boy background (1024 by 768 design units) is placed on a screen of the design size.
- The meaning of the byte `&widescreen;` reads, and of the traits `wraplimit`, `wraplines`, `ishtml`, `cropx`, `cropy`,
  `repeat*`, `explorefade` and `menufade`.
- The words `&xitem;`, `&xlist;`, `&xboxhint;`, `&first;`, `&last;`, `&prev;`, `&next;` are in some menu files but not in
  the executable's table. The loader leaves them as text.

Game files are never part of the repository. The menu files are read from the player's own install at run time.
