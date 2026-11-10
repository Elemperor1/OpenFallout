# Which script commands the games call

How often the plugins of the games call each script command, and which of them the engine has no handler for. It is for ordering the work on the rules of the world (milestone 16) and for finding the commands that stall a quest. The counts come from `esmtool scriptcode` and `esmtool dialogue` (see `scripts/openfallout/command_census.py`); they hold codes and counts only.

A **use** is a place that calls the command: a statement or an expression in a compiled script (object and quest scripts, the results of dialogue and quest stages, terminals, packages and perks), or a condition that names it (dialogue, quests, terminals, packages and perks). A script is counted for each record that holds it, so the figures are for ranking, not exact tallies.

Games counted: Fallout 3, New Vegas.

## How the uses split

| State of the command | Commands | Called at all | Uses | Share of the uses |
|---|---:|---:|---:|---:|
| handler | 267 | 248 | 215293 | 97.5% |
| stand-in | 38 | 37 | 2213 | 1.0% |
| no handler | 310 | 133 | 3201 | 1.5% |

177 commands with no handler are called by no script and no condition of the plugins counted; they can wait for the games' own gaps (the extenders, the console) or never be built.

## Commands with no handler, by uses

| # | Code | Command | FO3 calls | FO3 conditions | NV calls | NV conditions | Uses | Running share |
|--:|---|---|--:|--:|--:|--:|--:|--:|
| 1 | 0x11D1 | PlaceLeveledActorAtMe | 349 | 0 | 5 | 0 | 354 | 11.1% |
| 2 | 0x1249 | RemoveAllTypedItems | 0 | 0 | 295 | 0 | 295 | 20.3% |
| 3 | 0x116C | DamageObject | 112 | 0 | 89 | 0 | 201 | 26.6% |
| 4 | 0x11E2 | FireWeapon | 151 | 0 | 24 | 0 | 175 | 32.0% |
| 5 | 0x1223 | ForceTerminalBack | 89 | 0 | 80 | 0 | 169 | 37.3% |
| 6 | 0x11A0 | GetGroupMemberCount | 0 | 80 | 0 | 66 | 146 | 41.9% |
| 7 | 0x119F | Exists | 0 | 122 | 0 | 21 | 143 | 46.3% |
| 8 | 0x10A5 | RemoveMe | 27 | 0 | 107 | 0 | 134 | 50.5% |
| 9 | 0x1142 | Dispel | 43 | 0 | 56 | 0 | 99 | 53.6% |
| 10 | 0x1090 | GetTrespassWarningLevel | 0 | 44 | 0 | 29 | 73 | 55.9% |
| 11 | 0x118A | PushActorAway | 38 | 0 | 35 | 0 | 73 | 58.2% |
| 12 | 0x101B | GetLineOfSight | 25 | 1 | 46 | 0 | 72 | 60.4% |
| 13 | 0x10E6 | GetInCellParam | 30 | 17 | 0 | 18 | 65 | 62.4% |
| 14 | 0x10A6 | DropMe | 0 | 0 | 58 | 0 | 58 | 64.3% |
| 15 | 0x106D | IsWeaponSkillType | 0 | 2 | 0 | 50 | 52 | 65.9% |
| 16 | 0x10DB | GetAnimAction | 1 | 3 | 44 | 1 | 49 | 67.4% |
| 17 | 0x108F | GetCurrentAIProcedure | 10 | 5 | 32 | 0 | 47 | 68.9% |
| 18 | 0x1053 | ModDisposition | 41 | 0 | 0 | 0 | 41 | 70.2% |
| 19 | 0x111C | SetActorFullName | 10 | 0 | 26 | 0 | 36 | 71.3% |
| 20 | 0x1248 | SetSecuritronExpression | 0 | 0 | 36 | 0 | 36 | 72.4% |
| 21 | 0x118C | ClearOwnership | 33 | 0 | 1 | 0 | 34 | 73.5% |
| 22 | 0x11FB | SetRumble | 17 | 0 | 17 | 0 | 34 | 74.5% |
| 23 | 0x118B | SetActorsAI | 30 | 0 | 3 | 0 | 33 | 75.6% |
| 24 | 0x1189 | PurgeCellBuffers | 0 | 0 | 32 | 0 | 32 | 76.6% |
| 25 | 0x10BA | ScriptEffectElapsedSeconds | 6 | 0 | 20 | 0 | 26 | 77.4% |
| 26 | 0x11C2 | GetFactionRelation | 1 | 0 | 13 | 11 | 25 | 78.2% |
| 27 | 0x1213 | IsInCriticalStage | 0 | 0 | 24 | 0 | 24 | 78.9% |
| 28 | 0x118F | IsWeaponInList | 2 | 7 | 9 | 5 | 23 | 79.6% |
| 29 | 0x1266 | GetChallengeCompleted | 0 | 0 | 23 | 0 | 23 | 80.3% |
| 30 | 0x1070 | IsIdlePlaying | 11 | 0 | 11 | 0 | 22 | 81.0% |
| 31 | 0x1214 | RemoveFromAllFactions | 0 | 0 | 19 | 0 | 19 | 81.6% |
| 32 | 0x1212 | SetCriticalStage | 11 | 0 | 7 | 0 | 18 | 82.2% |
| 33 | 0x11B5 | ExitGame | 16 | 0 | 1 | 0 | 17 | 82.7% |
| 34 | 0x10D3 | SetForceSneak | 13 | 0 | 3 | 0 | 16 | 83.2% |
| 35 | 0x11BF | DisableNavMesh | 14 | 0 | 2 | 0 | 16 | 83.7% |
| 36 | 0x11C0 | EnableNavMesh | 11 | 0 | 5 | 0 | 16 | 84.2% |
| 37 | 0x11EB | SetVATSTarget | 6 | 0 | 10 | 0 | 16 | 84.7% |
| 38 | 0x1277 | AlwaysShowActorSubtitles | 0 | 0 | 16 | 0 | 16 | 85.2% |
| 39 | 0x1074 | GetMinorCrimeCount | 0 | 14 | 0 | 0 | 14 | 85.7% |
| 40 | 0x1104 | SetWeather | 0 | 0 | 14 | 0 | 14 | 86.1% |
| 41 | 0x1226 | IsGoreDisabled | 8 | 0 | 6 | 0 | 14 | 86.5% |
| 42 | 0x121E | Tan | 13 | 0 | 0 | 0 | 13 | 86.9% |
| 43 | 0x10D2 | GetForceSneak | 12 | 0 | 0 | 0 | 12 | 87.3% |
| 44 | 0x1122 | SetPackDuration | 0 | 0 | 12 | 0 | 12 | 87.7% |
| 45 | 0x1146 | Reset3DState | 0 | 0 | 12 | 0 | 12 | 88.1% |
| 46 | 0x11C4 | SetNPCRadio | 6 | 0 | 6 | 0 | 12 | 88.4% |
| 47 | 0x1148 | DispelAllSpells | 0 | 0 | 11 | 0 | 11 | 88.8% |
| 48 | 0x123A | RemoveReputation | 0 | 0 | 11 | 0 | 11 | 89.1% |
| 49 | 0x123B | AddReputationExact | 0 | 0 | 10 | 0 | 10 | 89.4% |
| 50 | 0x11DA | GetIsAlignment | 0 | 6 | 0 | 3 | 9 | 89.7% |
| 51 | 0x11DB | ResetQuest | 3 | 0 | 6 | 0 | 9 | 90.0% |
| 52 | 0x1211 | AttachAshPile | 5 | 0 | 4 | 0 | 9 | 90.3% |
| 53 | 0x10A0 | GetFurnitureMarkerID | 0 | 0 | 8 | 0 | 8 | 90.5% |
| 54 | 0x10F5 | ForceFlee | 0 | 0 | 8 | 0 | 8 | 90.8% |
| 55 | 0x1155 | SetItemValue | 6 | 0 | 2 | 0 | 8 | 91.0% |
| 56 | 0x1161 | IsActor | 7 | 0 | 1 | 0 | 8 | 91.3% |
| 57 | 0x1193 | HasFriendDisposition | 0 | 8 | 0 | 0 | 8 | 91.5% |
| 58 | 0x1240 | TriggerScreenSplatter | 0 | 0 | 8 | 0 | 8 | 91.8% |
| 59 | 0x111B | SetCellFullName | 7 | 0 | 0 | 0 | 7 | 92.0% |
| 60 | 0x1144 | TriggerHitShader | 7 | 0 | 0 | 0 | 7 | 92.2% |

