#include <components/esm4/loadmesg.hpp>

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
        return zString("EDID", "Text of EDID") + zString("DESC", "Text of DESC") + zString("FULL", "Text of FULL")
            + valueSubRecord<std::uint32_t>("INAM", 0x00010004) + subRecord("NAM0", bytePattern(3, 20))
            + subRecord("NAM1", bytePattern(4, 21)) + subRecord("NAM3", bytePattern(5, 22))
            + subRecord("NAM5", bytePattern(6, 23)) + subRecord("NAM8", bytePattern(7, 24))
            + subRecord("NAM9", bytePattern(3, 25)) + valueSubRecord<std::uint32_t>("DNAM", 100006)
            + valueSubRecord<std::uint32_t>("TNAM", 100007) + zString("ITXT", "First button")
            + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010005)) + zString("ITXT", "Second button");
    }

    void expectEverySubRecord(const ESM4::Message& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mDescription, "Text of DESC");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mIcon.toUint32(), 0x00010004u);
        ASSERT_EQ(result.mNams.size(), 6);
        EXPECT_EQ(result.mNams[0].mType, ESM::fourCC("NAM0"));
        EXPECT_EQ(std::string(result.mNams[0].mData.begin(), result.mNams[0].mData.end()), bytePattern(3, 20));
        EXPECT_EQ(result.mNams[1].mType, ESM::fourCC("NAM1"));
        EXPECT_EQ(std::string(result.mNams[1].mData.begin(), result.mNams[1].mData.end()), bytePattern(4, 21));
        EXPECT_EQ(result.mNams[2].mType, ESM::fourCC("NAM3"));
        EXPECT_EQ(std::string(result.mNams[2].mData.begin(), result.mNams[2].mData.end()), bytePattern(5, 22));
        EXPECT_EQ(result.mNams[3].mType, ESM::fourCC("NAM5"));
        EXPECT_EQ(std::string(result.mNams[3].mData.begin(), result.mNams[3].mData.end()), bytePattern(6, 23));
        EXPECT_EQ(result.mNams[4].mType, ESM::fourCC("NAM8"));
        EXPECT_EQ(std::string(result.mNams[4].mData.begin(), result.mNams[4].mData.end()), bytePattern(7, 24));
        EXPECT_EQ(result.mNams[5].mType, ESM::fourCC("NAM9"));
        EXPECT_EQ(std::string(result.mNams[5].mData.begin(), result.mNams[5].mData.end()), bytePattern(3, 25));
        EXPECT_EQ(result.mMessageFlags, 100006u);
        EXPECT_EQ(result.mDisplayTime, 100007u);
        ASSERT_EQ(result.mButtons.size(), 2u);
        EXPECT_EQ(result.mButtons[0].mText, "First button");
        ASSERT_EQ(result.mButtons[0].mConditions.size(), 1u);
        EXPECT_EQ(result.mButtons[0].mConditions[0].functionIndex, 14u);
        EXPECT_EQ(result.mButtons[1].mText, "Second button");
        EXPECT_TRUE(result.mButtons[1].mConditions.empty());
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Message>("MESG", record("MESG", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4MessageTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Message> records
            = loadRecords<ESM4::Message>("MESG", record("MESG", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4MessageTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Message> records
            = loadRecords<ESM4::Message>("MESG", compressedRecord("MESG", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4MessageTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("INAM", std::string(3, 'x'))), "ESM4::MESG::load - INAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("INAM", std::string(5, 'x'))), "ESM4::MESG::load - INAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(3, 'x'))), "ESM4::MESG::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(5, 'x'))), "ESM4::MESG::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TNAM", std::string(3, 'x'))), "ESM4::MESG::load - TNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TNAM", std::string(5, 'x'))), "ESM4::MESG::load - TNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(21, 'x'))), "ESM4::MESG::load - CTDA has an unexpected size");
    }

    TEST(ESM4MessageTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::MESG::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4MessageTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::MESG::load - record has unread bytes after its last sub-record");
    }

}
