#include <components/esm4/loadavif.hpp>

#include "syntheticplugin.hpp"

#include <components/toutf8/toutf8.hpp>

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
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + zString("DESC", "Text of DESC")
            + zString("ICON", "Text of ICON") + zString("ANAM", "Text of ANAM");
    }

    void expectEverySubRecord(const ESM4::ActorValueInfo& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mDescription, "Text of DESC");
        EXPECT_EQ(result.mIcon, "Text of ICON");
        EXPECT_EQ(result.mShortName, "Text of ANAM");
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::ActorValueInfo>("AVIF", record("AVIF", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ActorValueInfoTest, readsEverySubrecord)
    {
        const std::vector<ESM4::ActorValueInfo> records
            = loadRecords<ESM4::ActorValueInfo>("AVIF", record("AVIF", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ActorValueInfoTest, readsACompressedRecord)
    {
        const std::vector<ESM4::ActorValueInfo> records
            = loadRecords<ESM4::ActorValueInfo>("AVIF", compressedRecord("AVIF", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    /// The engine reads plugins with an encoder, which reads strings by another path than a reader without one: the
    /// text of a skill (a long DESC with an apostrophe of the Windows code page in it) must come out whole.
    TEST(ESM4ActorValueInfoTest, readsTheTextOfASkillThroughAnEncoder)
    {
        const ToUTF8::StatelessUtf8Encoder encoder(ToUTF8::WINDOWS_1252);
        const std::string description
            = "Skill in all small weapons, from pistols to rifles. A character\x92s aim.\r\nSecond line.";
        const std::string data = zString("EDID", "AVSmallGuns") + zString("FULL", "Small Guns")
            + zString("DESC", description) + zString("ICON", "interface\\icons\\guns.dds") + zString("ANAM", "Guns");

        const std::vector<ESM4::ActorValueInfo> records
            = loadRecords<ESM4::ActorValueInfo>("AVIF", record("AVIF", 7, data), 0, &encoder);

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEditorId, "AVSmallGuns");
        EXPECT_EQ(records[0].mFullName, "Small Guns");
        EXPECT_EQ(records[0].mDescription,
            "Skill in all small weapons, from pistols to rifles. A character\xE2\x80\x99s aim.\r\nSecond line.");
        EXPECT_EQ(records[0].mShortName, "Guns");
    }

    TEST(ESM4ActorValueInfoTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::AVIF::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ActorValueInfoTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::AVIF::load - record has unread bytes after its last sub-record");
    }

}
