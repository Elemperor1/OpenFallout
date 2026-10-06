#include <components/esm4/loadmicn.hpp>

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
        return zString("EDID", "Text of EDID") + zString("ICON", "Text of ICON");
    }

    void expectEverySubRecord(const ESM4::MenuIcon& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mIcon, "Text of ICON");
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::MenuIcon>("MICN", record("MICN", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4MenuIconTest, readsEverySubrecord)
    {
        const std::vector<ESM4::MenuIcon> records
            = loadRecords<ESM4::MenuIcon>("MICN", record("MICN", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4MenuIconTest, readsACompressedRecord)
    {
        const std::vector<ESM4::MenuIcon> records
            = loadRecords<ESM4::MenuIcon>("MICN", compressedRecord("MICN", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4MenuIconTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::MICN::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4MenuIconTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::MICN::load - record has unread bytes after its last sub-record");
    }

}
