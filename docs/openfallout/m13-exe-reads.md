# M13 executable reads: falling, conversation camera, sleep/wait

Read-only static examination, 2026-11-10. PASS = read and understood; FAIL = read and contradicts the stated expectation; NOT TRACED = the requested rule is not fully established. A setting or record's existence does not establish that a particular path uses it.
Addresses are preferred-image virtual addresses, not file offsets. NV precedes FO3 throughout. No game was launched; no executable, plugin, asset, script source, game text, or disassembly is reproduced here. Only the base masters were evaluated; DLC, mods and local INI overrides can change effective behavior.
Verified PE file versions: FalloutNV.exe 1.4.0.525; Fallout3.exe 1.7.0.3. Evidence: direct PE bytes/Capstone, Ghidra decompilation for both games, command records, GMST/SOUN/IMAD/IDLE/PERK/SPEL/FURN records, compiled script control flow, and the sleep/wait menu's numeric configuration in Fallout - Misc.bsa.
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
This is an actor-value damage operation, not a direct base-health assignment; the traced path bypasses the weapon-hit armour DR/DT calculation. No fall-specific perk or SPECIAL multiplier appears in the arithmetic-to-health path.
For difficulty VE/E/N/H/VH, fDiffMultHPToPCVE/E/N/H/VH defaults are 0.5/0.75/1/1.5/2; fDiffMultHPByPCVE/E/N/H/VH defaults are 2/1.5/1/0.75/0.5. Neither master overrides them.
The choice is whether the recipient is the player, even with the null attacker. Player health loss = D × the ToPC multiplier; eligible NPC health loss uses ByPC.
NV creature-base test `0x5F0C40` checks form type `0x2B` and flag `base+0x30 & 0x20`, making D zero. The non-player branch also skips health damage for player teammates: `0x566950` reads actor byte `+0x18D`, set by SetPlayerTeammate `0x11C5`, handler `0x5D9370` → `0x8BCA90`.
A separate non-player/virtual-`+0x218`/cell test suppresses damage when cell `+0x4C & 0x40` is set (`0x586320`); the flag's semantic name is NOT TRACED.
FO3 damage helper `0x6FD240`, difficulty helper `0x585150`, Health AV `0x10`, virtual `+0x3A8`: same difficulty mechanism. Its landing routine has the corresponding cell-bit check (`0x501190`), but lacks NV's teammate-exemption check before the damage call.
Hardcore and player stats are not inputs to either traced fall formula; this does not establish immunity from every downstream scripted effect.

### 3. PASS — Player leg damage

NV `0x8A62B0` performs two independent random-integer-modulo-100 tests, each strictly less than iFallLegDamageChance = 50.
Successful tests damage LeftMobilityCondition `0x1D` and RightMobilityCondition `0x1E`, respectively, by raw D × fFallLegDamageMult = 0.5 through the actor-value damage virtual; these are the registered names, correcting the previous report's informal leg names.
This uses D before difficulty scaling, has no additional positive-damage threshold, and is inside the player-only branch; it does not assign the same random leg damage to NPCs.
FO3 `0x709320` uses the same AVs, chance, multiplier and player-only distinction. Both GMSTs are absent from both masters, so these are executable defaults.
NV AV registration `0x66F260` assigns both legs callback `0x8B9830` → `0x8B9240`; a negative change crossing from positive condition to condition <= 0 triggers the cripple transition. FO3 registration `0x5A3000` assigns `0x7181B0` → `0x717DC0`, with the same zero crossing.
Default condition is fAVDDefaultBodyPartCondition = 100 (both masters absent), returned by NV `0x643A50` and FO3 `0x581610`.
Movement helpers NV `0x647D10`, FO3 `0x5845B0` use fMoveOneCrippledLegSpeedMult = 0.85 or fMoveTwoCrippledLegsSpeedMult = 0.75; both masters and executable defaults agree. AV `0x48` can bypass this movement penalty.

### 4. NOT TRACED — Fatal-fall completion and protection edge cases

