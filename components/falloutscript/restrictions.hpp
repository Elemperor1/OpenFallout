#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_RESTRICTIONS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_RESTRICTIONS_H

#include <string>
#include <vector>

namespace FalloutScript::Restrictions
{
    /// The one question the games ask before the player rests or travels: may it, here and now? The executables ask it
    /// in three places that are three views of this rule: the key that waits (and the command ShowSleepWaitMenu with
    /// the check on), a bed that is used, and the click on a place of the world map. Each asks the same facts in a
    /// fixed order and answers with a game setting that holds the message of the first one that fails. The engine
    /// supplies the facts; this says which of them matter for which action, in which order, and which setting speaks.
    ///
    /// Addresses are those of FalloutNV.exe (Fallout3.exe has the same checks in the same order, the places it differs
    /// are named). Wait: 0x969FA0 (FO3 0x788350). Sleep: the bed activation 0x5095B0 (FO3 0x4ADE00). Fast travel: the
    /// predicate 0x93D660 (FO3 0x774F40).
    enum class Action
    {
        /// The key that waits, and ShowSleepWaitMenu with its check argument: the messages are those of waiting
        Wait,
        /// A bed (a furniture that is a place to sleep) is used
        Sleep,
        /// A click on a place of the world map
        FastTravel,
    };

    /// The facts the engine knows about the player and the place it stands in
    struct Facts
    {
        /// The bed belongs to someone the player may not use it for (the owner of the furniture is not the player and
        /// the player has no permission, 0x567790 and 0x5785E0)
        bool mOwnedBed = false;
        /// The player is where it should not be (the flag of the character, 0x8D1E70)
        bool mTrespassing = false;
        /// An alarm is sounding for the player (0x953C80: a process of kind 0x15 that goes after the player)
        bool mAlarmSounding = false;
        /// A fraction of 7/8 or more of the height of the player is under the water (0x885520, the constant at
        /// 0x1084D20)
        bool mUnderwater = false;
        /// A character that is awake and enabled, and that is after the player or hostile to it, is within
        /// fHostileActorInteriorDistance (interior) or fHostileActorExteriorDistance of it (0x9764A0)
        bool mHostilesNear = false;
        /// The character is not on the ground: jumping or falling for waiting (controller state 1 or 2, 0x9306D0),
        /// falling for travel (0x8849C0)
        bool mAirborne = false;
        /// The place forbids waiting: the interior cell has the flag 0x80000 (the cell of the player), or the
        /// worldspace outside has it (0x5444C0)
        bool mNoWaitPlace = false;
        /// Waiting was switched off by a script (the control byte of the player, 0x944360; New Vegas only)
        bool mWaitSwitchedOff = false;
        /// Radiation is on the player (0x5098E0: the two flags of the radiation record of the manager)
        bool mInRadiation = false;
        /// What the process of the player says about radiation (waiting only, virtual 0x764)
        bool mRadiationProcess = false;
        /// An effect that lasts takes health from the player (0x822E00: a value modifier on the health that is
        /// detrimental)
        bool mTakingHealthDamage = false;

        /// Fast travel: the map marker was not found yet (0x798204)
        bool mUndiscovered = false;
        /// A script switched fast travel off (the flag of the player at +0x66D)
        bool mScriptBlocked = false;
        /// The player carries more than it can (0x954CC0: the weight of the inventory is above the limit of the
        /// strength and the perk entry point 0x16)
        bool mOverencumbered = false;
        /// A perk that lets a character that is over the limit travel (entry point 0x33 of 1 or more, New Vegas only)
        bool mTravelWhileOverencumbered = false;
        /// The fatigue or the health of the player is below 1
        bool mSpent = false;
        /// The place the player stands in forbids fast travel (0x544520): an interior unless the cell flag 4 of its
        /// record is set, an exterior if that flag is set or its worldspace forbids it
        bool mTravelForbidden = false;
    };

    /// The game setting that holds the message of the refusal, null when the player may. The order is the one of the
    /// executable.
    ///
    /// Wait: trespass, alarm, under water, hostile characters, in the air, the place (or waiting switched off), radiation
    /// (the manager or the process), a harmful effect. Sleep: the owned bed, then the place (the word of waiting,
    /// sNoWaitInCell: there is none for sleeping), trespass, hostile characters, radiation (the manager only), a harmful
    /// effect; nothing about the alarm, the water, the air or a script that switched waiting off. Fast travel: hostile
    /// characters, alarm, a harmful effect, a script, the weight, no fatigue or health, the air, the place, and last the
    /// marker that was not found.
    const char* refusal(Action action, const Facts& facts);

    /// A character the hostile check looks at
    struct Candidate
    {
        /// The distance to the player
        double mDistance = 0;
        bool mDead = false;
        bool mDisabled = false;
        /// The character has the player in its list of targets, or its aggression and factions make it an enemy of the
        /// player (0x8BC700, 0x8B06D0)
        bool mHostile = false;
    };

    /// The distance the check of hostile characters reaches: fHostileActorExteriorDistance outside, and
    /// fHostileActorInteriorDistance inside (the executables' defaults 3000 and 2000; the master files have neither)
    double hostileReach(bool interior, double exterior = 3000.0, double inside = 2000.0);

    /// Whether any of the characters counts (0x9764A0): one that is alive, enabled, hostile and within the reach
    bool hostileNear(const std::vector<Candidate>& candidates, double reach);

    /// Fast travel time: the hours a trip takes, trunc(path length / speed / 3600 x time scale) (0x93CDF0). A trip that
    /// is shorter than one game hour at the time scale takes none.
    int travelHours(double pathLength, double speed, double timeScale);

    /// The speed the player is taken to travel at (0x647F00): the speed multiplier of the actor values over 100 times
    /// fMoveBaseSpeed times fMoveRunMult, and with a leg crippled the setting of one (fMoveOneCrippledLegSpeedMult) or
    /// of two (fMoveTwoCrippledLegsSpeedMult). The armour, the weapon and the load terms of 0x647D10 are not in it.
    double travelSpeed(double speedMultiplier, int crippledLegs, double baseSpeed = 77.0, double runMultiplier = 4.0,
        double oneLeg = 0.85, double twoLegs = 0.75);

    /// The seconds of healing the player gets for each hour of the trip (0x88B510 with the constant at 0x10162C0)
    constexpr double travelHealingSeconds = 2.0;
}

#endif
