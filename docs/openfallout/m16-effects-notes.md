# M16 effects: active effects, challenges, quest commands and perk entry points

Work package `effects` of M16. Executable addresses are those of `FalloutNV.exe` unless the text says Fallout 3 (`Fallout3.exe`).
Each statement is marked **exe** (read in the executable at that address), **seen** (seen running in a round) or **chosen** (nothing
in the executables settles it, the code does what is written). Rounds only show that code ran on real data.

Command table addresses (exec = the function the table gives the command, eval = the condition function), New Vegas / Fallout 3:

| command | New Vegas | Fallout 3 |
|---|---|---|
| Dispel | exec 0x5C20E0 | 0x5234A0 |
| DispelAllSpells | 0x5C2190 | 0x523550 |
| ScriptEffectElapsedSeconds | 0x5C43A0 | 0x523BB0 |
| EvaluateSpellConditions | 0x5D7B50 | 0x52DA50 |
| GetSpellUsageNum | exec 0x5CEAF0, eval 0x5A24F0 | exec 0x529EF0, eval 0x512260 |
| SelectPlayerSpell | 0x5CD750 | 0x534ED0 |
| PlayMagicEffectVisuals / StopMagicEffectVisuals | 0x5D1DC0 / 0x5D21E0 | 0x5352D0 / 0x5355C0 |
| PlayMagicShaderVisuals / StopMagicShaderVisuals | 0x5D1B80 / 0x5D2130 | 0x535100 / 0x52B4B0 |
| SetActorAlpha / SetActorRefraction | 0x5D23D0 / 0x5D3F10 | 0x52B560 / 0x52BD20 |
| PreloadMagicEffect | 0x5D22D0 | 0x5356C0 |
| IncrementScriptedChallenge | 0x5DFE30 | (New Vegas only) |
| UnlockChallenge, RemoveRecurringFromChallenge | 0x5DEE30, 0x5DEF90 | (New Vegas only) |
| GetChallengeCompleted | exec 0x5DEEF0, eval 0x5A60F0 | (New Vegas only) |
| ChallengeLocked | exec 0x5DEE90, eval 0x5A6070 | (New Vegas only) |
| ForceActiveQuest / ResetQuest | 0x5D9AC0 / 0x5DA180 | 0x52EEB0 / 0x52F030 |
| AutoDisplayObjectives | 0x5CFC50 | 0x52A350 |
| KillQuestUpdates / SetShowQuestItems | 0x5DE350 / 0x5C6AD0 | 0x5305E0 / 0x5249B0 |
| ShowQuestStages | 0x5DC730 | 0x53D9C0 |

## 1. Active effects (`components/falloutscript/magic.*`, `itemuse.*`)

The one rule is `Magic`: the list of the effects that run on each character (a spell cast on it, an ability or disease it has, an
effect an item put on it) with what each one changed, its script, its seconds left, its magnitude and whether its conditions hold.
`ItemUse` keeps what a chem or a cast spell holds (the temporary change of an actor value, the points to give each second).

