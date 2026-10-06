# Plugin census results

`esmtool census` run on 2026-10-04 on Jacob's MacBook, through a Remote Control session, on 16 plugins: Fallout 3, its five add-ons, Fallout: New Vegas and its nine add-ons and packs. Later the same evening it was run on four Tale of Two Wastelands plugins (see "Tale of Two Wastelands plugins" below). The census program is the one merged in PR #2 (master `be6ca447`). This page holds counts only, plus each plugin's file name, format version and master list, which the reader takes from the plugin's `TES4` header. No other record content appears on it.

## What the census found

- **In the 16 plugins from the games' own installers, no record failed to load.** All 1,561,956 records after each plugin's `TES4` header were counted, and the Failed column is 0 everywhere in the first table. (The reader consumes the `TES4` header record before the census starts, so it has no row and the files hold 16 more records than the tables show.) No `Unknown subrecord` line was printed for any of them. Every loader that exists accepts every record of its type in those 16 plugins. "Parsed" only means the loader did not throw: it does not mean the fields are right, or that the engine keeps them (the roadmap's section 2 lists what is discarded after parsing).
- **1,550,732 records (99.3%) have a loader and 11,224 have none.** The gap is in the number of record types, not in the volume. The big types (`REFR`, `CELL`, `LAND`, `INFO`, `DIAL`, `NAVM`, `PACK`, `NPC_`, `SCPT`) are all parsed.
- **42 record types have no loader.** Fallout 3 uses 28 of them (2,973 of 718,951 records in `Fallout3.esm`). New Vegas uses all 42 (5,496 of 465,016 records in `FalloutNV.esm`). New Vegas adds 14 unparsed types of its own (`AMEF`, `CCRD`, `CDCK`, `CHAL`, `CHIP`, `CMNY`, `CSNO`, `DEHY`, `HUNG`, `LSCT`, `RCCT`, `RCPE`, `REPU`, `SLPD`).
- Whether a type has a loader is the same in both games for every type they share.
- **Update 2026-10-06: `FACT` now has a loader** (PR #10) and none of its 2,528 records in the 20 plugins fails. The tables below were made before it and still show `FACT` as having no loader, so the counts in this list and in the tables are the first runs'. See "Faction records".
- **Update 2026-10-06: every record type has a loader now, and no record fails** (PRs #11 and #12). The census on the head of PR #12 parses all records of all 20 plugins, with 0 in the No parser column and 0 in the Failed column, and the 17 failing `NPC_` records load. The tables above this one were made before that and still show the earlier counts. See "Loaders of M1 slices 3 to 5".
- **Tale of Two Wastelands: 17 `NPC_` records fail to load, and no unparsed record type is new.** The four TTW plugins hold 1,279,509 records after their headers. Every failure is an `NPC_` record: three are unknown subrecords (`DLVT` twice, `LSNA` once) and the other 14 are loader errors for which the census withholds the message. Every `NPC_` record in the vanilla `Fallout3.esm` and `FalloutNV.esm` loads, so the failing records exist only in the TTW-patched files. The files came from a community installer and were not checked against reference hashes, so the failures may come from the installer instead of from TTW's data.
- **Compiled scripts line up with their headers, except for the variable count.** The 16 plugins hold 16,180 `INFO`, 2,437 `QUST` and 6,035 `SCPT` scripts, with 3.1 MB of bytecode. In every script the compiled size in the header equals the bytecode read, and the reference count equals the references read in all but one source-only `INFO` script. The header's variable count does not equal the number of local variables read: it is above the highest declared index in 449 scripts. See "Scripts held by records".

## Where the files came from

- **Fallout 3:** the installed `Data` folder. A later check against the checksums in the GOG installer found it unmodified: 157 of 158 game files identical, and the one missing is a GOG helper tool, not part of the game.
- **New Vegas:** not installed on that Mac, only the GOG offline installer. The `.esm` plugins were unpacked from the installer with `innoextract` into a scratch folder outside the repository. This is the installer route the roadmap proposed, and it works.
- **Tale of Two Wastelands:** the official installer (v3.4) was run under Wine on the Mac and showed no progress for about 30 minutes, with the output folder empty, so it was stopped. It was not shown to be hung and may only have been slow. The plugins on this page come from the community installer `TTW_Linux_Installer` (GPL-3, commit `936b0ae`), built from source on the Mac after its code was read (it has no network code and only reads the game folders). It ran on fresh unpacked copies of both games, each checked against its GOG installer's checksums, with the game folders read-only. The install took about 3 minutes. It ran 196,915 operations, and 2 failed: `libvorbis.dll` and `libvorbisfile.dll`, "Permission denied", which is the read-only game folder (inferred).
- Each plugin was run from its own folder with a bare file name, so the headers hold no paths.

## Per plugin

| Plugin | Format version | Master | Records | Parsed | No parser | Failed |
|---|---|---|---:|---:|---:|---:|
| `Fallout3.esm` | 0.94 | none | 718,951 | 715,978 | 2,973 | 0 |
| `Anchorage.esm` | 0.94 | `Fallout3.esm` | 40,569 | 40,421 | 148 | 0 |
| `BrokenSteel.esm` | 0.94 | `Fallout3.esm` | 41,304 | 41,123 | 181 | 0 |
| `PointLookout.esm` | 0.94 | `Fallout3.esm` | 61,982 | 61,768 | 214 | 0 |
| `ThePitt.esm` | 0.94 | `Fallout3.esm` | 32,839 | 32,717 | 122 | 0 |
| `Zeta.esm` | 0.94 | `Fallout3.esm` | 36,523 | 36,274 | 249 | 0 |
| `FalloutNV.esm` | 1.34 | none | 465,016 | 459,520 | 5,496 | 0 |
| `DeadMoney.esm` | 1.32 | `FalloutNV.esm` | 40,084 | 39,589 | 495 | 0 |
| `HonestHearts.esm` | 1.33 | `FalloutNV.esm` | 36,201 | 35,902 | 299 | 0 |
| `OldWorldBlues.esm` | 1.34 | `FalloutNV.esm` | 52,683 | 52,150 | 533 | 0 |
| `LonesomeRoad.esm` | 1.34 | `FalloutNV.esm` | 34,988 | 34,606 | 382 | 0 |
| `GunRunnersArsenal.esm` | 1.34 | `FalloutNV.esm` | 778 | 652 | 126 | 0 |
| `ClassicPack.esm` | 1.32 | `FalloutNV.esm` | 19 | 16 | 3 | 0 |
| `CaravanPack.esm` | 1.32 | `FalloutNV.esm` | 7 | 6 | 1 | 0 |
| `MercenaryPack.esm` | 1.32 | `FalloutNV.esm` | 7 | 6 | 1 | 0 |
| `TribalPack.esm` | 1.32 | `FalloutNV.esm` | 5 | 4 | 1 | 0 |
| **All 16** | | | **1,561,956** | **1,550,732** | **11,224** | **0** |

Counts exclude each plugin's `TES4` header record.

## Tale of Two Wastelands plugins

TTW builds its plugins from the player's own Fallout 3 and New Vegas files, so these four exist only after an installer has run (see "Where the files came from"). The installer also produced patched copies of the add-on plugins and more than 40 rebuilt `.bsa` archives, which were not censused. All four census runs exited with status 0.

| Plugin | Format version | Master | Records | Parsed | No parser | Failed |
|---|---|---|---:|---:|---:|---:|
| `TaleOfTwoWastelands.esm` | 1.34 | 12 masters | 83,154 | 80,732 | 2,417 | 5 |
| `YUPTTW.esm` | 1.34 | 13 masters, including `TaleofTwoWastelands.esm` | 27,118 | 26,786 | 332 | 0 |
| `Fallout3.esm` (TTW-patched) | 1.34 | the New Vegas plugins (list not recorded) | 704,183 | 703,694 | 479 | 10 |
| `FalloutNV.esm` (TTW-patched) | 1.34 | none | 465,054 | 459,550 | 5,502 | 2 |

Counts exclude each plugin's `TES4` header record, as above.

Compared with the vanilla files, the patched `Fallout3.esm` has 14,768 fewer records (704,183 against 718,951), 2,494 fewer with no parser, and format version 1.34 where the vanilla file has 0.94. The patched `FalloutNV.esm` has 38 more records (465,054 against 465,016) and 6 more with no parser. The census does not say why.

### Records that fail to load

| Plugin | Record | Failed | What the census printed |
|---|---|---:|---|
| `TaleOfTwoWastelands.esm` | `NPC_` | 5 of 2,529 | 4 loader errors (message withheld), 1 `ESM4::NPC_::load - Unknown subrecord DLVT` |
| `Fallout3.esm` (TTW-patched) | `NPC_` | 10 of 1,642 | 8 loader errors (message withheld), 1 `Unknown subrecord DLVT`, 1 `Unknown subrecord LSNA` |
| `FalloutNV.esm` (TTW-patched) | `NPC_` | 2 of 3,816 | 2 loader errors (message withheld) |

`YUPTTW.esm` has no failures. In the vanilla files all 1,647 (Fallout 3) and 3,816 (New Vegas) `NPC_` records load. The census withholds loader error messages because they can contain record contents; the unknown-subrecord lines hold only a loader name and a four-letter code.

### Record types with no loader

28 of the 86 record types in `TaleOfTwoWastelands.esm` have no loader, and all of them are among the 42 listed in the next section. TTW adds no new unparsed type.

### Every record type in `TaleOfTwoWastelands.esm`

| Record | Records | Parsed | No parser | Failed |
|---|---:|---:|---:|---:|
| `ACHR` | 677 | 677 | 0 | 0 |
| `ACRE` | 641 | 641 | 0 | 0 |
| `ACTI` | 181 | 181 | 0 | 0 |
| `ADDN` | 16 | 0 | 16 | 0 |
| `ALCH` | 234 | 234 | 0 | 0 |
| `ALOC` | 9 | 9 | 0 | 0 |
| `AMEF` | 4 | 0 | 4 | 0 |
| `AMMO` | 130 | 130 | 0 | 0 |
| `ANIO` | 3 | 3 | 0 | 0 |
| `ARMA` | 81 | 81 | 0 | 0 |
| `ARMO` | 598 | 598 | 0 | 0 |
| `ASPC` | 67 | 67 | 0 | 0 |
| `AVIF` | 5 | 0 | 5 | 0 |
| `BOOK` | 9 | 9 | 0 | 0 |
| `BPTD` | 24 | 24 | 0 | 0 |
| `CELL` | 3,818 | 3,818 | 0 | 0 |
| `CHAL` | 194 | 0 | 194 | 0 |
| `CLAS` | 29 | 29 | 0 | 0 |
| `CONT` | 342 | 342 | 0 | 0 |
| `CPTH` | 37 | 0 | 37 | 0 |
| `CREA` | 1,282 | 1,282 | 0 | 0 |
| `CSTY` | 1 | 0 | 1 | 0 |
| `DEBR` | 2 | 0 | 2 | 0 |
| `DIAL` | 2,942 | 2,942 | 0 | 0 |
| `DOOR` | 166 | 166 | 0 | 0 |
| `ECZN` | 6 | 0 | 6 | 0 |
| `EFSH` | 10 | 0 | 10 | 0 |
| `ENCH` | 129 | 0 | 129 | 0 |
| `EXPL` | 18 | 0 | 18 | 0 |
| `EYES` | 1 | 1 | 0 | 0 |
| `FACT` | 588 | 0 | 588 | 0 |
| `FLST` | 322 | 322 | 0 | 0 |
| `FURN` | 9 | 9 | 0 | 0 |
| `GLOB` | 23 | 23 | 0 | 0 |
| `GMST` | 111 | 111 | 0 | 0 |
| `HAIR` | 1 | 1 | 0 | 0 |
| `HDPT` | 6 | 6 | 0 | 0 |
| `IDLE` | 193 | 193 | 0 | 0 |
| `IDLM` | 6 | 6 | 0 | 0 |
| `IMAD` | 5 | 0 | 5 | 0 |
| `IMOD` | 141 | 141 | 0 | 0 |
| `INFO` | 11,146 | 11,146 | 0 | 0 |
| `IPCT` | 15 | 0 | 15 | 0 |
| `IPDS` | 9 | 0 | 9 | 0 |
| `KEYM` | 28 | 28 | 0 | 0 |
| `LAND` | 46 | 46 | 0 | 0 |
| `LIGH` | 9 | 9 | 0 | 0 |
| `LSCR` | 401 | 0 | 401 | 0 |
| `LSCT` | 1 | 0 | 1 | 0 |
| `LVLC` | 125 | 125 | 0 | 0 |
| `LVLI` | 1,759 | 1,759 | 0 | 0 |
| `LVLN` | 380 | 380 | 0 | 0 |
| `MESG` | 185 | 0 | 185 | 0 |
| `MGEF` | 73 | 0 | 73 | 0 |
| `MISC` | 165 | 165 | 0 | 0 |
| `MSET` | 40 | 40 | 0 | 0 |
| `MSTT` | 12 | 12 | 0 | 0 |
| `MUSC` | 1 | 1 | 0 | 0 |
| `NAVI` | 1 | 1 | 0 | 0 |
| `NAVM` | 247 | 247 | 0 | 0 |
| `NOTE` | 435 | 435 | 0 | 0 |
| `NPC_` | 2,529 | 2,524 | 0 | 5 |
| `PACK` | 1,327 | 1,327 | 0 | 0 |
| `PERK` | 101 | 0 | 101 | 0 |
| `PGRE` | 677 | 677 | 0 | 0 |
| `PROJ` | 135 | 0 | 135 | 0 |
| `QUST` | 168 | 168 | 0 | 0 |
| `RACE` | 2 | 2 | 0 | 0 |
| `RCCT` | 4 | 0 | 4 | 0 |
| `RCPE` | 352 | 0 | 352 | 0 |
| `REFR` | 46,029 | 46,029 | 0 | 0 |
| `REGN` | 11 | 11 | 0 | 0 |
| `RGDL` | 3 | 0 | 3 | 0 |
| `SCOL` | 15 | 15 | 0 | 0 |
| `SCPT` | 1,263 | 1,263 | 0 | 0 |
| `SOUN` | 574 | 574 | 0 | 0 |
| `SPEL` | 78 | 0 | 78 | 0 |
| `STAT` | 660 | 660 | 0 | 0 |
| `TACT` | 59 | 59 | 0 | 0 |
| `TERM` | 136 | 136 | 0 | 0 |
| `TXST` | 240 | 240 | 0 | 0 |
| `VTYP` | 2 | 0 | 2 | 0 |
| `WATR` | 1 | 0 | 1 | 0 |
| `WEAP` | 546 | 546 | 0 | 0 |
| `WRLD` | 61 | 61 | 0 | 0 |
| `WTHR` | 42 | 0 | 42 | 0 |
| **All** | **83,154** | **80,732** | **2,417** | **5** |

The tables for the other three TTW plugins are not on this page.

## Record types with no loader in the 16 plugins

Ordered by how many records the two base plugins hold. "M1 order" is the order the roadmap gives for adding loaders (1 `FACT`, 2 weather, climate and water, 3 effects, perks and actor values, 4 the rest).

| Record | Meaning | `Fallout3.esm` | `FalloutNV.esm` | M1 order |
|---|---|---:|---:|:-:|
| `MESG` | Message | 518 | 1,144 | 4 |
| `FACT` | Faction | 326 | 682 | 1 |
| `CPTH` | Camera path | 307 | 418 | 4 |
| `CAMS` | Camera shot | 229 | 276 | 4 |
| `MGEF` | Base effect | 163 | 289 | 3 |
| `SPEL` | Actor effect (spell) | 160 | 270 | 3 |
| `LSCR` | Load screen | 150 | 208 | 4 |
| `IMAD` | Image space modifier | 112 | 215 | 4 |
| `CCRD` | Caravan card | - | 270 | 4 |
| `PERK` | Perk | 87 | 176 | 3 |
| `ENCH` | Object effect | 89 | 145 | 3 |
| `EXPL` | Explosion | 78 | 154 | 4 |
| `IPCT` | Impact | 102 | 125 | 4 |
| `VTYP` | Voice type | 75 | 100 | 4 |
| `PROJ` | Projectile | 52 | 95 | 4 |
| `ECZN` | Encounter zone | 129 | 17 | 4 |
| `CSTY` | Combat style | 48 | 84 | 4 |
| `WATR` | Water | 53 | 78 | 2 |
| `AVIF` | Actor value information | 60 | 64 | 3 |
| `IMGS` | Image space | 48 | 67 | 4 |
| `CHAL` | Challenge | - | 105 | 4 |
| `RCPE` | Recipe | - | 105 | 4 |
| `IPDS` | Impact data set | 41 | 60 | 4 |
| `WTHR` | Weather | 27 | 63 | 2 |
| `ADDN` | Addon node | 37 | 37 | 4 |
| `EFSH` | Effect shader | 28 | 35 | 4 |
| `RGDL` | Ragdoll | 23 | 38 | 4 |
| `AMEF` | Ammo effect | - | 54 | 4 |
| `CLMT` | Climate | 18 | 31 | 2 |
| `MICN` | Menu icon | 2 | 12 | 4 |
| `CDCK` | Caravan deck | - | 13 | 4 |
| `REPU` | Reputation | - | 13 | 4 |
| `DEBR` | Debris | 6 | 6 | 4 |
| `RADS` | Radiation stage | 5 | 5 | 4 |
| `RCCT` | Recipe category | - | 10 | 4 |
| `CMNY` | Caravan money | - | 6 | 4 |
| `CHIP` | Casino chip | - | 5 | 4 |
| `CSNO` | Casino | - | 5 | 4 |
| `DEHY` | Dehydration stage | - | 5 | 4 |
| `HUNG` | Hunger stage | - | 5 | 4 |
| `SLPD` | Sleep deprivation stage | - | 5 | 4 |
| `LSCT` | Load screen type | - | 1 | 4 |

The add-on plugins contain no unparsed type that is missing from this list.

## Every record type in the two base plugins

| Record | Loader | `Fallout3.esm` | `FalloutNV.esm` |
|---|:-:|---:|---:|
| `ACHR` | yes | 2,154 | 3,386 |
| `ACRE` | yes | 3,349 | 2,999 |
| `ACTI` | yes | 774 | 1,143 |
| `ADDN` | no | 37 | 37 |
| `ALCH` | yes | 71 | 189 |
| `ALOC` | yes | - | 89 |
| `AMEF` | no | - | 54 |
| `AMMO` | yes | 30 | 92 |
| `ANIO` | yes | 101 | 152 |
| `ARMA` | yes | 92 | 131 |
| `ARMO` | yes | 237 | 389 |
| `ASPC` | yes | 59 | 113 |
| `AVIF` | no | 60 | 64 |
| `BOOK` | yes | 26 | 27 |
| `BPTD` | yes | 32 | 49 |
| `CAMS` | no | 229 | 276 |
| `CCRD` | no | - | 270 |
| `CDCK` | no | - | 13 |
| `CELL` | yes | 42,410 | 30,497 |
| `CHAL` | no | - | 105 |
| `CHIP` | no | - | 5 |
| `CLAS` | yes | 53 | 74 |
| `CLMT` | no | 18 | 31 |
| `CMNY` | no | - | 6 |
| `CONT` | yes | 535 | 2,478 |
| `CPTH` | no | 307 | 418 |
| `CREA` | yes | 533 | 1,578 |
| `CSNO` | no | - | 5 |
| `CSTY` | no | 48 | 84 |
| `DEBR` | no | 6 | 6 |
| `DEHY` | no | - | 5 |
| `DIAL` | yes | 6,381 | 18,215 |
| `DOBJ` | yes | 1 | 1 |
| `DOOR` | yes | 319 | 320 |
| `ECZN` | no | 129 | 17 |
| `EFSH` | no | 28 | 35 |
| `ENCH` | no | 89 | 145 |
| `EXPL` | no | 78 | 154 |
| `EYES` | yes | 8 | 12 |
| `FACT` | no | 326 | 682 |
| `FLST` | yes | 243 | 464 |
| `FURN` | yes | 183 | 234 |
| `GLOB` | yes | 155 | 218 |
| `GMST` | yes | 530 | 648 |
| `GRAS` | yes | 9 | 24 |
| `HAIR` | yes | 67 | 67 |
| `HDPT` | yes | 61 | 61 |
| `HUNG` | no | - | 5 |
| `IDLE` | yes | 1,247 | 1,597 |
| `IDLM` | yes | 122 | 211 |
| `IMAD` | no | 112 | 215 |
| `IMGS` | no | 48 | 67 |
| `IMOD` | yes | - | 50 |
| `INFO` | yes | 22,327 | 23,247 |
| `INGR` | yes | 1 | 1 |
| `IPCT` | no | 102 | 125 |
| `IPDS` | no | 41 | 60 |
| `KEYM` | yes | 160 | 283 |
| `LAND` | yes | 40,256 | 29,363 |
| `LGTM` | yes | 19 | 31 |
| `LIGH` | yes | 368 | 501 |
| `LSCR` | no | 150 | 208 |
| `LSCT` | no | - | 1 |
| `LTEX` | yes | 51 | 89 |
| `LVLC` | yes | 60 | 343 |
| `LVLI` | yes | 972 | 2,738 |
| `LVLN` | yes | 89 | 365 |
| `MESG` | no | 518 | 1,144 |
| `MGEF` | no | 163 | 289 |
| `MICN` | no | 2 | 12 |
| `MISC` | yes | 237 | 507 |
| `MSET` | yes | - | 140 |
| `MSTT` | yes | 238 | 241 |
| `MUSC` | yes | 31 | 20 |
| `NAVI` | yes | 1 | 1 |
| `NAVM` | yes | 7,198 | 4,771 |
| `NOTE` | yes | 840 | 894 |
| `NPC_` | yes | 1,647 | 3,816 |
| `PACK` | yes | 3,266 | 4,163 |
| `PERK` | no | 87 | 176 |
| `PGRE` | yes | 350 | 174 |
| `PROJ` | no | 52 | 95 |
| `PWAT` | yes | 56 | 29 |
| `QUST` | yes | 192 | 436 |
| `RACE` | yes | 22 | 22 |
| `RADS` | no | 5 | 5 |
| `RCCT` | no | - | 10 |
| `RCPE` | no | - | 105 |
| `REFR` | yes | 568,107 | 307,710 |
| `REGN` | yes | 139 | 276 |
| `REPU` | no | - | 13 |
| `RGDL` | no | 23 | 38 |
| `SCOL` | yes | 54 | 98 |
| `SCPT` | yes | 1,257 | 2,576 |
| `SLPD` | no | - | 5 |
| `SOUN` | yes | 1,583 | 3,190 |
| `SPEL` | no | 160 | 270 |
| `STAT` | yes | 5,803 | 6,785 |
| `TACT` | yes | 49 | 87 |
| `TERM` | yes | 378 | 344 |
| `TREE` | yes | 9 | 3 |
| `TXST` | yes | 244 | 493 |
| `VTYP` | no | 75 | 100 |
| `WATR` | no | 53 | 78 |
| `WEAP` | yes | 160 | 261 |
| `WRLD` | yes | 32 | 14 |
| `WTHR` | no | 27 | 63 |
| **All** | | **718,951** | **465,016** |

## Scripts held by records

Run on 2026-10-05 on the same Mac, with the branch that keeps script bytecode (PR #7, commit `958266b1`; all 20 runs exited 0, the slowest took 3 seconds). It adds a "Scripts held in line by records" table to the census output (see `build-baseline.md`). The record counts in the main tables did not change in any cell, for all 20 plugins, and the Failures lines are the same. Scripts are counted per script, not per record: an `INFO` record can hold a begin script and an end script, and a `QUST` record holds one script per log entry. A script counts only if it holds something (bytecode, a non-zero compiled size, source, variables or references).

| Plugin | `INFO` scripts | `INFO` bytecode (bytes) | `QUST` scripts | `QUST` bytecode (bytes) | `SCPT` scripts | `SCPT` bytecode (bytes) |
|---|---:|---:|---:|---:|---:|---:|
| `Fallout3.esm` | 5,704 | 187,924 | 871 | 88,435 | 1,257 | 413,561 |
| `Anchorage.esm` | 176 | 6,420 | 32 | 2,927 | 192 | 85,385 |
| `BrokenSteel.esm` | 619 | 16,203 | 143 | 14,746 | 252 | 114,948 |
| `PointLookout.esm` | 215 | 6,663 | 126 | 8,270 | 209 | 78,732 |
| `ThePitt.esm` | 327 | 10,937 | 61 | 6,369 | 127 | 28,338 |
| `Zeta.esm` | 217 | 5,207 | 102 | 8,395 | 283 | 153,393 |
| `FalloutNV.esm` | 7,372 | 414,338 | 737 | 44,303 | 2,576 | 848,781 |
| `DeadMoney.esm` | 679 | 19,453 | 107 | 8,463 | 347 | 127,255 |
| `HonestHearts.esm` | 360 | 15,280 | 101 | 12,555 | 239 | 94,567 |
| `OldWorldBlues.esm` | 353 | 15,613 | 107 | 6,366 | 278 | 143,800 |
| `LonesomeRoad.esm` | 140 | 5,089 | 50 | 2,536 | 242 | 131,980 |
| `GunRunnersArsenal.esm` | 18 | 2,161 | - | - | 28 | 8,472 |
| `ClassicPack.esm` | - | - | - | - | 2 | 237 |
| `CaravanPack.esm` | - | - | - | - | 1 | 161 |
| `MercenaryPack.esm` | - | - | - | - | 1 | 161 |
| `TribalPack.esm` | - | - | - | - | 1 | 143 |
| **All 16** | **16,180** | **705,288** | **2,437** | **203,365** | **6,035** | **2,229,914** |

A dash means the census printed no row, because that plugin has no script of that kind. The four Tale of Two Wastelands plugins hold copies of scripts from the vanilla plugins (the patched `FalloutNV.esm` is almost the same as the vanilla one), so their rows are not added to the totals above and are not repeated here (the TTW check results are below).

### What the script header says against what was read

Each script has a header with three numbers (compiled size, reference count, variable count), and the census compares each with what the loader read. The final run, on branch head `958266b1`, printed this for the 16 installer plugins:

| Column | What it flags | Scripts flagged (of 24,652) |
|---|---|---:|
| Bad size | header compiled size differs from the `SCDA` bytes read | 0 |
| Bad refs | header reference count differs from the `SCRO` plus `SCRV` entries read | 1 |
| Bad vars | header variable count is lower than the highest `SLSD` index | 0 |

- **The one Bad refs script** is the end script of an `INFO` record in `FalloutNV.esm` (and its copy in the TTW-patched `FalloutNV.esm`). It has source text only, 65 bytes, no bytecode and no references. Its header says 1 reference and every other header field is 0.
- **The four TTW plugins** have Bad size 0 and Bad refs 0 apart from that same copy. Bad vars flags 13 `SCPT` scripts: 12 in `TaleOfTwoWastelands.esm` and 1 in `YUPTTW.esm`. In each of them the header count equals the number of `SLSD` entries, but the entries carry indices above it (for example a count of 2 with indices 10 and 11). The patched `Fallout3.esm` and `FalloutNV.esm` have none. The census does not say why TTW's own scripts do this.
- **The header variable count is not the number of `SLSD` entries.** The first version of the check compared the two and flagged 1,389 of 6,035 `SCPT` scripts, 23.0% (with 37 `INFO` and 6 `QUST`). A separate counting program, which reads the raw plugins and prints numbers only, found the cause. Against the highest `SLSD` index (H is the header count, M the highest index, M = 0 without `SLSD`):

| Script holder | Scripts | H = M | H > M | H < M |
|---|---:|---:|---:|---:|
| `SCPT` | 6,035 | 5,625 | 410 | 0 |
| `INFO` (begin 5,340, end 10,840) | 16,180 | 16,146 | 34 | 0 |
| `QUST` log entries | 2,437 | 2,432 | 5 | 0 |
| **All** | **24,652** | **24,203** | **449** | **0** |

- **Where H is above M, the extra variables are not declared in the source.** All 449 scripts have source text. In the 206 with at least one `SLSD` entry, the number of lines declaring a variable (a line starting with `short`, `long`, `int`, `float` or `ref`) equals the number of entries in every one. The other 243 have no `SLSD` entry and no declaration line. Among the scripts where H = M and that have source, the declaration lines equal the entries in 24,200 of 24,202. So the header seems to count variables that are no longer declared, for example deleted ones. That is a reading of these counts, not something the files state.
- **The 3 `SCPT` in `FalloutNV.esm` with a header count one below the entry count** (counts 7, 2 and 3 against 8, 3 and 4 entries) each list the index 1 twice, so H equals M and the check does not flag them.
- **`SLSD` index order cannot be assumed.** No script has index 0. 14 `SCPT` scripts repeat an index (`Fallout3.esm` 2, `PointLookout.esm` 1, `Zeta.esm` 1, `FalloutNV.esm` 10). 915 of the 4,095 scripts with any `SLSD` entry list their indices out of ascending order (914 `SCPT`, 1 `QUST`); in 351 of those, the indices are exactly 1 to the entry count once sorted. Every `SCRV` entry points at an index that exists among the `SLSD` entries (checked on `Fallout3.esm`).

So the compiled size and the reference table agree with the header in every installer-plugin script but one, which is what a virtual machine for the bytecode needs, and the variable count is the one number a virtual machine should not take from the header alone, because the header counts variables the record no longer declares.

## Faction records

Run on 2026-10-06 on the same Mac with the branch that adds the `FACT` loader (PR #10, first on commit `1d1ff5f2`, then rerun on `a8bf5b07` and on the final head `0070f8bd` after review fixes; every run exited 0 and the three outputs are identical line for line). The review fixes made the loader stricter about cut-off strings and leftover bytes, and changed `Reader::getSubRecordHeader` to take the end of a compressed record as the size in its header plus 4 bytes (see below). `Fallout3.esm` had 326 `FACT` records and `FalloutNV.esm` 682 in the "No parser" column of the first runs. They are now in "Parsed", every `FACT` record in all 20 plugins loads, and **no other cell of the main tables moved in any plugin**: the only differences from the earlier runs are the `FACT` rows and the totals row. The Failures lines are unchanged (the 17 `NPC_` records), and the "Scripts held in line by records" table is unchanged. The tables above this section are from the earlier runs, so they still show `FACT` as having no loader.

| Plugin | `FACT` records | Failed | `XNAM` relations | With `CNAM` | With `WMI1` | With ranks | Most ranks in one record |
|---|---:|---:|---:|---:|---:|---:|---:|
| `Fallout3.esm` | 326 | 0 | 666 | 35 | 0 | 51 | 8 |
| `Anchorage.esm` | 24 | 0 | 58 | 0 | 0 | 9 | 1 |
| `BrokenSteel.esm` | 38 | 0 | 33 | 0 | 0 | 1 | 1 |
| `PointLookout.esm` | 21 | 0 | 17 | 0 | 0 | 0 | - |
| `ThePitt.esm` | 32 | 0 | 33 | 0 | 0 | 0 | - |
| `Zeta.esm` | 10 | 0 | 40 | 0 | 0 | 1 | 5 |
| `FalloutNV.esm` | 682 | 0 | 1,314 | 34 | 46 | 57 | 9 |
| `DeadMoney.esm` | 14 | 0 | 17 | 0 | 0 | 0 | - |
| `HonestHearts.esm` | 36 | 0 | 159 | 0 | 0 | 1 | 1 |
| `OldWorldBlues.esm` | 25 | 0 | 29 | 0 | 0 | 0 | - |
| `LonesomeRoad.esm` | 13 | 0 | 28 | 0 | 2 | 0 | - |
| `GunRunnersArsenal.esm` | 2 | 0 | 0 | 0 | 0 | 0 | - |
| **All 16** | **1,223** | **0** | **2,394** | **69** | **48** | **120** | **9** |
| `TaleOfTwoWastelands.esm` | 588 | 0 | 1,270 | 0 | 39 | 59 | 8 |
| `YUPTTW.esm` | 9 | 0 | 53 | 0 | 1 | 0 | - |
| `Fallout3.esm` (TTW-patched) | 26 | 0 | 232 | 0 | 0 | 6 | 8 |
| `FalloutNV.esm` (TTW-patched) | 682 | 0 | 1,314 | 0 | 46 | 57 | 9 |

The four add-on packs (`ClassicPack.esm`, `CaravanPack.esm`, `MercenaryPack.esm`, `TribalPack.esm`) have no `FACT` records. The TTW rows are not added to the total, as with the scripts.

What the 2,528 records show about the layout, which the loader was written from public format references and not from these files:

- **Every one of the 5,263 `XNAM` sub-records is 12 bytes**, in `Fallout3.esm` too. The 8-byte layout that the references give for Fallout 3 does not occur in these plugins (it is Oblivion's), so the loader accepts both and the third value is part of every relation. Only the sizes were counted, so what Fallout 3 puts in that third value is not known; New Vegas names it the group combat reaction.
- `DATA` is 1 byte in 103 records and 4 bytes in 2,425, never 2 or 3. `CNAM` is 4 bytes and appears in 69 records (35 in `Fallout3.esm`, 34 in `FalloutNV.esm`). `WMI1` is 4 bytes and appears in 134 records, all in New Vegas-based plugins. Every `RNAM` is 4 bytes.
- 242 records have ranks and the most in one record is 9. No rank title has zero bytes, none comes before its `RNAM`, and no `FACT` sub-record type outside the loader's list occurs.
- **No `FACT` record is compressed**, and across all record types no compressed record (96,951 in the 11 plugins that have any) ends with a sub-record of 9 bytes or less. The reader took a compressed record to end 4 bytes before its data does, so it would not have seen such a last sub-record. The fix therefore changes no record in these 20 plugins, and matters only for plugins that are not in this census.
- The Tale of Two Wastelands copies of `Fallout3.esm` and `FalloutNV.esm` have no `CNAM` at all, where the vanilla files have 35 and 34, although the `DATA` sizes in the TTW `FalloutNV.esm` equal the vanilla ones. This was not looked into.

## Loaders of M1 slices 3 to 5

Run on 2026-10-06 on the same Mac, from the branch of PRs #11 and #12, on the same 20 plugins, each from its own folder with a bare file name. All runs exited 0 with nothing on standard error, no run printed an `ERROR` line or "Reading stopped early", and no census printed a Failures section. The tables above this section were made before these loaders and still show the earlier counts.

Three runs: `e84c2fc0` (PR #11, the first 22 types and the `NPC_` fix), `003492d1` (the head of PR #11, which decides by game whether a file gets the new loaders) and `8fb3e4b4` (the head of PR #12, which adds the last 19 types and the `TERM` and `PACK` scripts). The first two outputs are identical in all 20 plugins, byte for byte, so the check that Fallout files are accepted rejected none. None of the 20 plugins is from another game, so this run cannot show that other games are turned away; the unit tests do that.

Result of the last run: in every plugin Parsed equals Records, and No parser and Failed are 0. "No parser before" is the count of the run on `003492d1`, which is the same as on `e84c2fc0`.

| Plugin | Records | No parser before | No parser | Failed |
|---|---:|---:|---:|---:|
| `Fallout3.esm` | 718,951 | 1,022 | 0 | 0 |
| `Anchorage.esm` | 40,569 | 34 | 0 | 0 |
| `BrokenSteel.esm` | 41,304 | 68 | 0 | 0 |
| `PointLookout.esm` | 61,982 | 75 | 0 | 0 |
| `ThePitt.esm` | 32,839 | 48 | 0 | 0 |
| `Zeta.esm` | 36,523 | 93 | 0 | 0 |
| `FalloutNV.esm` | 465,016 | 2,630 | 0 | 0 |
| `DeadMoney.esm` | 40,084 | 326 | 0 | 0 |
| `HonestHearts.esm` | 36,201 | 174 | 0 | 0 |
| `OldWorldBlues.esm` | 52,683 | 377 | 0 | 0 |
| `LonesomeRoad.esm` | 34,988 | 248 | 0 | 0 |
| `GunRunnersArsenal.esm` | 778 | 83 | 0 | 0 |
| `ClassicPack.esm` | 19 | 3 | 0 | 0 |
| `CaravanPack.esm` | 7 | 1 | 0 | 0 |
| `MercenaryPack.esm` | 7 | 1 | 0 | 0 |
| `TribalPack.esm` | 5 | 1 | 0 | 0 |
| `TaleOfTwoWastelands.esm` | 83,154 | 1,120 | 0 | 0 |
| `YUPTTW.esm` | 27,118 | 275 | 0 | 0 |
| `Fallout3.esm` (TTW-patched) | 704,183 | 47 | 0 | 0 |
| `FalloutNV.esm` (TTW-patched) | 465,054 | 2,636 | 0 | 0 |

The 19 types of PR #12 have these record counts in the two base plugins (all parse, none fails). Fallout 3: `ENCH` 89, `MESG` 518, `MGEF` 163, `PERK` 87, `RADS` 5, `SPEL` 160. New Vegas: `AMEF` 54, `CCRD` 270, `CDCK` 13, `CHAL` 105, `CHIP` 5, `CMNY` 6, `CSNO` 5, `DEHY` 5, `ENCH` 145, `HUNG` 5, `MESG` 1,144, `MGEF` 289, `PERK` 176, `RADS` 5, `RCCT` 10, `RCPE` 105, `REPU` 13, `SLPD` 5, `SPEL` 270. For some types of PR #11: `WTHR` 27 in Fallout 3 and 63 in New Vegas, `IMAD` 112 and 215, `LSCR` 150 and 208, `PROJ` 52 and 95.

### Scripts of `TERM` menu items and `PACK` events

The census now counts these scripts in its "Scripts held in line by records" table, with the same checks as for the other holders. The `INFO`, `QUST` and `SCPT` rows are identical to the earlier runs.

| Plugin | `TERM` scripts | `TERM` bytecode (bytes) | `TERM` bad reference counts | `PACK` scripts | `PACK` bytecode (bytes) |
|---|---:|---:|---:|---:|---:|
| `Fallout3.esm` | 371 | 14,579 | 0 | 510 | 13,751 |
| `Anchorage.esm` | 31 | 1,714 | 1 | 73 | 1,498 |
| `BrokenSteel.esm` | 61 | 3,222 | 0 | 83 | 1,909 |
| `PointLookout.esm` | 20 | 711 | 0 | 24 | 480 |
| `ThePitt.esm` | 27 | 2,154 | 0 | 96 | 1,484 |
| `Zeta.esm` | 1 | 21 | 0 | 77 | 1,455 |
| `FalloutNV.esm` | 208 | 11,293 | 0 | 480 | 12,526 |
| `DeadMoney.esm` | 113 | 8,688 | 1 | 34 | 2,363 |
| `HonestHearts.esm` | 3 | 61 | 0 | 52 | 1,270 |
| `OldWorldBlues.esm` | 57 | 20,499 | 1 | 9 | 130 |
| `LonesomeRoad.esm` | 26 | 1,339 | 0 | 21 | 669 |
| **All 11** | **918** | **64,281** | **3** | **1,459** | **37,535** |
| `TaleOfTwoWastelands.esm` | 148 | 8,138 | 0 | 325 | 9,951 |
| `YUPTTW.esm` | 55 | 6,703 | 0 | 143 | 6,426 |
| `Fallout3.esm` (TTW-patched) | 303 | 12,909 | 0 | 503 | 13,624 |
| `FalloutNV.esm` (TTW-patched) | 208 | 11,293 | 0 | 480 | 12,526 |

The add-on packs and `GunRunnersArsenal.esm` have no `TERM` or `PACK` script. The TTW rows are not added to the total, as before. In every `TERM` and `PACK` script the compiled size in the header equals the bytecode read, the variable-count check flags no script, and the reference count equals the references read except in three `TERM` scripts, one in each of `Anchorage.esm`, `DeadMoney.esm` and `OldWorldBlues.esm` (the TTW rows show none). That is a property of those scripts as shipped, since an independent recount of the scripts of `INFO`, `PACK`, `QUST`, `SCPT` and `TERM` straight from the plugin files, with a separate parser, equals this table in every cell of all 20 plugins.

## Notes

- **One reader warning in `FalloutNV.esm`.** One compressed record, at file offset `0xb0cff20` (4,084 bytes compressed, 4,385 expected), failed zlib's data check. The reader's by-block retry (`components/esm4/reader.cpp:131`) read it, and the census counted the record as parsed. Reading the code, the retry stops once the expected 4,385 bytes are out and never reaches the checksum at the end of the stream, which fits a stream that is intact except for its checksum. That is a reading of the code. The checksum failure is in the file as GOG ships it: the file is byte-identical to the one in the GOG installer, and running the census on the unpacked game folder gives the same result, so the extraction did not cause it. A Steam copy was not checked. In the TTW-patched `FalloutNV.esm` (330,921,809 bytes against 245,650,747 for the vanilla one) the warning does not appear, and none of the four TTW runs printed a zlib line.
- **Not covered by the census:** `.bsa` archives (TTW's rebuilt ones included), the TTW-patched add-on plugins, the correctness of the parsed fields, the correctness of the community installer's output, and player mods. Only record counts and loader results were checked.
- To reproduce: run `esmtool census <plugin>` from the plugin's folder. The header it prints (file name, format version, masters) is fine to share. See `build-baseline.md` for what the columns mean.
