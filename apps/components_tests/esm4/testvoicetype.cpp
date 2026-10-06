#include <components/esm4/loadvtyp.hpp>

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
        return zString("EDID", "Text of EDID") + valueSubRecord<std::uint8_t>("DNAM", 5);
    }

    void expectEverySubRecord(const ESM4::VoiceType& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mVoiceFlags, 5);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::VoiceType>("VTYP", record("VTYP", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4VoiceTypeTest, readsEverySubrecord)
    {
        const std::vector<ESM4::VoiceType> records
            = loadRecords<ESM4::VoiceType>("VTYP", record("VTYP", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4VoiceTypeTest, readsACompressedRecord)
    {
        const std::vector<ESM4::VoiceType> records
            = loadRecords<ESM4::VoiceType>("VTYP", compressedRecord("VTYP", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4VoiceTypeTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(0, 'x'))), "ESM4::VTYP::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(2, 'x'))), "ESM4::VTYP::load - DNAM has an unexpected size");
    }

    TEST(ESM4VoiceTypeTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::VTYP::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4VoiceTypeTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::VTYP::load - record has unread bytes after its last sub-record");
    }

}
