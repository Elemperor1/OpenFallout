#include <components/esm4/loadclmt.hpp>

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

    std::string dataWeathers(int offset)
    {
        std::string data;
        append<ESM::FormId32>(data, 0x00010000u + offset + 0);
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 1)));
        append<ESM::FormId32>(data, 0x00010000u + offset + 2);
        return data;
    }

    std::string dataTiming(int offset)
    {
        std::string data;
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 0) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 1) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 2) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 3) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 4) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 5) % 50));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("WLST", dataWeathers(2) + dataWeathers(12))
            + zString("FNAM", "Text of FNAM") + zString("GNAM", "Text of GNAM") + zString("MODL", "Text of MODL")
            + subRecord("TNAM", dataTiming(6));
    }

    void expectEverySubRecord(const ESM4::Climate& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        ASSERT_EQ(result.mWeathers.size(), 2u);
        EXPECT_EQ(result.mWeathers[0].mWeather, 0x00010002u);
        EXPECT_EQ(result.mWeathers[0].mChance, -100003);
        EXPECT_EQ(result.mWeathers[0].mGlobal, 0x00010004u);
        EXPECT_EQ(result.mWeathers[1].mWeather, 0x0001000cu);
        EXPECT_EQ(result.mWeathers[1].mChance, -100013);
        EXPECT_EQ(result.mWeathers[1].mGlobal, 0x0001000eu);
        EXPECT_EQ(result.mSunTexture, "Text of FNAM");
        EXPECT_EQ(result.mSunGlareTexture, "Text of GNAM");
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(result.mTiming.mSunriseBegin, 9);
        EXPECT_EQ(result.mTiming.mSunriseEnd, 10);
        EXPECT_EQ(result.mTiming.mSunsetBegin, 11);
        EXPECT_EQ(result.mTiming.mSunsetEnd, 12);
        EXPECT_EQ(result.mTiming.mVolatility, 13);
        EXPECT_EQ(result.mTiming.mMoonPhaseLength, 14);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Climate>("CLMT", record("CLMT", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ClimateTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Climate> records
            = loadRecords<ESM4::Climate>("CLMT", record("CLMT", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ClimateTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Climate> records
            = loadRecords<ESM4::Climate>("CLMT", compressedRecord("CLMT", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ClimateTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("WLST", std::string(13, 'x'))), "ESM4::CLMT::load - WLST has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TNAM", std::string(5, 'x'))), "ESM4::CLMT::load - TNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TNAM", std::string(7, 'x'))), "ESM4::CLMT::load - TNAM has an unexpected size");
    }

    TEST(ESM4ClimateTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CLMT::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ClimateTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CLMT::load - record has unread bytes after its last sub-record");
    }

}
