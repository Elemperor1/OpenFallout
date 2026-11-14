# M16 law: crime, ownership, standing and karma

The rules by which the world judges the player, read from the two executables (Fallout 3 1.7, Fallout: New Vegas
1.4.0.525) and built as pure rules in `components/falloutscript/` with the engine glue in
`apps/openfallout/mwscript/falloutlaw.*`. Addresses below are New Vegas unless a line says Fallout 3. Where a number
comes from the executable and the data files set it differently, both are named.

## Where the code is

| What | File |
| --- | --- |
| Crime, witnesses, factions' crime counts, trespass, karma of kills and thefts, 30 commands | `components/falloutscript/crime.hpp/.cpp` |
| Disposition formula, pair modifier, faction reactions, karma classes, 24 commands | `components/falloutscript/standing.hpp/.cpp` |
| Reputation additions (exact and loss steps, percent) | `components/falloutscript/reputation.*` |
| Enemy-of-the-player flag seen by relation commands | `components/falloutscript/combatcommands.*` (`CombatWorld::playerEnemyOf`) |
| Engine glue: events, witnesses, reactions, trespass, saving | `apps/openfallout/mwscript/falloutlaw.hpp/.cpp` |
| Class guard flag read from CLAS | `components/esm4/loadclas.*` (`mFo3Flags`), `ESM4::Class` store in `mwworld/esmstore.hpp` |
| Tests | `apps/components_tests/falloutscript/testcrime.cpp`, `teststanding.cpp`, `testreputation.cpp`, `testcombatcommands.cpp` |

Both rules are saved: the `law` section (version 1) and the `standing` section (version 1) of the Fallout save.

## Crime

The executable has five crime kinds (constructor 0x9EB500): 0 steal, 1 pickpocket, 2 trespass, 3 attack, 4 murder.
Creators: steal 0x8BFA40, pickpocket 0x8C00E0, attack 0x8C0460, murder 0x8C09E0. There is no creator for the trespass
kind: trespass is a package ladder (0x918120) and the alarm sweep (0x8C0EC0).

Who cares (0x8B8360): a witness belongs to a faction with the TrackCrime flag (faction flags2 bit 0x01) and the
victim holds that faction too. The witness sets the faction's player-enemy flag (0x47EB90) and the faction collects the
crime into its minor or major counter (0x5FDA00 minor, 0x5FDA50 major). The counters have a hook into the reputation of
the faction (0x616C20) that adds infamy (New Vegas only); the amounts are `fReputationMinorCrimeNeg` 2 and
`fReputationMajorCrimeNeg` 30 (assumed from the setting names, the hook was not followed to its constants).

Attack (hit routine 0x898B40-0x898E5B): when the attacker is the player, the victim is a character that does not
fight yet (0x8A6650) and does not tolerate the blow, the attack crime is made. A victim that already fights only gets
the pair disposition modifier `fCrimeDispAttack` (-10). Murder (kill routine 0x8B01C0): a murder crime is made for a
player kill when the victim has the "crime on death" flag (+0xC4). The flag is dropped when the killer is the player
and the victim is a friend or ally with the "ignore friendly hits" reference flag (0x100000), or the kill is marked
"no crime". The murderer flag of the player (0x953F80, set at 0x89E2B0) needs: killer is the player, victim is a
character, victim is not evil (0x5678A0), flag +0xC4 set.

Steal (0x8BFA40) and pickpocket (0x8C00E0) keep a day warning: the first one of a day warns only; a second one in the
same day is an attack. The executable never raises the steal counter that would turn the warning into a crime, so
stealing is always "warn only" in both games; the code does the same.

Trespass: three warnings each `fAITrespassWarningTimer` (10 s) apart (none when the cell has the off-limits flag
0x20000), then the alarm. The lines are the default topics GuardTrespass (0xF2) and, for the no-crime case,
TrespassNoCrime (0xEB). The other default topics (the same form ids in both games): Steal 0xDF, Pickpocket 0xE3,
Assault 0xE4, Murder 0xE5, AssaultNoCrime 0xE7, MurderNoCrime 0xE8, PickpocketNoCrime 0xE9, StealNoCrime 0xEA.

