# M13 plugin data: fall, camera and sleep GMSTs, landing sounds

Read from the plugin files only (Fallout3.esm, FalloutNV.esm, and the add-on plugins of both games, which add no matches), on the Mac in round 13B of M13. Values are the plugin records; nothing here comes from an executable. What it settles for M13:

- Fall damage: the plugins hold `fJumpFallHeightMin` = 600, `fJumpFallHeightExponent` = 1.65 and `fJumpFallVelocityMin` = 2785 (same in both games) and **no** `fJumpFallHeightMult`, `fFallLegDamageMult` or `iFallLegDamageChance`, so the formula's multiplier and the leg damage are not in the data.
- Dialogue camera: no GMST names a conversation camera distance, height, offset, blend or field of view (the 35 names found are AI timers, combat barks and V.A.T.S.), so those numbers are not in the data either.
- Sleep: no GMST names a heal rate per hour or a limit of hours. New Vegas adds `fHCSleepDeprivationRate` = 50 (hardcore mode).

---

Message as received (round 13B):

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