Confirmed NV health callback chain: `0x93B7A0` → virtual `+0x4B8` (`0x9508F0`) → `0x898650`; health less than 1 enters generic death processing `0x89D900`, with a deferred-processing branch.
This is ordinary actor death processing with a null fall attacker, not a separate fall-death opcode. Final ragdoll, death camera and reload timing are NOT TRACED.
Confirmed NV god-mode gate: `0x952720` → `0x9526B0`, using global `0x11E07BA`; ToggleGodMode handler `0x5B6F80` toggles it through `0x9526A0`.
`0x89D6F0` rejects health damage when protected; `0x952730` also rejects negative Health and limb AVs `0x19..0x1F`. The landing sound branch itself is not guarded by that health-damage result.
FO3 ToggleGodMode handler `0x51EDC0` → setter `0x76CEB0`, getter `0x76CEC0`, global `0x107BA65`; the getter is PlayerCharacter's magic-target virtual `+0x10`. `0x6FD240` gates health damage and `0x76CED0` rejects negative Health/condition AV changes while protected.
FO3 player Health damage `0x76E570/0x76E7C0` invokes virtual `+0x4A8` (`0x76CC80`) → `0x71C320`; Health < 1 enters `0x71AC50`, or queues deferred processing through `0x6F2910`. This matches NV's ordinary-death threshold.
All essential/NPC exceptions and fatal-fall ragdoll/camera/reload timing remain NOT TRACED; limb-zero transitions are now established in item 3.

### 5. FAIL — ResetFallDamageTimer does not reset in this NV executable

Opcode `0x1134`: NV execute handler `0x5D10A0` obtains the actor controller through `0x9306D0`, calls `0x5D10F0`, and discards the returned float.
`0x5D10F0` only reads controller `+0x544`; it does not write a field or reset a timer. The actual field is a starting height, read for landing by `0xC70550`.
NV fall-state code stores that height at `0xCD446F` and `0xCD4855`; setter `0x573F20` also writes it. The actor relocation path `0x931620` calls the same ineffective getter.
FO3 PASS: handler `0x52ADB0` gets the controller through `0x764EA0` and writes zero to `+0x3D0`; `0x8ED050` subsequently returns zero height loss while that field remains zero.

### 6. NOT TRACED — Load, teleport and door grace

No numeric grace interval was established for load, MoveTo, SetPos, Enable or door travel in either game.
The reset-command field is spatial state, not a countdown; do not infer a number of protected seconds from its name.
NV MoveToMarker `0x109E`, handler `0x5CCF50` → `0x5CCB20`, queues player positioning through `0x93BE30`; consumer `0x93BEA0` moves to the destination and calls `0x573F20` with destination Z (`request+0x10`), establishing a fresh spatial baseline.
NV non-player relocation `0x573800` also writes the actor's current Z through `0x573F20`; these are actual writes, unlike ResetFallDamageTimer's getter.
NV SetPos `0x5C05F0` → `0x5C0740` updates position through `0x575830` and, when applicable, `0x5620E0` → `0xC6E390`; the latter relocates Havok state without writing starting height `+0x544` in this traced chain.
Enable's queued processing, every load/door route, and all state transitions after SetPos remain NOT TRACED. `0x93CDF0`, reached by a different position-request branch, is fast-travel processing; it is not evidence of a door grace timer.
FO3's clearing command alone does not prove that every relocation/loading path invokes it.
FO3 MoveTo `0x528A60` → `0x528730` queues player `+0x1B0` via `0x7882F0`; consumer `0x786560` → `0x782B00/0x7820F0` invokes position virtual `+0x2A8` (`0x766A40`), which clears starting height `+0x3D0` and landing bit `+0x294 & 0x80`. This is a zero reset, versus NV's destination-Z baseline.
FO3 SetPos `0x532410` → `0x522910` → `0x4E6580` → `0x8EA270` relocates Havok state through a different route; complete subsequent state reset remains NOT TRACED.

### 7. NOT TRACED — Landing presentation and special surfaces

