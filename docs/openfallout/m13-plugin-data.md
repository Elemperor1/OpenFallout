# M13 plugin data: fall, camera and sleep GMSTs, landing sounds

Read from the plugin files only (Fallout3.esm and FalloutNV.esm, and the five add-on plugins of each game that the Mac scanned too, which its report does not name; they add no matches), on the Mac in round 13B of M13. Values are the plugin records; nothing here comes from an executable. What it settles for M13:

- Fall damage: the plugins hold `fJumpFallHeightMin` = 600, `fJumpFallHeightExponent` = 1.65 and `fJumpFallVelocityMin` = 2785 (same in both games) and **no** `fJumpFallHeightMult`, `fFallLegDamageMult` or `iFallLegDamageChance`, so the formula's multiplier and the leg damage are not in the data.
- Dialogue camera: no GMST names a conversation camera distance, height, offset, blend or field of view (the 35 names found are AI timers, combat barks, V.A.T.S. cameras and the `fOverShoulder*` offset, position and blend settings of the player's over-the-shoulder view, none of which is a conversation camera), so those numbers are not in the data either.
- Sleep: no GMST names a heal rate per hour or a limit of hours. New Vegas adds `fHCSleepDeprivationRate` = 50 (hardcore mode).

Two things in the message below are wrong as it was sent (it is kept as received; the numbers here are the ones to use):

- The names it leaves out of the sound list because they only contain "Wasteland" or "Falls" are 12 in Fallout 3 and 9 in New Vegas, not the 10 and 7 it says. Counting the names it lists: Fallout 3 has 4 `AMBRWindWasteland`, 6 `AMBRInsectsWasteland` and 2 `DRSParadiseFalls` (12); New Vegas has the 6 insects, `UIWildWasteland` and the 2 `DRSParadiseFalls` (9). With them the totals reconcile: 23 records both games share plus 12 for New Vegas alone plus 9 left out is 44, and 23 plus the 12 left out is 35 for Fallout 3.
- The heading of its camera section (section 2) names seven search terms, but six of the 33 records it lists hold none of them: `fOverShoulderOffsetPointX`, `fOverShoulderOffsetPointZ`, `fOverShoulderPosX`, `fOverShoulderPosZ`, `fOverShoulderRotMult` and `fOverShoulderStartBlendDist`. Its scan matched on something more than the terms it names (the message does not say what), so the list is the scan's result and not what those seven terms alone give. The reading of it does not change: none of these is a conversation camera.

---

Message as received (round 13B):

```text
Plugin files only (Fallout3.esm, FalloutNV.esm; the five FO3 and five NV DLC plugins were also scanned and hold 0 matches for every pattern below). No executable was read for this message. Names, values and counts only; string-type GMSTs are listed by name and length, not text.

== 1. B5 ==
GMSTs beginning fJump / fFall / fSwim / fHeight (5 in each game, same FormIDs and same values in FO3 and NV; there is no fHeight and no fFall GMST record in either):
000C6E66 fJumpFallHeightExponent = 1.65
000946C6 fJumpFallHeightMin = 600
00061540 fJumpFallVelocityMin = 2785
00053615 fJumpSwimmingMult = 2
000CC61D fSwimRadiationDamageMult = 0.1
(fJumpFallHeightMult, fFallLegDamageMult, iFallLegDamageChance and the other fJump* names are not in any plugin.)

Sound records whose editor ID contains Land or Fall: FO3 35 records, NV 44 (FNAM given; a path ending in a backslash is a folder, not a single file). 10 FO3 and 7 NV of them are only the word "Wasteland"/"Falls" inside another name (AMBRWindWasteland A01 A02 B01 B02 [FO3 only], AMBRInsectsWasteland Cricket01 02 Flies01 02 Rustle01 02, UIWildWasteland [NV only], DRSParadiseFalls Open/Close [both]) and are left out below.
Same in both games unless marked:
0005A725 FSTLandHardHeavy  fx\fst\landhard\
0005A724 FSTLandHardLight  fx\fst\landhard\
0004452F FSTBabyLand  fx\fst\baby\fst_baby_land.wav
0003FB0A FSTSandLand  FO3 fx\fst\gravel\land\ ; NV fx\fst\dirt\land\
0003E5CD FSTGrassLand  fx\fst\grass\land\
00035E5B FSTMetalHollowLand  fx\fst\metal_hollow\land\
0003420F FSTWoodLand  fx\fst\wood\land\
000212EB FSTDirtLand  fx\fst\dirt\land\
00020619 FSTWaterLand  fx\fst\water\land\
000205FF FSTConcBrokenLand  fx\fst\conc_broken\land\
0001EEC9 FSTMetalSheetLand  fx\fst\metal_sheet\land\
0001D8FD FSTMetalSolidLand  fx\fst\metal_solid\land\
0001D8E0 FSTConcSolidLand  FO3 fx\fst\conc_solid\land\ ; NV fx\fst\gravel\land\
00000220 FSTSnowLand  FO3 no file name ; NV fx\xxx_placeholder_silence.wav
0000021E FootSoundWoodLand  fx\fst\woodland\
0000021D FootSoundWaterLand  fx\fx\bullet\impact\concrete\fx_bullet_impact_concrete_04.wav
0000021C FootSoundStoneLand  fx\fst\conc_solid\land\
0000021B FootSoundMetalLand  fx\fst\metal_solid\land\
0000021A FootSoundGrassLand  fx\fst\grassland\
00000219 FootSoundEarthLand  fx\fst\earthland\
000786FC OBJVertibirdLand  fx\obj\vertibird\obj_vertibird_land.wav
0002A304 DRSRivetCityBridgeLanding  fx\drs\drs_rivetcitybridge_landing.wav
000A6766 NPCRobotLibertyPrimePylonGrabFall  fx\npc\robotlibertyprime\npc_robotlibertyprime_pylongrab_fall.wav
NV only (all in FalloutNV.esm, none in FO3):
0010FA68 NPCBigHornerJumpLand  fx\npc\bighorner\jumpland\
00146710 NPCGiantRatLand  fx\npc\giantrat\land\
00151DD1 NPCMoleRatLand  fx\npc\molerat\land\
00151DDB NPCSuperMutantLand  fx\npc\bighorner\jumpland\
0015EB51 NPCDeathClawLand  fx\npc\bighorner\jumpland\
0015B6DD NPCMantisLand  fx\npc\mantis\land\
00156ED8 NPCSporeCarrierLand  fx\npc\giantrat\land\
0014393A NPCCazadoreBodyfall  fx\npc\cazadore\bodyfall\
0016347B NPCIdleCrucifixLand  fx\fol\idle\crucifix\land\
0016D531 FSTBabyRattleLand  fx\phy\tumbleweed\
001694C5 OBJVertibirdCrashLand  fx\obj\vertibird\vertibird\sfx_vertibird_crashland.wav
00155E98 EMTWaterFallLP  fx\emt\waterfall\emt_waterfall_lp.wav

== 2. C1 data: GMSTs containing Dialog / Conversation / Speak / Camera / Zoom / FOV / Focus ==
FO3 33 records; NV the same 33 with the same values plus 2 more (listed after).
00021402 fActionPointsFOVMult = 0.25
0009DEF9 fAISocialchanceForConversation = 25
0009DEFA fAISocialchanceForConversationInterior = 25
00071F75 fAISocialRadiusToTriggerConversationInterior = 1000
000BB229 fAISocialTimerForConversationsMax = 60
000BB228 fAISocialTimerForConversationsMin = 30
000302C5 fCombatDetectionDialogueIdleMaxElapsedTime = 15
000302C4 fCombatDetectionDialogueIdleMinElapsedTime = 5
000220C0 fCombatDialogueAttackMaxElapsedTime = 30
000220BF fCombatDialogueAttackMinElapsedTime = 5
0001505B fCombatSpeakAttackChance = 0.01
00015651 fCombatSpeakHitChance = 0.01
00015652 fCombatSpeakHitThreshold = 0.01
0001EF0D fGunParticleCameraDistance = 2048
00015615 fOverShoulderOffsetPointX = 98.4
00015616 fOverShoulderOffsetPointZ = -24.8
00015617 fOverShoulderOffsetPointZoomingX = 0
00015618 fOverShoulderOffsetPointZoomingY = 10000
00015619 fOverShoulderOffsetPointZoomingZ = -10
00015613 fOverShoulderPosX = 40
00004267 fOverShoulderPosZ = -10
00015614 fOverShoulderRotMult = 0.6
00004268 fOverShoulderStartBlendDist = 40000
000BFE68 fVATSCameraCutAwayDistance = 30
0006C30D fVATSCameraDollyMin = 1536
0004BBA6 fVATSCameraDollyTime = 0.38
0001CF91 fVATSMoveCameraLimbMult = 0.5
0001CF82 fVATSMoveCameraXPercent = 0.15
0009BA47 fVATSSmartCameraCheckHeight = 64
0005B140 fVATSSmartCameraCheckStepDistance = 64
0001CF7D fVATSTargetFOVMultFar = 34
0001CF79 fVATSTargetFOVMultFarDist = 2750
0001CF86 fVATSTargetFOVMultNear = 50
NV only:
00175C31 fKillCameraCooldown = 20
00179A13 iDialogueDummySpeakThisIntOrBelow = 3
Reading of the names only: none of the 35 names describes a conversation-camera distance, height, side offset, blend time or field of view; the Dialog/Conversation/Speak names are AI timers and combat-bark chances, and fActionPointsFOVMult is the V.A.T.S. zoom factor.

== 3. D5 data: GMSTs containing Sleep / Rest / Wait / Heal / Rested ==
FO3 20 records, NV 25. Values equal in both unless marked.
00034822 fActionPointsHeal = 10
0002008D fActionPointsRestoreRate = FO3 4 ; NV 0.06
00026ECC fAVDHealRateEndurance10Bonus = 10
00026ECA fAVDHealRateEndurance6Bonus = 5
00026ECB fAVDHealRateEndurance7Bonus = 5
00026ECD fAVDHealRateEndurance8Bonus = 5
00026ECE fAVDHealRateEndurance9Bonus = 10
00026EC9 fAVDHealthEnduranceMult = 20
00034816 fAVDHealthEnduranceOffset = 0
000539F2 fAVDHealthLevelMult = FO3 10 ; NV 5
000AE66A fAVDNPCHealthEnduranceMult = 5
000AE66B fAVDNPCHealthLevelMult = 5
0002A444 fCombatFleeWaitTime = 30
00075C1F fCombatItemRestoreTimer = 3
0001512E fGrenadeRestitution = 0.01
0002A31C fKnockdownCurrentHealthThreshold = 50
00063E85 iAICombatRestoreHealthPercentage = 60
0002689D iXPDeathRewardHealthThreshold = 40
00073D59 sNoSleepHostileActorsNear (string, 41 chars)
00073D5F sNoWaitHostilActorsNear (string, 40 chars)
NV only:
00162BCA fHCSleepDeprivationRate = 50
0017B360 sSleepDeprevationDecrease (string, 42 chars)
0017B361 sSleepDeprevationIncrease (string, 42 chars)
0017B362 sSleepDeprevationNotSick (string, 45 chars)
00173F91 sRestartBecauseContentRemoved (string, 101 chars)
No GMST in either game has Rested in its name, and none names a sleep-hours limit or a per-hour heal rate.

== 4. Correction to A4 ==
NV, the ten commands with the most calls: QUST and INFO callers are 25 (RemoveAllTypedItems 21 INFO + 3 QUST, PushActorAway 1 INFO) plus 1 PACK (PushActorAway). My A4 message said 26 pairs; 25 plus 1 PACK is right.
FO3 ModDisposition (0x1053): 41 scripts = INFO 39 + QUST 2. The --find list stops at 40 lines (39 INFO + QUST 0x00014E9E); the line cut is the second QUST. A scan of the FO3 plugins finds the opcode in QUST records in only one FormID: 00014E9E (editor ID MS11), once in Fallout3.esm and once in BrokenSteel.esm (the same FormID overridden), so I read the cut line as the BrokenSteel.esm copy of 00014E9E. That is my inference from the scan, not a printed line.

== Housekeeping ==
Plugin reads only, run with nice -n 10. New scratch files: scratch-m13/b13_gmst.py, scratch-m13/b13_qust.py, scratch-m13/data13b/ (GMST and sound lists). Nothing in the repository or the game folders was changed, nothing committed or pushed, no game started, no keys, pointer, screenshots or lldb; the front app was never touched. I did not go back to the saved Part B notes.

MAC ROUND 13B - DONE
```


---

# M13 plugin data, round 13C: effect scripts, the well-rested bonus and the hardcore needs

Read from the plugin files only (FalloutNV.esm and Fallout3.esm with their add-ons), on the Mac in round 13C of M13. What the port does with it:

- The blocks of the script of a magic effect are numbered 17 (ScriptEffectStart), 18 (ScriptEffectFinish) and 19 (ScriptEffectUpdate), the same in both games.
- A magic effect of the script archetype (1) holds its script in the associated-item field of its data (there is no SCRI subrecord). Casting `WellRestedSpell` (duration 1440) therefore runs the start block of `WellRestedMessageEffectSCRIPT` (a message, `player.resetHealth`, the perk `WellRestedPerk`) and, when the 1440 seconds are over, its finish block (a message, `RemovePerk`).
- New Vegas stores the hardcore needs as DEHY, HUNG and SLPD records: five stages each, from 200 to 1000, each naming a spell (the first four are diseases that lower attributes, the last an ability with DamageHealth 10000). The port reads them from the plugin and puts the spell of the stage the level is in on the player. Only `SetHardcore` (one script, the casual or hardcore question of the intro) switches the mode on or off.

Message as received (round 13C):

```text
Plugin files only (FalloutNV.esm + 9 add-on plugins; Fallout3.esm + 5 add-on plugins). No executable was opened, no game started. I read the records with my own read-only Python reader in scratch-m13 (b13c_lib.py, b13c_scpt.py, b13c_magic.py), not with esmtool; esmtool was not needed. "NV = FO3" below means byte-identical records (I compared SCHR, SCTX and SCDA bytes).

== 1. Scripts (SCPT) - NV and FO3 identical; each record is carried by the base master only, no add-on overrides ==
SCHR layout read as: refcount, compiled size, variable count, then u16 type (0 object, 1 quest, 0x100 effect), then u16 flags (all three have flags 0x1).

(a) 00062745 WellRestedMessageEffectSCRIPT: type 0x100 = effect; refcount 4, compiled size 116, variables 0.
Compiled block TYPE NUMBERS in order: Begin ScriptEffectStart = 17, Begin ScriptEffectFinish = 18.
SCTX in full:
scn WellRestedMessageEffectSCRIPT

; effect's job is to simply give a message when the effect starts and ends

begin ScriptEffectStart
	showMessage AbilityWellRestedStartMsg
	player.resetHealth		; completely heal the player and limbs

	If HasPerk WellRestedPerk == 0						; Give the player the perk if he doesn't already have it.
		AddPerk WellRestedPerk
	EndIf
end

begin ScriptEffectFinish
	showMessage AbilityWellRestedEndMsg
	RemovePerk WellRestedPerk
end

(b) 00062695 WellRestedAbilityEffectSCRIPT: type 0x100 = effect; refcount 1, compiled size 27, variables 0. Block: Begin ScriptEffectFinish = 18 (only block).
SCTX in full:
scn WellRestedAbilityEffectSCRIPT

; effect's job is to cast the Well-Rested spell on the player

begin ScriptEffectFinish
	CastImmediateOnSelf WellRestedSpell
end

(c) 000B5431 PlayerBedSCRIPT: type 0x0 = object; refcount 2, compiled size 206, variables 1 (short playerSleep). Blocks in order: Begin OnActivate = 2, Begin menumode = 1, Begin gamemode = 0.
SCTX in full:
scn PlayerBedSCRIPT

short playerSleep

begin OnActivate
	if isActionRef player == 1
		; set playerBed ref variable to me
		set playerSleep to 1
	endif
	Activate
end

begin menumode
	; player has activated me and is sleeping
	if playerSleep == 1 && IsPCSleeping == 1
		set playerSleep to 2
	endif
end

begin gamemode
	if playerSleep > 0
		if playerSleep == 2
			; player has actually slept in this bed
			; give player Well-Rested spell and then clear variable
			player.CastImmediateOnSelf WellRestedSpell
		endif
		; clear variable
		set playerSleep to 0
	endif
end

Block numbers read off the whole SCPT group (every Begin line matched to its compiled Begin statement; Begin statement = op 0x0010, data = block type u16 + block length u32):
NV (3,224 scripts aligned): ScriptEffectStart 17 (255 scripts), ScriptEffectFinish 18 (57), ScriptEffectUpdate 19 (44); GameMode 0, MenuMode 1, OnActivate 2, OnAdd 3, OnEquip 4, OnUnequip 5, OnDrop 6, SayToDone 7, OnHit 8, OnHitWith 9, OnDeath 10, OnCombatEnd 12, OnPackageStart 15, OnPackageDone/OnPackageEnd 16, OnLoad 21, OnTrigger 24, OnStartCombat 25, OnTriggerEnter 26, OnTriggerLeave 27, OnReset 30, OnOpen 31, OnClose 32, OnGrab 33, OnRelease 34, OnDestructionStageChange 35, OnFire 36.
FO3 (1,865 aligned): same numbers (ScriptEffectStart 17 x62, Finish 18 x32, Update 19 x17); FO3 also has OnPackageChange 20, OnActorEquip 28, OnActorUnequip 29.
A handful of scripts show other numbers (for example 512, 1792); those are misreads of my statement walker on scripts it could not align, not real block numbers. ScriptEffectUpdate does not occur in the three scripts above; 19 comes from the other effect scripts.

== 2. Magic effect 00062744 and spell 00062697 - NV = FO3 ==
MGEF 00062744 WellRestedMessageEffect: subrecords EDID, FULL, DESC, DATA (72 bytes). DATA: flags 0x01002310, base cost 0, associated item 00062745 (= WellRestedMessageEffectSCRIPT), school -1, resist -1, archetype 1 (Script), actor value -1. There is no SCRI subrecord; the script reference is the "associated item" field in DATA.
SPEL 00062697 WellRestedSpell: SPIT type 0 (Actor Effect), cost 0, level 0, flags 0x00. One effect: MGEF 00062744, magnitude 0, area 0, duration 1440, range 0, actor value -1. No SCIT subrecord.
Companion chain, same in both games: MGEF 00062692 WellRestedAbilityEffect (archetype 1 Script, associated item 00062695 = WellRestedAbilityEffectSCRIPT, flags 0x09002390) is the only effect of SPEL 00062696 PlayerWellRested (type 4 Ability, flags 0x2E, effect duration 0, magnitude 0). Nothing in the PERK/SPEL/NPC_/CREA records I scanned holds form 00062696 or 00062697 in an effect, perk-entry or spell-list subrecord, so who grants PlayerWellRested is not in those subrecords (it would be a script command, an object or a record type I did not index).

== 3. NV hardcore needs (FalloutNV.esm only; the add-ons have no DEHY/HUNG/SLPD) ==
Three record types, 5 records each, each with a single DATA of 8 bytes = u32 threshold + u32 form id of the stage spell. Thresholds 200 / 400 / 600 / 800 / 1000 (unit not stored in the plugin). All stage spells are in FalloutNV.esm. Actor value numbers: 5 Strength, 6 Perception, 7 Endurance, 8 Charisma, 9 Intelligence, 10 Agility, 16 Health (names from the effects' editor IDs; all effects have duration 0, magnitude as shown).
Stage spells 200..800 are SPEL type 1 (Disease); stage 1000 is type 4 (Ability) with DamageHealth (MGEF 00033595, AV 16, magnitude 10000, flags 0x75). The Reduce* effects are archetype 0 (Value Modifier), flags 0x76.
Effect MGEFs: ReduceStrength 00032DE5, ReduceCharisma 00032DDC, ReducePerception 00032DE3, ReduceEndurance 00032DE6, ReduceIntelligence 00032DE1, ReduceAgility 00032DE4.

DEHY (editor ID; form id; threshold -> stage spell id, editor ID: effects):
Water1 00161901; 200 -> 0015FA1C Dehydration200: ReduceEndurance 1
Water2 00161902; 400 -> 0015FA1D Dehydration400: ReduceEndurance 2, ReducePerception 1
Water3 00161903; 600 -> 0015FA1E Dehydration600: ReduceEndurance 3, ReducePerception 2, ReduceIntelligence 1
Water4 00161904; 800 -> 0015FA1F Dehydration800: ReduceEndurance 3, ReduceAgility 2, ReducePerception 2
Water5 00161905; 1000 -> 0015FA20 Dehydration1000: DamageHealth 10000 (Ability)
HUNG:
hunger1 00161906; 200 -> 0015FA21 Starvation200: ReduceStrength 1
hunger2 00161907; 400 -> 0015FA22 Starvation400: ReduceStrength 2, ReduceCharisma 1
hunger3 00161908; 600 -> 0015FA23 Starvation600: ReduceStrength 3, ReduceCharisma 2, ReducePerception 1
hunger4 00161909; 800 -> 0015FA24 Starvation800: ReduceStrength 3, ReduceCharisma 2, ReducePerception 2
hunger5 0016190A; 1000 -> 0015FA25 Starvation1000: DamageHealth 10000 (Ability)
SLPD:
sleep1 0016190B; 200 -> 0015FA26 SleepDeprivation200: ReduceAgility 1
sleep2 0016190C; 400 -> 0015FA27 SleepDeprivation400: ReduceAgility 2, ReduceIntelligence 1
sleep3 0016190D; 600 -> 0015FA28 SleepDeprivation600: ReduceAgility 3, ReduceIntelligence 2, ReduceEndurance 1
sleep4 0016190E; 800 -> 0015FA29 SleepDeprivation800: ReduceAgility 3, ReduceEndurance 2, ReduceIntelligence 2
sleep5 0016190F; 1000 -> 0015FA2A SleepDeprivation1000: DamageHealth 10000 (Ability)

GMSTs and globals: no GMST and no GLOB has "Hardcore" in its name in either game, and no GLOB has an HC prefix. The HC-prefixed GMSTs in NV: 00162BC9 fHCStarvationRate = 25; 00162BCA fHCSleepDeprivationRate = 50. (There is no fHC... entry for dehydration.) String GMSTs by name only: 00161FE6 sDehydrationDecrease, 00161FE7 sDehydrationIncrease, 00161FE8 sDehydrationNotSick; 0017B360 sSleepDeprevationDecrease, 0017B361 sSleepDeprevationIncrease, 0017B362 sSleepDeprevationNotSick. FO3 has none of the DEHY/HUNG/SLPD records, none of these GMSTs and no script that mentions hardcore.

== 4. What switches hardcore mode on or off (NV; FO3 none) ==
Source names seen in the plugins use the command names; two have aliases in the exe's command table, so a text search for "hardcore" misses their callers (IsAlwaysHardcore is written "behemoth", DisableHardcoreTracking is written "babby"). I matched callers by compiled opcode AND source name.
- SetHardcore (opcode 0x124B): called by exactly one record: SCPT 0011F291 VCG01CasualHardcoreMessageSCRIPT (object type, 161 bytes compiled, blocks OnActivate = 2 and GameMode = 0; SetHardcore 1 and SetHardcore 0 both appear in it). It is reached from INFO 001057E7 (quest 00104C1C VCG01), whose result script activates the reference VCG01CasualHardcoreMessageREF. The record carrying the script is the only SetHardcore caller in all 10 NV plugins.
- DisableHardcoreTracking (0x126A): no caller in the plugins.
- IsAlwaysHardcore (0x126B) callers, 4 quests: 00133075 vHDBattleController, 00137AB9 VHDLegionBattle, 00154233 VHDIndependentBattle, 00154234 VHDHouseBattle (the source line next to each is a comment about an achievement).
- IsHardcore (0x124A) readers, 9 records: QUST 00059C85 CGTutorial; SCPT 00059C86 CGTutorialSCRIPT, 00104C01 VCGTutorialSCRIPT, 001209D1 VNPCFollowersQuestSCRIPT, 0016007A HVBunkerExplosionScript, and in add-ons 0100C7DC NVDLC03AuralStealthSuitQuestSCRIPT (OldWorldBlues.esm), 0100CDCC NVDLC04AutoInjectQuestSCRIPT (LonesomeRoad.esm), 0100E883 NVDLC01GlobalToxicDamageSCRIPT and 010139DD NVDLC01GlobalToxicBarkTriggerSCRIPT (DeadMoney.esm).
- No GLOB and no GMST is used as a hardcore switch; the plugins hold no hardcore global. Other records that only mention the word in source comments: SCPT 0013408C RoseofSharonCassidyScript, VCG01CasualHardcoreMessageSCRIPT; DeadMoney QUST 01000FBF NVDLC01MQ01 and SCPT 01006E92 NVDLC01BombCollarQuestSCRIPT, 010039E4 NVDLC04WreckageDoorSCRIPT, 010039E6 NVDLC04MojaveDoorSCRIPT, 01013F24 NVDLC03AuralStealthSuitBarksSCRIPT, 01013F25 NVDLC03AuralStealthSuitObjectBACKUPSCRIPT contain the word "hardcore" in source (not opcode-matched; I did not confirm which of the four commands they call).

== Housekeeping ==
Plugin reads only, run with nice -n 10; the first build of the record index took under 4 seconds for both games. New scratch files: scratch-m13/b13c_lib.py, b13c_scpt.py, b13c_magic.py and scratch-m13/data13c/ (record index caches and script/magic dumps). Nothing in the repository or game folders changed; nothing committed; no game started; the front app was never touched.

MAC ROUND 13C - DONE
```
