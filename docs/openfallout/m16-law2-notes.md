# M16 law2: the Pip-Boy general page, crime exactness, the beggar and first-hour checks

Second wave on the law package (first wave: `m16-law-notes.md`). Everything below was read from the two executables
(Fallout 3 1.7, Fallout: New Vegas 1.4.0.525); addresses are New Vegas unless a line says Fallout 3. Counts, form ids and
addresses only: no game text is copied here.

## Where the code is

| What | File |
| --- | --- |
| Karma page, title setting, listed reputations, misc-stat setting names, page counts (43 / 26) | `components/falloutscript/pipboy.hpp/.cpp` |
| `sMiscStat%03d` defaults read from the executable's setting table | `components/falloutscript/exesettings.cpp` (`scanCounted`) |
| `core.pipboy.general()` (karma, counts, reputations) | `apps/openfallout/mwlua/pipboybindings.cpp`, `mwscript/falloutpipboy.cpp` (`pipboyGeneral`) |
| General page and Reputation page of the Stats tab, the toggle button | `files/data/scripts/omw/falloutpipboystats.lua` |
| `PipboyOpen` / `PipboyClose` events (rounds open the page like a key would) | `files/data/scripts/omw/falloutpipboy.lua` |
| Blow judgement (`Law::judgeBlow`), crime clock, `forgetRecent`, `crimeListed`, Fallout 3 kill karma | `components/falloutscript/crime.hpp/.cpp` |
| Engine glue: blow, detection, entry point 14 | `apps/openfallout/mwscript/falloutlaw.cpp`, `falloutscripts.cpp` |
| Perk entry point 14 constant | `components/falloutscript/perks.hpp` (`EntryPoint::disposition`) |
| Tests | `testcrime.cpp`, `testpipboy.cpp`, `testexesettings.cpp` in `apps/components_tests/falloutscript/` |
| Rounds | `scripts/openfallout/round/scenarios/m16-law2-*.lua`, `rounds/m16-law2-*.json` |

## Job 1: the General page of the Pip-Boy

The page is the game's own menu file (`main/stats_menu.xml`); the Lua only fills its tiles.

New Vegas, General tab (`stats_genrep_container`, id 50, user0 = page: 0 General, 1 Reputation, XML default 1):

- General page: the list `stats_general_container` (id 8) and a karma rectangle with `stats_karma_alignment` (id 35, the
  alignment word), `stats_karma_image` (id 36, the picture) and `stats_karma_title` (id 37, the rank title). The first
  wave had put the title into the alignment tile; that was wrong and is fixed.
- Reputation page: the list `stats_reputation_container` (id 46); on a hover or selection `stats_reputation_image` (id 47,
  the REPU record's ICON), `stats_reputation_title` (id 48, the fame/infamy title) and `stats_reputation_rep_title` (id 51,
  the reputation's name).
- Toggle `stats_genrep_button` (id 49, handler 0x7E02B0): shows the General word when the Reputation page is up and the
  Reputation word when the General page is up. A static flag (0x11A0AE0, starts at 1) is not reproduced.

Fallout 3: the same ids 35, 36 and 37 and a plain `stats_general_container`; there is no reputation and no toggle, and
the page has no rank title line.

Karma tile (updater 0x7DD090 NV, 0x6875C0 FO3): the class of the player's karma (0 good, 1 neutral, 2 evil, 3 very good,
4 very evil, 0x47E040) picks the picture setting (`karma_good`, neutral, `karma_bad`, `karma_saintly`, `karma_evil`)
and the word setting (`sAlignGood`, ..., `sAlignVeryEvil`). Same order in both games. The title (id 37) is set at 0x7DF5D0
from 0x47E0E0.

Counts list (0x7DA670): index 0 to 42 in New Vegas (43 entries, loop bound at 0x7DA885), 0 to 25 in Fallout 3 (26 entries,
`cmp esi, 0x1A` at 0x48C71C). All are listed in index order (no sort), name from the setting `sMiscStat%03d`, value in
the row's `user1`. The names are in neither ESM; they are exe defaults built in a loop (NV 0x4D5B60 over the table at
0x1189280; FO3 0x48C610 over 0xF4CA80). The old `ExeSettings` did not find them; `scanCounted` now reads them from the
executable (find the format string, the function that pushes it and its single call, the table load and the count).

