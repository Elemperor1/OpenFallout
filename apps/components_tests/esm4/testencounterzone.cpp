#include <components/esm4/loadeczn.hpp>

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

    std::string dataData(int offset)
    {
        std::string data;
        append<ESM::FormId32>(data, 0x00010000u + offset + 0);
        append<std::int8_t>(data, static_cast<std::int8_t>(3 + (offset + 1) % 50));
        append<std::int8_t>(data, static_cast<std::int8_t>(3 + (offset + 2) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 3) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 4) % 50));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("DATA", dataData(2));
    }

    void expectEverySubRecord(const ESM4::EncounterZone& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mData.mOwner, 0x00010002u);
        EXPECT_EQ(result.mData.mRank, 6);
        EXPECT_EQ(result.mData.mMinimumLevel, 7);
        EXPECT_EQ(result.mData.mFlags, 8);
        EXPECT_EQ(result.mData.mUnused, 9);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::EncounterZone>("ECZN", record("ECZN", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4EncounterZoneTest, readsEverySubrecord)
    {
        const std::vector<ESM4::EncounterZone> records
            = loadRecords<ESM4::EncounterZone>("ECZN", record("ECZN", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4EncounterZoneTest, readsACompressedRecord)
    {
        const std::vector<ESM4::EncounterZone> records
            = loadRecords<ESM4::EncounterZone>("ECZN", compressedRecord("ECZN", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4EncounterZoneTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(7, 'x'))), "ESM4::ECZN::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(9, 'x'))), "ESM4::ECZN::load - DATA has an unexpected size");
    }

    TEST(ESM4EncounterZoneTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::ECZN::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4EncounterZoneTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::ECZN::load - record has unread bytes after its last sub-record");
    }

}
