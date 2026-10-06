#include <components/esm4/loadlscr.hpp>

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

    std::string dataLocations(int offset)
    {
        std::string data;
        append<ESM::FormId32>(data, 0x00010000u + offset + 0);
        append<ESM::FormId32>(data, 0x00010000u + offset + 1);
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 2)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 3)));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("ICON", "Text of ICON") + zString("DESC", "Text of DESC")
            + subRecord("LNAM", dataLocations(4)) + subRecord("LNAM", dataLocations(14))
            + valueSubRecord<std::uint32_t>("WMI1", 0x00010005);
    }

    void expectEverySubRecord(const ESM4::LoadScreen& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mIcon, "Text of ICON");
        EXPECT_EQ(result.mDescription, "Text of DESC");
        ASSERT_EQ(result.mLocations.size(), 2u);
        EXPECT_EQ(result.mLocations[0].mDirect, 0x00010004u);
        EXPECT_EQ(result.mLocations[0].mIndirect, 0x00010005u);
        EXPECT_EQ(result.mLocations[0].mGridY, -106);
        EXPECT_EQ(result.mLocations[0].mGridX, -107);
        EXPECT_EQ(result.mLocations[1].mDirect, 0x0001000eu);
        EXPECT_EQ(result.mLocations[1].mIndirect, 0x0001000fu);
        EXPECT_EQ(result.mLocations[1].mGridY, -116);
        EXPECT_EQ(result.mLocations[1].mGridX, -117);
        EXPECT_EQ(result.mLoadScreenType.toUint32(), 0x00010005u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::LoadScreen>("LSCR", record("LSCR", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4LoadScreenTest, readsEverySubrecord)
    {
        const std::vector<ESM4::LoadScreen> records
            = loadRecords<ESM4::LoadScreen>("LSCR", record("LSCR", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4LoadScreenTest, readsACompressedRecord)
    {
        const std::vector<ESM4::LoadScreen> records
            = loadRecords<ESM4::LoadScreen>("LSCR", compressedRecord("LSCR", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4LoadScreenTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("LNAM", std::string(11, 'x'))), "ESM4::LSCR::load - LNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("LNAM", std::string(13, 'x'))), "ESM4::LSCR::load - LNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("WMI1", std::string(3, 'x'))), "ESM4::LSCR::load - WMI1 has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("WMI1", std::string(5, 'x'))), "ESM4::LSCR::load - WMI1 has an unexpected size");
    }

    TEST(ESM4LoadScreenTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::LSCR::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4LoadScreenTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::LSCR::load - record has unread bytes after its last sub-record");
    }

}
