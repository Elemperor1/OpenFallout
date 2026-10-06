#include <components/esm4/loadimgs.hpp>

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
        return zString("EDID", "Text of EDID") + subRecord("DNAM", bytePattern(132, 12));
    }

    void expectEverySubRecord(const ESM4::ImageSpace& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(132, 12));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::ImageSpace>("IMGS", record("IMGS", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ImageSpaceTest, readsEverySubrecord)
    {
        const std::vector<ESM4::ImageSpace> records
            = loadRecords<ESM4::ImageSpace>("IMGS", record("IMGS", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImageSpaceTest, readsACompressedRecord)
    {
        const std::vector<ESM4::ImageSpace> records
            = loadRecords<ESM4::ImageSpace>("IMGS", compressedRecord("IMGS", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImageSpaceTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(153, 'x'))), "ESM4::IMGS::load - DNAM has an unexpected size");
    }

    TEST(ESM4ImageSpaceTest, readsDataThatEndsAfterAnyMember)
    {
        for (const std::size_t size : { 132u, 136u, 140u, 144u, 148u, 149u, 152u })
        {
            const std::vector<ESM4::ImageSpace> records
                = loadRecords<ESM4::ImageSpace>("IMGS", record("IMGS", 1, subRecord("DNAM", bytePattern(size, 1))));
            ASSERT_EQ(records.size(), 1u) << size;
            EXPECT_EQ(records[0].mData.size(), size);
        }
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(131, 'x'))), "ESM4::IMGS::load - DNAM has an unexpected size");
    }

    TEST(ESM4ImageSpaceTest, rejectsDataThatEndsInsideAMember)
    {
        // The flags at the 149th byte are followed by three unused bytes, which are one member.
        for (const std::size_t size : { 133u, 135u, 147u, 150u, 151u })
            EXPECT_EQ(loadFailure(subRecord("DNAM", std::string(size, 'x'))),
                "ESM4::IMGS::load - DNAM has an unexpected size")
                << size;
    }

    TEST(ESM4ImageSpaceTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::IMGS::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ImageSpaceTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::IMGS::load - record has unread bytes after its last sub-record");
    }

}
