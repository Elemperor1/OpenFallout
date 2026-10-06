#include <components/esm4/loadchal.hpp>

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

    std::string challengeData()
    {
        std::string data;
        append<std::uint32_t>(data, 11); // type
        append<std::uint32_t>(data, 500); // threshold
        append<std::uint32_t>(data, 6); // flags
        append<std::uint32_t>(data, 30); // interval
        data += bytePattern(8, 15);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL")
            + valueSubRecord<std::uint32_t>("SCRI", 0x00010003) + zString("DESC", "Text of DESC")
            + subRecord("DATA", challengeData()) + valueSubRecord<std::uint32_t>("SNAM", 0x00010006)
            + valueSubRecord<std::uint32_t>("XNAM", 0x00010007);
    }

    void expectEverySubRecord(const ESM4::Challenge& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mScript.toUint32(), 0x00010003u);
        EXPECT_EQ(result.mDescription, "Text of DESC");
        EXPECT_EQ(result.mData.mType, 11u);
        EXPECT_EQ(result.mData.mThreshold, 500u);
        EXPECT_EQ(result.mData.mFlags, 6u);
        EXPECT_EQ(result.mData.mInterval, 30u);
        EXPECT_EQ(std::string(result.mData.mTypeData.begin(), result.mData.mTypeData.end()), bytePattern(8, 15));
        EXPECT_EQ(result.mSnam.toUint32(), 0x00010006u);
        EXPECT_EQ(result.mXnam.toUint32(), 0x00010007u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Challenge>("CHAL", record("CHAL", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ChallengeTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Challenge> records
            = loadRecords<ESM4::Challenge>("CHAL", record("CHAL", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ChallengeTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Challenge> records
            = loadRecords<ESM4::Challenge>("CHAL", compressedRecord("CHAL", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ChallengeTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("SCRI", std::string(3, 'x'))), "ESM4::CHAL::load - SCRI has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SCRI", std::string(5, 'x'))), "ESM4::CHAL::load - SCRI has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(23, 'x'))), "ESM4::CHAL::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(25, 'x'))), "ESM4::CHAL::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(3, 'x'))), "ESM4::CHAL::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(5, 'x'))), "ESM4::CHAL::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("XNAM", std::string(3, 'x'))), "ESM4::CHAL::load - XNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("XNAM", std::string(5, 'x'))), "ESM4::CHAL::load - XNAM has an unexpected size");
    }

    TEST(ESM4ChallengeTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CHAL::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ChallengeTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CHAL::load - record has unread bytes after its last sub-record");
    }

}
