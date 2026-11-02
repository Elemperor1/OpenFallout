---
-- Provides interfaces to interact with menu elements.
-- @context menu
-- @module menu
-- @usage local menu = require('openfallout.menu')

---
-- @type STATE
-- @field [parent=#STATE] NoGame
-- @field [parent=#STATE] Running
-- @field [parent=#STATE] Ended

---
-- All possible game states returned by @{#menu.getState}
-- @field [parent=#menu] #STATE STATE

---
-- Current game state
-- @function [parent=#menu] getState
-- @return #STATE

---
-- A table of parameters for @{#menu.newGame}
-- @type NewGameOptions
-- @field #boolean bypass Skip new game video and character generation (default: false)

---
-- Start a new game
-- @function [parent=#menu] newGame
-- @param #NewGameOptions options An optional table with additional optional arguments

---
-- Load the game from a save slot
-- @function [parent=#menu] loadGame
-- @param #string directory name of the save directory (e. g. character)
-- @param #string slotName name of the save slot

---
-- Delete a saved game
-- @function [parent=#menu] deleteGame
-- @param #string directory name of the save directory (e. g. character)
-- @param #string slotName name of the save slot

---
-- Current save directory
-- @function [parent=#menu] getCurrentSaveDir
-- @return #string

---
-- Save the game
-- @function [parent=#menu] saveGame
-- @param #string description human readable description of the save
-- @param #string slotName name of the save slot

---
-- @type SaveInfo
-- @field #string description
-- @field #string playerName
-- @field #string playerLevel
-- @field #number timePlayed Gameplay time for this saved game. Note: available even with [time played](../modding/settings/saves.html#timeplayed) turned off
-- @field #number creationTime Time at which the game was saved, as a timestamp in seconds. Can be passed as the second argument to `os.data`.
-- @field #list<#string> contentFiles

---
-- All the saves for the given directory
-- @function [parent=#menu] getSaves
-- @param #string directory name of the save directory (e. g. character)
-- @return #map<#string, #SaveInfo> map with save filenames as keys

---
-- List of all available saves, grouped by directory
-- @function [parent=#menu] getAllSaves
-- @return #map<#string, #map<#string, #SaveInfo>> map with directory names as keys, returning maps with save filenames as keys

---
-- Exit the game
-- @function [parent=#menu] quit

---
-- Whether the menu that opens with Escape is open (the screen on top is the menu)
-- @function [parent=#menu] mainMenuOpen
-- @return #boolean

---
-- A script draws the menu that opens with Escape (`drawn` true), as the game's own menu file `start_menu.xml` does, or
-- the engine does (false, as before): the engine's own buttons are not shown while a script draws it. The script says it
-- every frame: a claim that is a second old is no longer one, so a script that fails gives the menu back. With `escape`
-- the key that leaves the menu (Escape, the B button) is the script's, which goes back a step in a screen of its own;
-- the engine does not close the menu with it then.
-- @function [parent=#menu] claimMainMenu
-- @param #boolean drawn
-- @param #boolean escape optional

---
-- Close the menu and go on with the game (nothing when no game is running)
-- @function [parent=#menu] resume

---
-- Whether the game can be saved now: a game is running, the character is made and the place can be saved in
-- @function [parent=#menu] canSave
-- @return #boolean

---
-- Open the engine's window for the settings, and whether it is open
-- @function [parent=#menu] openSettings
-- @function [parent=#menu] settingsOpen
-- @return #boolean

return nil
