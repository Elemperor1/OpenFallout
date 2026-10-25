Saves Settings
##############

.. omw-setting::
   :title: character
   :type: string
   :default: ""

   The default character shown in the Load Game menu.
   Automatically updated when a different character is selected.

.. omw-setting::
   :title: autosave
   :type: boolean
   :range: true, false
   :default: true
   :location: :bdg-info:`In Game > Options > Prefs`

   Determines whether the game auto-saves when the character rests.

.. omw-setting::
   :title: max quicksaves
   :type: int
   :range: >0
   :default: 1
   :location: :bdg-success:`Launcher > Settings > Miscellaneous`

   Number of quicksave and autosave slots available.
   If greater than 1, quicksaves are created sequentially.
   When the max is reached, the oldest quicksave is overwritten on the next quicksave.

.. omw-setting::
   :title: allow scripted autosave
   :type: boolean
   :range: true, false
   :default: true

   Whether the commands ``Autosave`` and ``SystemSave`` of the scripts of Fallout 3 and New Vegas save the game.
   It is the ``bAllowScriptedAutosave`` setting of those games.
   A save that a script asks for is made in the first frame in which the interface allows one.

.. omw-setting::
   :title: allow scripted force save
   :type: boolean
   :range: true, false
   :default: true

   Whether the command ``ForceSave`` of the scripts of Fallout 3 and New Vegas saves the game.
   It is the ``bAllowScriptedForceSave`` setting of those games.
   The saves are named "Forced save", and once ``max quicksaves`` of them exist the oldest is replaced, as for a quicksave.