Reputation list (0x7DD8C0): the REPU records whose fame > 0 or infamy > 0 (thirteen REPU records in New Vegas, form ids
0x1558E6, 0x129A7A, 0x129A79, 0x124AD1, 0x11E662, 0x11989B, 0x118F61, 0x116F16, 0x104C22, 0x0FFAE8, 0x0F43DE, 0x0F43DD,
0x0F2406). The row text is the reputation's name; rows are sorted ascending by name (comparator 0x7E0510 through 0x404DC0 to
the C library's `_stricmp` at 0xEC68E4: case insensitive; the round's list came out in that order). The hover (0x7DC1B0, then ShowReputation 0x7DCB40) fills the right-hand tiles. The fame/infamy title is
`sRepTitlePos{None,One,Two,Three}Neg{...}`: entry 4 * infamyLevel + fameLevel (0x616710, levels from 0x616950).

Infamy amounts (confirmed, first wave had assumed them): 0x616C20 calls `AddReputationExact` (0x615730) on the negative
side with `fReputationMajorCrimeNeg` (30) for a major crime or `fReputationMinorCrimeNeg` (2) for a minor one; no plugin
overrides either. The faction counters 0x5FDA00 (minor) and 0x5FDA50 (major) apply it when the caller's flag is set.

## Job 2: crime and detection exactness

Blow (hit routine 0x8987F0):

- The creature test is the virtual at +0x21C (Character false, Creature true, Player false; +0x218 is true for Character and
  Player). A creature victim that does not fight makes no attack crime. If it has an owner (0x5D43C0, then 0x418660) the
  steal creator 0x8BFA40 is called, which only warns. A character victim gets the attack crime at 0x898E26 via 0x8C0460.
  `Law::judgeBlow(fights, creature, owned)` is that decision; `LawGlue::blow` uses it. This was the cause of the "Crime n:
  1 reactions" lines while shooting a radroach in the Vault 101 birthday scene: the first wave treated every hit as an
  attack on a character.
- The minor flag is the weapon's `flags2 & 0x10` (0x898FD0), passed as the second argument to 0x8C0460. For a minor
  weapon only the victim itself is a concerned witness; others that do not care get the no-crime line (topic 0x61A2D0
  with code 0xF) or, as friend of the victim, an Attack. `LawGlue::blow` now sets `blow.mMinor` from the WEAP record.
  Only 3 Fallout 3 weapons (0x7C10C, 0x3954F, 0x4339) and 1 New Vegas weapon (0x3954F) carry it; the CG02 BB gun (0xC0327)
  has flags2 0x88 and does not.
- The attack-crime block (0x898CB6 to 0x898E5B) is entered only when 0x8B0670(victim, attacker) is true (friend or ally).
  The tolerance of the blow ([ebp-0x55], computed at 0x898D56) is passed only to the victim's reaction call at 0x898E87 and
  is not read by the crime creation. The law is therefore asked even when the blow is tolerated. The engine now does that
  (see Decisions); a character with `SetIgnoreFriendlyHits` is still skipped.

Kill routine 0x8B01C0: a creature kill is not a murder: with an owner it makes an attack crime on the owner (0x8C0460),
without one an attack crime on the creature itself; a character victim without an owner goes to murder 0x8C09E0.

Expiry (0x971FD0): called only from 0x967DA0 (the combat yield) with `fCombatYieldTime` (10, exe default 0x1017B78, no ESM
override). A crime's stamp (+0x34) is the engine clock (global 0x11F1BF0, +1 per frame at 0x9915E0) at construction
(0x9EB5E9); age = clock - stamp (0x435E00); a crime is kept when `limit < age` (0x972099), so those with age at most the
limit are removed. The player-enemy flag is cleared (0x47EB90) for witnesses that no remaining crime holds (0x9722E0) and
the victim's list is cleared (0x972134). There is no long-term expiry. The code has `Crime::mTime`, `Law::advance` (called
every frame from `LawGlue::update`) and `Law::forgetRecent(seconds)`, which does exactly that; nothing calls it yet
(see Not done).