73 more commands with uses are in `docs/openfallout/coverage/tracker/command-usage.csv`; the table is 133 commands with 3201 uses in all.

## Stand-ins, by uses

| # | Code | Command | FO3 calls | FO3 conditions | NV calls | NV conditions | Uses | Running share | Note |
|--:|---|---|--:|--:|--:|--:|--:|--:|---|
| 1 | 0x1123 | PlayMagicShaderVisuals | 242 | 0 | 205 | 0 | 447 | 20.2% | accepted and does nothing: the shaders of effects are not drawn |
| 2 | 0x1269 | SwapTextureOnRef | 0 | 0 | 383 | 0 | 383 | 37.5% | accepted and does nothing: the textures of a model are not swapped |
| 3 | 0x1125 | StopMagicShaderVisuals | 178 | 0 | 79 | 0 | 257 | 49.1% | accepted and does nothing: the shaders of effects are not drawn |
| 4 | 0x1129 | SetActorAlpha | 59 | 0 | 127 | 0 | 186 | 57.5% | accepted and does nothing: the transparency of a character is not drawn |
| 5 | 0x1275 | IncrementScriptedChallenge | 0 | 0 | 134 | 0 | 134 | 63.6% | accepted and does nothing: the challenges are not kept |
| 6 | 0x105F | SendAssaultAlarm | 13 | 0 | 93 | 0 | 106 | 68.4% | accepted and does nothing: there is no crime that could be reported |
| 7 | 0x11D7 | GetDestructionStage | 73 | 0 | 21 | 9 | 103 | 73.0% | answers -1: the stages of destruction are not kept |
| 8 | 0x11DD | ForceActiveQuest | 0 | 0 | 82 | 0 | 82 | 76.7% | keeps the switch and does nothing with it: there is no active quest to show |
| 9 | 0x11D5 | SetBroadcastState | 7 | 0 | 65 | 0 | 72 | 80.0% | keeps the switch and does nothing with it: there is no radio |
| 10 | 0x118E | IsLimbGone | 15 | 1 | 51 | 0 | 67 | 83.0% | answers 0: dismemberment is not built and nothing sets the limb bits |
| 11 | 0x1114 | PlayBink | 60 | 0 | 4 | 0 | 64 | 85.9% | accepted and does nothing: videos are not played |
| 12 | 0x125E | UnlockChallenge | 0 | 0 | 44 | 0 | 44 | 87.9% | accepted and does nothing: the challenges are not kept |
| 13 | 0x104C | GetDisposition | 0 | 22 | 0 | 14 | 36 | 89.5% | answers 0: the disposition of a character towards another is not worked out |
| 14 | 0x1224 | PipBoyRadioOff | 18 | 0 | 11 | 0 | 29 | 90.8% | accepted and does nothing: there is no radio |
| 15 | 0x11D6 | StartRadioConversation | 10 | 0 | 16 | 0 | 26 | 92.0% | accepted and does nothing: there is no radio |
| 16 | 0x125C | GetCasinoWinningsLevel | 0 | 0 | 24 | 0 | 24 | 93.1% | answers 0: the casino of New Vegas is not built |
| 17 | 0x1236 | AddItemToLeveledList | 2 | 0 | 20 | 0 | 22 | 94.1% | accepted and does nothing: the lists that scripts add to (vendors, loot) are not rolled |
| 18 | 0x11ED | ResetInventory | 4 | 0 | 17 | 0 | 21 | 95.0% | accepted and does nothing: an inventory is not rebuilt from the form of the character |
| 19 | 0x1196 | SetCellImageSpace | 14 | 0 | 2 | 0 | 16 | 95.8% | accepted and does nothing: image spaces are not drawn |
| 20 | 0x107F | HasBeenEaten | 0 | 1 | 6 | 4 | 11 | 96.2% | answers 0: nothing feeds on a dead character |
| 21 | 0x1200 | SetLocationSpecificLoadScreensOnly | 11 | 0 | 0 | 0 | 11 | 96.7% | keeps the switch and does nothing with it: load screens are not chosen by the place |
| 22 | 0x1201 | ResetPipboyManager | 5 | 0 | 4 | 0 | 9 | 97.2% | accepted and does nothing: the Pip-Boy keeps nothing that could be reset |
| 23 | 0x11E5 | MatchRace | 7 | 0 | 0 | 0 | 7 | 97.5% | accepted and does nothing: the race of a character does not change |
| 24 | 0x11E6 | SetPCYoung | 7 | 0 | 0 | 0 | 7 | 97.8% | keeps the switch and does nothing with it: the body of the player does not change with its age |
| 25 | 0x11FC | SetNoActivationSound | 7 | 0 | 0 | 0 | 7 | 98.1% | accepted and does nothing: an activation that does nothing makes no sound |
| 26 | 0x1225 | AutoDisplayObjectives | 5 | 0 | 2 | 0 | 7 | 98.4% | keeps the switch and does nothing with it: objectives are not shown on the screen by themselves |
| 27 | 0x11E4 | AgeRace | 4 | 0 | 2 | 0 | 6 | 98.7% | accepted and does nothing: the race of a character does not change |
| 28 | 0x11F4 | GetWeaponHealthPerc | 4 | 0 | 2 | 0 | 6 | 99.0% | answers 100: nothing wears a weapon out yet |
| 29 | 0x122C | ForceRadioStationUpdate | 3 | 0 | 3 | 0 | 6 | 99.2% | accepted and does nothing: there is no radio |
| 30 | 0x11DF | MatchFaceGeometry | 5 | 0 | 0 | 0 | 5 | 99.5% | accepted and does nothing: the face of a character does not change |
| 31 | 0x11FD | ClearNoActivationSound | 3 | 0 | 0 | 0 | 3 | 99.6% | accepted and does nothing: an activation that does nothing makes no sound |
| 32 | 0x118D | GetCauseofDeath | 0 | 0 | 1 | 1 | 2 | 99.7% | answers -1: the cause of a death is not kept |
| 33 | 0x1202 | SetPCToddler | 2 | 0 | 0 | 0 | 2 | 99.8% | keeps the switch and does nothing with it: the body of the player does not change with its age |
| 34 | 0x124B | SetHardcore | 0 | 0 | 2 | 0 | 2 | 99.9% | keeps the switch (the needs of the hardcore mode run from it; stimpaks over time, doctor's bags and ammo weight are not done) |
| 35 | 0x1205 | TriggerScreenBlood | 1 | 0 | 0 | 0 | 1 | 99.9% | accepted and does nothing: the blood on the screen is not drawn |
| 36 | 0x1231 | KillQuestUpdates | 1 | 0 | 0 | 0 | 1 | 100.0% | keeps the switch and does nothing with it: the messages of quest updates are not held back |
| 37 | 0x1241 | ClearScreenSplatter | 0 | 0 | 1 | 0 | 1 | 100.0% | accepted and does nothing: the blood on the screen is not drawn |

## Codes the table of commands does not have

These codes are called by the data and are in no command table (the commands of the script extenders start at 0x1400): 0x104F (22983), 0x1035 (2960), 0x1264 (60), 0x1262 (25).
