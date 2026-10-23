#include <components/esm4/loadidle.hpp>
#include <components/esm4/loadmusc.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    // The layout the records of Fallout 3 and New Vegas have: a folder idle has no animation file, a leaf has one
    std::string leafIdle()
    {
        std::string parents;
        append<std::uint32_t>(parents, 0x00003B00);
        append<std::uint32_t>(parents, 0x00003B0E);
        return zString("EDID", "BedDefaultIdle") + zString("MODL", "Characters\\_Male\\IdleAnims\\bedleft_enter.kf")
            + subRecord("ANAM", parents) + subRecord("DATA", bytePattern(6, 7)) + subRecord("CTDA", bytePattern(28, 1))
            + subRecord("CTDA", bytePattern(28, 2));
    }

    TEST(ESM4IdleAnimationTest, readsTheAnimationFileAndTheTreeOfAFalloutIdle)
    {
        const std::vector<ESM4::IdleAnimation> records
            = loadRecords<ESM4::IdleAnimation>("IDLE", record("IDLE", 0x3B01, leafIdle()));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 0x3B01u);
        EXPECT_EQ(records[0].mEditorId, "BedDefaultIdle");
        EXPECT_EQ(records[0].mModel.getOriginal(), "Characters\\_Male\\IdleAnims\\bedleft_enter.kf");
        EXPECT_EQ(records[0].mParent.toUint32(), 0x3B00u);
        EXPECT_EQ(records[0].mPrevious.toUint32(), 0x3B0Eu);
    }

    TEST(ESM4IdleAnimationTest, skipsWhatItDoesNotKnow)
    {
        const std::vector<ESM4::IdleAnimation> records = loadRecords<ESM4::IdleAnimation>(
            "IDLE", record("IDLE", 5, zString("EDID", "Folder") + subRecord("ZZZZ", bytePattern(9, 3)) + leafIdle()));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEditorId, "BedDefaultIdle");
    }

    TEST(ESM4MusicTest, readsTheFileOfTheTrack)
    {
        std::string volume;
        append<float>(volume, -4.f);
        const std::vector<ESM4::Music> records = loadRecords<ESM4::Music>("MUSC",
            record("MUSC", 9,
                zString("EDID", "MusicFirst") + zString("FNAM", "special\\first.mp3") + subRecord("ANAM", volume)));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 9u);
        EXPECT_EQ(records[0].mEditorId, "MusicFirst");
        EXPECT_EQ(records[0].mMusicFile, "special\\first.mp3");
    }

    TEST(ESM4MusicTest, skipsWhatItDoesNotKnow)
    {
        const std::vector<ESM4::Music> records = loadRecords<ESM4::Music>("MUSC",
            record("MUSC", 9,
                zString("EDID", "MusicFirst") + subRecord("ZZZZ", bytePattern(3, 1)) + zString("FNAM", "a.mp3")));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mMusicFile, "a.mp3");
    }
}
