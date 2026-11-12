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

## Checks

- Unit tests: `FalloutScriptFootsteps*`, `FalloutScriptSoundData*` (components), `ESM4CreatureTest.readsTheSoundLists...`, `ESM4RepairListTest.armorReadsItsWornSounds...`, `OFSoundFalloutSoundTest.*` (the folder and file lookup of the sound records).
- The engine logs `Footstep of <ref>: material N, <surface>, <slot> -> <sound>` for the first few steps.