- **Dispel** (exe 0x5C20E0 calls 0x824400): ends the effects the magic item (a spell, an aid item, an ingredient) has running on the
  character. What the effects changed is changed back, the script of a script effect gets its finish block, the spell stays on the
  list of the character, and `ItemUse::end` takes back what a chem or cast spell held (through the `Ender` callback). Without an
  effect argument (the command has none) every effect of the item ends; `Magic::dispel(actor, source, effect)` also ends one effect
  (the executable's function takes the effect too; no command asks for it). The ender is told the character, the item and the magic
  effect for each entry ended.
- **DispelAllSpells** (exe 0x5C2190 calls 0x8249D0): ends the effects whose item is a spell (type 0), power (2), lesser power (3), aid
  item (7) or ingredient (8). It leaves diseases (1), abilities (4), poisons (5) and addictions (10). For an enchantment (6) the
  executable asks a field at +0x34 of the active effect, which was not read: **chosen** to leave all of them (the kinds are the
  executable's numbering at 0x824A28).
- **GetSpellUsageNum** (exe, evaluated for conditions too at 0x5A24F0): the sum of the floats at +0x1C of the active effects of the
  item (0x822EB0), which 0x803DCF sets to the magnitude the effect item starts with. The code sums the magnitudes of the running
  effects of the item that are on the character.
- **EvaluateSpellConditions** (exe 0x5D7B50 calls the function that updates all effects, 0x8255A0): the conditions of the effects the
  spells of the character keep are asked again at once. **Live conditions** (exe 0x8043F0): an ability or disease effect with
  conditions is on the list but does nothing until its conditions hold; they are asked again every
  `fActiveEffectConditionUpdateInterval` (1 second in both games' files; the host reads the setting). An effect whose conditions
  start to hold begins as when it was put on (value changed, script started), and one whose conditions stop holding ends the same way as a dispel
  does (value changed back, script finished). **chosen**: only the effects the `Magic` rule applies itself (abilities, diseases,
  addictions) are asked again; a cast spell and the effects of items keep the one check at the moment they start.
- **ScriptEffectElapsedSeconds** (exe 0x5C43A0): gives 0 when the script has no effect to ask; otherwise the executable reads a
  float behind the event list. The seconds the update of the script covers are what the code gives (**chosen**: the executable's
  source of the float was not followed to its writer, so the value is the one M13 already gave GetSecondsPassed for effects).
- **SelectPlayerSpell** (exe 0x5CD750): **chosen** to keep the form only (saved), nothing reads it (the port has no spell selection
  of its own).
- **Visual effects**: PlayMagicEffectVisuals, StopMagicEffectVisuals, PlayMagicShaderVisuals, StopMagicShaderVisuals, SetActorAlpha,
  SetActorRefraction and PreloadMagicEffect are accepted and ignored (listed in `stand-ins.csv`). The shader records (EFSH) change how
  the renderer draws a reference (a full-screen or per-object shader), which is renderer work and out of scope; the effect-model path
  of M14 only attaches nodes (muzzle flashes, impacts), it cannot show a shader.

Saves: the magic section is version 3 (magnitude and "on the character" flag of each effect, and the chosen spell), the item-use
section version 2 (the item or spell and the magic effect each held change comes from). Older sections load: effects of a game saved
before have no source (a dispel does not find them) and run on.

## 2. Challenges (New Vegas, `components/falloutscript/challenges.*`)

All read in `FalloutNV.exe`: the central function 0x5F5950 (type, amount, two forms, flag, three numbers) is called by every counter
(22 call sites, mapped to host hooks in `apps/openfallout/mwscript/falloutchallenges.cpp`); the 14 lists of challenges that count are
at 0x11CB3E0 by type (kill from list 0, specific kill 1, any kill 2, hit 3, discover marker 4, use item 5, acquire item 6, use skill
7, damage 8, use item from list 9, acquire item from list 10, misc stat 11, craft 12, scripted 13); a second set holds the
challenges that are locked (0x11CB450).

- The flag word of a challenge at +0x70: 1 unlocked, 2 completed, 4 repeated (a recurring one completed once), 8 recurring removed. The
  flags of the record at +0x5C: 1 starts disabled, 2 recurring, 4 shows zero progress. Progress at +0x6C, interval at +0x60 (default 100),
  the three values of the type at +0x64/+0x66/+0x68.
- Counting: a challenge on the list of the type whose test passes (the list SNAM, the XNAM rule and the first, second and third values
  by type) gets the amount. A message with the name, the progress and the threshold and the description under it is shown each time
  the progress is a multiple of the interval, only when the name and the description are not empty and the setting
  `bShowChallengeUpdates` (0x11CB4D4) is on.
- Completion: the script of the challenge runs for the player as a `ScriptEffectStart` block (0x5F5B8D), the Pip-Boy counter 27 (challenges
  completed) goes up by one unless the challenge counts counter 27 itself (0x5F5C8F), the interface sound 0x15 (the level-up sound)
  plays after the message, a recurring challenge carries `progress - threshold` over, the lists are built again (0x5F5880).
- Commands: `UnlockChallenge` sets the unlocked bit, `ChallengeLocked` answers that bit (**exe** 0x5A60D0: the name says locked, the
  bit it reads is the unlocked one), `GetChallengeCompleted` the command gives 1 for a challenge completed or repeated (0x5DEEF0), the
  condition function gives the completed bit, 2 (0x5A6150, registered as a condition function so a condition sees the executable's own
  number), `RemoveRecurringFromChallenge` sets bit 8, `IncrementScriptedChallenge` adds one to a challenge on the scripted list.
- Host (`falloutchallenges.cpp`): the CHAL records are in the ESM store (new); acquire (the item added to the player), use (an aid item
  eaten, a book read), marker discovered, crafting done, hit and kill hooks feed the events; the counters of the Pip-Boy feed type 11
  (`PlayerProgress::setMiscStatListener`).
- Save: section "challenges" version 1: for each challenge whose progress or state differs from a fresh game, the form, the progress and
  the state bits.
- **Pip-Boy / HUD display**: the notice is a HUD message (icon, sound), the same path as every other message. The Pip-Boy has no
  challenges page in the files of the menus the port draws (the games list them on the Stats tab only in the Mojave Challenges screen of the
  achievements, which is not part of the `menus/*.xml` the port has): **not done**.

## 3. Quest commands (`components/falloutscript/quests.*`)

- **ForceActiveQuest** (exe 0x5D9AC0; the setter of the active quest is at 0x9529D0): sets the quest the player follows, whether it runs or not
  (**chosen** that a quest the port does not know is refused). The first quest shown while there is none becomes the active one, and the
  one that is completed or fails stops being it (what the display does when it tells the player: 0x77A480). The Pip-Boy quest
  rows carry `active`, the Lua page marks the row with the selection square (the page's existing `_selected` mark). Data and Lua only:
  not looked at on a screen (hidden rounds).
- **ResetQuest** (exe 0x5DA180 calls the quest's own function at 0x60D720): the quest is as it was when the game started: its stages
  are not done, its objectives are not shown or done, it is not completed or failed, and it runs when its record says it starts enabled.
  The variables and the delay of its script are not touched (the function does not say they are: **chosen**); the quest stops being the
  active one when it does not run afterwards.
- **KillQuestUpdates** (exe 0x5DE350): the display throws away the quest updates it still holds (the host's hook,
  `QuestManager::setUpdatesKiller`, empties the quest text queue of the HUD).
- **SetShowQuestItems** (exe 0x5C6AD0): the flag of the player is kept and saved (1 for a number above 0); nothing in the port reads it,
  so the effect (showing quest items in the Pip-Boy list) is **not done**.
- **AutoDisplayObjectives** (exe 0x5CFC50): stays a stored switch; the HUD reminder of the objectives is not drawn.
- **ShowQuestStages** (exe 0x5DC730): writes the stages of a quest to the console of the games. Accepted and ignored (stand-in).
- GetQuestCompleted: done before this package.
- Save: the quests section is version 2 (the active quest and the show-items flag); version 1 loads.

## 4. Perk entry points (`components/falloutscript/perks.*`)

The function that applies an entry point is `0x5E58F0` (Fallout 3: `0x543420`, which refuses an entry above 36); its callers are the
call sites listed per entry in the executable. Entry points applied by the engine before: 23 (see `perk-entry-points.csv` and the
dashboard). This package applies four more at a site in a rule the port already has, and each writes a line to the log when the perk
changed the number (`Perk: entry point N (name) of <owner> changes <before> to <after>`):

| entry | name | exe | where it is applied |
|---|---|---|---|
| 22 | Get Max Carry Weight | 0x8A0C20: the Carry Weight value, then the entry | the Pip-Boy and the fast-travel weight check (`FalloutScripts::maxCarryWeight`), the companion capacity. No perk of either game has data for it: nothing to see in a number. |
| 26 | Adjust Drinking Radiation | NV 0x5805C0, FO3 0x4FE566 (the tooltip: NV 0x7789C3, FO3 0x6456F6) | the scale of the magnitude of the water spell that the player drinks (`Magic::castOnSelf(..., scale)`). A Fallout 3 perk has data for it. |
| 44 | Modify Radiation Consumed (NV) | 0x816667 in the function that applies an item effect (0x815D00): effect on Radiation Level, item with the food flag | the magnitude of the radiation effect of a food (`ItemUse`). A New Vegas perk (0x44ca9) has data for it (multiply 0.5). |
| 68 | Modify Item Damage (NV) | 0x891360, called with the owner and the damage only | the points a shot takes from the weapon of the player and a hit takes from armor (`wearWeaponOfShot`, `wearArmorOfHit`). Data: Full Maintenance 0.25, Regular Maintenance 0.5, Built to Destroy 1.15, all multiply. |

Entries with a call site that need rules owned by other work packages or by nothing built yet, so they are **not applied** here:

- damage package: 3 (weapon attack AP cost, Fallout 3 only), 33 (concentrated fire, in the V.A.T.S. hit chance at 0x7F1290), 41 (miss
  fortune), 46 (meltdown), 52 (knockdown chance), 57 (ammo item chance), 72 (explosion radius), 4 (mine explode chance), 53 and 54 (weapon
  strength requirement, aiming move speed: the port has no strength requirement rule);
- needs package: 51 (fast travel always: the overencumbered refusal in `falloutinteraction.cpp`, `conditions.mOverencumbered`, is theirs), 24 (addiction
  duration: no perk of either game uses it);
- actors and spawn packages: 14 and 15 (disposition, should attack), 21 (leveled list on death), 28 (mysterious stranger), 29 (paralyzing
  palm), 31 (ignore running during detection), 35 (player kill AP reward: the V.A.T.S. rule does not track kills), 37, 38, 42, 43 (reload,
  equip, run and attack speed: animation work), 47, 70, 71 (see enemy health, spotting, item detection: HUD and compass), 59 (throw
  velocity), 61 to 67 (unarmed power attacks), 73 (heavy weapon weight, no perk uses it);
- not mine and no rule yet: 27 (activate), 32 (ignore broken lock: the executable compares a field of the lock data at +0xC with 1, or with 2
  for a character with the perk, in the function at 0x430AE0; the port keeps a broken lock as a set entry, so the rule would be a
  count that the executable's writer was not read for), 62 to 66.

## What the code does, tests and rounds

Unit tests (all in `apps/components_tests/falloutscript/`): `testchallenges.cpp` (about 30 tests: counting by type, the list tests,
completion, notices, recurring, unlock, save and load, commands and conditions), `testmagic.cpp` (dispel by spell, by effect and all,
kinds kept and ended, usage, live conditions, the interval, EvaluateSpellConditions, save version 3 and a version 2 stream, the cast
scale), `testitemuse.cpp` (`end`, the magnitude told to the listener, radiation consumed), `testsavecharacters.cpp` (item-use section
version 2 and an old stream), `testquests.cpp` (the active quest, force, reset, show items, kill updates, save version 2 and 1),
`testobjects.cpp` (the seconds of a script effect, a script run once), `testplayerprogress.cpp` (the misc stat listener).

Rounds are in the "Rounds" section below.

## Rounds (hidden, headless, New Vegas, the profile's newest save)

Both rounds need a way to run a script command on a reference and read its answer, so there is a debug tool for that:
`debug.scriptCommand(name, reference | nil | "player", args)` (`apps/openfallout/mwlua/debugbindings.cpp`, run by
`FalloutScripts::debugCommand` in `falloutdebug.cpp`). It calls the opcode through the interpreter and writes one engine log line,
`Command: <Name> on <ref> (<args>) = <value>`. The perk entry points write `Perk: entry point N (what) of <owner> changes a to b`
(`falloutperks.cpp`), the challenge host writes `Challenge: ...` lines, the drink rule writes `Drink: ...`.

`m16-effects-magic` (PASS, 88 s): spell cast on the player by a command, GetSpellUsageNum 4 after four casts, Dispel of that spell
leaves the base value, DispelAllSpells restores every value to its baseline; an aid item used from the Pip-Boy: its resistance value 0 to 25, usage 25,
Dispel of the item gives 0 and usage 0; an ability with a script effect (ScriptEffectElapsedSeconds): health flat at 170 without the
ability, then 170.568, 171.132, 171.698, 172.254 at about 5.6 s steps of simulation time (about 0.1 per simulation second, as the
script divides the elapsed seconds by ten); radiation of a food 0 to 3 without the perk and 3 to 4.5 with the perk 0x44ca9 (entry 44, x0.5).

`m16-effects-quests` (PASS, 37 s): a challenge of the New Vegas master file (0x1165777, counted by misc stat 37): GetChallengeCompleted
0 and the next challenge (the one its script 0x116577f unlocks) at ChallengeLocked 0 before; three `ModPCMiscStat 37 1` give `Challenge: ... is at 1`,
`... at 2`, then its script runs and `... is completed (a notice is shown)`; GetChallengeCompleted 1, GetPCMiscStat 37 = 3, misc stat 27
(challenges completed) 0 to 1, and the follower's ChallengeLocked 0 to 1 (the completed challenge's own script unlocks it, as in the
executable). A start-disabled challenge (0x115eb2b): ChallengeLocked 0, UnlockChallenge answers 1, ChallengeLocked 1; then
`ModPCMiscStat 9 25` completes two challenges at once (0x115eb2a and 0x115eb2b, two scripts run, misc stat 27 0 to 1 to 3). ForceActiveQuest
on a listed quest that was not followed: the Pip-Boy quest list reports that quest as the active one after the command (it was nil before).

What the magic round could not show: removal by effect (no command reaches it, unit tests only), the Fallout 3 drink rule (entry 26,
no Fallout 3 save), entry 68 (item damage, unit tests only), and what ScriptEffectElapsedSeconds means in the executable (it reads a
float behind the event list at +0x10 that I did not trace to its writer; the per-update seconds is a chosen meaning).

What the quests round could not show: the notice on the screen and the Pip-Boy marking (the rounds are hidden, and the HUD script takes the
message queue each frame, so a round sees the engine's log line but not a drawn notice); IncrementScriptedChallenge on real data (the New
Vegas master file has no scripted-type challenge; the add-on ones need their plugins; unit tests only); a Fallout 3 round of any kind.

## Not done

- Visual effects of magic (EFSH shaders, alpha, refraction, effect visuals): renderer work.
- Pip-Boy page for challenges: no menu file.
- HUD reminder for AutoDisplayObjectives; the console text of ShowQuestStages.
- DispelAll leaves enchantment effects (the field at +0x34 was not read).
- Entry points listed above as left to other packages.
- No Fallout 3 round: the profile has no Fallout 3 save, so the drink rule (entry 26) is covered by unit tests only.

## Review fixes

- The script of an effect whose conditions do not hold gets no update (it has not started, or it has finished); the entry stays for the
  conditions to be asked again. Ending entries (dispel, removal of a source) first undoes their changes and tells whoever kept them, for all
  of them, and then finishes the scripts, so a script that casts again is not undone by the end of an earlier entry.
- A saved game gives two effects of one spell that are the same magic effect the conditions of their own (by their order).
- `ScriptEffectElapsedSeconds` is 0 for a start, an end and a script with no effect, and the seconds of an update that was running come back
  after a nested effect script.
- A completed challenge never counts again (also when a recurring one was ended and unlocked), and a completion is taken under the lock before its
  script, counter and notice run.

