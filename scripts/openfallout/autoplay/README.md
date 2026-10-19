# Autoplay

A developer tool that plays the New Game of Fallout 3 and New Vegas for someone who is not at the keyboard. It answers the
message boxes (the first button), finishes the screens of character creation (the name is `Autoplay`, everything else is left
as it is), picks the topics of a conversation (the one it has picked least often with that character, the first of them; a
conversation where every topic has been picked twice, or that has none to pick and says nothing, is ended as the Goodbye
button would end it), and logs what it did in lines that start with `AUTOPLAY` (counts and
numbers, no text of the game). The world script logs the stage of the quests of the two intros each time one changes and
quits when the intro is over (`CG04` or `VCG01` stage 200, or the main quest starts: `MQ01` or `VMQ01` stage 10) or after
forty minutes. The player script logs `AUTOPLAY status` every five seconds (the screen that is open, whether the world is paused,
the cell and the place of the player) and the world script logs `AUTOPLAY world` when what pauses the world changes, so that a run
that stands still can be told from one that waits.

Some stages wait for the player to walk into a trigger volume, to activate an object, to talk to a character or to shoot at
something, which no button does. `scripts/autoplaygates.lua` lists them (quest, stage, plugin, reference and `how`): when a quest
has stood at such a stage for twenty seconds, the world script does what the gate says and logs `AUTOPLAY gate`. `walk` puts the
player in the middle of the reference, `activate` makes the player activate it (or each of the `refs` of the gate, at once;
activating a character starts a talk), `fire` makes the player shoot at it with the weapon in hand (`shots` times, or with
`untilDead` until it is dead; the player script logs `AUTOPLAY fire` for each shot and `AUTOPLAY fire done` with the reason it
stopped), `teleport` takes the player to a place `at` in a cell of the plugin (a room that no door of the data leads to). A gate
is tried twice (its own `tries` if it has one). A gate can have a `settle` (how long the quest stands still first) and a `gap` (the
real seconds since the gate before it): of the gates that are ready, the first of the table goes first.

A gate can also put the quest at a stage (`setstage`): a stand-in for what the engine cannot bring about yet, and for what a gate
before it should have brought about. Such a stand-in has a `settle` of a minute, and each one that the log shows was used is
something the engine does not do yet. The table also names the button of a message box that is not the first (the box at the door
out of Vault 101 leaves with its last button). The table holds the references that the static surveys found; a stage that waits for
something else (the characters finishing their packages, the timers of the intros) is left as it is, and the quest line shows where
the run stopped.

```
openfallout --skip-menu --new-game --content Fallout3.esm \
    --data scripts/openfallout/autoplay --content autoplay.omwscripts
```

The scripts need `core.messageBox` (Lua API revision 170). `scripts/openfallout/fallout_start_smoke_test.py --only newgame`
runs the same files against the synthetic plugin, and `--only autoplay` runs the gates that shoot, teleport and activate
several things at once, on the arena of the plugin.