SendAssaultAlarm (0x5DA2A0) makes a major attack crime and sends the victim to fight. SendTrespassAlarm (0x5D47E0)
runs the alarm sweep (0x8C0EC0): responders attack, the first responder's tracking factions get minor+1 with infamy,
the player's minor count goes up.

### Karma of kills and thefts

Kill karma (0x89E30D-0x89E3FC) is given only when the killer is the player, the kill is not marked "no karma" and the
victim has a faction with the TrackCrime flag (0x8B8490). The value comes from the karma class of the victim:

| Victim | Setting | New Vegas | Fallout 3 |
| --- | --- | --- | --- |
| not evil, no class | `fKarmaModMurderingNonEvilNPC` | 0 | -100 |
| creature | `fKarmaModMurderingNonEvilCreature` | 0 | -25 |
| class good | `fKarmaModMurderingGoodNPC` | -50 | no setting |
| class very good | `fKarmaModMurderingVeryGoodNPC` | -100 | no setting |
| class evil | `fKarmaModKillingEvilActor` | 1 in the exe, 100 in the data | 1 / 100 |
| class very evil | `fKarmaModKillingVeryEvilActor` | 2 | no setting |

Stealing costs `fKarmaModStealing` (-5) when the owner is not evil, with or without a witness.

## Standing

Disposition (0x642A60), arguments: base, charisma of the other (AV 8), responsibility of self (AV 3), karma of the
other (AV 23), race modifier, faction reaction, pair-found flag (1 or -1), weapon drawn, diseases (0), pair modifier,
creature flag. The total is

    base + trunc((cha - CharOffset) * CharMult + CharBase)
         + [not creature] trunc(KarmaMult * karma * (RespBase + RespMult * resp))
         + trunc(RaceMult) * race
         + trunc(FactionMult * reaction * (RankBase + FactionMult * pairFlag))
         + [not creature] trunc(WeaponBase) * weaponDrawn
         + pairModifier

then the perk entry point 14 (identity in the code, see Not done), clamped to 0..100; the same character gives 100.
Each trunc is the executable's float-to-integer conversion (0xEC62C0, truncation toward zero).

Defaults of both executables: `fDispTargetCharBase` 0, `fDispTargetCharMult` 5 (10 in the data), `fDispTargetCharOffset` 5,
`fDispTargetKarmaMult` 0.002, `fDispActorRespBase` -50, `fDispActorRespMult` 1, `fDispTargetRaceMult` 1,
`fDispTargetFactionMult` 1, `fDispTargetFactionRankBase` 1 (the Mult setting of the rank is read by nothing),
`fDispTargetWeaponBase` -10, `fDispTargetDiseaseBase` 0, `iDialogueDispositionFriendValue` 50, alignment limits
`fAlignEvilMaxKarma` -250, `fAlignVeryEvilMaxKarma` -750, `fAlignGoodMinKarma` 250, `fAlignVeryGoodMinKarma` 750.
Defaults are replaced by the game settings when the data files carry them.

Pair modifier (setter vt+0x460): only clamps (the total to 0..100) when an entry exists; a new entry stores
`trunc(delta)`. Faction pair reaction (0x8B7FE0): pairs that share a faction give the highest reaction; if no faction
is shared, the lowest reaction among the other pairs (start values -100000 / 100000). Karma class (0x47E040): 0 good,
1 neutral, 2 evil, 3 very good, 4 very evil, by the four limits above. IsActorEvil is true when the character has
factions and every one has the Evil flag (0x47D740).

GetFactionRankDifference is the rank of the caller minus the rank of the parameter in the same faction (0 when either
is not a member); ModFactionRank adds to the rank, not below 0, members only. IsGuard reads the second flag of the
class record's DATA (CLAS DATA bytes 4-8, flag 2). GetPersuasionNumber and GetTotalPersuasionNumber answer 0 in the New
Vegas executable (0x5A2A50).