`GetActorCrimePlayerEnemy` (0x5A4930): 1 when the actor's own crime list is not empty and the actor passes vt+0x100; the
faction flag has no part. `Law::crimeListed` is the list test (an actor is on the list as a witness or victim: assumed).

`IsActorDetected` (0x973460): loaded actors that are characters (+0x218), detection level 3 (0x8A81E0), not one whose
combat target is the subject. `LawGlue::detected` now matches.

Perk entry point 14 (disposition, 0x87FD90): `0x5E58F0` with entry 0xE; owner is the asked actor, or the player for a
teammate (0x5E5911 to 0x5E5934); the other is the target; the result is cut and clamped to 0 to 100. `adjustDisposition`
applies it (was the identity).

Fallout 3 kill karma (0x71B1C5 to 0x71B255, inside 0x71AC50): the killer must be the player (0x71B15F). Class 4 (very evil)
pays `fKarmaModKillingEvilActor` always, tracked or not; class 2 (evil) pays nothing; classes 0, 1 and 3 pay only when the
kill counts as a crime and the victim has a tracked faction (0x70B5D0): `fKarmaModMurderingNonEvilNPC`, or `...Creature`
for a creature (+0x21C). The first wave used the New Vegas shape; `killKarmaFallout3` is the Fallout 3 one.

`CreateDetectionEvent` (0x1068, record 0x1191950, exec 0x5DAC80; actor, int (default 0), optional int (default 3)): calls
HighProcess vt+0xFC (0x903E40), which stores a 0x1C-byte event at process+0x3DC (the first int, the calling reference's
position, the clock, the calling reference). The consumers (callers of 0x903F50 and 0x903F90) are virtual and were not
followed. Not implemented (4 script uses in both games).

## Job 3: the beggar and the first hour

The beggar case (first wave, Fallout 3, round step A2): a blow at a non-hostile character was let pass by the friendly-fire
rule (`toleratesBlow`), and the law was never asked, so no crime was made. The executable asks the crime creator anyway (see
"Blow" above), so `FalloutScripts::applyDamage` now asks the law for every blow of the player at a character, except that
a character that ignores friendly hits (`SetIgnoreFriendlyHits`) and tolerates the blow still counts none. The tolerance
rule keeps its own job: whether the victim turns on the player at once.

First hour, by reading the data (a plugin reader over both ESMs, counts of faction records only):

- Fallout 3 Vault 101 scenes CG00 to CG03: the characters of the scenes (the father, Jonas, the Overseer, Amata, the
  Tunnel Snakes of the birthday scene, the teachers and classmates) hold only PlayerFaction, Vault101Faction,
  Vault101SecurityFaction and the scene's own Tunnel Snakes faction. None of those has the TrackCrime flag (faction
  flags2 bit 0x01), so no blow or death there can make a faction count a crime or set a faction's player-enemy flag, whatever
  the law is asked. The tracked ones in the vault are the quest factions of the later rebellion quest (MS16, 3 factions), the
  Holden pair of CG04 and the Butch family of CG04 (Butch, his mother); the autoplay does not strike those.
- 112 of 326 Fallout 3 factions and 188 of 682 New Vegas factions have TrackCrime.
- New Vegas Goodsprings: its residents (doctor, Sunny, trader, the militia) hold GoodspringsFaction and the militia faction,
  both tracked, and the dog Cheyenne holds the militia faction as a creature. A blow at the dog (or any creature) used to
  make an attack crime on a tracked faction; it now makes none (the dog has no owner) and the town does not turn. The intro
  geckos hold no tracked faction.
- The radroach of the Vault 101 birthday scene (the CG02 BB gun, which has no minor-crime flag) is a creature: the blow no
  longer reaches the law as an attack on a character, so the father, Jonas and Paul keep their packages; the four "Crime n: 1
  reactions" lines of the stalled run are the log line of an attack crime, which only a blow at a character made, so they came from
  the creature blow being judged as one (the single reaction is the creature itself, a victim that watches its own beating).

## What was run

Unit tests: `FalloutScript*` and `FalloutExe*` of `components-tests` (1704 tests, all pass) and `openfallout-tests` (837, all
pass). New tests: creature blow, minor weapon, the yield (`forgetRecent`), Fallout 3 kill karma, karma page, title setting,
listed reputations, misc-stat setting names, and `scanCounted` on a synthetic executable with both prologues.

