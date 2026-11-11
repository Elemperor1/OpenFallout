# M14 notes (move and fight)

What the M14 pull requests changed, and what they rest on. The exe reads are in [m13-exe-reads.md](m13-exe-reads.md) and the M12 notes; nothing from the games is copied here.

## Pad menus (PR 159)

The first hour of play with an Xbox pad (round 13X) found the menus clumsy and weapon equipping "broken". The cause was one thing: the engine's A is a click at the stick-driven pointer (`mwinput/controllermanager.cpp`), while the D-pad moved a pick inside the Lua screens, so A never acted on the pick, and X in the Pip-Boy is Drop (as in the games' own `inventory_menu.xml` hints: A Equip, X Drop, Y Repair, RB Hotkey, LB Mod, B Cancel).

- The pad follows the last device used (the games, on a pad: the cursor is hidden and the screen picks by selection, A acts on the selection). A button that moves the pick makes the screen hold it (`input._setPadPick`); while it is held the engine does not click at the pointer and `falloutpad.translate` gives A as Enter. The stick moving the pointer, or the end of the screens, gives A back to the pointer.
- The triggers are axes; a screen hears them as the buttons `LeftTrigger` and `RightTrigger` (`input.CONTROLLER_BUTTON`, 100 and 101) while a screen is open.
- Pip-Boy: LB/RB pages, D-pad sides tabs, LT/RT list pages, A use, X drop, Y repair. The tab buttons are a choice of this port; the games' files do not say.
- The engine logs keys, buttons and sticks as `Input:` lines (`mwinput/inputlog.*`, limited), and screens log `Pad:` lines.
- Checks: `scripts/openfallout/test_falloutpad.py` (the pick rules, the Pip-Boy map, the screens that must use `translate` or release the pick). The arena smoke test has a chest step that takes with A after the D-pad; **it needs the menu files of the games to show the container screen, so on the synthetic data of Linux the chest steps (and the lock step before them) fail on master as well** (round of 2026-11-11: the same lines with and without this change). Not tried with a real pad yet.

## Difficulty and the god mode

- The difficulty of the game scales the damage of a blow to health when the player is on one end of it (`FalloutScript::FallDamage::blowMultiplier`: fDiffMultHPToPCVE/E/N/H/VH = 0.5/0.75/1/1.5/2 for what the player takes, fDiffMultHPByPCVE/E/N/H/VH = 2/1.5/1/0.75/0.5 for what the player does; the helpers of both executables, see the exe reads). `FalloutScripts::strike` applies it to the damage that reaches the health, after the part of the body took its share; the condition of a limb and the wear of armor still go by the damage before it, as a fall does. Blows between two others are as they are. Companions are not counted as the player.
- The console's god mode keeps the health, limbs and armor of the player against a blow (the blow still lands: the one who struck does not think it missed). This is also the way for a test script to hold the player's health: `debug.toggleGodMode()` after `debug.isGodMode()`.

## Not done yet

NPC falls (nothing launches a character off the ground yet: they are put on the ground a ray finds, so a fall cannot happen), the animation groups, first-person arms, V.A.T.S. camera, detection and sneak, effects, the navigator mesh in interiors. These want rounds on the Mac.
