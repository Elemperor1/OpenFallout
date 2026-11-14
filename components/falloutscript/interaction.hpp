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
    /// experience for picking a lock of it (iXPRewardPickLock*: 20, 30, 40, 50 and 60 in both games' files; the
    /// iXPLevelPickLock* settings are the levels that the executable's reward function goes by; none for the class that
    /// can not be picked)
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

    /// The name of the game setting that holds the experience for hacking a terminal of that hacking level
    /// (iXPRewardHackComputer*, FalloutNV.exe 0x6705B0)
    const char* hackClassExperience(int level);
}

#endif
