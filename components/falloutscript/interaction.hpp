#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_INTERACTION_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_INTERACTION_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace FalloutScript::Interaction
{
    /// The value of a game setting by its name, the second argument when the files have none
    using Setting = std::function<double(std::string_view name, double fallback)>;

    /// The level of a lock that only its key opens
    constexpr int keyOnlyLevel = 255;

    /// How hard a lock is to pick, as the games class it
    enum class LockClass
    {
        VeryEasy = 0,
        Easy,
        Average,
        Hard,
        VeryHard,
        /// Only the key opens it
        Impossible,
    };

    /// The level a lock has for a character of that level (FalloutNV.exe 0x430A10): the level of the lock, plus
    /// trunc(fLeveledLockMult) times the level of the character when the lock is a leveled one, and never more than 99.
    /// A lock that only its key opens stays what it is.
    int lockLevel(int level, bool leveled, int characterLevel, const Setting& setting);

    /// The class of a lock of that level (0x430B40): by the iLockLevelMax* settings, 0 up to 25 easy, and so on;
    /// beyond very hard, or a level of 255, it is impossible
    LockClass lockClass(int level, const Setting& setting);

    /// The name of the game setting that holds the word for the class (sLockLevelName*) and the one that holds the
    /// experience for picking a lock of it (iXPLevelPickLock*, none for the class that can not be picked)
    const char* lockClassWord(LockClass lockClass);
    const char* lockClassExperience(LockClass lockClass);

    /// The chance that one bobby pin opens a lock of that level for a character of that Lockpick skill, from 0.05 up to
    /// 0.95. A stand-in: the executables give the minigame of the lock and not a chance, and the rule that decides if a
    /// character may try was not found. It grows with the skill (fLockSkillBase + fLockSkillMult * skill) and falls
    /// with the level of the lock, so that an average skill picks an average lock about every other pin.
    double pickChance(double skill, int level, const Setting& setting);

    /// The hacking level of a terminal (0x5011A0): its difficulty from 0 to 4, plus trunc(fHackLevelMult * level of the
    /// character) when it is a leveled one, and never more than 4
    int hackLevel(int difficulty, bool leveled, int characterLevel, const Setting& setting);

    /// What stops a character from waiting or sleeping, as far as the host can tell
    struct RestConditions
    {
        /// Sleeping, or waiting
        bool mSleeping = false;
        bool mTrespassing = false;
        bool mAlarmSounding = false;
        bool mUnderwater = false;
        bool mHostilesNear = false;
        bool mInAir = false;
        bool mInRadiation = false;
        bool mTakingHealthDamage = false;
        bool mInCombat = false;
        /// A bed that belongs to someone else (when sleeping)
        bool mOwnedBed = false;
    };

    /// The game setting that holds the refusal, none when the character may rest (the order is that of the checks of
    /// the executable, 0x969FA0 for waiting and 0x5095B0 for sleeping, as far as it was read)
    std::optional<std::string> restRefusal(const RestConditions& conditions);

    /// What stops a character from travelling at once to a place
    struct TravelConditions
    {
        bool mHostilesNear = false;
        bool mAlarmSounding = false;
        bool mTakingHealthDamage = false;
        bool mScriptBlocked = false;
        bool mOverencumbered = false;
        bool mInAir = false;
        /// The cell the character stands in forbids it (an interior without the flag that allows it, or a worldspace
        /// that forbids it)
        bool mCellForbids = false;
        /// The place has not been found yet
        bool mUndiscovered = false;
    };

    /// The game setting that holds the refusal, none when the character may go (the order of 0x93D660 as it was read)
    std::optional<std::string> travelRefusal(const TravelConditions& conditions);

    /// The hours a trip of that many units takes: a stand-in, since the formula of the executable was not found. It is
    /// an hour for each 5,000 units, and never less than half an hour.
    double travelHours(double distance);
}

#endif
