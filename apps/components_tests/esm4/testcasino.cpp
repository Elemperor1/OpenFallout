#include <components/esm4/loadcsno.hpp>

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
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL")
            + subRecord("DATA", bytePattern(56, 13)) + zString("MODL", "First of MODL")
            + zString("MODL", "Second of MODL") + zString("MOD2", "Text of MOD2") + zString("MOD3", "Text of MOD3")
            + zString("MOD4", "Text of MOD4") + zString("ICON", "First of ICON") + zString("ICON", "Second of ICON")
            + zString("ICO2", "First of ICO2") + zString("ICO2", "Second of ICO2");
    }

    void expectEverySubRecord(const ESM4::Casino& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(56, 13));
        EXPECT_THAT(result.mModels, ElementsAre("First of MODL", "Second of MODL"));
        EXPECT_EQ(result.mModel2, "Text of MOD2");
        EXPECT_EQ(result.mModel3, "Text of MOD3");
        EXPECT_EQ(result.mModel4, "Text of MOD4");
        EXPECT_THAT(result.mIcons, ElementsAre("First of ICON", "Second of ICON"));
        EXPECT_THAT(result.mIcons2, ElementsAre("First of ICO2", "Second of ICO2"));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Casino>("CSNO", record("CSNO", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CasinoTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Casino> records
            = loadRecords<ESM4::Casino>("CSNO", record("CSNO", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CasinoTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Casino> records
            = loadRecords<ESM4::Casino>("CSNO", compressedRecord("CSNO", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CasinoTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(55, 'x'))), "ESM4::CSNO::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(57, 'x'))), "ESM4::CSNO::load - DATA has an unexpected size");
    }

    TEST(ESM4CasinoTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CSNO::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CasinoTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CSNO::load - record has unread bytes after its last sub-record");
    }

}
