#include <components/esm4/loadterm.hpp>

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

    std::string scriptHeader(std::uint32_t references, std::uint32_t compiledSize, std::uint32_t variables)
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        append(data, references);
        append(data, compiledSize);
        append(data, variables);
        append<std::uint16_t>(data, 0);
        append<std::uint16_t>(data, 1);
        return data;
    }

    std::string formIdData(std::uint32_t id)
    {
        std::string data;
        append(data, id);
        return data;
    }

    std::string localVariable()
    {
        std::string data;
        append<std::uint32_t>(data, 1); // index
        data += bytePattern(20, 0);
        return data;
    }

    std::string firstItem()
    {
        return zString("ITXT", "Text of ITXT 1") + zString("RNAM", "Text of RNAM 1") + subRecord("ANAM", "\x03")
            + subRecord("INAM", formIdData(0x00010005)) + subRecord("TNAM", formIdData(0x00010006))
            + subRecord("SCHR", scriptHeader(1, 3, 1)) + subRecord("SCDA", bytePattern(3, 60))
            + subRecord("SCTX", "source") + subRecord("SLSD", localVariable()) + zString("SCVR", "variable")
            + subRecord("SCRO", formIdData(0x00010007)) + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010008));
    }

    std::string secondItem()
    {
        return zString("ITXT", "Text of ITXT 2") + zString("RNAM", "Text of RNAM 2") + subRecord("ANAM", "\x04")
            + subRecord("SCHR", scriptHeader(0, 0, 0)) + subRecord("CTDA", conditionData(0x60, 1.5f, 15, 0x00010009))
            + subRecord("CTDA", conditionData(0x80, 0.5f, 16, 0));
    }

    TEST(ESM4TerminalTest, readsMenuItemsWithTheirScriptsAndConditions)
    {
        const std::string data = zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL")
            + zString("DESC", "Text of DESC") + subRecord("SNAM", formIdData(0x00010001))
            + subRecord("DNAM", "\x02\x00\x00\x00") + firstItem() + secondItem();
        const std::vector<ESM4::Terminal> result = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data));

        ASSERT_EQ(result.size(), 1u);
        const ESM4::Terminal& terminal = result[0];
        EXPECT_EQ(terminal.mEditorId, "Text of EDID");
        EXPECT_EQ(terminal.mFullName, "Text of FULL");
        EXPECT_EQ(terminal.mText, "Text of DESC");
        EXPECT_EQ(terminal.mResultText, "Text of RNAM 2");
        EXPECT_THAT(terminal.mConditions, IsEmpty());

        ASSERT_EQ(terminal.mMenuItems.size(), 2u);
        const ESM4::Terminal::MenuItem& first = terminal.mMenuItems[0];
        EXPECT_EQ(first.mText, "Text of ITXT 1");
        EXPECT_EQ(first.mResultText, "Text of RNAM 1");
        EXPECT_EQ(first.mFlags, 3);
        EXPECT_EQ(first.mDisplayNote, ESM::FormId::fromUint32(0x00010005));
        EXPECT_EQ(first.mSubMenu, ESM::FormId::fromUint32(0x00010006));
        EXPECT_EQ(first.mScript.scriptHeader.compiledSize, 3u);
        EXPECT_THAT(first.mScript.compiledScript, ElementsAre(60, 61, 62));
        EXPECT_EQ(first.mScript.scriptSource, "source");
        ASSERT_EQ(first.mScript.localVarData.size(), 1u);
        EXPECT_EQ(first.mScript.localVarData[0].variableName, "variable");
        ASSERT_EQ(first.mScript.references.size(), 1u);
        EXPECT_EQ(first.mScript.references[0].formId, ESM::FormId::fromUint32(0x00010007));
        EXPECT_TRUE(first.mScript.isConsistent());
        ASSERT_EQ(first.mConditions.size(), 1u);
        EXPECT_EQ(first.mConditions[0].functionIndex, 14u);
        EXPECT_EQ(first.mConditions[0].reference, 0x00010008u);

        const ESM4::Terminal::MenuItem& second = terminal.mMenuItems[1];
        EXPECT_EQ(second.mText, "Text of ITXT 2");
        EXPECT_EQ(second.mFlags, 4);
        EXPECT_EQ(second.mDisplayNote, ESM::FormId());
        EXPECT_EQ(second.mSubMenu, ESM::FormId());
        EXPECT_THAT(second.mScript.compiledScript, IsEmpty());
        ASSERT_EQ(second.mConditions.size(), 2u);
        EXPECT_EQ(second.mConditions[0].functionIndex, 15u);
        EXPECT_EQ(second.mConditions[1].functionIndex, 16u);
    }

    TEST(ESM4TerminalTest, keepsConditionsBeforeTheFirstMenuItemOnTheRecord)
    {
        const std::string data = zString("EDID", "Text of EDID") + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0))
            + zString("ITXT", "Item");
        const std::vector<ESM4::Terminal> result = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data));

        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0].mConditions.size(), 1u);
        EXPECT_EQ(result[0].mConditions[0].functionIndex, 14u);
        ASSERT_EQ(result[0].mMenuItems.size(), 1u);
        EXPECT_THAT(result[0].mMenuItems[0].mConditions, IsEmpty());
    }

    TEST(ESM4TerminalTest, ignoresItemSubrecordsOfSizesItDoesNotKnow)
    {
        // The loader is shared with games whose terminals are laid out another way.
        const std::string data = subRecord("INAM", "123") + subRecord("TNAM", "") + subRecord("ANAM", "\x01\x02")
            + subRecord("CTDA", "12345") + zString("ITXT", "Item") + subRecord("ANAM", "\x01\x02")
            + subRecord("INAM", "123") + subRecord("TNAM", "") + subRecord("CTDA", "12345");
        const std::vector<ESM4::Terminal> result = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data));

        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0].mMenuItems.size(), 1u);
        EXPECT_EQ(result[0].mMenuItems[0].mFlags, 0);
        EXPECT_THAT(result[0].mMenuItems[0].mConditions, IsEmpty());
        EXPECT_THAT(result[0].mConditions, IsEmpty());
    }

    TEST(ESM4TerminalTest, startsAMenuItemWhereTheSubrecordsOfAnItemStartOver)
    {
        // ITXT is optional in the format reference: an item may start with any of its sub-records, and the scripts of
        // the items must not run together.
        const std::string withoutText = zString("RNAM", "Text of RNAM") + subRecord("ANAM", "\x05")
            + subRecord("SCHR", scriptHeader(1, 3, 0)) + subRecord("SCDA", bytePattern(3, 10))
            + subRecord("SCRO", formIdData(0x00010001)) + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0));
        const std::string withoutResult = subRecord("ANAM", "\x06") + subRecord("INAM", formIdData(0x00010002));
        const std::string onlyAScript
            = subRecord("SCHR", scriptHeader(0, 2, 0)) + subRecord("SCDA", bytePattern(2, 20));
        const std::string data = firstItem() + withoutText + onlyAScript + withoutResult + secondItem();
        const std::vector<ESM4::Terminal> result = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data));

        ASSERT_EQ(result.size(), 1u);
        const std::vector<ESM4::Terminal::MenuItem>& items = result[0].mMenuItems;
        ASSERT_EQ(items.size(), 5u);
        EXPECT_EQ(items[0].mText, "Text of ITXT 1");
        EXPECT_EQ(items[0].mScript.references.size(), 1u);
        EXPECT_TRUE(items[0].mScript.isConsistent());
        EXPECT_EQ(items[1].mText, "");
        EXPECT_EQ(items[1].mResultText, "Text of RNAM");
        EXPECT_EQ(items[1].mFlags, 5);
        EXPECT_THAT(items[1].mScript.compiledScript, ElementsAre(10, 11, 12));
        EXPECT_TRUE(items[1].mScript.isConsistent());
        EXPECT_EQ(items[1].mConditions.size(), 1u);
        EXPECT_THAT(items[2].mScript.compiledScript, ElementsAre(20, 21));
        EXPECT_TRUE(items[2].mScript.isConsistent());
        EXPECT_EQ(items[3].mFlags, 6);
        EXPECT_EQ(items[3].mDisplayNote, ESM::FormId::fromUint32(0x00010002));
        EXPECT_THAT(items[3].mScript.compiledScript, IsEmpty());
        EXPECT_EQ(items[4].mText, "Text of ITXT 2");
    }

    TEST(ESM4TerminalTest, startsAMenuItemAtTheFirstSubrecordThatBelongsToOne)
    {
        const std::string data = zString("EDID", "Text of EDID") + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0))
            + zString("RNAM", "First result") + subRecord("ANAM", "\x02");
        const std::vector<ESM4::Terminal> result = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data));

        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(result[0].mConditions.size(), 1u);
        ASSERT_EQ(result[0].mMenuItems.size(), 1u);
        EXPECT_EQ(result[0].mMenuItems[0].mResultText, "First result");
        EXPECT_EQ(result[0].mMenuItems[0].mFlags, 2);
    }

    TEST(ESM4TerminalTest, readsAnItemConditionOfTwentyBytesAndAnEmptyText)
    {
        const std::string older = conditionData(0x40, 2.5f, 14, 0x00010001).substr(0, 20);
        const std::string data = subRecord("ITXT", "") + subRecord("RNAM", "") + subRecord("CTDA", older);
        const std::vector<ESM4::Terminal> result = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data));

        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0].mMenuItems.size(), 1u);
        EXPECT_EQ(result[0].mMenuItems[0].mText, "");
        EXPECT_EQ(result[0].mMenuItems[0].mResultText, "");
        ASSERT_EQ(result[0].mMenuItems[0].mConditions.size(), 1u);
        EXPECT_EQ(result[0].mMenuItems[0].mConditions[0].functionIndex, 14u);
        EXPECT_EQ(result[0].mMenuItems[0].mConditions[0].reference, 0u);
    }

    TEST(ESM4TerminalTest, adjustsTheFormIdsOfItsItemsToTheLoadOrder)
    {
        const std::string data = zString("ITXT", "Item") + subRecord("INAM", formIdData(0x00000123))
            + subRecord("TNAM", formIdData(0)) + subRecord("SCHR", scriptHeader(1, 0, 0))
            + subRecord("SCRO", formIdData(0x00000456)) + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00000789));
        const std::vector<ESM4::Terminal> result
            = loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, data), 0, nullptr, 3);

        ASSERT_EQ(result.size(), 1u);
        ASSERT_EQ(result[0].mMenuItems.size(), 1u);
        const ESM4::Terminal::MenuItem& item = result[0].mMenuItems[0];
        EXPECT_EQ(item.mDisplayNote, (ESM::FormId{ 0x123, 3 }));
        EXPECT_EQ(item.mSubMenu.toUint32(), 0u);
        ASSERT_EQ(item.mScript.references.size(), 1u);
        EXPECT_EQ(item.mScript.references[0].formId, (ESM::FormId{ 0x456, 3 }));
        ASSERT_EQ(item.mConditions.size(), 1u);
        EXPECT_EQ(item.mConditions[0].reference, 0x03000789u);
    }

    TEST(ESM4TerminalTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_THROW(
            loadRecords<ESM4::Terminal>("TERM", record("TERM", 1, subRecord("ZZZZ", "1"))), std::runtime_error);
    }
}
