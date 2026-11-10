# M13 executable reads: falling, conversation camera, sleep/wait

Read-only static examination, 2026-11-10. PASS = read and understood; FAIL = read and contradicts the stated expectation; NOT TRACED = the requested rule is not fully established. A setting or record's existence does not establish that a particular path uses it.
Addresses are preferred-image virtual addresses, not file offsets. NV precedes FO3 throughout. No game was launched; no executable, plugin, asset, script source, game text, or disassembly is reproduced here. Only the base masters were evaluated; DLC, mods and local INI overrides can change effective behavior.
Verified PE file versions: FalloutNV.exe 1.4.0.525; Fallout3.exe 1.7.0.3. Evidence: direct PE bytes/Capstone, Ghidra decompilation for both games, command records, GMST/SOUN/IMAD/IDLE/PERK/SPEL/FURN/TACT/DEHY/HUNG/SLPD records, compiled script control flow, the intercom NIF node hierarchy, and the sleep/wait menu's numeric configuration in Fallout - Misc.bsa.
Exe SHA-256 NV: `89e7020487e80f20ddc92ea641c4433b2c6839a82f45a076e414d167d11dd0fe`; FO3: `df9f0e1ec2c5413a9cef2db2dde6d926b99707d528049af4eb15538587d55f99`.
Master SHA-256 FalloutNV.esm: `50991d36804b7d1e70df1afd7471b72f0e29d1b456ee2516a9717c002564e7c1`; Fallout3.esm: `d9fb0a33af495ddb43992b96ea74f2741b123fefdb1fcdcea28096f7649b0d06`.

### 1. PASS — Landing damage arithmetic

NV `0x8A62B0`, actor movement virtual slot `+0x250`, receives movement/frame arguments, calls `0x92F260`, then tests controller landing flag `+0x414 & 0x80` through `0x87CEE0` → `0x621270`; `+0x410` is the flag wrapper, not its bits.
Height reader `0xC70550` obtains h = stored starting height at controller `+0x544` minus current world Z; a zero starting-height field returns zero. This is height loss, not elapsed airborne time or impact velocity.
For h <= fJumpFallHeightMin, no fall damage; otherwise D = fJumpFallHeightMult × (h − fJumpFallHeightMin)^fJumpFallHeightExponent. No upper cap occurs in this calculation.
Both masters set fJumpFallHeightMin = 600 (`000946C6`) and fJumpFallHeightExponent = 1.65 (`000C6E66`); fJumpFallHeightMult is absent, leaving executable default 0.025. Exe-only defaults are 256, 2, 0.025 respectively.
Thus the first damaging height is strictly greater than 600; growth is a power, not linear. At h = 11,700, D is approximately 118,229 before difficulty and eligibility checks.
NV rounds the subtraction, the power result (`0x4DD130`/`0x4DD150`), and the multiplied result to float32. Exact stored exponent = 1.649999976158142; multiplier = 0.02500000037252903.
FO3 `0x709320`, height reader `0x8ED050`, starting-height field `+0x3D0`, landing flag `+0x294 & 0x80`: same arithmetic and master values. FO3 clears that flag before its height test.
fJumpFallVelocityMin = 2785 in both masters (default 600), but neither traced damage calculation reads it; neither uses fJumpFallSkillBase, fJumpFallSkillMult or fJumpFallRiderMult.

### 2. PASS — Health damage, difficulty and direct exemptions

NV passes D, zero fatigue damage and a null attacker to virtual `+0x338`, implementation `0x89D6F0`; this calls difficulty helper `0x8808A0` → `0x648CB0`, then damages Health AV `0x10` through virtual `+0x3AC`.
This is an actor-value damage operation, not a direct base-health assignment; it bypasses weapon-hit armour DR/DT calculation. No fall-specific perk, SPECIAL or hardcore multiplier appears in this arithmetic-to-health path.
For difficulty VE/E/N/H/VH, fDiffMultHPToPCVE/E/N/H/VH defaults are 0.5/0.75/1/1.5/2; fDiffMultHPByPCVE/E/N/H/VH defaults are 2/1.5/1/0.75/0.5. Neither master overrides them.
The choice is whether the recipient is the player, even with the null attacker: player health loss = D × ToPC; eligible NPC health loss uses ByPC.
NV creature-base test `0x5F0C40` checks form type `0x2B` and flag `base+0x30 & 0x20`, making D zero. FO3 `0x54A8F0` checks the corresponding base flag at `+0x34 & 0x20`.
NV non-player health damage also skips player teammates: `0x566950` reads actor byte `+0x18D`, set by SetPlayerTeammate `0x11C5`, handler `0x5D9370` → `0x8BCA90`; FO3 has no corresponding teammate test here.
The other exemption is a worldspace flag (correcting the earlier cell attribution). NV `0x575D70` → `0x54DDD0` obtains exterior cell `+0xC0`; `0x586320` tests worldspace `+0x4C & 0x40`. Non-player humanoids (virtual `+0x218` true, `0x8D0360`) skip health damage when this bit is set; creatures return false (`0x47C850`).
FO3 `0x4E8880` → `0x4D3780` → `0x501190` has the same worldspace-bit exemption. Its damage helper `0x6FD240`, difficulty helper `0x585150`, Health AV `0x10` and virtual `+0x3A8` implement the same difficulty mechanism.

### 3. PASS — Player leg damage

NV `0x8A62B0` performs two independent random-integer-modulo-100 tests, each strictly less than iFallLegDamageChance = 50.
Successful tests damage LeftMobilityCondition `0x1D` and RightMobilityCondition `0x1E`, respectively, by raw D × fFallLegDamageMult = 0.5 through the actor-value damage virtual; these are the registered names, correcting the previous report's informal leg names.
This uses D before difficulty scaling, has no additional positive-damage threshold, and is inside the player-only branch; it does not assign the same random leg damage to NPCs.
FO3 `0x709320` uses the same AVs, chance, multiplier and player-only distinction. Both GMSTs are absent from both masters, so these are executable defaults.
NV AV registration `0x66F260` assigns both legs callback `0x8B9830` → `0x8B9240`; a negative change crossing from positive condition to condition <= 0 triggers the cripple transition. FO3 registration `0x5A3000` assigns `0x7181B0` → `0x717DC0`, with the same zero crossing.
Default condition is fAVDDefaultBodyPartCondition = 100 (both masters absent), returned by NV `0x643A50` and FO3 `0x581610`.
Movement helpers NV `0x647D10`, FO3 `0x5845B0` use fMoveOneCrippledLegSpeedMult = 0.85 or fMoveTwoCrippledLegsSpeedMult = 0.75; both masters and executable defaults agree. AV `0x48` can bypass this movement penalty.

### 4. PASS — Fatal falls, god mode and essential recovery

NV Health < 1: `0x93B7A0` → virtual `+0x4B8` (`0x9508F0`) → `0x898650` → generic death `0x89D900`, with a deferred-processing branch; FO3 `0x76E570/0x76E7C0` → `+0x4A8` (`0x76CC80`) → `0x71C320` → `0x71AC50` (deferred `0x6F2910`).
Ordinary death requests animation group NV `0xE0` / FO3 `0xA5`; player sound is MUSDeath `00078595`, `fx\mus\mus_death.wav` at NV `0x89EA87` / FO3 `0x71B6C9`. Animation/process state chooses physical-body activation rather than a special fall-death animation.
Physical-body branch NV `0x89EFFD` → `0xC6A350`, FO3 `0x71AC50` → `0x8D7B90`, traverses the scene bodies with operation 5 and requested motion value 1; operation 5 is not itself the motion-type argument. Animation interface virtual `+0xD8` receives death state.
Player death camera NV `0x9CA1F0`, FO3 `0x7D08A0`, creates cinematic request type `0xF` targeting the player and selects mode 4 (NV `0x9C6C30`); an existing mode-4/type-F request is not duplicated. This is the ordinary death camera, not a fall-specific camera.
NV life state `+0x108` 1/2 starts countdown `0x11A3B34`; subtract frame delta and reload via `0x8512F0` → `0x850760` when < 0 (`0x93FEDF..0x93FF85`). FO3 uses state `+0xFC`, countdown `0xF6EA20`, `0x6D5960` → `0x6D5750` (`0x78B194..0x78B240`). fPlayerDeathReloadTime = 5 seconds by default, absent from both masters; nonpositive selects the death-menu branch.
God mode: NV ToggleGodMode `0x5B6F80` → `0x9526A0`, global `0x11E07BA`, getters `0x952720/0x9526B0`; FO3 `0x51EDC0` → `0x76CEB0`, global `0x107BA65`, getter `0x76CEC0`. Fall-health helpers reject protected damage; NV `0x952730` / FO3 `0x76CED0` also reject negative Health and condition AVs `0x19..0x1F`. Hurt-landing sound selection still uses D.
Essential handling NV `0x89DE28..0x89DF74`, FO3 `0x71AED9..0x71B07B`: bEssentialTakeNoDamage:GamePlay = 1 gates protection; incapacitation supplies process virtual `+0xE8` with fEssentialDeathTime = 10. Life states 3/5 restore toward 0.3 × integer base Health (fEssentialHealthPercentReGain); both GMSTs are master-absent defaults.
NV essential eligibility `0x87F3D0` includes base/template essential status, actor `+0x140 & 0x80000000`, and non-hardcore teammates; FO3 `0x6F4A10` tests base/template `+0x34 & 2`.
Recovery: NV `0x888B50` (`0x889090..0x8891C1`) / FO3 `0x711D60` (`0x712068..0x7120C2`) handle life state 6 with process state 1/3; timer `process+0xA4` decrements by frame delta through NV `0x907620` / FO3 `0x749FB0`, read by `0x812870` / `0x73AEA0`. Below zero, set life state 0 and fully restore through `0x8A0960` / `0x6F6A80`.
NV additionally pauses teammate recovery while player predicate `0x953C50` (`player+0xDF0`) is true; this is a recovery condition, not an extra multiplier on D.

