#include <components/esm4/packageschedule.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace
{
    using namespace testing;

    constexpr std::uint8_t any = 0xff;
    constexpr int sunday = 0;
    constexpr int monday = 1;
    constexpr int friday = 5;
    constexpr int saturday = 6;

    ESM4::AIPackage::PSDT schedule(std::uint8_t time, std::uint32_t duration, std::uint8_t day = any)
    {
        ESM4::AIPackage::PSDT result;
        result.time = time;
        result.duration = duration;
        result.dayOfWeek = day;
        return result;
    }

    ESM4::PackageClock clock(float hour, int day = monday)
    {
        ESM4::PackageClock result;
        result.mHour = hour;
        result.mDayOfWeek = day;
        return result;
    }

    ESM4::AIPackage package(int type, ESM4::AIPackage::PSDT when = ESM4::AIPackage::PSDT())
    {
        ESM4::AIPackage result;
        result.mData.type = type;
        result.mSchedule = when;
        return result;
    }

    ESM::FormId id(std::uint32_t value)
    {
        return ESM::FormId::fromUint32(value);
    }

    TEST(ESM4PackageScheduleTest, aScheduleWithoutATimeIsAlwaysOn)
    {
        const ESM4::AIPackage::PSDT always;
        for (const float hour : { 0.f, 5.5f, 12.f, 23.99f })
            EXPECT_TRUE(ESM4::scheduleIncludes(always, clock(hour))) << hour;
    }

    TEST(ESM4PackageScheduleTest, aScheduleWithATimeLastsForItsDuration)
    {
        const ESM4::AIPackage::PSDT morning = schedule(8, 4);
        EXPECT_FALSE(ESM4::scheduleIncludes(morning, clock(7.99f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(morning, clock(8.f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(morning, clock(11.99f)));
        EXPECT_FALSE(ESM4::scheduleIncludes(morning, clock(12.f)));
        EXPECT_FALSE(ESM4::scheduleIncludes(morning, clock(20.f)));
    }

    TEST(ESM4PackageScheduleTest, aScheduleCanGoPastMidnight)
    {
        const ESM4::AIPackage::PSDT night = schedule(22, 8);
        EXPECT_FALSE(ESM4::scheduleIncludes(night, clock(21.5f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(night, clock(22.f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(night, clock(23.5f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(night, clock(0.f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(night, clock(5.99f)));
        EXPECT_FALSE(ESM4::scheduleIncludes(night, clock(6.f)));
        EXPECT_FALSE(ESM4::scheduleIncludes(night, clock(12.f)));
    }

    TEST(ESM4PackageScheduleTest, aDurationOfADayOrMoreIsAllDay)
    {
        for (const float hour : { 0.f, 8.f, 9.f, 23.9f })
        {
            EXPECT_TRUE(ESM4::scheduleIncludes(schedule(9, 24), clock(hour))) << hour;
            EXPECT_TRUE(ESM4::scheduleIncludes(schedule(9, 100), clock(hour))) << hour;
        }
    }

    TEST(ESM4PackageScheduleTest, aTimeWithoutADurationLastsAnHour)
    {
        const ESM4::AIPackage::PSDT brief = schedule(10, 0);
        EXPECT_FALSE(ESM4::scheduleIncludes(brief, clock(9.9f)));
        EXPECT_TRUE(ESM4::scheduleIncludes(brief, clock(10.5f)));
        EXPECT_FALSE(ESM4::scheduleIncludes(brief, clock(11.f)));
    }

    TEST(ESM4PackageScheduleTest, anHourThatIsNotOneIsNeverOn)
    {
        EXPECT_FALSE(ESM4::scheduleIncludes(schedule(24, 5), clock(0.5f)));
        EXPECT_FALSE(ESM4::scheduleIncludes(schedule(100, 5), clock(12.f)));
    }

    TEST(ESM4PackageScheduleTest, aDayOfTheWeekIsOneDay)
    {
        const ESM4::AIPackage::PSDT onFriday = schedule(any, 0, friday);
        for (int day = 0; day < 7; ++day)
            EXPECT_EQ(ESM4::scheduleIncludes(onFriday, clock(12.f, day)), day == friday) << day;
    }

    TEST(ESM4PackageScheduleTest, theGroupsOfDays)
    {
        const struct
        {
            std::uint8_t setting;
            std::vector<int> days;
        } groups[] = {
            { 7, { 1, 2, 3, 4, 5 } },
            { 8, { 0, 6 } },
            { 9, { 1, 3, 5 } },
            { 10, { 2, 4 } },
        };
        for (const auto& group : groups)
            for (int day = 0; day < 7; ++day)
                EXPECT_EQ(ESM4::scheduleIncludes(schedule(any, 0, group.setting), clock(12.f, day)),
                    std::find(group.days.begin(), group.days.end(), day) != group.days.end())
                    << int(group.setting) << " " << day;
    }

    TEST(ESM4PackageScheduleTest, aPackageThatPassesMidnightKeepsTheDayItStartedOn)
    {
        const ESM4::AIPackage::PSDT weekdayNights = schedule(22, 8, 7);
        EXPECT_TRUE(ESM4::scheduleIncludes(weekdayNights, clock(23.f, friday)));
        // At two in the morning on Saturday it is still the night of Friday
        EXPECT_TRUE(ESM4::scheduleIncludes(weekdayNights, clock(2.f, saturday)));
        EXPECT_FALSE(ESM4::scheduleIncludes(weekdayNights, clock(23.f, saturday)));
        // and at two on Sunday and Monday it is the night of Saturday and of Sunday
        EXPECT_FALSE(ESM4::scheduleIncludes(weekdayNights, clock(2.f, sunday)));
        EXPECT_FALSE(ESM4::scheduleIncludes(weekdayNights, clock(2.f, monday)));
        EXPECT_TRUE(ESM4::scheduleIncludes(weekdayNights, clock(2.f, 2)));
    }

    TEST(ESM4PackageScheduleTest, aDayOfTheWeekOutsideOfTheWeekIsWrapped)
    {
        EXPECT_TRUE(ESM4::scheduleIncludes(schedule(any, 0, 0), clock(12.f, 7)));
        EXPECT_TRUE(ESM4::scheduleIncludes(schedule(any, 0, 6), clock(12.f, -1)));
    }

    TEST(ESM4PackageScheduleTest, theBehaviourOfAKindOfPackage)
    {
        for (const int type : std::initializer_list<int>{
                 ESM4::Package_Travel, ESM4::Package_Guard, ESM4::Package_Eat, ESM4::Package_Sleep })
            EXPECT_EQ(ESM4::packageBehaviour(type), ESM4::PackageBehaviour::Stay) << type;
        for (const int type :
            std::initializer_list<int>{ ESM4::Package_Sandbox, ESM4::Package_Wander, ESM4::Package_Patrol })
            EXPECT_EQ(ESM4::packageBehaviour(type), ESM4::PackageBehaviour::Roam) << type;
        for (const int type : std::initializer_list<int>{ ESM4::Package_Follow, ESM4::Package_Accompany })
            EXPECT_EQ(ESM4::packageBehaviour(type), ESM4::PackageBehaviour::Follow) << type;
        for (const int type : std::initializer_list<int>{ ESM4::Package_Find, ESM4::Package_Escort,
                 ESM4::Package_UseItemAt, ESM4::Package_Ambush, ESM4::Package_FleeNotCombat, ESM4::Package_CastMagic,
                 ESM4::Package_Dialogue, ESM4::Package_UseWeapon, 99, -1 })
            EXPECT_EQ(ESM4::packageBehaviour(type), ESM4::PackageBehaviour::None) << type;
    }

    TEST(ESM4PackageScheduleTest, aPackageIsSkippedForTheFirstReasonThatHolds)
    {
        const ESM4::PackageClock noon = clock(12.f);

        EXPECT_EQ(ESM4::packageSkip(package(ESM4::Package_Sandbox), noon), ESM4::PackageSkip::None);
        EXPECT_EQ(ESM4::packageSkip(package(ESM4::Package_Find), noon), ESM4::PackageSkip::Behaviour);
        EXPECT_EQ(ESM4::packageSkip(package(ESM4::Package_Follow), noon), ESM4::PackageSkip::None);
        EXPECT_EQ(
            ESM4::packageSkip(package(ESM4::Package_Sandbox, schedule(20, 2)), noon), ESM4::PackageSkip::Schedule);
        // A package of no use is skipped whatever its schedule says
        EXPECT_EQ(
            ESM4::packageSkip(package(ESM4::Package_Dialogue, schedule(20, 2)), noon), ESM4::PackageSkip::Behaviour);

        ESM4::AIPackage conditional = package(ESM4::Package_Sandbox);
        conditional.mTargetConditions.emplace_back();
        EXPECT_EQ(ESM4::packageSkip(conditional, noon), ESM4::PackageSkip::Conditions);
        ESM4::AIPackage older = package(ESM4::Package_Sandbox);
        older.mConditions.emplace_back();
        EXPECT_EQ(ESM4::packageSkip(older, noon), ESM4::PackageSkip::Conditions);

        // and the schedule is looked at before the conditions
        conditional.mSchedule = schedule(20, 2);
        EXPECT_EQ(ESM4::packageSkip(conditional, noon), ESM4::PackageSkip::Schedule);
    }

    TEST(ESM4PackageScheduleTest, aPackageWithoutADataRecordHasNoSchedule)
    {
        // The defaults of a package that has no PSDT: any time, any day
        const ESM4::AIPackage::PSDT none;
        EXPECT_EQ(none.time, any);
        EXPECT_EQ(none.dayOfWeek, any);
        EXPECT_EQ(none.duration, 0u);
    }

    TEST(ESM4PackageScheduleTest, theFirstPackageOfTheListThatIsFollowedIsChosen)
    {
        const ESM4::AIPackage sleep = package(ESM4::Package_Sleep, schedule(22, 8));
        ESM4::AIPackage quest = package(ESM4::Package_Travel);
        quest.mTargetConditions.emplace_back();
        const ESM4::AIPackage talk = package(ESM4::Package_Dialogue);
        const ESM4::AIPackage work = package(ESM4::Package_Sandbox, schedule(8, 12));
        const ESM4::AIPackage idle = package(ESM4::Package_Wander);
        const std::vector<const ESM4::AIPackage*> list{ nullptr, &quest, &talk, &sleep, &work, &idle };

        EXPECT_EQ(ESM4::choosePackage(list, clock(23.f)), &sleep);
        EXPECT_EQ(ESM4::choosePackage(list, clock(3.f)), &sleep);
        EXPECT_EQ(ESM4::choosePackage(list, clock(9.f)), &work);
        EXPECT_EQ(ESM4::choosePackage(list, clock(21.f)), &idle);
        EXPECT_EQ(ESM4::choosePackage({ &quest, &talk }, clock(9.f)), nullptr);
        EXPECT_EQ(ESM4::choosePackage({}, clock(9.f)), nullptr);
    }

    ESM4::Npc npc(std::uint32_t value, std::uint16_t templateFlags, std::uint32_t templateId,
        std::initializer_list<std::uint32_t> packages)
    {
        ESM4::Npc result{};
        result.mId = id(value);
        result.mIsFONV = true;
        result.mBaseConfig.fo3.templateFlags = templateFlags;
        if (templateId != 0)
            result.mBaseTemplate = id(templateId);
        for (const std::uint32_t package : packages)
            result.mAIPackages.push_back(id(package));
        return result;
    }

    TEST(ESM4PackageScheduleTest, aCharacterHasItsOwnPackages)
    {
        const ESM4::Npc own = npc(0x1000, 0, 0, { 0x2000, 0x2001 });
        EXPECT_THAT(ESM4::characterPackages({ &own }), ElementsAre(id(0x2000), id(0x2001)));
    }

    TEST(ESM4PackageScheduleTest, aCharacterThatUsesATemplateHasThePackagesOfTheTemplate)
    {
        const ESM4::Npc own = npc(0x1000, ESM4::Npc::Template_UseAIPackage, 0x1001, { 0x2000 });
        const ESM4::Npc base = npc(0x1001, 0, 0, { 0x2002, 0x2003 });
        EXPECT_THAT(ESM4::characterPackages({ &own, &base }), ElementsAre(id(0x2002), id(0x2003)));

        // The flags of other things do not take the packages from the template
        const ESM4::Npc looks = npc(0x1000, ESM4::Npc::Template_UseTraits, 0x1001, { 0x2000 });
        EXPECT_THAT(ESM4::characterPackages({ &looks, &base }), ElementsAre(id(0x2000)));
    }

    TEST(ESM4PackageScheduleTest, aChainOfTemplatesGivesThePackagesOfTheFirstRecordThatHasItsOwn)
    {
        const ESM4::Npc first = npc(0x1000, ESM4::Npc::Template_UseAIPackage, 0x1001, {});
        const ESM4::Npc second = npc(0x1001, ESM4::Npc::Template_UseAIPackage, 0x1002, { 0x2000 });
        const ESM4::Npc third = npc(0x1002, 0, 0, { 0x2004 });
        EXPECT_THAT(ESM4::characterPackages({ &first, &second, &third }), ElementsAre(id(0x2004)));
        // Nothing to take the packages from
        EXPECT_THAT(ESM4::characterPackages({ &first, &second }), IsEmpty());
        EXPECT_THAT(ESM4::characterPackages({}), IsEmpty());
    }

    TEST(ESM4PackageScheduleTest, aCreatureHasThePackagesOfItsRecordOrItsTemplate)
    {
        ESM4::Creature own{};
        own.mId = id(0x1000);
        own.mIsFONV = true;
        own.mBaseConfig.fo3.templateFlags = ESM4::Creature::Template_UseAIPackage;
        own.mAIPackages.push_back(id(0x2000));
        ESM4::Creature base{};
        base.mId = id(0x1001);
        base.mIsFONV = true;
        base.mAIPackages.push_back(id(0x2001));

        EXPECT_THAT(ESM4::creaturePackages({ &own, &base }), ElementsAre(id(0x2001)));
        own.mBaseConfig.fo3.templateFlags = 0;
        EXPECT_THAT(ESM4::creaturePackages({ &own, &base }), ElementsAre(id(0x2000)));
    }
}
