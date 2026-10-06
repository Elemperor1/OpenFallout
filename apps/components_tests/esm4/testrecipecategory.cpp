#include <components/esm4/loadrcct.hpp>

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
            + valueSubRecord<std::uint8_t>("DATA", 6);
    }

    void expectEverySubRecord(const ESM4::RecipeCategory& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mCategoryFlags, 6);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::RecipeCategory>("RCCT", record("RCCT", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4RecipeCategoryTest, readsEverySubrecord)
    {
        const std::vector<ESM4::RecipeCategory> records
            = loadRecords<ESM4::RecipeCategory>("RCCT", record("RCCT", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4RecipeCategoryTest, readsACompressedRecord)
    {
        const std::vector<ESM4::RecipeCategory> records
            = loadRecords<ESM4::RecipeCategory>("RCCT", compressedRecord("RCCT", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4RecipeCategoryTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(0, 'x'))), "ESM4::RCCT::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(2, 'x'))), "ESM4::RCCT::load - DATA has an unexpected size");
    }

    TEST(ESM4RecipeCategoryTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::RCCT::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4RecipeCategoryTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::RCCT::load - record has unread bytes after its last sub-record");
    }

}
