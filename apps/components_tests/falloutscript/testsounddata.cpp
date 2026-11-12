#include <components/falloutscript/sounddata.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
    using FalloutScript::SoundData;

    // The record of FSTDirtWalkL in FalloutNV.esm: 40 and 16 (200 units and 1600), frequency 3, the random shift, the
    // static attenuation 1938
    std::vector<unsigned char> dirtWalk()
    {
        std::vector<unsigned char> bytes(36, 0);
        bytes[0] = 40;
        bytes[1] = 16;
        bytes[2] = 3;
        bytes[4] = 0x01;
        bytes[5] = 0x10;
        bytes[8] = 1938 & 0xff;
        bytes[9] = 1938 >> 8;
        const short curve[5] = { 100, 50, 20, 5, 0 };
        for (int i = 0; i < 5; ++i)
        {
            bytes[12 + 2 * i] = static_cast<unsigned char>(curve[i] & 0xff);
            bytes[13 + 2 * i] = static_cast<unsigned char>(curve[i] >> 8);
        }
        bytes[24] = 255;
        return bytes;
    }

    TEST(FalloutScriptSoundDataTest, the_settings_are_read_from_the_record)
    {
        const std::vector<unsigned char> bytes = dirtWalk();
        const SoundData data = FalloutScript::soundDataFromBytes(bytes.data(), bytes.size());
        EXPECT_EQ(data.mMinAttenuation, 40);
        EXPECT_EQ(data.mMaxAttenuation, 16);
        EXPECT_EQ(data.mFrequencyAdjustment, 3);
        EXPECT_EQ(data.mFlags, 0x1001);
        EXPECT_EQ(data.mStaticAttenuation, 1938);
        EXPECT_EQ(data.mPriority, 255);
        EXPECT_FLOAT_EQ(data.minDistance(), 200.f);
        EXPECT_FLOAT_EQ(data.maxDistance(), 1600.f);
    }

    TEST(FalloutScriptSoundDataTest, a_short_record_keeps_the_defaults)
    {
        const unsigned char bytes[4] = { 1, 2, 3, 4 };
        const SoundData data = FalloutScript::soundDataFromBytes(bytes, 4);
        EXPECT_EQ(data.mMaxAttenuation, 0);
        EXPECT_EQ(data.mCurve[0], 100);
        EXPECT_EQ(data.mCurve[4], 0);
    }

    TEST(FalloutScriptSoundDataTest, a_record_of_twelve_bytes_has_the_default_curve)
    {
        std::vector<unsigned char> bytes = dirtWalk();
        const SoundData data = FalloutScript::soundDataFromBytes(bytes.data(), 12);
        EXPECT_EQ(data.mCurve[1], 50);
        EXPECT_EQ(data.mMaxAttenuation, 16);
    }

    TEST(FalloutScriptSoundDataTest, the_static_attenuation_is_hundredths_of_a_decibel)
    {
        SoundData data;
        EXPECT_FLOAT_EQ(data.staticGain(), 1.f);
        data.mStaticAttenuation = 2000;
        EXPECT_NEAR(data.staticGain(), 0.1f, 1e-5f);
        data.mStaticAttenuation = 600;
        EXPECT_NEAR(data.staticGain(), std::pow(10.f, -0.3f), 1e-5f);
    }

    TEST(FalloutScriptSoundDataTest, the_gain_is_full_inside_the_minimum_and_none_past_the_maximum)
    {
        const std::vector<unsigned char> bytes = dirtWalk();
        const SoundData data = FalloutScript::soundDataFromBytes(bytes.data(), bytes.size());
        EXPECT_FLOAT_EQ(data.distanceGain(0.f), 1.f);
        EXPECT_FLOAT_EQ(data.distanceGain(200.f), 1.f);
        EXPECT_FLOAT_EQ(data.distanceGain(1600.f), 0.f);
        EXPECT_FLOAT_EQ(data.distanceGain(5000.f), 0.f);
    }

    TEST(FalloutScriptSoundDataTest, the_curve_runs_between_the_five_points)
    {
        const std::vector<unsigned char> bytes = dirtWalk();
        const SoundData data = FalloutScript::soundDataFromBytes(bytes.data(), bytes.size());
        // The points are at 200, 550, 900, 1250 and 1600
        EXPECT_NEAR(data.distanceGain(550.f), 0.5f, 1e-5f);
        EXPECT_NEAR(data.distanceGain(900.f), 0.2f, 1e-5f);
        EXPECT_NEAR(data.distanceGain(1250.f), 0.05f, 1e-5f);
        // halfway between the first two
        EXPECT_NEAR(data.distanceGain(375.f), 0.75f, 1e-5f);
    }

    TEST(FalloutScriptSoundDataTest, a_sound_with_no_maximum_or_a_flat_one_has_no_distance)
    {
        SoundData data;
        data.mMinAttenuation = 20;
        EXPECT_FLOAT_EQ(data.distanceGain(100000.f), 1.f);
        data.mMaxAttenuation = 10;
        EXPECT_FLOAT_EQ(data.distanceGain(100000.f), 0.f);
        data.mFlags = SoundData::TwoDimensional;
        EXPECT_FLOAT_EQ(data.distanceGain(100000.f), 1.f);
        data.mFlags = SoundData::MenuSound;
        EXPECT_FLOAT_EQ(data.distanceGain(100000.f), 1.f);
    }

    TEST(FalloutScriptSoundDataTest, the_pitch_is_the_adjustment_or_a_shift_within_it)
    {
        SoundData data;
        data.mFrequencyAdjustment = 3;
        EXPECT_NEAR(data.pitch(0.f), 1.03f, 1e-5f);
        EXPECT_NEAR(data.pitch(1.f), 1.03f, 1e-5f);
        data.mFlags = SoundData::RandomFrequencyShift;
        EXPECT_NEAR(data.pitch(0.f), 0.97f, 1e-5f);
        EXPECT_NEAR(data.pitch(0.5f), 1.f, 1e-5f);
        EXPECT_NEAR(data.pitch(1.f), 1.03f, 1e-5f);
        data.mFrequencyAdjustment = -4;
        EXPECT_NEAR(data.pitch(0.f), 0.96f, 1e-5f);
        EXPECT_NEAR(data.pitch(1.f), 1.04f, 1e-5f);
    }

    TEST(FalloutScriptSoundDataTest, the_flags_say_two_dimensional_and_looping)
    {
        SoundData data;
        EXPECT_FALSE(data.isLooping());
        EXPECT_FALSE(data.isTwoDimensional());
        data.mFlags = SoundData::Loop | SoundData::ThreeSixtyLfe;
        EXPECT_TRUE(data.isLooping());
        EXPECT_FALSE(data.isTwoDimensional());
        data.mFlags = SoundData::TwoDimensional;
        EXPECT_TRUE(data.isTwoDimensional());
    }
}
