#include <components/esm4/loadwatr.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string dataRelatedWaters(int offset)
    {
        std::string data;
        append<ESM::FormId32>(data, 0x00010000u + offset + 0);
        append<ESM::FormId32>(data, 0x00010000u + offset + 1);
        append<ESM::FormId32>(data, 0x00010000u + offset + 2);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + zString("NNAM", "Text of NNAM")
            + valueSubRecord<std::uint8_t>("ANAM", 7) + valueSubRecord<std::uint8_t>("FNAM", 8)
            + valueSubRecord<std::uint8_t>("MNAM", 9) + valueSubRecord<std::uint32_t>("SNAM", 0x00010007)
            + valueSubRecord<std::uint32_t>("XNAM", 0x00010008) + subRecord("DATA", bytePattern(2, 19))
            + subRecord("DNAM", bytePattern(184, 20)) + subRecord("GNAM", dataRelatedWaters(11));
    }

    void expectEverySubRecord(const ESM4::Water& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mNoiseTexture, "Text of NNAM");
        EXPECT_EQ(result.mOpacity, 7);
        EXPECT_EQ(result.mWaterFlags, 8);
        EXPECT_THAT(result.mMnam, ElementsAre(9));
        EXPECT_EQ(result.mSound.toUint32(), 0x00010007u);
        EXPECT_EQ(result.mActorEffect.toUint32(), 0x00010008u);
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(2, 19));
        EXPECT_EQ(std::string(result.mVisualData.begin(), result.mVisualData.end()), bytePattern(184, 20));
        EXPECT_EQ(result.mRelatedWaters.mDaytime, 0x0001000bu);
        EXPECT_EQ(result.mRelatedWaters.mNighttime, 0x0001000cu);
        EXPECT_EQ(result.mRelatedWaters.mUnderwater, 0x0001000du);
    }

    // The first 52 bytes of the settings of the clean water of New Vegas, then bytes that no member of them reads
    std::string cleanWaterSettings(std::size_t size, float reflectivity = 0.6f)
    {
        std::string data(16, '\0');
        append<float>(data, 826.f); // sun power
        append<float>(data, reflectivity);
        append<float>(data, 0.75f); // fresnel
        data += std::string(4, '\0');
        append<float>(data, 100.f); // fog near
        append<float>(data, 1000.f); // fog far
        data += std::string("\x34\x4b\x42\xff", 4); // shallow 52, 75, 66
        data += std::string("\x16\x29\x22\xff", 4); // deep 22, 41, 34
        data += std::string("\x15\x20\x17\xff", 4); // reflection 21, 32, 23
        data += bytePattern(size - data.size(), 3);
        return data;
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Water>("WATR", record("WATR", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4WaterTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Water> records
            = loadRecords<ESM4::Water>("WATR", record("WATR", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4WaterTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Water> records
            = loadRecords<ESM4::Water>("WATR", compressedRecord("WATR", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4WaterTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("ANAM", std::string(0, 'x'))), "ESM4::WATR::load - ANAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ANAM", std::string(2, 'x'))), "ESM4::WATR::load - ANAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("FNAM", std::string(0, 'x'))), "ESM4::WATR::load - FNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("FNAM", std::string(2, 'x'))), "ESM4::WATR::load - FNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(3, 'x'))), "ESM4::WATR::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(5, 'x'))), "ESM4::WATR::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("XNAM", std::string(3, 'x'))), "ESM4::WATR::load - XNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("XNAM", std::string(5, 'x'))), "ESM4::WATR::load - XNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(187, 'x'))), "ESM4::WATR::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(197, 'x'))), "ESM4::WATR::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("GNAM", std::string(11, 'x'))), "ESM4::WATR::load - GNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("GNAM", std::string(13, 'x'))), "ESM4::WATR::load - GNAM has an unexpected size");
    }

    TEST(ESM4WaterTest, readsAMaterialIdOfAnySizeAndVisualDataThatEndsAfterAnyMember)
    {
        // The format reference calls MNAM a string, and lets DNAM end after any member from the 184th byte on.
        for (const std::size_t size : { 184u, 188u, 192u, 196u })
        {
            const std::vector<ESM4::Water> result = loadRecords<ESM4::Water>(
                "WATR", record("WATR", 1, zString("MNAM", "Material") + subRecord("DNAM", bytePattern(size, 1))));
            ASSERT_EQ(result.size(), 1u) << size;
            EXPECT_EQ(result[0].mMnam.size(), 9u);
            EXPECT_EQ(result[0].mVisualData.size(), size);
        }
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(183, 'x'))), "ESM4::WATR::load - DNAM has an unexpected size");
        EXPECT_TRUE(loadFailure(subRecord("MNAM", "")).empty());
    }

    TEST(ESM4WaterTest, rejectsVisualDataThatEndsInsideAMember)
    {
        for (const std::size_t size : { 185u, 186u, 187u, 193u, 195u })
            EXPECT_EQ(loadFailure(subRecord("DNAM", std::string(size, 'x'))),
                "ESM4::WATR::load - DNAM has an unexpected size")
                << size;
    }

    void expectCleanWater(const std::optional<ESM4::Water::Appearance>& appearance)
    {
        ASSERT_TRUE(appearance.has_value());
        EXPECT_EQ(appearance->mShallow.mRed, 52);
        EXPECT_EQ(appearance->mShallow.mGreen, 75);
        EXPECT_EQ(appearance->mShallow.mBlue, 66);
        EXPECT_EQ(appearance->mDeep.mRed, 22);
        EXPECT_EQ(appearance->mDeep.mGreen, 41);
        EXPECT_EQ(appearance->mDeep.mBlue, 34);
        EXPECT_EQ(appearance->mReflection.mRed, 21);
        EXPECT_EQ(appearance->mReflection.mGreen, 32);
        EXPECT_EQ(appearance->mReflection.mBlue, 23);
        EXPECT_FLOAT_EQ(appearance->mSunPower, 826.f);
        EXPECT_FLOAT_EQ(appearance->mReflectivity, 0.6f);
        EXPECT_FLOAT_EQ(appearance->mFresnel, 0.75f);
    }

    std::vector<ESM4::Water> loadWater(const std::string& subRecords)
    {
        return loadRecords<ESM4::Water>("WATR", record("WATR", 1, subRecords));
    }

    TEST(ESM4WaterTest, readsTheAppearanceFromTheVisualData)
    {
        for (const std::size_t size : { 184u, 196u })
        {
            const std::vector<ESM4::Water> result
                = loadWater(subRecord("DATA", bytePattern(2, 1)) + subRecord("DNAM", cleanWaterSettings(size)));
            ASSERT_EQ(result.size(), 1u) << size;
            expectCleanWater(result[0].appearance());
        }
    }

    TEST(ESM4WaterTest, readsTheAppearanceFromTheLongFormOfDataWhenThereIsNoVisualData)
    {
        const std::vector<ESM4::Water> result = loadWater(subRecord("DATA", cleanWaterSettings(186)));
        ASSERT_EQ(result.size(), 1u);
        expectCleanWater(result[0].appearance());
    }

    TEST(ESM4WaterTest, hasNoAppearanceWithoutSettings)
    {
        EXPECT_FALSE(loadWater(zString("EDID", "x"))[0].appearance().has_value());
        // The short form of DATA is the damage only
        EXPECT_FALSE(loadWater(subRecord("DATA", bytePattern(2, 1)))[0].appearance().has_value());
    }

    TEST(ESM4WaterTest, hasNoAppearanceWhenANumberIsNotFinite)
    {
        const float infinity = std::numeric_limits<float>::infinity();
        const float notANumber = std::numeric_limits<float>::quiet_NaN();
        for (const float reflectivity : { infinity, -infinity, notANumber })
            EXPECT_FALSE(
                loadWater(subRecord("DNAM", cleanWaterSettings(196, reflectivity)))[0].appearance().has_value())
                << reflectivity;
    }

    TEST(ESM4WaterTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::WATR::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4WaterTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::WATR::load - record has unread bytes after its last sub-record");
    }

}
