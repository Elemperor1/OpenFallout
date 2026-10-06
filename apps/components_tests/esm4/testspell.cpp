#include <components/esm4/loadspel.hpp>

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

    std::string dataEffect(int offset)
    {
        std::string data;
        for (int i = 1; i <= 4; ++i)
            append<std::uint32_t>(data, 100000 + offset - 1 + i);
        append<std::int32_t>(data, 100000 + offset - 1 + 5);
        return data;
    }

    std::string dataData(int offset)
    {
        std::string data;
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 0)));
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 1)));
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 2)));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 3) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 4) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 5) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 6) % 50));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + subRecord("SPIT", dataData(3))
            + valueSubRecord<std::uint32_t>("EFID", 0x00010001) + subRecord("EFIT", dataEffect(1))
            + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010005))
            + subRecord("CTDA", conditionData(0x60, 2.5f, 15, 0x00010006))
            + valueSubRecord<std::uint32_t>("EFID", 0x00010002) + subRecord("EFIT", dataEffect(50));
    }

    void expectEverySubRecord(const ESM4::Spell& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mData.mType, 100003u);
        EXPECT_EQ(result.mData.mCost, 100004u);
        EXPECT_EQ(result.mData.mLevel, 100005u);
        EXPECT_EQ(result.mData.mSpellFlags, 9);
        EXPECT_EQ(result.mData.mUnused1, 10);
        EXPECT_EQ(result.mData.mUnused2, 11);
        EXPECT_EQ(result.mData.mUnused3, 12);
        ASSERT_EQ(result.mEffects.size(), 2u);
        EXPECT_EQ(result.mEffects[0].mBaseEffect.toUint32(), 0x00010001u);
        EXPECT_EQ(result.mEffects[0].mData.mMagnitude, 100001u);
        EXPECT_EQ(result.mEffects[0].mData.mArea, 100002u);
        EXPECT_EQ(result.mEffects[0].mData.mDuration, 100003u);
        EXPECT_EQ(result.mEffects[0].mData.mRange, 100004u);
        EXPECT_EQ(result.mEffects[0].mData.mActorValue, 100005);
        ASSERT_EQ(result.mEffects[0].mConditions.size(), 2u);
        EXPECT_EQ(result.mEffects[0].mConditions[0].functionIndex, 14u);
        EXPECT_EQ(result.mEffects[0].mConditions[1].reference, 0x00010006u);
        EXPECT_EQ(result.mEffects[1].mBaseEffect.toUint32(), 0x00010002u);
        EXPECT_EQ(result.mEffects[1].mData.mMagnitude, 100050u);
        EXPECT_TRUE(result.mEffects[1].mConditions.empty());
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Spell>("SPEL", record("SPEL", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4SpellTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Spell> records
            = loadRecords<ESM4::Spell>("SPEL", record("SPEL", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4SpellTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Spell> records
            = loadRecords<ESM4::Spell>("SPEL", compressedRecord("SPEL", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4SpellTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("SPIT", std::string(15, 'x'))), "ESM4::SPEL::load - SPIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SPIT", std::string(17, 'x'))), "ESM4::SPEL::load - SPIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EFID", std::string(3, 'x'))), "ESM4::SPEL::load - EFID has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EFIT", std::string(19, 'x'))), "ESM4::SPEL::load - EFIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EFIT", std::string(21, 'x'))), "ESM4::SPEL::load - EFIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(24, 'x'))), "ESM4::SPEL::load - CTDA has an unexpected size");
    }

    TEST(ESM4SpellTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::SPEL::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4SpellTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::SPEL::load - record has unread bytes after its last sub-record");
    }

    TEST(ESM4SpellTest, rejectsAnEffectThatComesBeforeItsId)
    {
        EXPECT_EQ(loadFailure(subRecord("EFIT", dataEffect(1))), "ESM4::SPEL::load - EFIT comes before EFID");
    }

    TEST(ESM4SpellTest, keepsAConditionThatComesBeforeTheFirstEffect)
    {
        const auto records = loadRecords<ESM4::Spell>("SPEL",
            record("SPEL", 1,
                subRecord("CTDA", conditionData(0x40, 1.f, 14, 0)) + valueSubRecord<std::uint32_t>("EFID", 1)));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mConditions.size(), 1u);
        ASSERT_EQ(records[0].mEffects.size(), 1u);
        EXPECT_TRUE(records[0].mEffects[0].mConditions.empty());
    }

}
