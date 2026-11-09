#include <components/esm4/loadmset.hpp>
#include <components/esm4/reader.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    TEST(ESM4MediaSetTest, readsEverySubrecordOfAMediaSet)
    {
        const std::string data = zString("EDID", "MusicSet") + zString("FULL", "Full name")
            + valueSubRecord<std::int32_t>("NAM1", 1) + valueSubRecord<std::uint8_t>("PNAM", 0x2B)
            + zString("NAM2", "day outer") + zString("NAM3", "day middle") + zString("NAM4", "day inner")
            + zString("NAM5", "night outer") + zString("NAM6", "night middle") + zString("NAM7", "night inner")
            + valueSubRecord<std::uint32_t>("HNAM", 0x00000811) + valueSubRecord<std::uint32_t>("INAM", 0x00000812)
            + valueSubRecord<float>("NAM8", -1.f) + valueSubRecord<float>("NAM9", -2.f)
            + valueSubRecord<float>("NAM0", -3.f) + valueSubRecord<float>("ANAM", -4.f)
            + valueSubRecord<float>("BNAM", -5.f) + valueSubRecord<float>("CNAM", -6.f)
            + valueSubRecord<float>("JNAM", 10.f) + valueSubRecord<float>("KNAM", 20.f)
            + valueSubRecord<float>("LNAM", 30.f) + valueSubRecord<float>("MNAM", 40.f)
            + valueSubRecord<float>("NNAM", 50.f) + valueSubRecord<float>("ONAM", 60.f)
            + valueSubRecord<float>("DNAM", 1.f) + valueSubRecord<float>("ENAM", 2.f)
            + valueSubRecord<float>("FNAM", 3.f) + valueSubRecord<float>("GNAM", 4.f)
            + subRecord("DATA", bytePattern(4, 1));

        const std::vector<ESM4::MediaSet> sets
            = loadRecords<ESM4::MediaSet>("MSET", record("MSET", 0x801, data));

        ASSERT_EQ(sets.size(), 1u);
        const ESM4::MediaSet& set = sets[0];
        EXPECT_EQ(set.mId.toUint32(), 0x801u);
        EXPECT_EQ(set.mEditorId, "MusicSet");
        EXPECT_EQ(set.mFullName, "Full name");
        EXPECT_EQ(set.mSetType, 1);
        EXPECT_EQ(set.mEnabled, 0x2B);
        EXPECT_EQ(set.mSet2, "day outer");
        EXPECT_EQ(set.mSet3, "day middle");
        EXPECT_EQ(set.mSet4, "day inner");
        EXPECT_EQ(set.mSet5, "night outer");
        EXPECT_EQ(set.mSet6, "night middle");
        EXPECT_EQ(set.mSet7, "night inner");
        EXPECT_EQ(set.mSoundIntro.toUint32(), 0x811u);
        EXPECT_EQ(set.mSoundOutro.toUint32(), 0x812u);
        EXPECT_EQ(set.mLevel8, -1.f);
        EXPECT_EQ(set.mLevel9, -2.f);
        EXPECT_EQ(set.mLevel0, -3.f);
        EXPECT_EQ(set.mLevelA, -4.f);
        EXPECT_EQ(set.mLevelB, -5.f);
        EXPECT_EQ(set.mLevelC, -6.f);
        EXPECT_EQ(set.mBoundaryDayOuter, 10.f);
        EXPECT_EQ(set.mBoundaryDayMiddle, 20.f);
        EXPECT_EQ(set.mBoundaryDayInner, 30.f);
        EXPECT_EQ(set.mBoundaryNightOuter, 40.f);
        EXPECT_EQ(set.mBoundaryNightMiddle, 50.f);
        EXPECT_EQ(set.mBoundaryNightInner, 60.f);
        EXPECT_EQ(set.mTime1, 1.f);
        EXPECT_EQ(set.mTime2, 2.f);
        EXPECT_EQ(set.mTime3, 3.f);
        EXPECT_EQ(set.mTime4, 4.f);
    }

    TEST(ESM4MediaSetTest, hasNoTypeUntilTheRecordGivesOne)
    {
        const std::vector<ESM4::MediaSet> sets
            = loadRecords<ESM4::MediaSet>("MSET", record("MSET", 1, zString("EDID", "Plain")));

        ASSERT_EQ(sets.size(), 1u);
        EXPECT_EQ(sets[0].mSetType, -1);
    }

    TEST(ESM4MediaSetTest, doesNotReadASubrecordItDoesNotKnow)
    {
        try
        {
            loadRecords<ESM4::MediaSet>("MSET", record("MSET", 1, subRecord("XXYZ", "")));
            FAIL() << "No exception";
        }
        catch (const std::runtime_error& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord XXYZ"));
        }
    }
}
