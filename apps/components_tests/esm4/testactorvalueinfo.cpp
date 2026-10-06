#include <components/esm4/loadavif.hpp>

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