Hidden rounds (`round.sh law2`, engine hidden, nobody touches a key or the pointer; every number is from the round's log,
pictures are in the round's output folder):

| Round | Verdict | What it showed |
| --- | --- | --- |
| `m16-law2-general-nv` | PASS, 84 s, 6 pictures | counts = 43, names found in the executable (`scanCounted` works on the real New Vegas executable); six reputations listed after setting fame and infamy with `SetReputation`, in name order; picked rows show the picture, the fame/infamy title and the name; karma 300 shows the good word, picture and title, karma -800 the very evil ones |
| `m16-law2-general-fo3` (first run) | PASS, 58 s | counts = 26 with values but **no names**: the first version of `scanCounted` did not find the Fallout 3 setting builder |
| `m16-law2-general-fo3` (after the fix) | PASS, 64 s, 4 pictures | counts = 26 with names; neutral, very good, evil and very evil karma each with their own word, picture and title; no reputation page |
| `m16-law2-blows-nv` | PASS, 75 s | a blow at an owned creature near Goodsprings: `Crime 1: 1 reactions, 0 factions turned, karma -5` (the steal path: the owner's resident faction minor count 0 to 1, no enemy flag, player major crimes stay 0, the creature fights); the control blow at a resident: `Crime 2: 6 reactions, 1 factions turned`, major 1, the resident's faction flag set |
| `m16-law2-blows-fo3` | PASS, 84 s | B: a resident struck alone: major 0 to 1, two factions count a major crime, victim fights. N: the same one struck with a minor-flag weapon (damage 0.4): `Crime 2: 1 reactions, 0 factions turned`, player major stays 1, the factions' minor counts 0 to 2. F (the beggar case): a character with one faction struck with a neighbour looking: `Crime 3: 3 reactions, 1 factions turned`, major 1 to 2, the faction's major count 0 to 1 and its flag set, the victim not yet in combat 4 s later. C (a blow at the placed creature of the outskirts) was not reached: the creature's cell did not load for the script ("not available"), so a blow at an unowned creature is shown by the unit test only |

## Decisions for the lead

- A blow at an owned creature is now a theft of the owner's property with a warning (0x8BFA40 applies `fKarmaModStealing`, so the
  karma drops by 5 even for a creature kept by a faction): shown by the New Vegas round (karma -5, the owner's faction minor 0 to
  1, no enemy flag).
- The law is now asked for the player's blows even when the engine's friendly-fire rule tolerates them (the executable does
  the same at 0x898E26). A friend or ally the player hits can now be a crime with witnesses. Revert by restoring the
  `!tolerated` condition in `FalloutScripts::applyDamage` (`falloutscripts.cpp`, search "judged by the law").

## Not done

- CreateDetectionEvent consumers (above).
- A blow at an unowned creature on real data (Fallout 3 round step C could not load the creature); the Fallout 3 and New Vegas
  intros were not played (the lead runs the autoplay): the first-hour check is the faction data above plus the creature rounds.
- `forgetRecent` is not called: the combat yield (0x967DA0) has no counterpart in this engine, and the crime clock is not
  saved, so crimes made before a save do not expire after a load.
- Creature kills still go through the murder path in `Law::murder`; the executable uses an attack crime on the owner or the
  creature (0x8B02B4 to 0x8B0363). A kill of an unowned creature should make nothing the law minds.
- The second crime path of the hit routine (0x898F1C: the victim fights already, and the byte at +0x104 is clear) was read: +0x104
  is set to 1 by the start-of-combat routine (0x8A01B8, when a combat controller is made) and to 0 otherwise, so the path
  is a victim that counts as fighting without a controller. Everything here that fights has one, so nothing was added.
- `crimeListed` assumes witnesses and victims are on the actor's crime list.
- Fallout 3 has no reputation or rank lines; the static genrep flag (0x11A0AE0) quirk is ignored.
- `scanCounted` finds the count by a heuristic (largest `cmp imm8` in a window after the table load).
- Vault 101 factions' TrackCrime flags were not read.
