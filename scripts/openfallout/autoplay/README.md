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

Some stages wait for the player to walk into a trigger volume, to activate an object or to talk to a character, which no
button does. `scripts/autoplaygates.lua` lists them (quest, stage, plugin, reference, `walk` or `activate`; activating a
character starts a talk): when a quest has stood at such a stage for twenty seconds, the world script puts the player in the
middle of the reference or makes the player activate it, and logs `AUTOPLAY gate`. A gate is tried twice. A gate can also put the
quest at a stage (`setstage`): a stand-in for what the engine cannot bring about yet, the fights of the intro of Fallout 3 and the
switches in the reactor, and what a gate before it should have brought about: such a stand-in has a `settle` of a minute, and each one that the log shows was used is something the engine does not do yet. The table also names the button of a message box that is not the first (the box at the door out of Vault
101 leaves with its last button). The table holds the
references that the static surveys found; a stage that waits for something else (the characters finishing their packages, the
timers of the intros, a fight) is left as it is, and the quest line shows where the run stopped.

```
openfallout --skip-menu --new-game --content Fallout3.esm \
    --data scripts/openfallout/autoplay --content autoplay.omwscripts
```

The scripts need `core.messageBox` (Lua API revision 170). `scripts/openfallout/fallout_start_smoke_test.py --only newgame`
runs the same files against the synthetic plugin.
