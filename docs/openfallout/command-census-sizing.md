# What the command census says about the work

Read with `docs/openfallout/command-census.md` (the ranked lists) and `docs/openfallout/coverage/tracker/command-usage.csv` (every command). The counts come from the plugins of Fallout 3 (the game and its five add-ons) and New Vegas (the game and its ten add-on plugins), read on the Mac with `esmtool` in milestone 13; they hold codes and counts only.

## How big the gap is

- The engine has a handler for 267 of the commands the games call and a stand-in for 38; 310 have none. The handler-less ones are called 3,201 times in all, 1.5% of the 220,707 uses of the games' own scripts and conditions. 177 of them are called by nothing in the plugins.
- 133 handler-less commands are called at all. The first 24 are 77% of the uses (`PlaceLeveledActorAtMe` 354, `RemoveAllTypedItems` 295, `DamageObject` 201, `FireWeapon` 175, `ForceTerminalBack` 169, `GetGroupMemberCount` 146, `Exists` 143, `RemoveMe` 134, `Dispel` 99); the first 60 are 92%. That is the list for milestone 16.
- The stand-ins are called 2,213 times, and most of what they stand for is something to draw or play: effect shaders (`PlayMagicShaderVisuals`, `StopMagicShaderVisuals`, 704 calls), `SwapTextureOnRef` (383), `SetActorAlpha` (186), videos (`PlayBink` 64), the radio (about 130 calls in five commands) and challenges (178). Those belong to milestones 15 (sound) and 18 (sights), not 16.
- Four codes are in no command table and are only named in conditions: `GetQuestVariable` (0x104F, 22,983 uses) and `GetScriptVariable` (0x1035, 2,960), which the condition evaluator answers itself, and two New Vegas functions, 0x1262 (25 uses) and 0x1264 (60), that nothing names yet.

## Who calls them

- 70% of the scripts that call a handler-less command in New Vegas (65% in Fallout 3) are object and quest scripts, 11% (19%) terminals and 15% (16%) quest stage results and dialogue results. A missing command in a script step over is logged and skipped; in a condition it answers as if false.
- The main quest lines are almost clean: no quest of the New Vegas main line (VMQ00 to VMQ05) and none of Fallout 3's (MQ01 to MQ04, MQ06 to MQ16) has a stage result or a response that calls a handler-less command. The two that are not clean: `MQ05` (one response calls `StopWaiting`) and the New Vegas chargen quest `VCG00` (`SetGlobalTimeMultiplier`, `TriggerScreenSplatter`, `SetScreenSplatterFade`, once each).
- The side quests that lean on them most: `MS06LincolnArtifacts` (32 scripts call `ModDisposition`), `VDialogueArcadeGannon` (12, `RemoveAllTypedItems`), `vDialogueGomorrah` (11, `RemoveFromAllFactions`), `MS11` (10), `VFreeformFreeside` (7), `VMS02` and `VFreeformTheFort` (6 each).
- `ForceTerminalBack` (169 uses) is in the terminal scripts only (76 in New Vegas, 89 in Fallout 3); `RemoveMe`, `DamageObject`, `FireWeapon`, `Dispel` and `PushActorAway` are in object scripts.

## For sizing milestone 16

A handler for each of the first 24 commands is a small piece of work apiece (most are one-line world changes: remove, drop, damage, fire, push, place a leveled actor, remove the items of a type); the conditions `GetGroupMemberCount`, `Exists`, `GetTrespassWarningLevel`, `IsWeaponSkillType` and `GetInCellParam` are 450 uses of the 555 handler-less condition uses. The rest of the 133 are one to eight uses each.
