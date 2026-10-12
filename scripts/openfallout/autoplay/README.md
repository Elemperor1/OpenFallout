# Autoplay

A developer tool that plays the New Game of Fallout 3 and New Vegas for someone who is not at the keyboard. It answers the
message boxes (the first button), finishes the screens of character creation (the name is `Autoplay`, everything else is left
as it is), picks the first topic of a conversation, and logs what it did in lines that start with `AUTOPLAY` (counts and
numbers, no text of the game). The world script logs the stage of the quests of the two intros each time one changes and
quits when the intro is over (`CG04` or `VCG01` stage 200, or the main quest starts: `MQ01` or `VMQ01` stage 10) or after
fifteen minutes.

Some stages wait for the player to walk into a trigger volume or to activate an object, which no button does.
`scripts/autoplaygates.lua` lists them (quest, stage, plugin, reference, `walk` or `activate`): when a quest has stood at
such a stage for twenty seconds, the world script puts the player in the middle of the reference or makes the player
activate it, and logs `AUTOPLAY gate`. A gate is tried twice. The table holds the references that the static surveys found;
a stage that waits for something else (a talk the player has to start, the gate of the playpen in Vault 101, whose id is not
known) is left as it is, and the quest line shows where the run stopped.

```
openfallout --skip-menu --new-game --content Fallout3.esm \
    --data scripts/openfallout/autoplay --content autoplay.omwscripts
```

The scripts need `core.messageBox` (Lua API revision 168). `scripts/openfallout/fallout_start_smoke_test.py --only newgame`
runs the same files against the synthetic plugin.
