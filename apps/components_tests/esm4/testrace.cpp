#include <components/esm4/loadrace.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    TEST(ESM4RaceTest, readsTheVoiceTypesOfTheMaleAndTheFemale)
    {
        std::string voices;
        append<std::uint32_t>(voices, 0x000A0001);
        append<std::uint32_t>(voices, 0x000A0002);

        const std::vector<ESM4::Race> races
            = loadRecords<ESM4::Race>("RACE", record("RACE", 1, zString("EDID", "Human") + subRecord("VTCK", voices)));

        ASSERT_EQ(races.size(), 1u);
        EXPECT_EQ(races[0].mDefaultVoice[0].toUint32(), 0x000A0001u);
        EXPECT_EQ(races[0].mDefaultVoice[1].toUint32(), 0x000A0002u);
    }

    TEST(ESM4RaceTest, hasNoVoiceTypesWhenTheRecordGivesNone)
    {
        const std::vector<ESM4::Race> races
            = loadRecords<ESM4::Race>("RACE", record("RACE", 1, zString("EDID", "Human")));

        ASSERT_EQ(races.size(), 1u);
        EXPECT_TRUE(races[0].mDefaultVoice[0].isZeroOrUnset());
        EXPECT_TRUE(races[0].mDefaultVoice[1].isZeroOrUnset());
    }
}
