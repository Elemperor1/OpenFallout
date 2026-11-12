# M15 notes (sound)

What the M15 pull requests changed, and what they rest on. Everything below that is a number or a rule was read from the executables (FalloutNV.exe and Fallout3.exe, the addresses are given) or from the data of the games; nothing from the games is copied here. "Listened to by ear" is Jacob's part and has not been done yet: what the Mac rounds can show is log lines and the state of the sound manager, not what comes out of the speakers.

## What was wrong

- `mwsound` was OpenMW's sound manager. A sound record of Fallout 3 or New Vegas (`SOUN`) was played from its file with a volume of 1, a minimum distance of 1 and a maximum of 255: the data of the record (`SNDD`) was read and thrown away, a record that names a folder (half of New Vegas's do) had no file, and the editor-id lookup did not exist.
- The animations of the actors never set a text-key listener, so no key of a `.kf` file made a sound: the `Sound: <editor id>` keys (about 2,550 in New Vegas's animations) and the `Enum: Left/Right/BackLeft/BackRight` keys that the games' footsteps are made from.

## SOUN data

`SNDD` is 36 bytes: the settings (12) and the curve (24). `FalloutScript::SoundData` (`components/falloutscript/sounddata.*`) reads them:

| Bytes | Field | Meaning |
|---|---|---|
| 0 | minimum attenuation | times 5 units: inside this the sound is at full volume |
| 1 | maximum attenuation | times 100 units (the executable compares a distance with this byte times 100: FalloutNV.exe 0x8384F0, 0x838608); 0 means the sound has no distance |
| 2 | frequency adjustment | percent, signed |
| 4-5 | flags | 1 random frequency shift, 2 play at random, 4 environment ignored, 8 random location, 0x10 loop, 0x20 menu sound, 0x40 2D, 0x80 360 LFE |
| 8-9 | static attenuation | hundredths of a decibel, the whole sound quieter by this |
| 10, 11 | stop time, start time | of the day, in 1440/256 minutes |
| 12-21 | curve | the volume in percent at five distances spread evenly from the minimum to the maximum (the editor's default is 100 50 20 5 0) |
| 24-27 | priority | |

The sound manager now plays a sound record with its own curve: the gain is worked out in the sound manager from the distance of the listener each update (`SoundParams::mDistanceGain`), and the output's own model of distance is turned off for such a sound (rolloff 0). The static attenuation, pitch (with the random shift flag), loop, environment-ignored and 2D flags are used. `OFBase::SoundManager::playFalloutSound` plays a record by editor id or by form; a record that names a folder plays one of its files at random. The script command `PlaySound`, the weapon shots and the footsteps all go through it.

## Footsteps (FalloutNV.exe 0x837CC0, "ImpactMixer")

The animation says a foot came down (`Enum: Left` 0, `Right` 1, `BackLeft` 2, `BackRight` 3: the index in the table of the words at 0x1199B50; the same table has Idle 4, Aware 5, Attack 6, Hit 7, Death 8, Equip 9, Unequip 10, CameraShake 11, StopSounds 12, HitShader 13, RiderSoft 14, RiderLoud 15, Weapon1 16, FoleySneak 17, FoleyWalk 18, FoleyRun 19, ArmorSneak 20, ArmorWalk 21, ArmorRun 22, Auxiliary1-8 23-30). The sound is a `SOUN` record named `FST<Surface><Slot>`:

- Surfaces (0x11A1888): ConcSolid, ConcBroken, MetalSolid, MetalHollow, MetalSheet, Wood, Sand, Dirt, Grass, Water, then ChainLink, Tile, Carpet in New Vegas (Fallout 3: 10 surfaces and 9 slots).
- Slots (0x11A18BC): WalkL, WalkR, RunR, RunL, SneakL, SneakR, StairsL, StairsR, Land, Jump (no Jump in Fallout 3). The key `Left` (value 0) plays the slot named R and `Right` the one named L; the back feet are the front ones (value minus 2). Sneaking, then stairs, then running, then walking decides which.
- The surface comes from the Havok material of what the foot is on, by the table at 0x11A17F8 (36 entries; Fallout 3's at 0xF6C538 has 32): material 0 and the ones past the end are concrete; water when the character wades. A surface that lacks a sound falls back through its parent (0x11A17C4: ConcBroken to ConcSolid, MetalHollow to MetalSolid, MetalSheet to MetalHollow, Sand and Grass and ChainLink to Dirt) and in the end to `FSTConcSolidWalkR` (the table is built at 0x839AB0).
- Volume: the player in the first person has the INI's `fPlayerFootVolume` (0.65), everyone else a random volume between 0.9 and 1 (0x476B70); a step is heard no farther than `fMaxFootstepDistance` (1100) from the listener, and the sound's own maximum distance counts too. Power armor adds `FSTArmorPowerL/R`, other heavy armor `FSTArmorL/R` (0x8382A0-0x838386), with a volume one minus a random tenth.
- The Havok material of a collision shape is read from the NIF (`BulletShape::mHavokMaterial`, from the first solid body), that of land from the `LTEX` record of the texture that shows most at the point (`HNAM`), through `OFBase::World::getFloorMaterial`.
- Not done: whether the ground is stairs is a bit of the actor's movement flags in the executable (0x1000000 of the flags at +0x410 of its process, tested at 0x838750) that the physics sets; the port does not know it yet, so the stairs slots are never chosen.

Landing plays the `Land` slot of the surface at any landing of the player (with the hard-landing sound over it when it hurts), and a jump the `Jump` slot (New Vegas only).

## Creatures and foley

- A creature's sounds are in its `CREA` record: each `CSDT` (a type, the same number as the `Enum` key) is followed by its `CSDI` sounds with a `CSDC` chance each; `CSCR` names the creature whose sounds it takes when it has none. The text key plays one of them, picked by the chances (a list that comes to less than 100 leaves the rest as no sound). Idle, aware, attack, hit and death keys of the animations play the same way.
- New Vegas armor has `SNAM` sounds (the sound, a chance, three bytes, a type of 17 to 22) and a `TNAM` audio template (`ArmorHeavyAudioTemplate`, `ArmorLightAudioTemplate`, ...) that it takes them from when it has none. `Enum: FoleyWalk` plays the type 18 sound of what the character wears.

## The player's body

The body of the player in the third person stops being updated when it is hidden (first person); its steps come from its animation, so it keeps its update going when it is not drawn (the node mask is `Mask_UpdateVisitor`, which is what the NIF loader uses for hidden nodes). The sounds that the animations name (`Sound:` keys) come from the first-person arms while they show and from the body when they do not, so that none is made twice.

## Music (FalloutNV.exe 0x992D90 combat, 0x591A40 folder picks, 0x830050 / 0x830080 attenuation)

What the engine does now, and what it does not.

**Fallout 3.** A cell names a music record (`XCMO`; `1NoMusic` is silence), a Fallout 3 cell can ask for a kind instead (`XCMT` 1 public, 2 dungeon), and a worldspace has its own (`ZNAM`). The record's `FNAM` ends with a backslash when it names a folder: every `.mp3` in `Data\Music\<folder>` is a track. The pick is the executable's: a random one that is not the last (a pick equal to the last one, other than the first, becomes the first, and a first that follows the first becomes the second), and the first track of a folder starts anywhere.

**New Vegas.** The music is not in the music records of the cells but in the audio markers of the world: a static reference with `MMRK` has a media location controller (`CNAM`, record `ALOC`), a radius (`XRDS`, 5000 or 10000) and the shares at which the layers of the music come in (`MNAM` 0.33, `NNAM` 0.67). The controller names location sets (`LNAM`) and battle sets (`GNAM`) of `MSET` records. 87 of the 93 controllers name location sets; of the other six, four (Goodsprings, Doc Mitchell's house, a small RV camp, Highway 95) keep them in the lists of the faction of the controller (`RNAM`) and two (Vicky and Vance) have none; the NCR and Legion camps have both: `HNAM` for a neutral standing of the faction to the player, `ZNAM` for an ally, `XNAM` a friend, `YNAM` an enemy. A controller with location sets of its own uses those; the ones without take the list of the faction's standing to the player (the same rule as for two characters: what scripts said, else the records), and the neutral list when the one asked for is empty.

A media set has up to six files (`NAM2` to `NAM7`: three layers for the day, three for the night), the decibels each is attenuated by, the boundaries (`JNAM` to `ONAM`, percent of the marker's radius) where the layers change, which of them are on (`PNAM`), fades and waits (`DNAM`, `ENAM`, `FNAM`). The player in a marker's radius hears the innermost layer that is on and whose boundary holds the player's distance. All the layers of the half-day play in step and only the wanted one is audible, so that a layer change is a cross-fade, not a restart. Day starts at 6:00 and night at 23:54 when the controller says `use defaults`, else at its `NAM5` and `NAM6` (256ths of a day).

**Fights.** The music of a fight is on when the enemies that hunt the player are a threat: the player's strength divided by the sum of theirs is under `fCombatMusicPlayerTargetedThreatRatio` (2); or characters nearer than `fCombatMusicNearCombatInnerRadius` (2500) fight with a friend of the player and are stronger than the player (times `fCombatMusicPlayerNearStrengthMult`). While it plays, a hunter keeps it on, and so does a fight nearer than the outer radius (3500). When the fight seems over a timer of `fCombatMusicStopTime` (5 s) starts, and the music stops at the look after it has run out. A look is every `fCombatMusicUpdateTime` seconds (1). The engine counts the strength of a character as its health and counts a hunter only while the cell it is in is loaded around the player (a character far away has lost the player). The music of a fight is the battle media set of the controller when the place has one, or else the battle music of the default objects (`DOBJ`).

**Volume and loops.** A music record's `ANAM` is a number of decibels: its size is how much quieter the track is, a positive one makes it loop. A change of music fades over 1 s (`iMusicTransitionInMS`).

**Scripts.** `PlayMusic` plays the track of a music record (with the record's attenuation and loop). While it plays, the music of the place is quiet and it comes back after it. The title music is `Data\Music\Special\MainTitle.mp3`.

**What the port chose, because nothing that was read says it:** how long the music of a place rests between two tracks (60 to 180 s, a random time; a track that loops has no rest), the delay before the first one (8 s) and after a fight (10 s, the battle sets' wait), the fade of a battle track (3 s). A layer is always the whole half-day set, never a retrigger or a random start in the track.

**Not done:** the intro and outro sounds of dungeon sets and the incidental sets (`NAM1` 2 and 3 of `MSET`), the `StreamMusic` command (the vanilla table row has no parameters), the retrigger and random loop modes of a controller (they play as loops), the marker's own triggers `MNAM` and `NNAM` (the boundaries of the set are used), a fight that the player is near but not in (the engine does not hand the music a list of characters that fight others), stingers for death, success and level up (`DOBJ`).

## Ambient sound (`FalloutNV.exe`, the acoustic space and the region)

**The space.** A cell names an acoustic space (`XCAS`, record `ASPC`). The space has up to four loops (`SNAM`: dawn, day, dusk, night) and, for an interior, only the first is used. The loops change at 5, 8, 18 and 20 o'clock (`fAmbientDayStart` and its kin of the game settings). A loop starts about 2 s after the player is in the space (it is not played in the instant of a door), fades in over 3 s, and fades out over 5 s when the space or the time of day changes to another loop. The loop of the space is a normal `SOUN` record: its flags, volume and file are those of the sound.

**The region.** The space names a sound region (`RDAT` of the sound kind and `RDSD` in the `REGN` record: a sound, the weather it is for, a chance). A sound of the region that loops is on while the weather is one it is for (pleasant `1`, cloudy `2`, rainy `4`, snow `8`: the classification bits of the weather) and fades in over 0.6 s and out over 1 s. A sound that does not loop is rolled now and then; one that asks for a random location (`SOUN` flag `0x08`) is played at a place around the player that is between a quarter and three quarters of how far the sound is heard.

**The weather.** The `SNAM` sounds of the weather in use (rain, wind, thunder) are heard out of doors: those that loop play while the weather is, the others are rolled like the sounds of a region.

**What the port chose, because nothing that was read says it:** how often the sounds of a region are rolled (each sound has its own clock of 120 s that starts at a random time, and it is rolled with its chance as a share of a million when the clock runs out, so the sounds are spread out), the chance of a thunder-like sound of the weather (0.3 per roll), the distance of a sound with a random location. The weather in use is the one the sky shows now (the port does not blend the sounds of the weather it is changing to).

**Files.** 340 sound records of New Vegas (most of the loops: a campfire, flames, water, the hum of a house) name `.wav` files that the archives have only as `.ogg`; the port looks for the `.ogg` when the `.wav` and the `.mp3` are not there.

**Not done:** the radio (the Pip-Boy stations, `iRadioUpdateInterval`, the signal of a transmitter), the sounds of water (`WATR` `SNAM`: not loops in the port yet), the loops of an interior cell that has no space, the random and retrigger loop modes.

## Checks

- Unit tests: `FalloutScriptFootsteps*`, `FalloutScriptSoundData*` (components), `ESM4CreatureTest.readsTheSoundLists...`, `ESM4RepairListTest.armorReadsItsWornSounds...`, `OFSoundFalloutSoundTest.*` (the folder and file lookup of the sound records).
- Music: `FalloutScriptMusic*` (the rules, components), `ESM4MusicRecordTest.*` (loaders), `OFSoundFalloutMusic*` (files and mixer), `FalloutScriptCombatCommandsTest.a_faction_stands*`. The round `scripts/openfallout/round/rounds/m15-music.json` logs `Music:` lines (the controller and set at each place, the track layers and their gains, combat music on and off, scripts' tracks) and `Music track N plays <file>` for each file that is started.
- The engine logs `Footstep of <ref>: material N, <surface>, <slot> -> <sound>` for the first few steps.
- Ambient: `FalloutScriptAmbient*` (the rules, components), `ESM4MusicRecordTest.regionReadsItsSounds` (the loader). The round `scripts/openfallout/round/rounds/m15-ambient.json` logs `Ambient:` lines (the space the player is in and its region, each loop that starts or fades out, each sound that is played).
