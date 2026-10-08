#include <components/esm4/common.hpp>
#include <components/esm4/packagecensus.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    constexpr std::uint32_t wander = 0x1001;
    constexpr std::uint32_t sleep = 0x1002;
    constexpr std::uint32_t sandbox = 0x1003;
    constexpr std::uint32_t shortData = 0x1004;
    constexpr std::uint32_t broken = 0x1005;
    constexpr std::uint32_t noRecord = 0x1099;

    constexpr std::uint32_t useAIPackage = 0x0020;
    constexpr std::uint8_t any = 0xff;

    // PKDT of Fallout 3 and New Vegas: the flags, the type, a byte, and three words
    std::string packageData(std::uint8_t type)
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        append(data, type);
        append<std::uint8_t>(data, 0);
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 0);
        return subRecord("PKDT", data);
    }

    std::string schedule(std::uint8_t month, std::uint8_t day, std::uint8_t date, std::uint8_t time)
    {
        std::string data;
        append(data, month);
        append(data, day);
        append(data, date);
        append(data, time);
        append<std::int32_t>(data, 4);
        return subRecord("PSDT", data);
    }

    std::string typed(std::string_view code, std::int32_t type, std::size_t size)
    {
        std::string data;
        append(data, type);
        data.resize(size, '\0');
        return subRecord(code, data);
    }

    std::string condition()
    {
        return subRecord("CTDA", std::string(28, '\0'));
    }

    std::string listed(std::uint32_t id)
    {
        return valueSubRecord("PKID", id);
    }

    // The base configuration of Fallout 3 and New Vegas, 24 bytes, with the flags of the template at the end
    std::string configuration(std::uint16_t templateFlags)
    {
        std::string data(22, '\0');
        append(data, templateFlags);
        return subRecord("ACBS", data);
    }

    std::string basePlugin()
    {
        const std::string packages
            // wanders from 8 in the morning in the cell it names, when a condition holds
            = record("PACK", wander, packageData(5) + schedule(any, any, 0, 8) + typed("PLDT", 1, 12) + condition())
            // sleeps at 22 on a Wednesday, near a reference, with an idle for when it begins
            + record("PACK", sleep,
                packageData(4) + schedule(any, 3, 0, 22) + typed("PLDT", 0, 12) + typed("PTDT", 0, 16)
                    + valueSubRecord<std::uint32_t>("INAM", 0x1234))
            // whenever, wherever
            + record("PACK", sandbox, packageData(12) + schedule(any, any, 0, any))
            // a type that is too short to have a kind
            + record("PACK", shortData, subRecord("PKDT", std::string(4, '\0')))
            // a sub-record that says it has more data than the record has
            + record("PACK", broken, std::string("PKDT") + std::string("\x32\x00", 2) + std::string(8, '\0'));

        const std::string characters
            // sleeps first, wanders second, and lists a package that no record has
            = record("NPC_", 0x2001, configuration(0) + listed(sleep) + listed(wander) + listed(noRecord))
            // takes its packages from the template
            + record("NPC_", 0x2002, configuration(useAIPackage) + listed(wander))
            + record("NPC_", 0x2003, configuration(0))
            + record("NPC_", 0x2004, configuration(0) + listed(sandbox))
            // deleted after it was listed
            + record("NPC_", 0x2005, configuration(0) + listed(sandbox))
            + record("NPC_", 0x2005, "", ESM4::Rec_Deleted);

        return header() + topGroup("PACK", packages) + topGroup("NPC_", characters);
    }

    void collect(ESM4::PackageCensus& census, const std::string& plugin)
    {
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "base.esm", nullptr, nullptr);
        reader.setModIndex(0);
        census.collect(reader);
    }

    TEST(ESM4PackageCensusTest, countsThePackagesByKindScheduleLocationAndTarget)
    {
        ESM4::PackageCensus census;
        collect(census, basePlugin());
        const ESM4::PackageCensus::Summary summary = census.summarize();

        // the broken record is left out
        EXPECT_EQ(summary.mPackages, 4u);
        EXPECT_THAT(summary.mTypes,
            UnorderedElementsAre(Pair(5, 1u), Pair(4, 1u), Pair(12, 1u), Pair(ESM4::PackageCensus::noType, 1u)));
        EXPECT_EQ(summary.mWithTime, 2u);
        EXPECT_EQ(summary.mWithDay, 1u);
        EXPECT_EQ(summary.mWithMonth, 0u);
        EXPECT_EQ(summary.mWithDate, 0u);
        EXPECT_EQ(summary.mWithConditions, 1u);
        EXPECT_EQ(summary.mWithEvents, 1u);
        EXPECT_THAT(summary.mLocations,
            UnorderedElementsAre(Pair(1, 1u), Pair(0, 1u), Pair(ESM4::PackageCensus::noLocation, 2u)));
        EXPECT_THAT(summary.mTargets, UnorderedElementsAre(Pair(0, 1u), Pair(ESM4::PackageCensus::noTarget, 3u)));
        EXPECT_TRUE(census.getFatalErrors().empty());
    }

    TEST(ESM4PackageCensusTest, countsThePackagesThatCharactersList)
    {
        ESM4::PackageCensus census;
        collect(census, basePlugin());
        const ESM4::PackageCensus::Summary summary = census.summarize();

        // 0x2005 is deleted
        EXPECT_EQ(summary.mCharacters, 4u);
        EXPECT_EQ(summary.mPackagesFromTemplate, 1u);
        EXPECT_EQ(summary.mNoPackages, 1u);
        EXPECT_EQ(summary.mListed[3], 1u);
        EXPECT_EQ(summary.mListed[1], 1u);
        EXPECT_EQ(summary.mListedNoRecord, 1u);
        // the first package of one is the sleep, of the other the sandbox
        EXPECT_THAT(summary.mFirstType, UnorderedElementsAre(Pair(4, 1u), Pair(12, 1u)));
        EXPECT_THAT(summary.mAnyType, UnorderedElementsAre(Pair(4, 1u), Pair(5, 1u), Pair(12, 1u)));
        EXPECT_EQ(summary.mCharactersWithTime, 1u);
    }

    std::string scheduleWithDuration(std::uint8_t day, std::uint8_t time, std::uint32_t duration)
    {
        std::string data;
        append<std::uint8_t>(data, any);
        append(data, day);
        append<std::uint8_t>(data, 0);
        append(data, time);
        append(data, duration);
        return subRecord("PSDT", data);
    }

    TEST(ESM4PackageCensusTest, countsTheHoursThatPackagesLast)
    {
        ESM4::PackageCensus census;
        collect(census, basePlugin());
        // The wander package and the sleep package start at an hour and last for four hours
        EXPECT_THAT(census.summarize().mDurations, UnorderedElementsAre(Pair(4u, 2u)));
    }

    TEST(ESM4PackageCensusTest, countsWhichPackageCharactersFollowAtEachHour)
    {
        const std::string packages
            // travels from 9 to 12 on weekdays
            = record("PACK", 0x1001, packageData(6) + scheduleWithDuration(7, 9, 3))
            // sleeps from 22 to 6
            + record("PACK", 0x1002, packageData(4) + scheduleWithDuration(any, 22, 8))
            // sandboxes at any other time
            + record("PACK", 0x1003, packageData(12) + scheduleWithDuration(any, any, 0))
            // finds something, which the game does nothing for yet
            + record("PACK", 0x1004, packageData(0) + scheduleWithDuration(any, any, 0));
        const std::string characters
            = record(
                  "NPC_", 0x2001, configuration(0) + listed(0x1004) + listed(0x1001) + listed(0x1002) + listed(0x1003))
            // nothing of theirs is on from 6 to 22
            + record("NPC_", 0x2002, configuration(0) + listed(0x1002) + listed(0x1004));
        ESM4::PackageCensus census;
        collect(census, header() + topGroup("PACK", packages) + topGroup("NPC_", characters));
        const ESM4::PackageCensus::Summary summary = census.summarize();

        EXPECT_THAT(summary.mFollowedByHour[3], UnorderedElementsAre(Pair(4, 2u)));
        EXPECT_THAT(
            summary.mFollowedByHour[8], UnorderedElementsAre(Pair(12, 1u), Pair(ESM4::PackageCensus::followsNone, 1u)));
        EXPECT_THAT(
            summary.mFollowedByHour[9], UnorderedElementsAre(Pair(6, 1u), Pair(ESM4::PackageCensus::followsNone, 1u)));
        EXPECT_THAT(
            summary.mFollowedByHour[11], UnorderedElementsAre(Pair(6, 1u), Pair(ESM4::PackageCensus::followsNone, 1u)));
        EXPECT_THAT(summary.mFollowedByHour[12],
            UnorderedElementsAre(Pair(12, 1u), Pair(ESM4::PackageCensus::followsNone, 1u)));
        EXPECT_THAT(summary.mFollowedByHour[22], UnorderedElementsAre(Pair(4, 2u)));

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("  09:00  nothing 1  Travel 1\n"));
        EXPECT_THAT(out.str(), HasSubstr("  03:00  Sleep 2\n"));
    }

    TEST(ESM4PackageCensusTest, namesThePackageTypes)
    {
        EXPECT_EQ(ESM4::PackageCensus::packageTypeName(5), "Wander");
        EXPECT_EQ(ESM4::PackageCensus::packageTypeName(6), "Travel");
        EXPECT_EQ(ESM4::PackageCensus::packageTypeName(12), "Sandbox");
        EXPECT_EQ(ESM4::PackageCensus::packageTypeName(ESM4::PackageCensus::noType), "none");
        EXPECT_EQ(ESM4::PackageCensus::packageTypeName(99), "type 99");
    }

    TEST(ESM4PackageCensusTest, writesTheCounts)
    {
        ESM4::PackageCensus census;
        collect(census, basePlugin());
        std::ostringstream out;
        census.write(out);

        EXPECT_THAT(out.str(), HasSubstr("Package records: 4\n"));
        EXPECT_THAT(out.str(), HasSubstr("  with an hour of the day: 2\n"));
        EXPECT_THAT(out.str(), HasSubstr("Wander"));
        EXPECT_THAT(out.str(), HasSubstr("Characters (NPC_ records): 4\n"));
        EXPECT_THAT(out.str(), HasSubstr("  packages with no record that was read: 1\n"));
        EXPECT_THAT(out.str(), Not(HasSubstr("Reading stopped early")));
    }
}
