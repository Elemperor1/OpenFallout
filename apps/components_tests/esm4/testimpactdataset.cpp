#include <components/esm4/loadipds.hpp>

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
        return zString("EDID", "Text of EDID")
            + subRecord("DATA", std::string("\x02\x00\x01\x00\x03\x00\x01\x00\x04\x00\x01\x00", 12));
    }

    void expectEverySubRecord(const ESM4::ImpactDataSet& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        ASSERT_EQ(result.mImpacts.size(), 3u);
        EXPECT_EQ(result.mImpacts[0].toUint32(), 0x00010002u);
        EXPECT_EQ(result.mImpacts[1].toUint32(), 0x00010003u);
        EXPECT_EQ(result.mImpacts[2].toUint32(), 0x00010004u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::ImpactDataSet>("IPDS", record("IPDS", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ImpactDataSetTest, readsEverySubrecord)
    {
        const std::vector<ESM4::ImpactDataSet> records
            = loadRecords<ESM4::ImpactDataSet>("IPDS", record("IPDS", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImpactDataSetTest, readsACompressedRecord)
    {
        const std::vector<ESM4::ImpactDataSet> records
            = loadRecords<ESM4::ImpactDataSet>("IPDS", compressedRecord("IPDS", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImpactDataSetTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(5, 'x'))), "ESM4::IPDS::load - DATA has an unexpected size");
    }

    TEST(ESM4ImpactDataSetTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::IPDS::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ImpactDataSetTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::IPDS::load - record has unread bytes after its last sub-record");
    }

}