## What happens in the engine

Hooks (all in the Fallout engine only; the Morrowind path is untouched):

- A blow by the player that nobody lets pass (`toleratesBlow`) goes to `LawGlue::blow`: witnesses are the characters in
  range that can see the player; the attack crime, infamy and the victim fight follow. A kill goes to `killed` (murder).
- `MechanicsManager::itemTaken` calls `OFScript::noteTaking`: an owned item (reference owner, cell owner, container
  owner) taken by the player is a theft; owners and witnesses react.
- Lockpicking an owned lock and a correct hack of an owned terminal call `tamperedWith` (theft with no goods; a hack
  costs the karma only).
- `LawGlue::update` runs the trespass ladder twice a second for the player in an owned non-public cell.
- Reactions run on the main thread: the character says the line through `sayTopic` or fights through the package
  system (`startFight`, `evaluatePackage`). Infamy goes to the reputation (`Reputation::addExact`) and karma to
  `ProgressState::rewardKarma`.
- `relationOfActors` and `relationOfFaction` give Enemy first when the faction has the player-enemy flag.

## What was run

Unit tests: `FalloutScript*` of `components-tests` (1332 tests, all pass) and `openfallout-tests` (836 tests, all pass).

Real-data hidden rounds (`scripts/openfallout/round/scenarios/m16-law-nv.lua`, `rounds/m16-law-nv.json`; the engine runs
hidden, nobody touches a key or the pointer; `core.combat.law(actor)` is the read-only Lua window on the law state that
the scenarios read, with the karma from `core.pipboy.stats()`). Every number below is from the log of the round.

New Vegas, the open ground of the first settlement and two interiors, New Vegas data only, 143 s, PASS (the scenario
finished, 0 errors):

| Step | What the round did | What the engine showed |
| --- | --- | --- |
| A1 | one shot at a non-hostile resident, no one else near | player major crimes 0 to 1, the victim's factions major 1, the faction player-enemy flag set, the victim fights, karma unchanged |
| A2 | one shot at another resident with a militia neighbour beside it | major 2, the flag stays, the neighbour is told the crime (it does not fight: a witness says the line) |
| M | the second resident shot until dead, the neighbour beside it | murderer flag true, karma 0 to -50 (the value of `fKarmaModMurderingGoodNPC`; the round did not read the victim's class), major 4 (two attack crimes and the murder), disposition of the neighbour 25 to 30 (the formula's karma term) |
| T1 | an owned item taken with the owner switched off | karma -50 to -55 (`fKarmaModStealing`), no crime, no warning |
| T2 | an owned item taken with the owner four metres away | karma -55 to -60, crime 5, the minor-crime counts of the owner's factions add up to 3 (the split among its factions was not read), no fight (steal only warns) |
| R1 | entered a room owned by a character who is in it | warnings 1, 2, 3 ten seconds apart (at 0 s, 6 s, 16 s), then at 26 s the alarm: player minor crimes 0 to 1 and the owner fights |
| R2 | an item of that room taken while the owner fights | karma -60 to -65, crime 6 |

Fallout 3, the open ground outside the main settlement of the Capital Wasteland and two interiors (`m16-law-fo3.lua`), 108 s,
PASS. Only two non-hostile placed characters were reachable on that ground (two other placed ones are not available to
a script there), so A2 used the character of A1 as the neighbour:

