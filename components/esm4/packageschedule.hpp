#ifndef OPENFALLOUT_COMPONENTS_ESM4_PACKAGESCHEDULE_H
#define OPENFALLOUT_COMPONENTS_ESM4_PACKAGESCHEDULE_H

#include <vector>

#include <components/esm/formid.hpp>

#include "loadcrea.hpp"
#include "loadnpc.hpp"
#include "loadpack.hpp"

namespace ESM4
{
    // Which of the AI packages of a character of Fallout 3 and New Vegas it follows at a time of day. A character lists
    // its packages in order of priority (a character that takes them from a template has those of the template), and
    // a package is followed while its schedule says it is time for it, unless a package before it in the list is as
    // well. This is the part of that decision that needs nothing of the world; where the package sends the character
    // and how it gets there is for the game.

    // The kind of a package, the byte after the flags in PKDT
    enum PackageType : int
    {
        Package_Find = 0,
        Package_Follow = 1,
        Package_Escort = 2,
        Package_Eat = 3,
        Package_Sleep = 4,
        Package_Wander = 5,
        Package_Travel = 6,
        Package_Accompany = 7,
        Package_UseItemAt = 8,
        Package_Ambush = 9,
        Package_FleeNotCombat = 10,
        Package_CastMagic = 11,
        Package_Sandbox = 12,
        Package_Patrol = 13,
        Package_Guard = 14,
        Package_Dialogue = 15,
        Package_UseWeapon = 16,
    };

    // What the game does for a package. Most packages that the data has (Travel, Sandbox, Patrol, Sleep, Guard, Eat and
    // Wander are 4,000 of the 4,885 of New Vegas) place the character at or around a location; Follow and Accompany
    // (184) keep it near another character; the others need what is not there yet (a target to attack, a conversation).
    enum class PackageBehaviour
    {
        None, // the game has nothing to do for it, so the next package in the list is followed
        Stay, // the character goes to the location and stays there
        Roam, // the character moves about the location, standing still for a while at each place it goes to
        Follow, // the character keeps within the distance of the package of the target of the package
    };

    PackageBehaviour packageBehaviour(int packageType);

    // What the location of a package (PLDT) is, the type in its first four bytes
    enum PackageLocationType : int
    {
        Location_NearReference = 0, // a placed reference, by its form id
        Location_InCell = 1, // a cell, by its form id
        Location_NearCurrent = 2, // where the character is when the package starts
        Location_NearEditorLocation = 3, // where the character was placed
        Location_ObjectId = 4, // any reference of a base record, by its form id
        Location_ObjectType = 5, // any reference of a type of record
        Location_NearLinkedReference = 6, // the reference that the character is linked to
        Location_None = 0xff, // the package has no PLDT
    };

    // The time of day and the day of the week; day 0 is Sunday
    struct PackageClock
    {
        float mHour = 0.f; // 0 to 24
        int mDayOfWeek = 0;
    };

    // A schedule that sets a time but no duration is taken to last this many hours. The files of the games do not say
    // what it means, and an hour keeps the character from doing the same thing all day.
    constexpr float scheduleDurationWithoutLength = 1.f;

    // Whether the clock is in the time that the schedule allows. A time of 0xff is any, and so is a day of 0xff; the
    // days are 0 to 6, 7 on weekdays, 8 at weekends, 9 on Monday, Wednesday and Friday and 10 on Tuesday and
    // Thursday. A package that starts at 22 and lasts eight hours is followed from 22 to 6, and on the days it is set
    // for, at its start. The month and the date are not used by any file of the games, so they are not looked at.
    bool scheduleIncludes(const AIPackage::PSDT& schedule, const PackageClock& clock);

    // Why a package is not followed
    enum class PackageSkip
    {
        None, // it is followed
        Schedule, // it is not the time for it
        Conditions, // it has conditions, and the game cannot tell yet whether they hold
        Behaviour, // the game does nothing for this kind of package
    };

    // The conditions of a package (CTDA) are about the state of the world and of the quests, which the game does not
    // keep yet: a package that has any is not followed (the data has them on 2,242 of 4,885 packages of New Vegas, most
    // of them to switch a quest's behaviour on, which a game that has not started the quest has off).
    PackageSkip packageSkip(const AIPackage& package, const PackageClock& clock);
    PackageSkip packageSkip(
        int packageType, const AIPackage::PSDT& schedule, bool hasConditions, const PackageClock& clock);

    // The first package of the list that is followed, null when there is none. Null entries are skipped (a package
    // that no file has a record for).
    const AIPackage* choosePackage(const std::vector<const AIPackage*>& packages, const PackageClock& clock);

    // The list of packages that a character has: those of the first record of its template chain that does not take
    // them from its template. Empty when every record takes them or the record that has them lists none.
    const std::vector<ESM::FormId>& characterPackages(const std::vector<const Npc*>& chain);
    const std::vector<ESM::FormId>& creaturePackages(const std::vector<const Creature*>& chain);
}

#endif