### 5. FAIL — ResetFallDamageTimer does not reset in this NV executable

Opcode `0x1134`: NV execute handler `0x5D10A0` obtains the actor controller through `0x9306D0`, calls `0x5D10F0`, and discards the returned float.
`0x5D10F0` only reads controller `+0x544`; it does not write a field or reset a timer. The actual field is a starting height, read for landing by `0xC70550`.
NV fall-state code stores that height at `0xCD446F` and `0xCD4855`; setter `0x573F20` also writes it. The actor relocation path `0x931620` calls the same ineffective getter.
FO3 PASS: handler `0x52ADB0` gets the controller through `0x764EA0` and writes zero to `+0x3D0`; `0x8ED050` subsequently returns zero height loss while that field remains zero.

### 6. PASS — Spatial resets after load, relocation and Enable

Neither landing calculation nor these controller/relocation paths tests a post-load or post-teleport grace duration. Protection comes from zero/new-position fall-height state, not a fixed number of immune seconds.
NV controller constructor `0xC741E0` initializes baseline `+0x544`, airborne timer `+0x548` and flags `+0x414` to zero; actor controller creation `0x930C70` uses it when a controller is absent. Ground departure `0xCD4800` and jump `0xCD4280` establish a new starting Z.
NV MoveToMarker `0x109E`, `0x5CCF50` → `0x5CCB20` → player request queue `0x93BE30`; consumer `0x93BEA0` writes destination Z (`request+0x10`) through `0x573F20`. Non-player relocation `0x573800` writes current Z through the same setter.
NV door activation `0x5180B0` → `0x518B70` builds destination XYZ/rotation and callback `0x518CF0`, then uses the same `0x93BE30/0x93BEA0` queue and new-Z baseline. Door loading adds no separate fall-damage timer.
NV SetPos `0x5C05F0` → `0x5C0740` → `0x575830/0x5620E0` → `0xC6E390` relocates Havok state and clears its velocity, but does not write the fall baseline or request a new controller state. Subsequent ground/jump/swim transitions still govern that state.
NV Enable `0x5C43D0` → `0x5AA5D0/0x5AA580` queues work; consumer `0x5AA720` → `0x573F40` enables/builds 3D. Existing actor virtual `+0x260` (`0x8860D0`) updates process level, not fall height; a newly created controller gets constructor zeros. Enable does not independently grant timed immunity.
FO3 constructor `0x8F1290`, used by controller creation `0x769230`, zeros baseline `+0x3D0`, timer `+0x3D4`, flags `+0x294`. MoveTo `0x528A60/0x528730` → queue `0x7882F0` → consumer `0x786560` → `0x782B00/0x7820F0` → position virtual `+0x2A8` (`0x766A40`) clears baseline and landing bit `0x80`, unlike NV's destination-Z baseline.
FO3 door activation `0x4B92B0` → `0x4B91C0` builds the same position request (callback `0x4B8DA0`) and uses `0x7882F0`; asynchronous actor reposition task `0x6F1300` also zeros baseline at `0x6F1F39`. SetPos `0x532410/0x522910` → `0x4E6580` → `0x8EA270` is a separate Havok relocation without that baseline write.
FO3 Enable `0x523BD0` → `0x518FF0` queues `0x1071BBC`; consumer `0x5190A0` → `0x4F91E0` builds/enables 3D and invokes existing actor virtual `+0x260` (`0x6FB550`), again with no fall-reset timer.
Separate stuck-air recovery: NV `0xCD3FB0` / FO3 `0x96B630` tests airborne timer > 1 and height loss < 0.10000000149011612 while next state is `0xB`, then requests ground state 0. NV updater `0xC6E2E0` accumulates step `+0x4E8` into `+0x548`; FO3 fields are `+0x3D4/+0x350`. This one-second test is not relocation grace.

### 7. PASS — Landing sounds, animation, pain and water

