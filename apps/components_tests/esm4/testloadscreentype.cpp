#include <components/esm4/loadlsct.hpp>

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
        return zString("EDID", "Text of EDID") + subRecord("DATA", bytePattern(88, 12));
    }

    void expectEverySubRecord(const ESM4::LoadScreenType& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(88, 12));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::LoadScreenType>("LSCT", record("LSCT", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4LoadScreenTypeTest, readsEverySubrecord)
    {
        const std::vector<ESM4::LoadScreenType> records
            = loadRecords<ESM4::LoadScreenType>("LSCT", record("LSCT", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4LoadScreenTypeTest, readsACompressedRecord)
    {
        const std::vector<ESM4::LoadScreenType> records
            = loadRecords<ESM4::LoadScreenType>("LSCT", compressedRecord("LSCT", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4LoadScreenTypeTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(87, 'x'))), "ESM4::LSCT::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(89, 'x'))), "ESM4::LSCT::load - DATA has an unexpected size");
    }

    TEST(ESM4LoadScreenTypeTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::LSCT::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4LoadScreenTypeTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::LSCT::load - record has unread bytes after its last sub-record");
    }

}
