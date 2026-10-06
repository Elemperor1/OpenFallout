#include <components/esm4/loadrcpe.hpp>

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

    std::string dataData(int offset)
    {
        std::string data;
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 0)));
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 1)));
        append<ESM::FormId32>(data, 0x00010000u + offset + 2);
        append<ESM::FormId32>(data, 0x00010000u + offset + 3);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL")
            + subRecord("CTDA", conditionData(0x43, 2.5f, 14, 0x00010003)) + subRecord("DATA", dataData(4))
            + valueSubRecord<std::uint32_t>("RCIL", 0x00010001) + valueSubRecord<std::uint32_t>("RCQY", 3)
            + valueSubRecord<std::uint32_t>("RCIL", 0x00010002) + valueSubRecord<std::uint32_t>("RCQY", 4)
            + valueSubRecord<std::uint32_t>("RCOD", 0x00010003) + valueSubRecord<std::uint32_t>("RCQY", 5);
    }

    void expectEverySubRecord(const ESM4::Recipe& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        ASSERT_EQ(result.mConditions.size(), 1u);
        EXPECT_EQ(result.mConditions[0].condition, 0x43u);
        EXPECT_EQ(result.mConditions[0].comparison, 2.5f);
        EXPECT_EQ(result.mConditions[0].functionIndex, 14u);
        EXPECT_EQ(result.mConditions[0].reference, 0x00010003u);
        EXPECT_EQ(result.mData.mSkill, -100004);
        EXPECT_EQ(result.mData.mLevel, 100005u);
        EXPECT_EQ(result.mData.mCategory, 0x00010006u);
        EXPECT_EQ(result.mData.mSubCategory, 0x00010007u);
        ASSERT_EQ(result.mIngredients.size(), 2u);
        EXPECT_EQ(result.mIngredients[0].mItem.toUint32(), 0x00010001u);
        EXPECT_EQ(result.mIngredients[0].mCount, 3u);
        EXPECT_EQ(result.mIngredients[1].mItem.toUint32(), 0x00010002u);
        EXPECT_EQ(result.mIngredients[1].mCount, 4u);
        ASSERT_EQ(result.mOutputs.size(), 1u);
        EXPECT_EQ(result.mOutputs[0].mItem.toUint32(), 0x00010003u);
        EXPECT_EQ(result.mOutputs[0].mCount, 5u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Recipe>("RCPE", record("RCPE", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4RecipeTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Recipe> records
            = loadRecords<ESM4::Recipe>("RCPE", record("RCPE", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4RecipeTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Recipe> records
            = loadRecords<ESM4::Recipe>("RCPE", compressedRecord("RCPE", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4RecipeTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(24, 'x'))), "ESM4::RCPE::load - CTDA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(32, 'x'))), "ESM4::RCPE::load - CTDA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(15, 'x'))), "ESM4::RCPE::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(17, 'x'))), "ESM4::RCPE::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RCIL", std::string(3, 'x'))), "ESM4::RCPE::load - RCIL has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RCOD", std::string(5, 'x'))), "ESM4::RCPE::load - RCOD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RCQY", std::string(3, 'x'))), "ESM4::RCPE::load - RCQY has an unexpected size");
    }

    TEST(ESM4RecipeTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::RCPE::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4RecipeTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::RCPE::load - record has unread bytes after its last sub-record");
    }

    TEST(ESM4RecipeTest, rejectsAQuantityThatComesBeforeItsItem)
    {
        EXPECT_EQ(
            loadFailure(valueSubRecord<std::uint32_t>("RCQY", 1)), "ESM4::RCPE::load - RCQY comes before RCIL or RCOD");
    }

}
