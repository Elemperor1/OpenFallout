#include <components/esm4/loadcmny.hpp>

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

    std::string dataBounds(int offset)
    {
        std::string data;
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 0)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 1)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 2)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 3)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 4)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 5)));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("OBND", dataBounds(2)) + zString("FULL", "Text of FULL")
            + zString("MODL", "Text of MODL") + zString("ICON", "Text of ICON") + zString("MICO", "Text of MICO")
            + valueSubRecord<std::uint32_t>("DATA", 100007);
    }

    void expectEverySubRecord(const ESM4::CaravanMoney& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mBounds.mX1, -102);
        EXPECT_EQ(result.mBounds.mY1, -103);
        EXPECT_EQ(result.mBounds.mZ1, -104);
        EXPECT_EQ(result.mBounds.mX2, -105);
        EXPECT_EQ(result.mBounds.mY2, -106);
        EXPECT_EQ(result.mBounds.mZ2, -107);
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(result.mIcon, "Text of ICON");
        EXPECT_EQ(result.mSmallIcon, "Text of MICO");
        EXPECT_EQ(result.mAbsoluteValue, 100007u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::CaravanMoney>("CMNY", record("CMNY", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CaravanMoneyTest, readsEverySubrecord)
    {
        const std::vector<ESM4::CaravanMoney> records
            = loadRecords<ESM4::CaravanMoney>("CMNY", record("CMNY", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CaravanMoneyTest, readsACompressedRecord)
    {
        const std::vector<ESM4::CaravanMoney> records
            = loadRecords<ESM4::CaravanMoney>("CMNY", compressedRecord("CMNY", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CaravanMoneyTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(11, 'x'))), "ESM4::CMNY::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(13, 'x'))), "ESM4::CMNY::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(3, 'x'))), "ESM4::CMNY::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(5, 'x'))), "ESM4::CMNY::load - DATA has an unexpected size");
    }

    TEST(ESM4CaravanMoneyTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CMNY::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CaravanMoneyTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CMNY::load - record has unread bytes after its last sub-record");
    }

}
