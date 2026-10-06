#include <components/esm4/loadench.hpp>

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
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + subRecord("ENIT", dataData(3))
            + valueSubRecord<std::uint32_t>("EFID", 0x00010001) + subRecord("EFIT", dataEffect(1))
            + subRecord("CTDA", conditionData(0x40, 2.5f, 14, 0x00010005))
            + subRecord("CTDA", conditionData(0x60, 2.5f, 15, 0x00010006))
            + valueSubRecord<std::uint32_t>("EFID", 0x00010002) + subRecord("EFIT", dataEffect(50));
    }

    void expectEverySubRecord(const ESM4::Enchantment& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mData.mType, 100003u);
        EXPECT_EQ(result.mData.mChargeAmount, 100004u);
        EXPECT_EQ(result.mData.mEnchantCost, 100005u);
        EXPECT_EQ(result.mData.mEnchantFlags, 9);
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
            loadRecords<ESM4::Enchantment>("ENCH", record("ENCH", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4EnchantmentTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Enchantment> records
            = loadRecords<ESM4::Enchantment>("ENCH", record("ENCH", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4EnchantmentTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Enchantment> records
            = loadRecords<ESM4::Enchantment>("ENCH", compressedRecord("ENCH", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4EnchantmentTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("ENIT", std::string(15, 'x'))), "ESM4::ENCH::load - ENIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ENIT", std::string(17, 'x'))), "ESM4::ENCH::load - ENIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EFID", std::string(3, 'x'))), "ESM4::ENCH::load - EFID has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EFIT", std::string(19, 'x'))), "ESM4::ENCH::load - EFIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EFIT", std::string(21, 'x'))), "ESM4::ENCH::load - EFIT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(24, 'x'))), "ESM4::ENCH::load - CTDA has an unexpected size");
    }

    TEST(ESM4EnchantmentTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::ENCH::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4EnchantmentTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::ENCH::load - record has unread bytes after its last sub-record");
    }

    TEST(ESM4EnchantmentTest, readsEffectsWhoseIdIsMissingOrWhoseDataIsMissing)
    {
        // EFID is optional in the format reference. An EFIT starts an effect of its own unless the last effect has an
        // EFID and no EFIT yet.
        const auto records = loadRecords<ESM4::Enchantment>("ENCH",
            record("ENCH", 1,
                subRecord("EFIT", dataEffect(1)) + subRecord("EFIT", dataEffect(10))
                    + valueSubRecord<std::uint32_t>("EFID", 0x00010001) + subRecord("EFIT", dataEffect(20))
                    + valueSubRecord<std::uint32_t>("EFID", 0x00010002)
                    + valueSubRecord<std::uint32_t>("EFID", 0x00010003) + subRecord("EFIT", dataEffect(30))));

        ASSERT_EQ(records.size(), 1u);
        const std::vector<ESM4::EffectEntry>& effects = records[0].mEffects;
        ASSERT_EQ(effects.size(), 5u);
        EXPECT_TRUE(effects[0].mHasData);
        EXPECT_TRUE(effects[0].mBaseEffect.isZeroOrUnset());
        EXPECT_EQ(effects[0].mData.mMagnitude, 100001u);
        EXPECT_TRUE(effects[1].mHasData);
        EXPECT_TRUE(effects[1].mBaseEffect.isZeroOrUnset());
        EXPECT_EQ(effects[1].mData.mMagnitude, 100010u);
        EXPECT_EQ(effects[2].mBaseEffect.toUint32(), 0x00010001u);
        EXPECT_EQ(effects[2].mData.mMagnitude, 100020u);
        EXPECT_EQ(effects[3].mBaseEffect.toUint32(), 0x00010002u);
        EXPECT_FALSE(effects[3].mHasData);
        EXPECT_EQ(effects[4].mBaseEffect.toUint32(), 0x00010003u);
        EXPECT_EQ(effects[4].mData.mMagnitude, 100030u);
    }

    TEST(ESM4EnchantmentTest, adjustsTheFormIdsOfItsEffectsToTheLoadOrder)
    {
        const auto records = loadRecords<ESM4::Enchantment>("ENCH",
            record("ENCH", 1,
                subRecord("CTDA", conditionData(0x40, 1.f, 14, 0x00000009))
                    + valueSubRecord<std::uint32_t>("EFID", 0x00000123) + subRecord("EFIT", dataEffect(1))
                    + subRecord("CTDA", conditionData(0x40, 1.f, 14, 0x00000008))
                    + subRecord("CTDA", conditionData(0x40, 1.f, 14, 0)) + valueSubRecord<std::uint32_t>("EFID", 0)
                    + subRecord("EFIT", dataEffect(2))),
            0, nullptr, 3);

        ASSERT_EQ(records.size(), 1u);
        ASSERT_EQ(records[0].mConditions.size(), 1u);
        EXPECT_EQ(records[0].mConditions[0].reference, 0x03000009u);
        ASSERT_EQ(records[0].mEffects.size(), 2u);
        EXPECT_EQ(records[0].mEffects[0].mBaseEffect, (ESM::FormId{ 0x123, 3 }));
        ASSERT_EQ(records[0].mEffects[0].mConditions.size(), 2u);
        EXPECT_EQ(records[0].mEffects[0].mConditions[0].reference, 0x03000008u);
        EXPECT_EQ(records[0].mEffects[0].mConditions[1].reference, 0u);
        EXPECT_EQ(records[0].mEffects[1].mBaseEffect.toUint32(), 0u);
    }

    TEST(ESM4EnchantmentTest, keepsAConditionThatComesBeforeTheFirstEffect)
    {
        const auto records = loadRecords<ESM4::Enchantment>("ENCH",
            record("ENCH", 1,
                subRecord("CTDA", conditionData(0x40, 1.f, 14, 0)) + valueSubRecord<std::uint32_t>("EFID", 1)));
        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mConditions.size(), 1u);
        ASSERT_EQ(records[0].mEffects.size(), 1u);
        EXPECT_TRUE(records[0].mEffects[0].mConditions.empty());
    }

}
