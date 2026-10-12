# Autoplay

A developer tool that plays the New Game of Fallout 3 and New Vegas for someone who is not at the keyboard. It answers the
message boxes (the first button), finishes the screens of character creation (the name is `Autoplay`, everything else is left
as it is), picks the first topic of a conversation, and logs what it did in lines that start with `AUTOPLAY` (counts and
numbers, no text of the game). The world script logs the stage of the quests of the two intros each time one changes and
quits when the main quest starts (`MQ01` or `VMQ01` stage 10) or after ten minutes.

```
openfallout --skip-menu --new-game --content Fallout3.esm \
    --data scripts/openfallout/autoplay --content autoplay.omwscripts
```

The scripts need `core.messageBox` (Lua API revision 168). `scripts/openfallout/fallout_start_smoke_test.py --only newgame`
runs the same files against the synthetic plugin.
