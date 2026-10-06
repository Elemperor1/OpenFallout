#include <components/esm4/loadperk.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    std::string scriptHeader()
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0); // references
        append<std::uint32_t>(data, 3); // compiled size
        append<std::uint32_t>(data, 0); // variables
        append<std::uint16_t>(data, 0x100);
        append<std::uint16_t>(data, 1);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + zString("DESC", "Text of DESC")
            + zString("ICON", "Text of ICON") + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010001))
            + subRecord("DATA", bytePattern(5, 1)) + subRecord("PRKE", bytePattern(3, 20))
            + subRecord("DATA", bytePattern(3, 30)) + subRecord("PRKC", "\x04")
            + subRecord("CTDA", conditionData(0x60, 1.5f, 15, 0x00010002))
            + subRecord("CTDA", conditionData(0x80, 0.5f, 16, 0x00010003)) + subRecord("EPFT", "\x05")
            + subRecord("EPFD", bytePattern(8, 40)) + zString("EPF2", "Entry text")
            + subRecord("EPF3", bytePattern(2, 50)) + subRecord("SCHR", scriptHeader())
            + subRecord("SCDA", bytePattern(3, 60)) + subRecord("SCTX", "src") + subRecord("PRKF", "")
            + subRecord("PRKE", bytePattern(3, 70)) + subRecord("DATA", bytePattern(8, 80)) + subRecord("PRKF", "");
    }

    void expectEverySubRecord(const ESM4::Perk& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mDescription, "Text of DESC");
        EXPECT_EQ(result.mIcon, "Text of ICON");
        ASSERT_EQ(result.mConditions.size(), 1u);
        EXPECT_EQ(result.mConditions[0].functionIndex, 14u);
        EXPECT_EQ(result.mData.mTrait, 1);
        EXPECT_EQ(result.mData.mMinimumLevel, 2);
        EXPECT_EQ(result.mData.mRanks, 3);
        EXPECT_EQ(result.mData.mPlayable, 4);
        EXPECT_EQ(result.mData.mHidden, 5);
        ASSERT_EQ(result.mEntries.size(), 2u);
        EXPECT_EQ(result.mEntries[0].mType, 20);
        EXPECT_EQ(result.mEntries[0].mRank, 21);
        EXPECT_EQ(result.mEntries[0].mPriority, 22);
        EXPECT_EQ(std::string(result.mEntries[0].mData.begin(), result.mEntries[0].mData.end()), bytePattern(3, 30));
        ASSERT_EQ(result.mEntries[0].mConditionGroups.size(), 1u);
        EXPECT_TRUE(result.mEntries[0].mConditionGroups[0].mHasRunOn);
        EXPECT_EQ(result.mEntries[0].mConditionGroups[0].mRunOn, 4);
        ASSERT_EQ(result.mEntries[0].mConditionGroups[0].mConditions.size(), 2u);
        EXPECT_EQ(result.mEntries[0].mConditionGroups[0].mConditions[1].reference, 0x00010003u);
        EXPECT_EQ(result.mEntries[0].mFunctionType, 5);
        EXPECT_EQ(std::string(result.mEntries[0].mFunctionData.begin(), result.mEntries[0].mFunctionData.end()),
            bytePattern(8, 40));
        EXPECT_EQ(result.mEntries[0].mFunctionText, "Entry text");
        EXPECT_EQ(std::string(result.mEntries[0].mButtonFlags.begin(), result.mEntries[0].mButtonFlags.end()),
            bytePattern(2, 50));
        EXPECT_EQ(result.mEntries[0].mScript.compiledScript.size(), 3u);
        EXPECT_EQ(result.mEntries[0].mScript.scriptSource, "src");
        EXPECT_EQ(result.mEntries[0].mScript.scriptHeader.compiledSize, 3u);
        EXPECT_EQ(result.mEntries[1].mType, 70);
        EXPECT_EQ(std::string(result.mEntries[1].mData.begin(), result.mEntries[1].mData.end()), bytePattern(8, 80));
        EXPECT_TRUE(result.mEntries[1].mConditionGroups.empty());
        EXPECT_TRUE(result.mEntries[1].mScript.compiledScript.empty());
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Perk>("PERK", record("PERK", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4PerkTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Perk> records
            = loadRecords<ESM4::Perk>("PERK", record("PERK", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4PerkTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Perk> records
            = loadRecords<ESM4::Perk>("PERK", compressedRecord("PERK", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4PerkTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(2, 'x'))), "ESM4::PERK::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("PRKE", std::string(2, 'x'))), "ESM4::PERK::load - PRKE has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("PRKE", std::string(4, 'x'))), "ESM4::PERK::load - PRKE has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("PRKC", std::string(2, 'x'))), "ESM4::PERK::load - PRKC has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("PRKF", std::string(1, 'x'))), "ESM4::PERK::load - PRKF has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EPFT", std::string(2, 'x'))), "ESM4::PERK::load - EPFT has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("PRKE", bytePattern(3, 1)) + subRecord("EPFT", "\x01")
                      + subRecord("EPFD", std::string(3, 'x'))),
            "ESM4::PERK::load - EPFD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EPF3", std::string(3, 'x'))), "ESM4::PERK::load - EPF3 has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(24, 'x'))), "ESM4::PERK::load - CTDA has an unexpected size");
    }

    TEST(ESM4PerkTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::PERK::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4PerkTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::PERK::load - record has unread bytes after its last sub-record");
    }

    TEST(ESM4PerkTest, readsAConditionOfAnEntryThatHasNoGroup)
    {
        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>("PERK",
            record(
                "PERK", 1, subRecord("PRKE", bytePattern(3, 1)) + subRecord("CTDA", conditionData(0x40, 1.f, 14, 0))));

        ASSERT_EQ(perks.size(), 1u);
        EXPECT_TRUE(perks[0].mConditions.empty());
        ASSERT_EQ(perks[0].mEntries.size(), 1u);
        ASSERT_EQ(perks[0].mEntries[0].mConditionGroups.size(), 1u);
        EXPECT_FALSE(perks[0].mEntries[0].mConditionGroups[0].mHasRunOn);
        EXPECT_EQ(perks[0].mEntries[0].mConditionGroups[0].mConditions.size(), 1u);
    }

    TEST(ESM4PerkTest, readsAScriptThatComesBeforeTheFirstEntry)
    {
        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>(
            "PERK", record("PERK", 1, subRecord("SCHR", scriptHeader()) + subRecord("SCDA", bytePattern(3, 1))));

        ASSERT_EQ(perks.size(), 1u);
        EXPECT_EQ(perks[0].mScript.compiledScript.size(), 3u);
    }

    TEST(ESM4PerkTest, readsTheDataOfAPerkWithThreeOrFourBytes)
    {
        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>("PERK",
            record("PERK", 1, subRecord("DATA", bytePattern(3, 1)))
                + record("PERK", 2, subRecord("DATA", bytePattern(4, 1))));

        ASSERT_EQ(perks.size(), 2u);
        EXPECT_EQ(perks[0].mData.mRanks, 3);
        EXPECT_EQ(perks[0].mData.mPlayable, 0);
        EXPECT_EQ(perks[1].mData.mPlayable, 4);
        EXPECT_EQ(perks[1].mData.mHidden, 0);
    }

    TEST(ESM4PerkTest, rejectsASubrecordOfAnEntryThatComesBeforeTheEntry)
    {
        EXPECT_EQ(loadFailure(subRecord("PRKC", "\x01")), "ESM4::PERK::load - PRKC comes before PRKE");
        EXPECT_EQ(loadFailure(subRecord("EPFT", "\x01")), "ESM4::PERK::load - EPFT comes before PRKE");
        EXPECT_EQ(loadFailure(subRecord("EPFD", bytePattern(4, 1))), "ESM4::PERK::load - EPFD comes before PRKE");
        EXPECT_EQ(loadFailure(zString("EPF2", "text")), "ESM4::PERK::load - EPF2 comes before PRKE");
        EXPECT_EQ(loadFailure(subRecord("EPF3", bytePattern(2, 1))), "ESM4::PERK::load - EPF3 comes before PRKE");
    }

    TEST(ESM4PerkTest, rejectsEntryDataOfASizeThePerkDataMayHave)
    {
        const std::string entry = subRecord("PRKE", bytePattern(3, 1));
        EXPECT_EQ(loadFailure(entry + subRecord("DATA", bytePattern(5, 1))),
            "ESM4::PERK::load - DATA has an unexpected size");
        EXPECT_EQ(loadFailure(subRecord("DATA", bytePattern(8, 1))), "ESM4::PERK::load - DATA has an unexpected size");
    }

    std::string questStageData()
    {
        std::string data;
        append<std::uint32_t>(data, 0x00010005);
        append<std::uint8_t>(data, 7);
        data.append(3, '\0');
        return data;
    }

    TEST(ESM4PerkTest, readsTheDataOfEachTypeOfEntry)
    {
        const std::string quest
            = subRecord("PRKE", std::string("\x00\x01\x02", 3)) + subRecord("DATA", questStageData());
        const std::string ability
            = subRecord("PRKE", std::string("\x01\x01\x02", 3)) + valueSubRecord<std::uint32_t>("DATA", 0x00010006);
        const std::string point
            = subRecord("PRKE", std::string("\x02\x01\x02", 3)) + subRecord("DATA", std::string("\x09\x0a\x0b", 3));

        const std::vector<ESM4::Perk> perks
            = loadRecords<ESM4::Perk>("PERK", record("PERK", 1, quest + ability + point));

        ASSERT_EQ(perks.size(), 1u);
        ASSERT_EQ(perks[0].mEntries.size(), 3u);
        EXPECT_EQ(perks[0].mEntries[0].mQuest.toUint32(), 0x00010005u);
        EXPECT_EQ(perks[0].mEntries[0].mQuestStage, 7);
        EXPECT_EQ(perks[0].mEntries[1].mAbility.toUint32(), 0x00010006u);
        EXPECT_EQ(perks[0].mEntries[2].mEntryPoint, 9);
        EXPECT_EQ(perks[0].mEntries[2].mFunction, 10);
        EXPECT_EQ(perks[0].mEntries[2].mTabCount, 11);
        EXPECT_TRUE(perks[0].mEntries[2].mData.empty());
    }

    TEST(ESM4PerkTest, rejectsEntryDataOfASizeThatItsTypeDoesNotHave)
    {
        const std::string quest = subRecord("PRKE", std::string("\x00\x01\x02", 3));
        const std::string ability = subRecord("PRKE", std::string("\x01\x01\x02", 3));
        const std::string point = subRecord("PRKE", std::string("\x02\x01\x02", 3));
        EXPECT_EQ(loadFailure(quest + subRecord("DATA", bytePattern(4, 1))),
            "ESM4::PERK::load - DATA has an unexpected size");
        EXPECT_EQ(loadFailure(ability + subRecord("DATA", bytePattern(8, 1))),
            "ESM4::PERK::load - DATA has an unexpected size");
        EXPECT_EQ(loadFailure(point + subRecord("DATA", bytePattern(4, 1))),
            "ESM4::PERK::load - DATA has an unexpected size");
    }

    std::string functionData(std::uint8_t type, const std::string& data)
    {
        return subRecord("PRKE", std::string("\x02\x01\x02", 3)) + valueSubRecord<std::uint8_t>("EPFT", type)
            + subRecord("EPFD", data);
    }

    TEST(ESM4PerkTest, readsFunctionDataOfEveryTypeThatHasASize)
    {
        std::string leveled;
        append<std::uint32_t>(leveled, 0x00010008);
        const std::string data = functionData(0, bytePattern(5, 1)) + functionData(0, "")
            + functionData(1, bytePattern(4, 10)) + functionData(2, bytePattern(8, 20)) + functionData(3, leveled)
            + functionData(4, "") + functionData(5, bytePattern(8, 30));

        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>("PERK", record("PERK", 1, data));

        ASSERT_EQ(perks.size(), 1u);
        const std::vector<ESM4::Perk::Entry>& entries = perks[0].mEntries;
        ASSERT_EQ(entries.size(), 7u);
        EXPECT_EQ(entries[0].mFunctionData.size(), 5u);
        EXPECT_TRUE(entries[1].mFunctionData.empty());
        EXPECT_EQ(entries[2].mFunctionData.size(), 4u);
        EXPECT_EQ(entries[3].mFunctionData.size(), 8u);
        EXPECT_EQ(entries[4].mLeveledItem.toUint32(), 0x00010008u);
        EXPECT_TRUE(entries[4].mFunctionData.empty());
        EXPECT_TRUE(entries[5].mFunctionData.empty());
        EXPECT_EQ(entries[6].mFunctionData.size(), 8u);
    }

    TEST(ESM4PerkTest, rejectsFunctionDataOfASizeThatItsTypeDoesNotHave)
    {
        for (const auto& [type, size] :
            { std::pair<std::uint8_t, int>{ 1, 8 }, { 2, 4 }, { 3, 8 }, { 4, 4 }, { 5, 4 } })
            EXPECT_EQ(loadFailure(functionData(type, std::string(size, 'x'))),
                "ESM4::PERK::load - EPFD has an unexpected size")
                << int(type);
    }

    TEST(ESM4PerkTest, readsFunctionDataOfATypeTheFormatReferenceDoesNotList)
    {
        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>("PERK",
            record("PERK", 1, functionData(9, bytePattern(3, 1)) + functionData(9, "") + functionData(200, "ab")));

        ASSERT_EQ(perks.size(), 1u);
        ASSERT_EQ(perks[0].mEntries.size(), 3u);
        EXPECT_EQ(perks[0].mEntries[0].mFunctionType, 9);
        EXPECT_EQ(perks[0].mEntries[0].mFunctionData.size(), 3u);
        EXPECT_TRUE(perks[0].mEntries[1].mFunctionData.empty());
        EXPECT_EQ(perks[0].mEntries[2].mFunctionType, 200);
        EXPECT_EQ(perks[0].mEntries[2].mFunctionData.size(), 2u);
    }

    TEST(ESM4PerkTest, readsTheRunOnOfAConditionGroupAsASignedByte)
    {
        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>("PERK",
            record("PERK", 1, subRecord("PRKE", bytePattern(3, 1)) + valueSubRecord<std::uint8_t>("PRKC", 0xFF)));

        ASSERT_EQ(perks.size(), 1u);
        ASSERT_EQ(perks[0].mEntries.size(), 1u);
        ASSERT_EQ(perks[0].mEntries[0].mConditionGroups.size(), 1u);
        EXPECT_EQ(perks[0].mEntries[0].mConditionGroups[0].mRunOn, -1);
    }

    TEST(ESM4PerkTest, adjustsTheFormIdsToTheLoadOrder)
    {
        const std::string quest
            = subRecord("PRKE", std::string("\x00\x01\x02", 3)) + subRecord("DATA", questStageData());
        const std::string nullQuest
            = subRecord("PRKE", std::string("\x00\x01\x02", 3)) + subRecord("DATA", std::string(8, '\0'));
        const std::string ability
            = subRecord("PRKE", std::string("\x01\x01\x02", 3)) + valueSubRecord<std::uint32_t>("DATA", 0x00000006);
        std::string leveled;
        append<std::uint32_t>(leveled, 0x00000008);
        const std::string condition = subRecord("CTDA", conditionData(0x40, 1.f, 14, 0x00000009));

        const std::vector<ESM4::Perk> perks = loadRecords<ESM4::Perk>("PERK",
            record("PERK", 1, condition + quest + nullQuest + ability + functionData(3, leveled)), 0, nullptr, 3);

        ASSERT_EQ(perks.size(), 1u);
        EXPECT_EQ(perks[0].mConditions[0].reference, 0x03000009u);
        ASSERT_EQ(perks[0].mEntries.size(), 4u);
        EXPECT_EQ(perks[0].mEntries[0].mQuest, (ESM::FormId{ 0x10005, 3 }));
        EXPECT_EQ(perks[0].mEntries[1].mQuest.toUint32(), 0u);
        EXPECT_EQ(perks[0].mEntries[2].mAbility, (ESM::FormId{ 6, 3 }));
        EXPECT_EQ(perks[0].mEntries[3].mLeveledItem, (ESM::FormId{ 8, 3 }));
    }
}
