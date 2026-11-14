#include "interaction.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Interaction
{
    namespace
    {
        double get(const Setting& setting, std::string_view name, double fallback)
        {
            return setting ? setting(name, fallback) : fallback;
        }
    }

    int lockLevel(int level, bool leveled, int characterLevel, const Setting& setting)
    {
        if (level >= keyOnlyLevel)
            return keyOnlyLevel;
        int result = level;
        if (leveled)
            result += static_cast<int>(get(setting, "fLeveledLockMult", 1.5)) * characterLevel;
        return std::min(result, 99);
    }

    LockClass lockClass(int level, const Setting& setting)
    {
        if (level >= keyOnlyLevel)
            return LockClass::Impossible;
        if (level <= get(setting, "iLockLevelMaxVeryEasy", 0))
            return LockClass::VeryEasy;
        if (level <= get(setting, "iLockLevelMaxEasy", 25))
            return LockClass::Easy;
        if (level <= get(setting, "iLockLevelMaxAverage", 50))
            return LockClass::Average;
        if (level <= get(setting, "iLockLevelMaxHard", 75))
            return LockClass::Hard;
        if (level <= get(setting, "iLockLevelMaxVeryHard", 100))
            return LockClass::VeryHard;
        return LockClass::Impossible;
    }

    const char* lockClassWord(LockClass lockClass)
    {
        switch (lockClass)
        {
            case LockClass::VeryEasy:
                return "sLockLevelNameVeryEasy";
            case LockClass::Easy:
                return "sLockLevelNameEasy";
            case LockClass::Average:
                return "sLockLevelNameAverage";
            case LockClass::Hard:
                return "sLockLevelNameHard";
            case LockClass::VeryHard:
                return "sLockLevelNameVeryHard";
            case LockClass::Impossible:
                return "sLockLevelNameImpossible";
        }
        return "sLockLevelNameAverage";
    }

    const char* lockClassExperience(LockClass lockClass)
    {
        switch (lockClass)
        {
            case LockClass::VeryEasy:
                return "iXPRewardPickLockVeryEasy";
            case LockClass::Easy:
                return "iXPRewardPickLockEasy";
            case LockClass::Average:
                return "iXPRewardPickLockAverage";
            case LockClass::Hard:
                return "iXPRewardPickLockHard";
            case LockClass::VeryHard:
                return "iXPRewardPickLockVeryHard";
            case LockClass::Impossible:
                return nullptr;
        }
        return nullptr;
    }

    double pickChance(double skill, int level, const Setting& setting)
    {
        const double strength
            = get(setting, "fLockSkillBase", 10) + get(setting, "fLockSkillMult", 1) * std::clamp(skill, 0.0, 100.0);
        return std::clamp((strength + 40.0 - level) / 100.0, 0.05, 0.95);
    }

    int hackLevel(int difficulty, bool leveled, int characterLevel, const Setting& setting)
    {
        int result = std::clamp(difficulty, 0, 4);
        if (leveled)
            result += static_cast<int>(get(setting, "fHackLevelMult", 0.25) * characterLevel);
        return std::min(result, 4);
    }

    const char* hackClassExperience(int level)
    {
        switch (std::clamp(level, 0, 4))
        {
            case 0:
                return "iXPRewardHackComputerVeryEasy";
            case 1:
                return "iXPRewardHackComputerEasy";
            case 2:
                return "iXPRewardHackComputerAverage";
            case 3:
                return "iXPRewardHackComputerHard";
            default:
                return "iXPRewardHackComputerVeryHard";
        }
    }
}
