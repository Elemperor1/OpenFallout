#include <components/esm4/loadcsty.hpp>

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
        return zString("EDID", "Text of EDID") + subRecord("CSTD", bytePattern(92, 12))
            + subRecord("CSAD", bytePattern(84, 13)) + subRecord("CSSD", bytePattern(64, 14));
    }

    void expectEverySubRecord(const ESM4::CombatStyle& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(std::string(result.mStandard.begin(), result.mStandard.end()), bytePattern(92, 12));
        EXPECT_EQ(std::string(result.mAdvanced.begin(), result.mAdvanced.end()), bytePattern(84, 13));
        EXPECT_EQ(std::string(result.mSimple.begin(), result.mSimple.end()), bytePattern(64, 14));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::CombatStyle>("CSTY", record("CSTY", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CombatStyleTest, readsEverySubrecord)
    {
        const std::vector<ESM4::CombatStyle> records
            = loadRecords<ESM4::CombatStyle>("CSTY", record("CSTY", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CombatStyleTest, readsACompressedRecord)
    {
        const std::vector<ESM4::CombatStyle> records
            = loadRecords<ESM4::CombatStyle>("CSTY", compressedRecord("CSTY", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CombatStyleTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("CSTD", std::string(91, 'x'))), "ESM4::CSTY::load - CSTD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CSTD", std::string(93, 'x'))), "ESM4::CSTY::load - CSTD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CSAD", std::string(83, 'x'))), "ESM4::CSTY::load - CSAD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CSAD", std::string(85, 'x'))), "ESM4::CSTY::load - CSAD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CSSD", std::string(63, 'x'))), "ESM4::CSTY::load - CSSD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CSSD", std::string(65, 'x'))), "ESM4::CSTY::load - CSSD has an unexpected size");
    }

    TEST(ESM4CombatStyleTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CSTY::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CombatStyleTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CSTY::load - record has unread bytes after its last sub-record");
    }

}