Confirmed NV `0x8A62B0`, player-only positive-D branch: FSTLandHardHeavy (`0005A725`) if D > fHardLandingDamageThreshold:Audio = 500; otherwise FSTLandHardLight (`0005A724`). FO3 `0x709320` has the same selection and default.
Both sound records use `fx\fst\landhard\`; both are hurt-landing sounds, not a hurt/plain pair.
Both masters contain FSTDirtLand (`000212EB`, `fx\fst\dirt\land\`), FSTWaterLand (`00020619`, `fx\fst\water\land\`), FSTMetalSolidLand (`0001D8FD`, `fx\fst\metal_solid\land\`) and FSTGrassLand (`0003E5CD`, `fx\fst\grass\land\`).
JumpLand is animation-group `0xE5` in NV's table at `0x11977D8`, and `0xAA` in FO3's table at `0xF5ACA0`; existence is confirmed, but the landing animation/surface-sound dispatch is NOT TRACED.
NV pain path `0x8A7D50` calls image-space application `0x5299A0` with GetHit IMAD `00000162`, fGetHitPainMult = 1.5 and zero third argument, subject to its alive/state gates.
Both masters' GetHit record declares duration 0.4 seconds; both executable pain rumble defaults are fRumblePainTime = 0.5 and fRumblePainStrength = 0.75. Pain helpers also read fPainDelay = 1.5 (both masters absent); complete effect lifetime/retrigger interaction remains NOT TRACED.
FO3 pain helper `0x6FF9E0` calls `0x4C2700` with fGetHitPainMult = 1.5 and zero third argument; lookup `0x51DEC0` resolves GetHit form `00000162`, matching NV's effect selection, subject to alive/state/VATS gates.
Water damage exemption, shallow/deep-water distinctions, landing on another character, and whether that character receives damage remain NOT TRACED.

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

### 10. NOT TRACED — Conversation actors, controls and visibility

Confirmed NV `0x953060` turns player pitch/yaw through `0x931E50/0x931D30`; the seated heading branch adjusts player `+0x6E4` instead.
The menu retains its target at `+0x80` and updates that target every frame; no reverse-shot cut to the player was found in this path, but this is not proof covering scripted/multi-actor conversations.
Opening requests actor dialogue/facing state; the complete head-versus-body turn controller and its speed are NOT TRACED in either game.
NV opener/closer set/clear head/hair byte `+0xE2` through `0x762130`; diagnostic `0x663D10` identifies it as bIAmInDialogue, and reader `0x663050` uses it to retain facial updates despite distance checks. It is not a head-hide flag.
NV `0x951A10(1)` sets the ordinary player-root cull bit and clears the first-person-root cull bit (`player+0x694`): the normal opening branch hides the third-person body and enables first-person geometry.
FO3 opener `0x63E9A0` sets the same head/hair `+0xE2` flag, named bIAmInDialogue by `0x59A63A`; `0x7700B0(1)` similarly culls the ordinary root and unculls first-person root `player+0x5F0`.
Mouse/stick look suppression, exceptional visibility branches, and all speaker-switching rules remain NOT TRACED.

### 11. NOT TRACED — Conversation exit and unusual speakers

Confirmed NV closing state in `0x762950` disables target turning, ramps progress down over 0.5 seconds, restores view-mode request flags `0x11F21D0/0x11F21D1` from saved menu `+0x130`, and deletes the menu.
`0x762160` clears dialogue states; `0x953CE0` clears player `+0x6CC/+0x6D0`. `0x93E860` consumes the saved-POV flags via `0x950110`. Dialogue-state virtual `0x961D90` saves/restores seated heading `+0x6E4`; complete restoration of world pitch/yaw and position across exceptional modes remains NOT TRACED.
NV ordinary FOV updater `0x95DE30` approaches its target by min(remaining difference, dt × 30 / fIronSightsFOVTimeChange), default 0.25: 120 degrees/second. Caller `0x943748` skips it while the dialogue-active test succeeds; it resumes after dialogue.
Ordinary targets are fDefaultWorldFOV:Display = 75 and fDefault1stPersonFOV:Display = 75; bIronSightsZoomEnable:Combat = 1 gates this updater. Weapon aiming, VATS and Pip-Boy have additional branches, so this is not a universal exit duration.
FO3 close `0x63F060` writes saved-POV flags `0x108D018/0x108D019`, calls focus with skipTurning and refreshes camera through `0x77D670`; ordinary FOV updater `0x777DB0` has the same dt × 30 / 0.25 step and 75-degree defaults.
If neither a usable head nor a fallback node exists, NV `0x953060` skips target-framing/turn math; a node without a head geometry can use radius 32.
No explicit race-specific camera formula appeared in the traced focus function; creatures/robots can use their available nodes/bounds. Their actual model-by-model framing was not verified.
FO3 `0x775D80` likewise skips target-framing/turn math if neither head geometry nor its configured head node is usable; its node-only radius is 32. Vault 101 intercom model/script behavior remains NOT TRACED; no invented intercom offset is supplied.

### 12. PASS — Distinct terminal, barter, repair and container entry paths

NV `0x953060` direct calls were located in dialogue opening/update and in `0x706AE0`, a general menu-closing path; this does not make all menus dialogue cameras.
NV ShowBarterMenu `0x1173`: `0x5CEEB0` → `0x704F80` → `0x72D250`, opening `Data\Menus\barter_menu.xml`; ShowRepairMenu `0x117A`: `0x5D5200` → `0x704690` → `0x7B7570`, opening `Data\Menus\repair_services_menu.xml`.
Those two menu constructors do not call FocusOnActor or the dialogue opener; a menu opened from dialogue can inherit an already-focused view without being a new dialogue-camera transition.
FO3 corresponding chains are `0x52A230` → `0x619B10` → `0x634870` (barter menu `0x41D`) and `0x52CA00` → `0x619A40` → `0x673790` (repair services `0x422`), opening the same respective XML filenames; barter's constructor likewise has no FocusOnActor call.
NV container opener `0x75B310` and FO3 `0x63BAD0` load `Data\Menus\container_menu.xml` (menu `0x3F0`); neither constructor calls FocusOnActor or the dialogue opener.
NV rendered-terminal setup `0x7FC970` allocates a separate NiCamera (`0xA712F0`, vtable `0x109CB9C`), sets its frustum at `0x7FD2E9`, and registers it through `0x709D50`. FO3 setup `0x69BDE0` likewise builds its own camera/projection; these are not the dialogue focus routine.
Terminal frustum construction uses tan(fDefaultFOV × c × fRenderedTerminalFOV), c = 0.01745329238474369 (stored degrees-to-radians constant), defaults 75 and 0.15, plus aspect/near-plane scaling; 0.15 is an INI multiplier, not a 0.15-degree field of view or a dialogue zoom time.
Complete nested-menu restoration and every terminal subtype's close behavior remain NOT TRACED; the distinct entry paths above do not establish identical return behavior.

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
Audio manager receives a mode-4 request per hour (NV `0xAD8780`, FO3 `0x78C030`); its audible mixing consequences are NOT TRACED.

### 16. FAIL — A lying player camera is not part of normal sleep

The player bed activation paths in item 13 do not initiate a lying camera or a third-person lying body sequence; they open the menu at the existing player location, then cover the world with the sleep fader.
Consequently there are no player lie-down/get-up camera offsets or animation seconds to substitute into this normal menu path.
AI furniture sleeping, forced idle commands, and scripted bed scenes can be separate behavior; their full animation/camera timing is NOT TRACED here.

### 17. PASS — Sleep healing and hourly simulation

NV `0x94DF80` advances simulation by 3600 / current TimeScale per selected game hour through `0x867A40/0x96D4B0`; it does not merely multiply the live TimeScale by an arbitrary sleep speed.
On the final selected hour, if sleeping and not hardcore, `0x8A0960` restores damaged Health `0x10` and Fatigue `0x16`; for negative damage modifier m it requests 1 − m healing.
It also requests 1000 restoration for each condition AV `0x1F,0x1D,0x1E,0x1B,0x1C,0x1A,0x19`; helper `0x88B740` only issues positive restoration when that AV has a negative damage modifier. This is completion restoration, not a fixed HP-per-hour rule.
The per-hour path can also run general regeneration `0x88B510`: helper `0x648B00` gives HealRate/3600, or (2 × HealRate + 2)/3600 if actor state `+0x1AC == 9`, then multiplies by simulated delta; player perk entry point `0x0C` can add a rate.
Do not assume that animation-state-9 branch is set merely because the sleep-menu flag is true; these are different fields.
Hardcore skips this completion full restoration; when sleeping, it reduces SleepDeprivation AV `0x4B` by min(current value, fHCSleepRestorationMod = 60) per hourly step. fHCSleepDeprivationRate is 50 in FalloutNV.esm, default 0.26. The separate rested-bed ResetHealth script in item 18 has no hardcore guard.
FO3 `0x76C8F0` uses the same 3600/TimeScale step and calls full restore `0x6F6A80` at sleep completion, with the same Health/Fatigue/condition restoration requests; no hardcore branch exists there.

### 18. PASS — Rested-bed script flow and separate healing

NV PlayerBedSCRIPT `000B5431`: player activation sets local state 1, then invokes ordinary activation; MenuMode changes 1 to 2 if IsPCSleeping (`0x10AF`) is true; the next GameMode casts WellRestedSpell `00062697` on the player if state 2, then clears state.
The script itself tests neither hours nor ownership; qualification here is attachment to the bed plus observing the sleeping flag. Cancellation after that observation is not excluded by this script; activation restrictions still apply separately.
WellRestedMessageEffectSCRIPT `00062745`, on effect start, calls player ResetHealth (`0x1150`) and adds WellRestedPerk `00061822` if absent; on effect finish it removes that perk. WellRestedAbilityEffectSCRIPT `00062695` casts WellRestedSpell on effect finish.
NV ResetHealth `0x5C6B60` restores Health `0x10` by maximum minus current, then requests 999 restoration for conditions `0x1D,0x1E,0x1B,0x1C,0x19,0x1A,0x1F`; it has no hardcore test. This separate script call can heal despite the hardcore completion-heal exclusion in item 17.
WellRestedPerk uses entry `0x09`, multiply operation `0x03`, factor 1.1, conditioned on magic effect `00062744`. RewardXP `0x5D5140` → player virtual `+0x488` (`0x94FD10`) → `0x8D5100` applies entry 9 before adding Experience AV `0x18`: the bonus is +10% XP before integer conversion/caps.
NV FURN records attached to PlayerBedSCRIPT: SafehouseBed01 `001592E9`, SafehouseBed02 `001592E8`, SafehouseCot `001592E7`, NovacQueenBedRight `000CEECD`, HD01DukovsBed01SingleSide `0007E21A`, HD01BedHeart `0007E219`, HD02PlayerBed02 `000B543B`, HD01PlayerBed02 `000B5439`, HotelBedQueenLeft `0008B5E2`.
FO3 has identical compiled instructions and reference lists for all three scripts. ResetHealth `0x524A10` makes the same Health and 999-condition requests; RewardXP `0x52C940` → virtual `+0x478` (`0x76CA70`) → `0x72A300` likewise applies entry 9 before Experience AV `0x18`.
FO3's attached FURN set retains `0007E21A,0007E219,000B543B,000B5439,0008B5E2`, adds HD02PlayerBed01 `000B543A` and HD01PlayerBed01 `000B5438`, and lacks the four listed NV Safehouse/Novac attachments. Reference scripts/quest substitutions are not exhaustively inventoried.

### 19. PASS — Waiting without a bed

NV and FO3 use the same sleep/wait menu and hourly advancement, with the sleep flag false for waiting.
Both confirm handlers start the black fader only for sleep. Waiting does not invoke a lying animation or the sleep completion full-heal call; general simulation/regeneration can still run.
The same 1..24 selection and nominal one game-hour-per-real-second countdown apply; waiting is not ordinary real-time simulation with an enlarged TimeScale.

### 20. NOT TRACED — Rested duration and remaining interruption rules

Both masters' WellRestedSpell effect `00062744` has duration field 1440; its active-effect clock and conversion to game-clock hours remain NOT TRACED. The numeric field alone is not proof of 1440 in-game seconds.
Both `Fallout - Misc.bsa` menu files configure 24 choices; code adds 1 to the zero-based slider, giving 1..24 selected hours.
NV tick `0x7C0580` checks cancel input and remaining hours; cancel handler `0x7C0220` clears hours/sleep flag and closes. Full environmental/quest interruption behavior remains NOT TRACED.
NV nearby-hostility test `0x9764A0` uses fHostileActorInteriorDistance = 2000 or fHostileActorExteriorDistance = 3000; it checks enabled, living nearby actors and hostility predicates. Both GMSTs are absent from both masters; both executables have those defaults.
Activation also tests ownership, combat, trespass, radiation state and damaging Health effects; some predicate semantics and radiation thresholds remain NOT TRACED. Do not infer an exact threshold from the refusal-setting name.
Hardcore hunger/thirst/death interactions during the accelerated hours, audio mode-4 mixing, all quest interruptions, and FO3's complete restriction call graph remain NOT TRACED.

### 21. PASS — Numeric GMST inventory; NOT TRACED entries are explicit

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
| fHCSleepDeprivationRate | 50 / 0.26 | — / — |
| fHCSleepRestorationMod | — / 60 | — / — |
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
