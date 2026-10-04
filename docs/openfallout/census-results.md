# Plugin census results

`esmtool census` run on 2026-10-04 on Jacob's MacBook, through a Remote Control session, on 16 plugins: Fallout 3, its five add-ons, Fallout: New Vegas and its nine add-ons and packs. The census program is the one merged in PR #2 (master `be6ca447`). This page holds counts only. Nothing on it was read out of a record.

## What the census found

- **No record failed to load.** All 1,561,956 records in the 16 plugins were counted, and the Failed column is 0 everywhere. No `Unknown subrecord` line was printed for any plugin. Every loader that exists accepts every record of its type in the real files. "Parsed" only means the loader did not throw: it does not mean the fields are right, or that the engine keeps them (the roadmap's section 2 lists what is discarded after parsing).
- **1,550,732 records (99.3%) have a loader and 11,224 have none.** The gap is in the number of record types, not in the volume. The big types (`REFR`, `CELL`, `LAND`, `INFO`, `DIAL`, `NAVM`, `PACK`, `NPC_`, `SCPT`) are all parsed.
- **42 record types have no loader.** Fallout 3 uses 28 of them (2,973 of 718,951 records in `Fallout3.esm`). New Vegas uses all 42 (5,496 of 465,016 records in `FalloutNV.esm`). New Vegas adds 14 unparsed types of its own (`AMEF`, `CCRD`, `CDCK`, `CHAL`, `CHIP`, `CMNY`, `CSNO`, `DEHY`, `HUNG`, `LSCT`, `RCCT`, `RCPE`, `REPU`, `SLPD`).
- Whether a type has a loader is the same in both games for every type they share.

## Where the files came from

- **Fallout 3:** the installed `Data` folder.
- **New Vegas:** not installed on that Mac, only the GOG offline installer. The `.esm` plugins were unpacked from the installer with `innoextract` into a scratch folder outside the repository. This is the installer route the roadmap proposed, and it works.
- **Tale of Two Wastelands:** no TTW files exist on that machine, so there is no TTW census yet.
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

## Record types with no loader

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

## Notes

- **One reader warning in `FalloutNV.esm`.** One compressed record, at file offset `0xb0cff20` (4,084 bytes compressed, 4,385 expected), failed zlib's data check. The reader's by-block retry (`components/esm4/reader.cpp:131`) read it, and the census counted the record as parsed. Reading the code, the retry stops once the expected 4,385 bytes are out and never reaches the checksum at the end of the stream, which fits a stream that is intact except for its checksum. That is a reading of the code, not something checked on the data, so it is unknown whether the checksum is bad in Bethesda's file or came from the extraction. It is worth checking the same record in an installed (not unpacked) copy before M1.
- **Not covered by the census:** `.bsa` archives, Tale of Two Wastelands, the correctness of the parsed fields, and player mods. Only record counts and loader results were checked.
- To reproduce: run `esmtool census <plugin>` from the plugin's folder. The header it prints (file name, format version, masters) is fine to share. See `build-baseline.md` for what the columns mean.