Hurt landing, player-only positive D: NV `0x8A62B0` / FO3 `0x709320` selects FSTLandHardHeavy `0005A725` when D > fHardLandingDamageThreshold:Audio = 500, otherwise FSTLandHardLight `0005A724`; both records use `fx\fst\landhard\`. These are additional hurt sounds, not the ordinary surface landing sound.
Ordinary animation selection NV `0x895110` / FO3 `0x71F3C0` changes JumpStart/JumpLoop to JumpLand on ground state 0: group `0xE5` / `0xAA`; directional movement bits 1/2/4/8 instead select NV `0xF1..0xF4` / FO3 `0xB3..0xB6`. The same branch calls surface sound NV `0x8391B0` / FO3 `0x6C5B70`.
Sound lookup NV `0x839AB0` / FO3 `0x6C4480` builds FST + surface group + Land, follows at most two fallback groups, then falls back to FSTConcSolidWalkR. Exact sound records and material mappings are tabulated below; player baby state substitutes FSTBabyLand `0004452F`.
Pain NV `0x8A7D50` / FO3 `0x6FF9E0` requires eligible alive/high-process state. Clock = actor `+0x114/+0x108`; deadline = process `+0x344/+0x33C`. Suppress strictly when deadline − 1.5 < now < deadline; otherwise set deadline = now + fPainDelay (default 1.5).
For the player outside VATS mode, this requests GetHit IMAD `00000162`, fGetHitPainMult = 1.5, through NV `0x5299A0` / FO3 `0x4C2700`. Outside MenuMode it also requests 500 ms rumble (fRumblePainTime = 0.5), strength 0.75 on both motors (fRumblePainStrength).
GetHit duration is 0.4 seconds in both masters. Each request appends a distinct effect; first update stamps the effect clock, and expiry is elapsed > duration, not >=. NV `0x5297B0/0x5316C0`, manager `0x86FD90/0x86FF50`; FO3 `0x4C1DA0/0x4C46E0`, manager `0x6EA9B0`. The clock advances with unpaused simulation; retrigger does not replace the previous instance.
Controller update NV `0xC73170` / FO3 `0x8EFE30` derives support flag `0x100` from physics support result 2 and contact eligibility (NV `+0x480/+0x478`, FO3 `+0x300/+0x2F8`, with force/support overrides). Air update `0xCD3FB0/0x96B630` then sets landing `0x80` and requests ground; swimming update `0xCD44E0/0x96BCA0` does not set that landing flag.
Actor updates NV `0x888B50` / FO3 `0x711D60` use submerged fraction clamp((water height − feet Z) / actor height, 0, 1), helpers `0x885560/0x6F5EC0`. Ordinary non-flying actors become wet at 0.01, swimming at 0.7 and underwater at 0.875; swimming requests state 5, leaving it requests state 0.
Deep-water entry can therefore bypass a ground landing; shallow water can still produce fall damage. There is no blanket water exemption in D. The sound dispatcher selects Water when feet are below available water height and its water-state predicate permits it, independently of D.
Support on another actor has no separate fall-damage recipient or crush-damage call in these paths: damage targets the falling actor only. Whether a contact provides usable support is decided by the physics support test, not by a special actor-landing damage formula.

### 8. FAIL — Dialogue is not a fixed 90-unit speaker camera

NV opener `0x761A20` loads `Data\Menus\Dialog\dialog_menu.xml`, menu ID `0x3F1`, and stores the target at menu `+0x80`; it saves prior view mode at `+0x130`.
The normal branch calls `0x951A10(1)` for first-person view and `0x953060(target, progress, skipTurning)`, the executable's FocusOnActor path. Menu subtype 2 bypasses that opening call.
`0x953060` uses current camera world position and the target head's world bounding sphere; fallback is a configured head node, with NV also looking for `Bip01 Speaker`.
Look point = sphere centre with Z adjusted by fDlgLookAdj:Interface = −5; node fallback uses radius 32. No fixed forward distance, lateral offset or race-height constant is used by the normal FOV-focus branch.
Let r be that radius and d the camera-to-look-point distance: desired FOV = min(fDefaultFOV:Display, 100 × atan(r × fDlgFocus:Interface / d)); defaults are 75 and 3.2, atan in radians.
It updates player FOV fields `+0x670/+0x674` and player pitch/yaw; target geometry therefore affects framing, and initial pitch affects the turn. This is not a camera placed 90 units from the speaker.
A fallback outside the normal camera-object branch sets FOV through `0x950610`, player `+0x65C`: 15 + trunc(((d − 45) / 100) × 25). This corrects the previous report's erroneous distance/d + 15 interpretation; truncation is toward zero (`0xEC62C0`).
FO3 opener `0x63E9A0`, focus `0x775D80`: corresponding head-sphere/FOV rule and INI defaults; the added NV `Bip01 Speaker` string/fallback was not found in FO3.
FO3's fallback computes 15 − trunc(((d − 45) / 100) × −25), then sets FOV through `0x76FC10`, player `+0x5BC`, and scene projection `0xAAEAF0`; it is likewise not a distance offset.

### 9. PASS — Dialogue blend and tracking constants

NV menu update `0x762950`: opening progress p = min(elapsed / fDialogZoomInSeconds, 1), default 1.5 seconds; closing p = 1 − min(elapsed / fDialogZoomOutSeconds, 1), default 0.5 seconds.
Both masters leave these GMSTs absent; both executables register 1.5/0.5. fDialogSpeechDelaySeconds = 2, fDialogFocalDepthRange = 300, fDialogFocalDepthStrength = 0.65 are also defaults, not offsets.
NV focus uses a remaining-time step a = min(dt / ((1 − previousProgress) × 1.5), 1); each FOV/initial-angle update adds a × remaining difference. This is not a smoothstep curve.
After opening, angular correction is remaining angle × fDlgLookMult:Interface × dt, default multiplier 2; heading difference is normalized across ±pi.
Start/stop tracking thresholds are 13/0.2 degrees for pitch and heading, each scaled by current FOV / default FOV: fDlgLookDegStart/Stop and fDlgHeadingDegStart/Stop, all in Interface.
FO3 menu update `0x63F060` independently implements the same elapsed/1.5 opening and 1 − elapsed/0.5 closing progress, clamped by `0x90D870`; it passes progress to focus `0x775D80`.
The 0.5-second closing progress is not an independent proof that restoring the world FOV takes exactly 0.5 seconds; the ordinary FOV updater is described in item 11.

### 10. PASS — Conversation facing, controls and visibility

NV focus `0x953060` turns player pitch/yaw through `0x931E50/0x931D30`; when seated it adjusts player heading `+0x6E4`. FO3 `0x775D80` follows the same focus/turn approach. Initial player pitch affects the starting error, not a new target-offset constant.
The menu retains target `+0x80`; response update NV `0x762FF0` continues using it. There is no automatic reverse shot to the player or respondent swap per dialogue line; an explicit new conversation target can start a new focus operation.
NV actor dialogue setup `0x8B2170` creates package `0x1C` and enables head tracking through process `0x901120/0x901050`; FO3 `0x701FF0` uses process virtuals `+0x63C/+0x62C`. This is ordinary head tracking toward the other actor, with whole-body facing handled separately.
Menu ticks NV `0x762950` / FO3 `0x63F060` invoke speaker virtual `+0x264`, NV `0x8A5580` / FO3 `0x6FE810`: with a movement controller, no furniture/combat/active turn, and wrapped heading error > 1 degree, request a body turn toward player XYZ (`0x9DCE20` / `0x7E02B0`).
Turn update NV `0x9E7D70`, FO3 `0x7E1510`: step = turningSpeed × 0.01745329238474369 × dt × INI scale; clamp to remaining wrapped angle, snap within 1 degree. Character virtual `+0x354` (`0x8D3290/0x7289B0`) uses fCharacterDefaultTurningSpeed = 90 degrees/s; noncombat scale 1.5 gives 135 degrees/s.
Creature/robot virtual `+0x354` (`0x8D47F0/0x729C50`) uses nonzero base `+0x134`, otherwise fCreatureDefaultTurningSpeed = 45; creature scale 1.25 gives default 56.25 degrees/s. Combat scales are character 2.5, creature 1.25; the ordinary dialogue body-turn request itself excludes combat.
NV input dispatch `0x86F940` takes menu update `0x9481D0`, bypassing gameplay mouse/stick reader `0x9445B0`; FO3 `0x6E46E0` takes `0x76EE70`, bypassing gameplay player update `0x788350`. Menu exception predicates `0x709BC0/0x61AAB0` test UI state `+0x4BC == 2`; normal dialogue uses menu controls, not gameplay look/orbit.
NV head/hair byte `+0xE2`, set/cleared through `0x762130`, is bIAmInDialogue (`0x663D10`); `0x663050` keeps facial updates active despite distance. FO3 `0x63E9A0/0x59A63A` has the same flag. It does not hide the speaker's head.
Normal opening selects first person: NV `0x951A10(1)` culls the ordinary player root and unculls first-person root `player+0x694`; FO3 `0x7700B0(1)` does the same with `player+0x5F0`. Thus the third-person player body is hidden during the normal focused conversation.

### 11. PASS — Conversation exit and unusual speakers

NV closing update `0x762950` disables target turning, ramps progress down over 0.5 seconds, restores POV request flags `0x11F21D0/0x11F21D1` from saved menu `+0x130`, then deletes the menu. `0x93E860` consumes them through `0x950110`.
Closer `0x762160` clears dialogue states; `0x953CE0` clears player `+0x6CC/+0x6D0`. These paths do not save/restore preconversation world XYZ or pitch/yaw: the player remains at the current position/facing, with POV restored. Dialogue virtual `0x961D90` separately saves/restores seated heading `+0x6E4`.
NV ordinary FOV updater `0x95DE30` approaches its target by min(remaining difference, dt × 30 / fIronSightsFOVTimeChange), default 0.25: 120 degrees/s. Caller `0x943748` skips it during dialogue, then resumes it on exit; world/first-person target defaults are both 75, gated by bIronSightsZoomEnable:Combat = 1.
FO3 `0x63F060` restores POV flags `0x108D018/0x108D019` and refreshes camera `0x77D670`; closer `0x63D0A0` clears dialogue state. Player virtual `0x76D830` saves/restores seated `+0x640`; FOV updater `0x777DB0` uses the same 120-degrees/s rule. No separate universal 0.5-second FOV return is implied.
Both focus functions use head geometry/bounds or the configured head node; if neither is usable they skip target-framing/turn mathematics. Creatures and robots follow this same node/bounds rule, without race-specific camera offsets; their model geometry supplies their actual framing.
Both masters map CG02DinerIntercom `00031D47` (TACT) to `Clutter\Consoles\Intercom01.NIF`. Its child NiNode `Bip01 Head` has local XYZ (−0.014251708984375, 6.549276351928711, 1.108830213546753), identity rotation, scale 1: this intercom has a usable camera-target node.
NV fallback `0x953230` → `0x8A30F0`, FO3 `0x775E73`, searches configured `Bip01 Head` (initializers `0x4B7A05..0x4B7A37` / `0x4769DB..0x4769F9`). Node-only framing uses its world position, radius 32, look-Z adjustment −5; NV additionally accepts `Bip01 Speaker`. There is no intercom-specific distance.
CG02DinerIntercomSCRIPT `00077786` has identical compiled control flow in both masters: player activation invokes ordinary Activate at CG02 stages 35 <= stage < 38, otherwise ShowMessage; non-player activation invokes Activate. It issues no camera-offset command and uses the same dialogue exit path.

### 12. PASS — Distinct terminal, barter, repair and container entry paths

NV `0x953060` direct calls were located in dialogue opening/update and in `0x706AE0`, a general menu-closing path; this does not make all menus dialogue cameras.
NV ShowBarterMenu `0x1173`: `0x5CEEB0` → `0x704F80` → `0x72D250`, opening `Data\Menus\barter_menu.xml`; ShowRepairMenu `0x117A`: `0x5D5200` → `0x704690` → `0x7B7570`, opening `Data\Menus\repair_services_menu.xml`.
Those two menu constructors do not call FocusOnActor or the dialogue opener; a menu opened from dialogue can inherit an already-focused view without being a new dialogue-camera transition.
FO3 corresponding chains are `0x52A230` → `0x619B10` → `0x634870` (barter menu `0x41D`) and `0x52CA00` → `0x619A40` → `0x673790` (repair services `0x422`), opening the same respective XML filenames; barter's constructor likewise has no FocusOnActor call.
NV container opener `0x75B310` and FO3 `0x63BAD0` load `Data\Menus\container_menu.xml` (menu `0x3F0`); neither constructor calls FocusOnActor or the dialogue opener.
NV rendered-terminal setup `0x7FC970` allocates a separate NiCamera (`0xA712F0`, vtable `0x109CB9C`), sets its frustum at `0x7FD2E9`, and registers it through `0x709D50`. FO3 setup `0x69BDE0` likewise builds its own camera/projection; these are not the dialogue focus routine.
Terminal frustum construction uses tan(fDefaultFOV × c × fRenderedTerminalFOV), c = 0.01745329238474369 (stored degrees-to-radians constant), defaults 75 and 0.15, plus aspect/near-plane scaling; 0.15 is an INI multiplier, not a 0.15-degree field of view or a dialogue zoom time.
NV general menu closer `0x706AE0` calls `0x953060` again only when dialogue-active test `0x7050D0` succeeds, using current dialogue target `0x705190`; closing an auxiliary menu can therefore refocus an underlying conversation without making its constructor a dialogue camera.

### 13. FAIL — Normal player bed use has no lying sequence

NV furniture activation `0x5095B0` selects/validates a marker and distinguishes bed marker types below 10 from chairs; after restrictions, its bed branch calls `0x7054F0(true)` → sleep/wait menu opener `0x7BFC30`.
The furniture-movement path `0x953D80` is on the non-bed branch. The normal player bed branch does not walk/snap onto the bed, start an enter idle, adopt a lying pose, or play a get-up exit idle.
Menu `Data\Menus\sleep_wait_menu.xml`, ID `0x3F4`: confirmation `0x7C0220` optionally autosaves, starts the sleep fade, stores hours/sleep flag through `0x5C1A00` at player `+0x654/+0x658`, then advances hours.
Tick `0x7C0580` → `0x94DF80` advances one game hour per accumulated 1 second; completion/cancel calls close `0x7C01B0`, requesting fade-in and closing the menu. No enter/exit animation duration belongs to this path.
FO3 activation `0x4ADE00` likewise takes its bed branch directly through `0x61B900` → `0x674AA0`; confirm/tick/close are `0x6745A0/0x674990/0x674530`, per-hour update `0x76C8F0`.
Both masters contain BedSleep `0004BA0E`, BedDefaultIdle `00003B00`, BedDynamicIdle `00003B01` (`Characters\_Male\IdleAnims\DynamicIdle_Sleep.kf`) and BedExitFast `0009DE74`; their existence does not put them in this player menu sequence.

### 14. PASS — Furniture marker selection and placement

NV selector `0x5686B0`: enumerates enabled usable entries, filters bed/chair types and occupied entries, transforms marker-local positions by the furniture world transform, and chooses the closest eligible transformed position to the supplied actor position.
The strict less-than distance comparison preserves the first candidate on an exact tie; a two-sided bed therefore uses the closest eligible entry, not a fixed left/right preference.
Legacy entries have 12-byte XYZ offset, unsigned 16-bit orientation at `+0xC`, type byte at `+0xE`, stride 16; heading helper `0x568650` gives orientation / 1000 radians, added to furniture reference heading.
FO3 selector `0x4EC240` independently shows the same closest-entry comparison, 16-byte entries and unsigned orientation / 1000 plus reference heading (`0x4EC4B1..0x4EC4E0`).
These are selected entry coordinates, not proof that the player is teleported there: the normal bed branch in item 13 leaves placement alone. There is no post-sleep return-to-entry or return-to-old-position move in that path.

### 15. PASS — Sleep fade mechanism and timing

NV `0x7C0220` starts `0x700960(type 0, fFadeToBlackFadeSeconds, false)` only when the menu sleep flag is set; default duration is 0.3 seconds, absent from both masters.
FaderManager uses fullscreen scene geometry/material alpha with `Data\Textures\Interface\Faders\Black.dds`, on its below-menu layer; this path is not an IMAD or a menu tile-alpha animation.
Updater `0x7011D0` changes alpha by ±dt/duration, clamped to [0,1]: linear fade out in 0.3 seconds. Close `0x7C01B0` calls `0x7010E0(0,false)` to reverse using the same duration, giving 0.3 seconds fade in.
There is no separate fixed hold duration here: black persists through the hourly countdown, so more selected hours keep it black longer. At one tick per second, H hours nominally occupy H seconds, subject to frame scheduling and interruption.
The tick discards accumulator remainder after each hour, so this is not an exact wall-clock guarantee and can be slower at low update rates.
FO3 confirmation `0x6745A0` calls fader `0x6181F0` with default 0.3; close `0x674530` reverses it through `0x617FD0`. Alpha updater `0x618090` likewise adds/subtracts effective frame delta / duration, clamps to [0,1], and writes material alpha `+0x78`; its delta is divided by global `0xF7544C` (static value 1).
The hourly mode-4 call is a no-op in both shipped executables: NV `0xAD8780` returns without reading its argument or changing state; FO3 `0x78C030` immediately returns. It causes no audio fade, mute or mix change. Opening the menu (`0x7BFC30/0x674AA0`) separately plays UIPopUpMessageGeneral `00022E14`, `fx\ui\popup\ui_popup_messagewindow.wav`, in both games.

### 16. FAIL — A lying player camera is not part of normal sleep

The player bed activation paths in item 13 do not initiate a lying camera or a third-person lying body sequence; they open the menu at the existing player location, then cover the world with the sleep fader.
Consequently there are no player lie-down/get-up camera offsets or animation seconds to substitute into this normal menu path.
AI furniture sleeping, forced idle commands and scripted bed scenes are separate from the ordinary player bed-activation path evaluated here; their existence does not add a lying phase to this menu.

### 17. PASS — Sleep healing and hourly simulation

NV `0x94DF80` advances simulation by 3600 / current TimeScale per selected game hour through `0x867A40/0x96D4B0`; it does not merely multiply the live TimeScale by an arbitrary sleep speed.
On the final selected hour, if sleeping and not hardcore, `0x8A0960` restores damaged Health `0x10` and Fatigue `0x16`; for negative damage modifier m it requests 1 − m healing.
It also requests 1000 restoration for each condition AV `0x1F,0x1D,0x1E,0x1B,0x1C,0x1A,0x19`; helper `0x88B740` only issues positive restoration when that AV has a negative damage modifier. This is completion restoration, not a fixed HP-per-hour rule.
The per-hour path can also run general regeneration `0x88B510`: helper `0x648B00` gives HealRate/3600, or (2 × HealRate + 2)/3600 if actor state `+0x1AC == 9`, then multiplies by simulated delta; player perk entry point `0x0C` can add a rate.
Do not assume that animation-state-9 branch is set merely because the sleep-menu flag is true; these are different fields.
Hardcore skips this completion full restoration; when sleeping, it reduces SleepDeprevation AV `0x4B` (the executable registration spells it this way) by min(current value, fHCSleepRestorationMod = 60) per hourly step. fHCSleepDeprivationRate is 50 in FalloutNV.esm, default 0.26. The separate rested-bed ResetHealth script in item 18 has no hardcore guard.
FO3 `0x76C8F0` uses the same 3600/TimeScale step and calls full restore `0x6F6A80` at sleep completion, with the same Health/Fatigue/condition restoration requests; no hardcore branch exists there.

### 18. PASS — Rested-bed script flow and separate healing

NV PlayerBedSCRIPT `000B5431`: player activation sets local state 1, then invokes ordinary activation; MenuMode changes 1 to 2 if IsPCSleeping (`0x10AF`) is true; the next GameMode casts WellRestedSpell `00062697` on the player if state 2, then clears state.
The script itself tests neither hours nor ownership; qualification here is attachment to the bed plus observing the sleeping flag. Cancellation after that observation is not excluded by this script; activation restrictions still apply separately.
WellRestedMessageEffectSCRIPT `00062745`, on effect start, calls player ResetHealth (`0x1150`) and adds WellRestedPerk `00061822` if absent; on effect finish it removes that perk. WellRestedAbilityEffectSCRIPT `00062695` casts WellRestedSpell on effect finish.
NV ResetHealth `0x5C6B60` restores Health `0x10` by maximum minus current, then requests 999 restoration for conditions `0x1D,0x1E,0x1B,0x1C,0x19,0x1A,0x1F`; it has no hardcore test. This separate script call can heal despite the hardcore completion-heal exclusion in item 17.
WellRestedPerk uses entry `0x09`, multiply operation `0x03`, factor 1.1, conditioned on magic effect `00062744`. RewardXP `0x5D5140` → player virtual `+0x488` (`0x94FD10`) → `0x8D5100` applies entry 9 before adding Experience AV `0x18`: the bonus is +10% XP before integer conversion/caps.
NV FURN records attached to PlayerBedSCRIPT: SafehouseBed01 `001592E9`, SafehouseBed02 `001592E8`, SafehouseCot `001592E7`, NovacQueenBedRight `000CEECD`, HD01DukovsBed01SingleSide `0007E21A`, HD01BedHeart `0007E219`, HD02PlayerBed02 `000B543B`, HD01PlayerBed02 `000B5439`, HotelBedQueenLeft `0008B5E2`.
FO3 has identical compiled instructions and reference lists for all three scripts. ResetHealth `0x524A10` makes the same Health and 999-condition requests; RewardXP `0x52C940` → virtual `+0x478` (`0x76CA70`) → `0x72A300` likewise applies entry 9 before Experience AV `0x18`.
FO3's attached FURN set retains `0007E21A,0007E219,000B543B,000B5439,0008B5E2`, adds HD02PlayerBed01 `000B543A` and HD01PlayerBed01 `000B5438`, and lacks the four listed NV Safehouse/Novac attachments.

### 19. PASS — Waiting without a bed

NV and FO3 use the same sleep/wait menu and hourly advancement, with the sleep flag false for waiting.
Both confirm handlers start the black fader only for sleep. Waiting does not invoke a lying animation or the sleep completion full-heal call; general simulation/regeneration can still run.
The same 1..24 selection and nominal one game-hour-per-real-second countdown apply; waiting is not ordinary real-time simulation with an enlarged TimeScale.

### 20. PASS — Rested duration clock and expiry

Both masters' WellRestedSpell `00062697` supplies duration 1440 to effect `00062744`; NV constructor `0x803D30` reads it through `0x4044D0` into ActiveEffect `+0x20`, with elapsed time `+0x04` initially zero.
NV updater `0x804560`, reached through `0x823C40`, adds positive simulation delta to elapsed, clamps the last update to remaining duration, and marks expired at elapsed >= duration (`+0x13 = 1`); it invokes the finish virtual once.
Actor update `0x8C3C40` uses the simulation-clock difference, or 3600 / TimeScale for an hourly skipped-time update; sleep/wait `0x94DF80` invokes that hourly path. This is a simulation-seconds budget, not 1440 game-clock seconds.
Both masters' TimeScale GLOB `0000003A` is 30: an unmodified 1440-second budget corresponds to 1440 × 30 / 3600 = 12 game hours, or 24 minutes of ordinary unpaused simulation. Changed TimeScale changes that game-clock equivalence.
FO3 constructor `0x6A13F0` and updater `0x6A27E0` use the same elapsed `+0x04`, duration `+0x20` and expiry flag `+0x13`; list updater `0x6B8190` calls it. Hourly caller `0x76C8F0` → `0x723100` also supplies 3600 / TimeScale.
Script-effect finish dispatch reaches the effect-finish script, which removes WellRestedPerk as described in item 18. Dispel/recast and other interruptions can end or replace the effect before its budget expires; this is not an unconditional wall-clock promise.

### 21. PASS — New Vegas hardcore needs during skipped hours

NV menu tick `0x7C0580` calls `0x969C30` when hardcore is active, before calling hourly advancement `0x94DF80`; the needs update therefore observes time already elapsed, not the hour about to be advanced.
`0x969C30` reads absolute game minutes through `0x867EA0`, tracks separate previous timestamps at `0x11E0D7C/0x11E0D78/0x11E0D74`, and uses interval I = rate × TimeScale / 60 game minutes per point.
When elapsed minutes >= I, add trunc(elapsed / I) through actor virtual `+0x3B0`, then replace that need's timestamp with current minutes: fractional remainder is discarded. Initial zero timestamp or player byte `+0xE39` resets the baselines; player `+0x680 & 4` skips the updater.
Rates: fHCDehydrationRate = 10 (master absent/default 10), fHCStarvationRate = 25 (`00162BC9`, default 0.39), fHCSleepDeprivationRate = 50 (`00162BCA`, default 0.26). These are intervals, so a larger rate slows accumulation.
AVs are Dehydration `0x49`, Hunger `0x4A`, SleepDeprevation `0x4B`. At TimeScale 30, their intervals are 5, 12.5 and 25 game minutes. A fully elapsed 60-minute interval gives integer additions 12, 4 and 2; this is not a guaranteed addition on the first or final menu tick.
While sleeping (`0x5A1E50`), the sleep-deprivation addition is suppressed but its timestamp still advances; hunger and dehydration have no equivalent sleep suppression. The separate hourly reduction is min(current SleepDeprevation, 60), as in item 17.
Callbacks `0x8C5610/0x8C5890/0x8C5B10` replace stage spells when the selected stage changes; selectors `0x4018B0/0x402030/0x402760` compare value >= each record threshold. Base-master DEHY/HUNG/SLPD records supply thresholds 200/400/600/800/1000.
At 1000, records Water5 `00161905`, hunger5 `0016190A`, sleep5 `0016190F` select Dehydration1000 `0015FA20`, Starvation1000 `0015FA25`, SleepDeprivation1000 `0015FA2A`: each specifies DamageHealth `00033595`, magnitude 10000, duration 0, Health AV `0x10`. This is a damaging effect, not a menu-specific instant-kill opcode.
FO3 has no corresponding hardcore needs path. Fatal-stage DamageHealth uses the ordinary Health AV gate, so NV god mode (`0x952730`) rejects its negative Health change. Tick `0x7C0580` has no fatal-stage-specific wake call or Health threshold: death uses item 4, while menu closure still follows cancellation/remaining hours; it is not an unconditional menu kill command.

### 22. PASS — Sleep/wait limits, restrictions and interruptions

Both `Fallout - Misc.bsa` menu files configure 24 choices; code adds 1 to the zero-based slider, giving 1..24 selected hours. This is a menu limit, not a limit imposed on every script command.
NV bed activation `0x5095B0` rejects a bed with an owner (`0x567790`) when ownership permission `0x5785E0(actor,1)` fails; FO3 `0x4ADE00` uses `0x4F26C0/0x4F3990`. Trespass predicates are actor virtual `+0x448` (`0x8D1E70`) / `+0x438` (`0x7281F0`), correcting the earlier combat label.
No-wait location test NV `0x5444C0`, FO3 `0x4D4C50`: interior cell form flag `+0x08 & 0x80000`, or exterior worldspace form flag `+0x08 & 0x80000` through `0x586230/0x501120`. This differs from the fall exemption's worldspace `+0x4C` bit.
Nearby-hostility tests NV `0x9764A0`, FO3 `0x78DBE0` use fHostileActorInteriorDistance = 2000 / fHostileActorExteriorDistance = 3000, executable defaults absent from both masters, with enabled/living/hostility checks.
Radiation predicates NV `0x5098E0`, FO3 `0x4AD9D0` read manager bytes `+0x15C || +0x15D`. Producers `0x9777A0/0x78DE60` set `+0x15C` when maximum current exposure after distance falloff and resistance is > 0; this is not an accumulated-Rads threshold.
Damaging-effect predicates NV `0x822E00`, FO3 `0x6B6E50` scan active effects for a value-modifying archetype, target Health `0x10`, and detrimental effect flag `0x04`; no minimum magnitude is tested here.
Wait-key paths NV `0x9423AC..`, FO3 `0x789ECE..0x78A0B9` also reject alarm state, underwater, and airborne controller state 1/2; radiation includes a nonzero process virtual (`+0x764/+0x758`) result. NV additionally checks wait-control enable `player+0x66E` (`0x944360`). Success opens `0x7054F0(false)` / `0x61B900(false)`.
Running ticks `0x7C0580/0x674990` check cancellation, remaining hours and modal-menu state, but do not repeat these entry checks each hour. Cancel/completion closes through `0x7C01B0/0x674530`; simulated effects and scripts can independently change the remaining count or kill the player through normal Health processing.
WakeUpPC `0x10AE`, NV `0x5CD5B0` / FO3 `0x528D40`, acts only while sleeping, defaults/clamps requested remaining hours to >= 0; zero closes through `0x7055C0/0x619C60`, positive replaces the remaining count. SetPCSleepHours `0x10BD`, `0x5CDE40/0x529380`, stores its signed hours and sets the sleep flag without a 1..24 clamp.
These commands write NV player `+0x654/+0x658`, FO3 `+0x5B4/+0x5B8`; hourly progression consumes the same fields. There is no universal extra quest-interruption timer: scripted wake/hour changes and ordinary menu cancellation are explicit interruption mechanisms.

### 23. PASS — Numeric GMST and related setting inventory

The table below covers all numeric registered/extracted GMST names containing Jump, Fall, Dialog, Conversation, Speak, Camera, Zoom, FOV, Focus, Sleep, Rest or Wait or Heal, case-insensitively, plus the directly relevant additional GMSTs.
This deliberately includes broad matches such as Health, Restore, Restrictions and OverShoulder; a match does not establish use by these three features. No separate numeric Rested-family GMST was found.
Each cell is plugin override / executable default. A dash means no matching record/default was found, not zero; defaults come from NV static initializer arguments and FO3 static setting records.
Decimal float values denote the nearest float32; short decimals below round-trip to the original 32-bit value. String GMST values are withheld to obey the no-game-text requirement; their names and presence follow separately.

| GMST | NV plugin / exe | FO3 plugin / exe |
|---|---:|---:|
| fAIConversationExploreTime | — / 0.25 | — / 0.25 |
| fAIEscortWaitDistanceExterior | — / 2 | — / 2 |
| fAIEscortWaitDistanceInterior | — / 512 | — / 512 |
| fAIFleeHealthMult | — / 20 | — / 20 |
| fAIIdleWaitTime | — / 1 | — / 1 |
| fAISocialRadiusToTriggerConversation | — / 2000 | — / 2000 |
| fAISocialRadiusToTriggerConversationInterior | 1000 / 500 | 1000 / 500 |
| fAISocialTimerForConversationsMax | 60 / 30 | 60 / 30 |
| fAISocialTimerForConversationsMin | 30 / 5 | 30 / 5 |
| fAISocialchanceForConversation | 25 / 60 | 25 / 60 |
| fAISocialchanceForConversationInterior | 25 / 25 | 25 / 25 |
| fAIUpdateMovementRestrictionsDistance | — / 48 | — / 48 |
| fCharacterDefaultTurningSpeed | — / 90 | — / 90 |
| fCreatureDefaultTurningSpeed | — / 45 | — / 45 |
| fAVDDefaultBodyPartCondition | — / 100 | — / 100 |
| fAVDHealRateEndurance10Bonus | 10 / 2 | 10 / 2 |
| fAVDHealRateEndurance6Bonus | 5 / 1 | 5 / 1 |
| fAVDHealRateEndurance7Bonus | 5 / 1 | 5 / 1 |
| fAVDHealRateEndurance8Bonus | 5 / 1 | 5 / 1 |
| fAVDHealRateEndurance9Bonus | 10 / 2 | 10 / 2 |
| fAVDHealthEnduranceMult | 20 / 5 | 20 / 5 |
| fAVDHealthEnduranceOffset | 0 / -1 | 0 / -1 |
| fAVDHealthLevelMult | 5 / 20 | 10 / 20 |
| fAVDNPCHealthEnduranceMult | 5 / 5 | 5 / 5 |
| fAVDNPCHealthEnduranceOffset | — / -1 | — / -1 |
| fAVDNPCHealthLevelMult | 5 / 20 | 5 / 20 |
| fActionPointsFOVBase | — / 0 | — / 0 |
| fActionPointsFOVMult | 0.25 / 1 | 0.25 / 1 |
| fActionPointsHeal | 10 / 1 | 10 / 1 |
| fActionPointsRestoreRate | 0.06 / 1 | 4 / 1 |
| fAiAcquireStealBase | — / 0 | — / 0 |
| fAiAcquireStealMult | — / -1 | — / -1 |
| fCameraShakeDistFadeDelta | — / 2500 | — / 2500 |
| fCameraShakeDistFadeStart | — / 2000 | — / 2000 |
| fCameraShakeDistMin | — / 300 | — / 300 |
| fCameraShakeExplosionDistMult | — / 0.001 | — / 0.001 |
| fCameraShakeFadeTime | — / 0.25 | — / 0.25 |
| fCameraShakeMultMin | — / 0 | — / 0 |
| fCameraShakeTime | — / 1.25 | — / 1.25 |
| fChaseCameraMax | — / 120 | — / — |
| fCheckPositionFallDistance | — / 72 | — / 72 |
| fCombatDetectionDialogueIdleMaxElapsedTime | 15 / 10 | 15 / 10 |
| fCombatDetectionDialogueIdleMinElapsedTime | 5 / 3 | 5 / 3 |
| fCombatDetectionDialogueMaxElapsedTime | — / 3 | — / 3 |
| fCombatDetectionDialogueMinElapsedTime | — / 1 | — / 1 |
| fCombatDialogueAttackMaxElapsedTime | 30 / 5 | 30 / 5 |
| fCombatDialogueAttackMinElapsedTime | 5 / 1 | 5 / 1 |
| fCombatDialogueAvoidThreatMaxElapsedTime | — / 5 | — / 5 |
| fCombatDialogueAvoidThreatMinElapsedTime | — / 3 | — / 3 |
| fCombatDialogueFireExplosiveMaxElapsedTime | — / 5 | — / 5 |
| fCombatDialogueFireExplosiveMinElapsedTime | — / 1 | — / 1 |
| fCombatDialogueFleeMaxElapsedTime | — / 5 | — / 5 |
| fCombatDialogueFleeMinElapsedTime | — / 3 | — / 3 |
| fCombatFleeInitialDoorRestrictChance | — / 0.75 | — / 0.75 |
| fCombatFleeLastDoorRestrictTime | — / 30 | — / 30 |
| fCombatFleeUseDoorRestrictTime | — / 15 | — / 15 |
| fCombatFleeWaitTime | 30 / 60 | 30 / 60 |
| fCombatItemRestoreTimer | 3 / 30 | 3 / 30 |
| fCombatSpeakAttackChance | 0.01 / 0.1 | 0.01 / 0.1 |
| fCombatSpeakHitChance | 0.01 / 0.35 | 0.01 / 0.35 |
| fCombatSpeakHitThreshold | 0.01 / 0.1 | 0.01 / 0.1 |
| fCombatSpeakPowerAttackChance | — / 0.5 | — / 0.5 |
| fCombatSpeakTauntChance | — / 0.01 | — / 0.01 |
| fCombatThreatDangerousObjectHealth | — / 30 | — / 30 |
| fCoverWaitReloadClipPercent | — / 0.33 | — / 0.33 |
| fDialogFocalDepthRange | — / 300 | — / 300 |
| fDialogFocalDepthStrength | — / 0.65 | — / 0.65 |
| fDialogSpeechDelaySeconds | — / 2 | — / 2 |
| fDialogZoomInSeconds | — / 1.5 | — / 1.5 |
| fDialogZoomOutSeconds | — / 0.5 | — / 0.5 |
| fDiffMultHPByPCE | — / 1.5 | — / 1.5 |
| fDiffMultHPByPCH | — / 0.75 | — / 0.75 |
| fDiffMultHPByPCN | — / 1 | — / 1 |
| fDiffMultHPByPCVE | — / 2 | — / 2 |
| fDiffMultHPByPCVH | — / 0.5 | — / 0.5 |
| fDiffMultHPToPCE | — / 0.75 | — / 0.75 |
| fDiffMultHPToPCH | — / 1.5 | — / 1.5 |
| fDiffMultHPToPCN | — / 1 | — / 1 |
| fDiffMultHPToPCVE | — / 0.5 | — / 0.5 |
| fDiffMultHPToPCVH | — / 2 | — / 2 |
| fDistancetoPlayerforConversations | — / 3000 | — / 3000 |
| fEnemyHealthBarTimer | — / 5.5 | — / 5.5 |
| fEssentialDeathTime | — / 10 | — / 10 |
| fEssentialHealthPercentReGain | — / 0.3 | — / 0.3 |
| fExplosiveProjectileBlockedWaitTime | — / 2.5 | — / 2.5 |
| fFadeToBlackFadeSeconds | — / 0.3 | — / 0.3 |
| fFallLegDamageMult | — / 0.5 | — / 0.5 |
| fFirstPersonCameraMult | — / 1 | — / 1 |
| fFirstPersonZoomMaxMult | — / 5 | — / 5 |
| fFirstPersonZoomMinMult | — / 0.25 | — / 0.25 |
| fGetHitPainMult | — / 1.5 | — / 1.5 |
| fGrenadeRestitution | 0.01 / 0 | 0.01 / 0 |
| fGunDecalCameraDistance | — / 2048 | — / 2048 |
| fGunParticleCameraDistance | 2048 / 1024 | 2048 / 1024 |
| fGunShellCameraDistance | — / 512 | — / 512 |
| fHCDehydrationRate | — / 10 | — / — |
| fHCSleepDeprivationRate | 50 / 0.26 | — / — |
| fHCSleepRestorationMod | — / 60 | — / — |
| fHCStarvationRate | 25 / 0.39 | — / — |
| fHandHealthMax | — / 20 | — / 20 |
| fHandHealthMin | — / 1 | — / 1 |
| fHealingRateSleepingBase | — / 2 | — / 2 |
| fHealingRateSleepingMult | — / 2 | — / 2 |
| fHostileActorExteriorDistance | — / 3000 | — / 3000 |
| fHostileActorInteriorDistance | — / 2000 | — / 2000 |
| fIronSightsFOVTimeChange | — / 0.25 | — / 0.25 |
| fJumpDoubleMult | — / 0.5 | — / 0.5 |
| fJumpFallHeightExponent | 1.65 / 2 | 1.65 / 2 |
| fJumpFallHeightMin | 600 / 256 | 600 / 256 |
| fJumpFallHeightMult | — / 0.025 | — / 0.025 |
| fJumpFallRiderMult | — / 0 | — / 0 |
| fJumpFallSkillBase | — / 1.25 | — / 1.25 |
| fJumpFallSkillMult | — / -0.01 | — / -0.01 |
| fJumpFallVelocityMin | 2785 / 600 | 2785 / 600 |
| fJumpHeightMin | — / 64 | — / 64 |
| fJumpMoveBase | — / 0 | — / 0 |
| fJumpMoveMult | — / 0.3 | — / 0.3 |
| fJumpSwimmingMult | 2 / 2 | 2 / 2 |
| fKillCameraCooldown | 20 / 15 | — / — |
| fKillCameraTime | — / 3.5 | — / — |
| fKnockdownBaseHealthThreshold | — / 75 | — / 75 |
| fKnockdownCurrentHealthThreshold | 50 / 25 | 50 / 25 |
| fLocalMapMaxZoom | — / 0.9 | — / 0.9 |
| fLocalMapMinZoom | — / 0.1 | — / 0.1 |
| fLockpickBonusHealth | — / 0.01 | — / — |
| fLowLevelNPCBaseHealthMult | — / 0.25 | — / 0.25 |
| fMeleeMovementRestrictionsUpdateTime | — / 1 | — / 1 |
| fMoveOneCrippledLegSpeedMult | 0.85 / 0.85 | 0.85 / 0.85 |
| fMoveTwoCrippledLegsSpeedMult | 0.75 / 0.75 | 0.75 / 0.75 |
| fNPCAttributeHealthMult | — / 0.5 | — / 0.5 |
| fOverShoulderFOV | — / 55 | — / — |
| fOverShoulderOffsetPointX | 98.4 / 98.4 | 98.4 / 98.4 |
| fOverShoulderOffsetPointY | — / 16 | — / 16 |
| fOverShoulderOffsetPointZ | -24.8 / -24.8 | -24.8 / -24.8 |
| fOverShoulderOffsetPointZoomingX | 0 / 0 | 0 / 0 |
| fOverShoulderOffsetPointZoomingY | 10000 / 10000 | 10000 / 10000 |
| fOverShoulderOffsetPointZoomingZ | -10 / -10 | -10 / -10 |
| fOverShoulderPosX | 40 / 40 | 40 / 40 |
| fOverShoulderPosZ | -10 / -10 | -10 / -10 |
| fOverShoulderRotMult | 0.6 / 0.6 | 0.6 / 0.6 |
| fOverShoulderStartBlendDist | 40000 / 0 | 40000 / 0 |
| fPCBaseHealthMult | — / 2 | — / 2 |
| fPainDelay | — / 1.5 | — / 1.5 |
| fPerkJumpFatigueExpertMult | — / 0.5 | — / 0.5 |
| fPersAdmireStre | — / 1 | — / 1 |
| fPlayerDeathReloadTime | — / 5 | — / 5 |
| fPlayerHealthHeartbeatFast | — / 0.1 | — / 0.1 |
| fPlayerHealthHeartbeatSlow | — / 0.2 | — / 0.2 |
| fRumblePainStrength | — / 0.75 | — / 0.75 |
| fRumblePainTime | — / 0.5 | — / 0.5 |
| fSandBoxExtraDialogueRange | — / 384 | — / 384 |
| fSandboxSleepDurationMax | — / 10 | — / 10 |
| fSandboxSleepDurationMin | — / 6 | — / 6 |
| fSandboxSleepStartMax | — / 1 | — / 1 |
| fSandboxSleepStartMin | — / 19 | — / 19 |
| fSneakSleepBonus | — / -0.2 | — / -0.2 |
| fStatsHealthLevelMult | — / 0.1 | — / 0.1 |
| fStatsHealthStartMult | — / 0.5 | — / 0.5 |
| fVATSCamZoomInTime | — / 0.75 | — / 0.75 |
| fVATSCameraCutAwayDistance | 30 / 25 | 30 / 25 |
| fVATSCameraDollyMin | 1536 / 256 | 1536 / 256 |
| fVATSCameraDollyTime | 0.38 / 1 | 0.38 / 1 |
| fVATSCameraEffectVolume | — / 1 | — / — |
| fVATSCameraMaxTime | — / 20 | — / 20 |
| fVATSCameraMinTime | — / 0 | — / 0 |
| fVATSMoveCameraLimbMult | 0.5 / 0.35 | 0.5 / 0.35 |
| fVATSMoveCameraLimbPercent | — / 0.1 | — / 0.1 |
| fVATSMoveCameraMaxSpeed | — / 20000 | — / 20000 |
| fVATSMoveCameraXPercent | 0.15 / 0 | 0.15 / 0 |
| fVATSMoveCameraYPercent | — / 0.15 | — / 0.15 |
| fVATSSmartCameraCheckHeight | 64 / 96 | 64 / 96 |
| fVATSSmartCameraCheckStepCount | — / 5 | — / 5 |
| fVATSSmartCameraCheckStepDistance | 64 / 128 | 64 / 128 |
| fVATSTargetFOVMinDist | — / 150 | — / 150 |
| fVATSTargetFOVMinFOV | — / 1.5 | — / 1.5 |
| fVATSTargetFOVMultFar | 34 / 30 | 34 / 30 |
| fVATSTargetFOVMultFarDist | 2750 / 1750 | 2750 / 1750 |
| fVATSTargetFOVMultNear | 50 / 46 | 50 / 46 |
| fWorldMapMaxZoom | — / 5 | — / 5 |
| fWorldMapMinZoom | — / 0.75 | — / 0.75 |
| iAICombatRestoreFatiguePercentage | — / 30 | — / 30 |
| iAICombatRestoreHealthPercentage | 60 / 30 | 60 / 30 |
| iAICombatRestoreMagickaPercentage | — / 30 | — / 30 |
| iChanceFleeDialog | — / 25 | — / 25 |
| iClassHealer | — / 2320 | — / 2320 |
| iDialogueDispositionFriendValue | — / 50 | — / 50 |
| iDialogueDummySpeakThisIntOrBelow | 3 / 4 | — / — |
| iFallLegDamageChance | — / 50 | — / 50 |
| iNPCBasePerLevelHealthMult | — / 4 | — / 4 |
| iPerkHeavyArmorJumpSum | — / 35 | — / 35 |
| iSpeakSoundLipDistance | — / 750 | — / 750 |
| iVATSCameraHitDist | — / 100 | — / 100 |
| iXPDeathRewardHealthThreshold | 40 / 75 | 40 / 75 |

String GMST inventory: `E` = executable registration found; `P` = master override found; `—` = neither found. Values intentionally omitted, not interpreted.

| GMST | NV | FO3 |
|---|---|---|
| sActionPointsHeal | E | E |
| sCWheelWaitHere | E | — |
| sCameraPitch | E | E |
| sDialogSubtitles | E | E |
| sFastTravelNoTravelHealthDamage | E | E |
| sFullHealth | E | E |
| sHUDHealth | E | E |
| sHealth | E | E |
| sHeavyArmorNoJump | E | E |
| sHolidaySunsRest | E | E |
| sHowManyWait | E | E |
| sMagicEffectItemRestore | E | E |
| sMagicSchoolRestoration | E | E |
| sMustRestart | E | E |
| sNoJumpWarning | E | E |
| sNoRestart | E | E |
| sNoSleepCombat | E | E |
| sNoSleepHostileActorsNear | PE | PE |
| sNoSleepInOwnedBed | E | E |
| sNoSleepInRadiation | E | E |
| sNoSleepTakingHealthDamage | E | E |
| sNoSleepTrespass | E | E |
| sNoWaitHostilActorsNear | PE | PE |
| sNoWaitInAir | E | E |
| sNoWaitInCell | E | E |
| sNoWaitInCombat | E | E |
| sNoWaitInJail | E | E |
| sNoWaitInRadiation | E | E |
| sNoWaitTakingHealthDamage | E | E |
| sNoWaitTrespass | E | E |
| sNoWaitUnderWater | E | E |
| sNoWaitWhileAlarmSounding | E | E |
| sRestartBecauseContentRemoved | PE | E |
| sRestartSignedOut | E | E |
| sRestartToUseNewContent | E | E |
| sRestartToUseProfileContent | E | E |
| sSaveOnRest | E | E |
| sSaveOnWait | E | E |
| sSeasonFall | E | E |
| sSleep | E | E |
| sSleepDeprevationDecrease | PE | — |
| sSleepDeprevationIncrease | PE | — |
| sSleepDeprevationNotSick | PE | — |
| sWait | E | E |
| sYesRestart | E | E |

Related INI settings, not GMSTs: values below are executable defaults; no local INI overrides were evaluated.

| Setting | NV default; object VA | FO3 default; object VA |
|---|---|---|
| fHardLandingDamageThreshold:Audio | 500; 0x11df6ec | 500; 0x107acf8 |
| fDlgFocus:Interface | 3.2; 0x11e0b84 | 3.2; 0x107bde4 |
| fDlgLookAdj:Interface | -5; 0x11e08d0 | -5; 0x107bb54 |
| fDlgLookMult:Interface | 2; 0x11e0958 | 2; 0x107bbdc |
| fDlgLookDegStart:Interface | 13; 0x11e094c | 13; 0x107bbd0 |
| fDlgLookDegStop:Interface | 0.2; 0x11e0900 | 0.2; 0x107bb84 |
| fDlgHeadingDegStart:Interface | 13; 0x11e0b28 | 13; 0x107bd88 |
| fDlgHeadingDegStop:Interface | 0.2; 0x11e08c4 | 0.2; 0x107bb48 |
| fDefaultFOV:Display | 75; 0x1203150 | 75; 0x116d9f0 |
| fRenderedTerminalFOV:RenderedTerminal | 0.15; 0x11db5ec | 0.15; 0x10778f4 |
| fDefaultWorldFOV:Display | 75; 0x120315c | 75; 0x116d9fc |
| fDefault1stPersonFOV:Display | 75; 0x1203168 | 75; 0x116da08 |
| fPipboy1stPersonFOV:Display | 68; 0x1203174 | 68; 0x116da14 |
| bIronSightsZoomEnable:Combat | 1; 0x11e097c | 1; 0x107bc00 |
| bEssentialTakeNoDamage:GamePlay | 1; 0x11e0888 | 1; 0x107bb18 |
| fAITurnSpeedScale:Pathfinding | 1.5; 0x11f2ad8 | 1.5; 0x108d760 |
| fAICombatTurnSpeedScale:Pathfinding | 2.5; 0x11f2ac4 | 2.5; 0x108d74c |
| fCreatureTurnSpeedScale:Pathfinding | 1.25; 0x11f2c10 | 1.25; 0x108d86c |
| fCreatureCombatTurnSpeedScale:Pathfinding | 1.25; 0x11f2b10 | 1.25; 0x108d794 |

Landing sound inventory (item 7): these are editor IDs and file/directory paths, not copied sound data.

| Editor ID | Form ID (both) | NV file/directory | FO3 file/directory |
|---|---|---|---|
| FSTConcSolidLand | 0001D8E0 | `fx\fst\gravel\land\` | `fx\fst\conc_solid\land\` |
| FSTConcBrokenLand | 000205FF | `fx\fst\conc_broken\land\` | `fx\fst\conc_broken\land\` |
| FSTMetalSolidLand | 0001D8FD | `fx\fst\metal_solid\land\` | `fx\fst\metal_solid\land\` |
| FSTMetalHollowLand | 00035E5B | `fx\fst\metal_hollow\land\` | `fx\fst\metal_hollow\land\` |
| FSTMetalSheetLand | 0001EEC9 | `fx\fst\metal_sheet\land\` | `fx\fst\metal_sheet\land\` |
| FSTWoodLand | 0003420F | `fx\fst\wood\land\` | `fx\fst\wood\land\` |
| FSTSandLand | 0003FB0A | `fx\fst\dirt\land\` | `fx\fst\gravel\land\` |
| FSTDirtLand | 000212EB | `fx\fst\dirt\land\` | `fx\fst\dirt\land\` |
| FSTGrassLand | 0003E5CD | `fx\fst\grass\land\` | `fx\fst\grass\land\` |
| FSTWaterLand | 00020619 | `fx\fst\water\land\` | `fx\fst\water\land\` |
| FSTBabyLand | 0004452F | `fx\fst\baby\fst_baby_land.wav` | `fx\fst\baby\fst_baby_land.wav` |
| FSTConcSolidWalkR | 0001D8E1 | `fx\fst\conc_solid\run\right\` | `fx\fst\conc_solid\walk\right\` |

Surface group indices NV 0..12: ConcSolid, ConcBroken, MetalSolid, MetalHollow, MetalSheet, Wood, Sand, Dirt, Grass, Water, ChainLink, Tile, Carpet; FO3 has only groups 0..9.
Material-index → group arrays: NV `0x11A17F8` for indices 0..35 = [0,0,7,0,8,2,0,0,9,5,0,2,5,2,0,0,3,4,6,1,2,2,3,4,6,4,2,2,5,5,2,5,10,11,12,7]; FO3 `0xF6C538` for 0..31 is the same first 32 entries. Out-of-range/default selects group 0; unknown-material 36 (NV) / 32 (FO3) first attempts landscape material lookup.
Fallback arrays NV `0x11A17C4` = [13,0,0,2,3,0,7,0,7,0,7,0,0], FO3 `0xF6C4EC` = [10,0,0,2,3,0,7,0,7,0]; 13/10 means no further group. Missing ChainLink/Tile/Carpet landing records therefore resolve through Dirt/ConcSolid/ConcSolid.
Landing slot is suffix index 8, NV table `0x11DD650` with 40-byte group stride, FO3 `0x10793C0` with 36-byte stride. Both apply pitch = 0.949999988079071 − random(0, 0.10000000149011612), attach sound to the actor root and position, and use flags `0x410A`; NV first-person player branch uses `0x4109`.
