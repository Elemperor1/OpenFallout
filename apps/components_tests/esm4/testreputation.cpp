#include <components/esm4/loadrepu.hpp>

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

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + zString("ICON", "Text of ICON")
            + subRecord("DATA", bytePattern(4, 14));
    }

    void expectEverySubRecord(const ESM4::Reputation& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mIcon, "Text of ICON");
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(4, 14));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Reputation>("REPU", record("REPU", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ReputationTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Reputation> records
            = loadRecords<ESM4::Reputation>("REPU", record("REPU", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ReputationTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Reputation> records
            = loadRecords<ESM4::Reputation>("REPU", compressedRecord("REPU", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ReputationTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(3, 'x'))), "ESM4::REPU::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(5, 'x'))), "ESM4::REPU::load - DATA has an unexpected size");
    }

    TEST(ESM4ReputationTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::REPU::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ReputationTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::REPU::load - record has unread bytes after its last sub-record");
    }

}
