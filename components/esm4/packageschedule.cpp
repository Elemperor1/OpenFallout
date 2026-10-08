#include "packageschedule.hpp"

#include <algorithm>

#include "creaturemodel.hpp"
#include "wornarmor.hpp"

namespace ESM4
{
    namespace
    {
        constexpr std::uint8_t any = 0xff;
        constexpr float hoursInDay = 24.f;

        // The values of the day of the week of a schedule that are not a day
        constexpr int weekdays = 7;
        constexpr int weekends = 8;
        constexpr int mondayWednesdayFriday = 9;
        constexpr int tuesdayThursday = 10;

        bool dayIncludes(std::uint8_t setting, int day)
        {
            switch (setting)
            {
                case any:
                    return true;
                case weekdays:
                    return day >= 1 && day <= 5;
                case weekends:
                    return day == 0 || day == 6;
                case mondayWednesdayFriday:
                    return day == 1 || day == 3 || day == 5;
                case tuesdayThursday:
                    return day == 2 || day == 4;
                default:
                    return setting == day;
            }
        }
    }

    PackageBehaviour packageBehaviour(int packageType)
    {
        switch (packageType)
        {
            case Package_Travel:
            case Package_Guard:
            case Package_Eat:
            case Package_Sleep:
                return PackageBehaviour::Stay;
            case Package_Sandbox:
            case Package_Wander:
            case Package_Patrol:
                return PackageBehaviour::Roam;
            case Package_Follow:
            case Package_Accompany:
                return PackageBehaviour::Follow;
            default:
                return PackageBehaviour::None;
        }
    }

    bool scheduleIncludes(const AIPackage::PSDT& schedule, const PackageClock& clock)
    {
        const int day = ((clock.mDayOfWeek % 7) + 7) % 7;
        if (schedule.time == any)
            return dayIncludes(schedule.dayOfWeek, day);
        if (schedule.time >= hoursInDay)
            return false;

        const float length = schedule.duration == 0 ? scheduleDurationWithoutLength
                                                    : std::min(static_cast<float>(schedule.duration), hoursInDay);
        float elapsed = clock.mHour - static_cast<float>(schedule.time);
        if (elapsed < 0.f)
            elapsed += hoursInDay;
        if (elapsed >= length)
            return false;

        // The days are those on which the package starts: at two in the morning it is the evening before that counts
        return dayIncludes(schedule.dayOfWeek, clock.mHour < static_cast<float>(schedule.time) ? (day + 6) % 7 : day);
    }

    PackageSkip packageSkip(
        int packageType, const AIPackage::PSDT& schedule, bool hasConditions, const PackageClock& clock)
    {
        if (packageBehaviour(packageType) == PackageBehaviour::None)
            return PackageSkip::Behaviour;
        if (!scheduleIncludes(schedule, clock))
            return PackageSkip::Schedule;
        if (hasConditions)
            return PackageSkip::Conditions;
        return PackageSkip::None;
    }

    PackageSkip packageSkip(const AIPackage& package, const PackageClock& clock)
    {
        return packageSkip(package.mData.type, package.mSchedule,
            !package.mConditions.empty() || !package.mTargetConditions.empty(), clock);
    }

    const AIPackage* choosePackage(const std::vector<const AIPackage*>& packages, const PackageClock& clock)
    {
        for (const AIPackage* package : packages)
            if (package != nullptr && packageSkip(*package, clock) == PackageSkip::None)
                return package;
        return nullptr;
    }

    namespace
    {
        const std::vector<ESM::FormId> noPackages;
    }

    const std::vector<ESM::FormId>& characterPackages(const std::vector<const Npc*>& chain)
    {
        const Npc* owner = templateOwner(chain, Npc::Template_UseAIPackage);
        return owner != nullptr ? owner->mAIPackages : noPackages;
    }

    const std::vector<ESM::FormId>& creaturePackages(const std::vector<const Creature*>& chain)
    {
        const Creature* owner = creatureTemplateOwner(chain, Creature::Template_UseAIPackage);
        return owner != nullptr ? owner->mAIPackages : noPackages;
    }
}