| Step | What the engine showed |
| --- | --- |
| A1 | one shot at a resident: player major crimes 0 to 1, the victim's two factions each count a major crime, the flag set, the victim fights, karma unchanged |
| A2 | one shot at a second character (one faction, not a crime-tracking one as far as the round could tell): no crime was made, the victim stayed at peace (the existing friendly-fire rule `toleratesBlow` let the blow pass; the law was never asked) |
| M | the second character killed: murderer true, karma 0 to -100 (`fKarmaModMurderingNonEvilNPC`, the Fallout 3 value), crimes 1 to 2, the neighbour fights |
| T1 | an item taken in a house whose owner is not in it: karma -100 to -105 (`fKarmaModStealing`), no crime |
| R1 | the owner of a house inside it: warnings 1, 2, 3 (at 0, 6 and 16 s), at 26 s the alarm: player minor crimes 0 to 1, the owner's factions' minor counts 0 to 2, the owner fights |
| R2 | an item taken in the house after the alarm: karma -105 to -110, crimes 2 to 3, the owner's factions' minor counts 2 to 4 |

What the round could not show: the lines that the characters say (the log has the number of reactions, `Crime N: R
reactions`, not text), the faces and animations of the reactions (nothing was drawn or photographed), pickpocketing, the
saving of this state (unit tests do that), a guard fighting the player after the alarm (the rooms have none), and what the
rest of a settlement does once the flag is set (the scenarios end).

Two things the first rounds found and the code fixed: Fallout items and container takes never reach the Morrowind
`itemTaken` (the loose-item path is `FalloutScripts::pickUp`, the container path `transferItems`), and the Morrowind
actor list holds no Fallout character (they live in `FalloutActors`), so witnesses and trespass watchers come from
`getFalloutActorsInRange`.

## Not done

- Review fixes: StopCombatAlarmOnActor also runs `Law::stopAlarm` (the crimes the character is the victim of or a witness to are forgotten, and the enemy flags nothing else holds are cleared, under the law's lock); `IsTrespassing` asks about the trespasser (the player while any watcher holds a trespass package) and not about the watcher that warns; the restrictions on waiting and travel read that trespass state, and the alarm sounds while a character that an alarm turned on the player still fights it (the alarm package of the executable is not run here); a character with no mind (nothing to do, no one to fight) is a witness too (`FalloutActors` keeps the loaded ones without a mind). Not done: the owner's karma for a hacked terminal is the default when no owner stands near (the karma of an NPC base record that is not loaded is not read), and `IsActorDetected` leaves the player out as an observer.
- Crime expiry (0x971FD0) was not read: crimes and player-enemy flags last until a command or `StopCombatAlarmOnActor`
  clears them. A faction's enemy flag therefore stays set for the rest of the game unless a script clears it.
- Pickpocketing has a rule (`Law::pickpocket`) and a command table entry but no event: the Fallout interface has no
  pickpocket model yet (the `itemTaken` hook that Morrowind's pickpocketing used is never reached by Fallout items).
- CreateDetectionEvent (0x5D...) was not read.
- Perk entry point 14 for the disposition: the perk system has no entry point list for it yet (`adjustDisposition` is the
  identity).
- IsActorDetected and GetActorCrimePlayerEnemy are approximations of 0x973460 and 0x5A4930.
- The Pip-Boy General tab has no reputation lines: the page is the game's own menu file, whose general-tab tiles were not
  read, so the lines were not wired. The data is there (`Reputation`, `core.combat.law`).
- The Fallout 3 death routine was not read separately: the kill karma uses the New Vegas routine's shape with the three
  Fallout 3 settings.
- The infamy amounts of the crime counters (`fReputationMinorCrimeNeg` 2, `fReputationMajorCrimeNeg` 30) are assumed from
  the setting names; the hook at 0x616C20 was not followed to its constants.
- The rounds did not read the lines the characters say or show the reactions (no picture or animation was checked), and
  the character of a guard (a faction or class guard) reacting to the alarm was not run.
- Glue tests: `falloutlaw.cpp` needs a whole engine host (`FalloutScripts` with a store, a world and mechanics), which the
  unit tests cannot stand up; the engine side is covered by the two rounds, the rules by `testcrime.cpp` and
  `teststanding.cpp` with fakes.
- A blow at a character that the existing friendly-fire rule (`toleratesBlow`) lets pass makes no crime: a Fallout 3
  character in A2 above; whether the executable would judge that blow is open.
