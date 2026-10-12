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

    TEST(ESM4RaceTest, doesNotReadVoiceTypesOfASizeThatIsNotTwoForms)
    {
        std::string voices;
        append<std::uint32_t>(voices, 0x000A0001); // one form, as a short record has
        std::string others;
        append<std::uint32_t>(others, 0x000A0003);
        append<std::uint32_t>(others, 0x000A0004);
        append<std::uint32_t>(others, 0x000A0005);

        for (const std::string& data : { voices, others })
        {
            const std::vector<ESM4::Race> races = loadRecords<ESM4::Race>("RACE",
                record("RACE", 1, zString("EDID", "Human") + subRecord("VTCK", data) + zString("FULL", "Human")));

            ASSERT_EQ(races.size(), 1u);
            EXPECT_TRUE(races[0].mDefaultVoice[0].isZeroOrUnset());
            EXPECT_TRUE(races[0].mDefaultVoice[1].isZeroOrUnset());
            EXPECT_EQ(races[0].mEditorId, "Human");
            EXPECT_EQ(races[0].mFullName, "Human");
        }
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
