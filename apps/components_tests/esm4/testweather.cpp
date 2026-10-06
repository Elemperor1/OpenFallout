#include <components/esm4/loadwthr.hpp>

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

    std::string dataFog(int offset)
    {
        std::string data;
        append<float>(data, static_cast<float>(offset + 0) + 0.5f);
        append<float>(data, static_cast<float>(offset + 1) + 0.5f);
        append<float>(data, static_cast<float>(offset + 2) + 0.5f);
        append<float>(data, static_cast<float>(offset + 3) + 0.5f);
        append<float>(data, static_cast<float>(offset + 4) + 0.5f);
        append<float>(data, static_cast<float>(offset + 5) + 0.5f);
        return data;
    }

    std::string dataData(int offset)
    {
        std::string data;
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 0) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 1) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 2) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 3) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 4) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 5) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 6) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 7) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 8) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 9) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 10) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 11) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 12) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 13) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 14) % 50));
        return data;
    }

    std::string dataSounds(int offset)
    {
        std::string data;
        append<ESM::FormId32>(data, 0x00010000u + offset + 0);
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 1)));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + valueSubRecord<std::uint32_t>(std::string("\000IAD", 4), 0x00010002)
            + valueSubRecord<std::uint32_t>(std::string("\001IAD", 4), 0x00010003)
            + valueSubRecord<std::uint32_t>(std::string("\002IAD", 4), 0x00010004)
            + valueSubRecord<std::uint32_t>(std::string("\003IAD", 4), 0x00010005)
            + valueSubRecord<std::uint32_t>(std::string("\004IAD", 4), 0x00010006)
            + valueSubRecord<std::uint32_t>(std::string("\005IAD", 4), 0x00010007) + zString("DNAM", "Text of DNAM")
            + zString("CNAM", "Text of CNAM") + zString("ANAM", "Text of ANAM") + zString("BNAM", "Text of BNAM")
            + valueSubRecord<std::uint32_t>("LNAM", 100004) + subRecord("ONAM", bytePattern(4, 15))
            + subRecord("PNAM", bytePattern(64, 16)) + subRecord("NAM0", bytePattern(160, 17))
            + subRecord("FNAM", dataFog(8)) + subRecord("INAM", bytePattern(304, 19)) + subRecord("DATA", dataData(10))
            + subRecord("SNAM", dataSounds(11)) + subRecord("SNAM", dataSounds(21));
    }

    void expectEverySubRecord(const ESM4::Weather& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mImageSpaceModifiers[0].toUint32(), 0x00010002u);
        EXPECT_EQ(result.mImageSpaceModifiers[1].toUint32(), 0x00010003u);
        EXPECT_EQ(result.mImageSpaceModifiers[2].toUint32(), 0x00010004u);
        EXPECT_EQ(result.mImageSpaceModifiers[3].toUint32(), 0x00010005u);
        EXPECT_EQ(result.mImageSpaceModifiers[4].toUint32(), 0x00010006u);
        EXPECT_EQ(result.mImageSpaceModifiers[5].toUint32(), 0x00010007u);
        EXPECT_EQ(result.mCloudTextures[0], "Text of DNAM");
        EXPECT_EQ(result.mCloudTextures[1], "Text of CNAM");
        EXPECT_EQ(result.mCloudTextures[2], "Text of ANAM");
        EXPECT_EQ(result.mCloudTextures[3], "Text of BNAM");
        EXPECT_EQ(result.mLnam, 100004u);
        EXPECT_EQ(std::string(result.mCloudSpeeds.begin(), result.mCloudSpeeds.end()), bytePattern(4, 15));
        EXPECT_EQ(std::string(result.mCloudColours.begin(), result.mCloudColours.end()), bytePattern(64, 16));
        EXPECT_EQ(std::string(result.mColours.begin(), result.mColours.end()), bytePattern(160, 17));
        EXPECT_EQ(result.mFog.mDayNear, 8.5f);
        EXPECT_EQ(result.mFog.mDayFar, 9.5f);
        EXPECT_EQ(result.mFog.mNightNear, 10.5f);
        EXPECT_EQ(result.mFog.mNightFar, 11.5f);
        EXPECT_EQ(result.mFog.mDayPower, 12.5f);
        EXPECT_EQ(result.mFog.mNightPower, 13.5f);
        EXPECT_EQ(std::string(result.mInam.begin(), result.mInam.end()), bytePattern(304, 19));
        EXPECT_EQ(result.mData.mWindSpeed, 13);
        EXPECT_EQ(result.mData.mCloudSpeedLower, 14);
        EXPECT_EQ(result.mData.mCloudSpeedUpper, 15);
        EXPECT_EQ(result.mData.mTransDelta, 16);
        EXPECT_EQ(result.mData.mSunGlare, 17);
        EXPECT_EQ(result.mData.mSunDamage, 18);
        EXPECT_EQ(result.mData.mPrecipitationBeginFadeIn, 19);
        EXPECT_EQ(result.mData.mPrecipitationEndFadeOut, 20);
        EXPECT_EQ(result.mData.mThunderBeginFadeIn, 21);
        EXPECT_EQ(result.mData.mThunderEndFadeOut, 22);
        EXPECT_EQ(result.mData.mThunderFrequency, 23);
        EXPECT_EQ(result.mData.mClassification, 24);
        EXPECT_EQ(result.mData.mLightningRed, 25);
        EXPECT_EQ(result.mData.mLightningGreen, 26);
        EXPECT_EQ(result.mData.mLightningBlue, 27);
        ASSERT_EQ(result.mSounds.size(), 2u);
        EXPECT_EQ(result.mSounds[0].mSound, 0x0001000bu);
        EXPECT_EQ(result.mSounds[0].mType, 100012u);
        EXPECT_EQ(result.mSounds[1].mSound, 0x00010015u);
        EXPECT_EQ(result.mSounds[1].mType, 100022u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Weather>("WTHR", record("WTHR", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4WeatherTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Weather> records
            = loadRecords<ESM4::Weather>("WTHR", record("WTHR", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4WeatherTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Weather> records
            = loadRecords<ESM4::Weather>("WTHR", compressedRecord("WTHR", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4WeatherTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(loadFailure(subRecord(std::string("\000IAD", 4), std::string(3, 'x'))),
            "ESM4::WTHR::load - \\x00IAD has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord(std::string("\001IAD", 4), std::string(3, 'x'))),
            "ESM4::WTHR::load - \\x01IAD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("LNAM", std::string(3, 'x'))), "ESM4::WTHR::load - LNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("LNAM", std::string(5, 'x'))), "ESM4::WTHR::load - LNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ONAM", std::string(3, 'x'))), "ESM4::WTHR::load - ONAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ONAM", std::string(5, 'x'))), "ESM4::WTHR::load - ONAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("PNAM", std::string(97, 'x'))), "ESM4::WTHR::load - PNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("NAM0", std::string(241, 'x'))), "ESM4::WTHR::load - NAM0 has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("FNAM", std::string(23, 'x'))), "ESM4::WTHR::load - FNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("FNAM", std::string(25, 'x'))), "ESM4::WTHR::load - FNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("INAM", std::string(303, 'x'))), "ESM4::WTHR::load - INAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("INAM", std::string(305, 'x'))), "ESM4::WTHR::load - INAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(14, 'x'))), "ESM4::WTHR::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(16, 'x'))), "ESM4::WTHR::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(7, 'x'))), "ESM4::WTHR::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(9, 'x'))), "ESM4::WTHR::load - SNAM has an unexpected size");
    }

    TEST(ESM4WeatherTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::WTHR::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4WeatherTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::WTHR::load - record has unread bytes after its last sub-record");
    }

}
