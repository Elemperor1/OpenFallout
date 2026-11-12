#include <components/esm4/loadmusc.hpp>
#include <components/esm4/loadrefr.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    // A music record of New Vegas names a track or a folder (FNAM) and how loud it is (ANAM)
    TEST(ESM4MusicRecordTest, musicReadsItsFileAndAttenuation)
    {
        const std::string data = zString("EDID", "musDocMitchellHouse") + zString("FNAM", "loc\\corporateruins\\a.mp3")
            + valueSubRecord<float>("ANAM", 6.f);
        const std::vector<ESM4::Music> records = loadRecords<ESM4::Music>("MUSC", record("MUSC", 7, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEditorId, "musDocMitchellHouse");
        EXPECT_EQ(records[0].mMusicFile, "loc\\corporateruins\\a.mp3");
        EXPECT_TRUE(records[0].mHasAttenuation);
        EXPECT_FLOAT_EQ(records[0].mAttenuation, 6.f);
    }

    TEST(ESM4MusicRecordTest, musicOfFallout3HasNoAttenuation)
    {
        const std::string data = zString("EDID", "DefaultExplore") + zString("FNAM", "explore\\");
        const std::vector<ESM4::Music> records = loadRecords<ESM4::Music>("MUSC", record("MUSC", 7, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mMusicFile, "explore\\");
        EXPECT_FALSE(records[0].mHasAttenuation);
        EXPECT_FLOAT_EQ(records[0].mAttenuation, 0.f);
    }

    TEST(ESM4MusicRecordTest, musicWithAnAnamThatIsNotAFloatIsSkipped)
    {
        const std::string data = zString("EDID", "Odd") + subRecord("ANAM", bytePattern(3, 1)) + zString("FNAM", "a.mp3");
        const std::vector<ESM4::Music> records = loadRecords<ESM4::Music>("MUSC", record("MUSC", 7, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_FALSE(records[0].mHasAttenuation);
        EXPECT_EQ(records[0].mMusicFile, "a.mp3");
    }

    // The audio marker of New Vegas: a reference with MMRK that has a media location controller (CNAM), a radius
    // (XRDS) and the shares at which the layers of the music come in (MNAM, NNAM)
    TEST(ESM4MusicRecordTest, referenceReadsAnAudioMarker)
    {
        const std::string data = valueSubRecord("NAME", std::uint32_t(0x23)) + subRecord("MMRK", "")
            + zString("FULL", "") + valueSubRecord<std::uint32_t>("CNAM", 0x0015D020)
            + valueSubRecord<std::uint32_t>("BNAM", 1) + valueSubRecord<float>("MNAM", 0.33f)
            + valueSubRecord<float>("NNAM", 0.67f) + valueSubRecord<float>("XRDS", 10000.f);
        const std::vector<ESM4::Reference> records = loadRecords<ESM4::Reference>("REFR", record("REFR", 0x2001, data));
        ASSERT_EQ(records.size(), 1u);
        const ESM4::Reference& marker = records[0];
        EXPECT_TRUE(marker.mAudioMarker.mIsMarker);
        EXPECT_EQ(marker.mAudioLocation.toUint32(), 0x0015D020u);
        EXPECT_FLOAT_EQ(marker.mAudioMarker.mRadius, 10000.f);
        EXPECT_FLOAT_EQ(marker.mAudioMarker.mLayer2Trigger, 0.33f);
        EXPECT_FLOAT_EQ(marker.mAudioMarker.mLayer3Trigger, 0.67f);
        EXPECT_EQ(marker.mAudioMarker.mFlags, 1u);
    }

    TEST(ESM4MusicRecordTest, aReferenceWithoutMmrkIsNoAudioMarker)
    {
        const std::string data = valueSubRecord("NAME", std::uint32_t(0x23)) + valueSubRecord<float>("XRDS", 256.f);
        const std::vector<ESM4::Reference> records = loadRecords<ESM4::Reference>("REFR", record("REFR", 0x2001, data));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_FALSE(records[0].mAudioMarker.mIsMarker);
    }
}
