#include <components/esm4/loadmgef.hpp>

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
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 0)));
        append<float>(data, static_cast<float>(offset + 1) + 0.5f);
        append<ESM::FormId32>(data, 0x00010000u + offset + 2);
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 3)));
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 4)));
        append<std::uint16_t>(data, static_cast<std::uint16_t>((100 + offset + 5)));
        append<std::uint16_t>(data, static_cast<std::uint16_t>((100 + offset + 6)));
        append<ESM::FormId32>(data, 0x00010000u + offset + 7);
        append<float>(data, static_cast<float>(offset + 8) + 0.5f);
        append<ESM::FormId32>(data, 0x00010000u + offset + 9);
        append<ESM::FormId32>(data, 0x00010000u + offset + 10);
        append<ESM::FormId32>(data, 0x00010000u + offset + 11);
        append<ESM::FormId32>(data, 0x00010000u + offset + 12);
        append<ESM::FormId32>(data, 0x00010000u + offset + 13);
        append<ESM::FormId32>(data, 0x00010000u + offset + 14);
        append<float>(data, static_cast<float>(offset + 15) + 0.5f);
        append<float>(data, static_cast<float>(offset + 16) + 0.5f);
        append<std::uint32_t>(data, static_cast<std::uint32_t>((100000 + offset + 17)));
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 18)));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + zString("DESC", "Text of DESC")
            + zString("MODL", "Text of MODL") + subRecord("DATA", dataData(5));
    }

    void expectEverySubRecord(const ESM4::MagicEffect& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mDescription, "Text of DESC");
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(result.mData.mEffectFlags, 100005u);
        EXPECT_EQ(result.mData.mBaseCost, 6.5f);
        EXPECT_EQ(result.mData.mAssociatedItem, 0x00010007u);
        EXPECT_EQ(result.mData.mMagicSchool, -100008);
        EXPECT_EQ(result.mData.mResistValue, -100009);
        EXPECT_EQ(result.mData.mCounterEffectCount, 110);
        EXPECT_EQ(result.mData.mUnused, 111);
        EXPECT_EQ(result.mData.mLight, 0x0001000cu);
        EXPECT_EQ(result.mData.mProjectileSpeed, 13.5f);
        EXPECT_EQ(result.mData.mEffectShader, 0x0001000eu);
        EXPECT_EQ(result.mData.mObjectDisplayShader, 0x0001000fu);
        EXPECT_EQ(result.mData.mEffectSound, 0x00010010u);
        EXPECT_EQ(result.mData.mBoltSound, 0x00010011u);
        EXPECT_EQ(result.mData.mHitSound, 0x00010012u);
        EXPECT_EQ(result.mData.mAreaSound, 0x00010013u);
        EXPECT_EQ(result.mData.mConstantEffectEnchantmentFactor, 20.5f);
        EXPECT_EQ(result.mData.mConstantEffectBarterFactor, 21.5f);
        EXPECT_EQ(result.mData.mArchetype, 100022u);
        EXPECT_EQ(result.mData.mActorValue, -100023);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::MagicEffect>("MGEF", record("MGEF", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4MagicEffectTest, readsEverySubrecord)
    {
        const std::vector<ESM4::MagicEffect> records
            = loadRecords<ESM4::MagicEffect>("MGEF", record("MGEF", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4MagicEffectTest, readsACompressedRecord)
    {
        const std::vector<ESM4::MagicEffect> records
            = loadRecords<ESM4::MagicEffect>("MGEF", compressedRecord("MGEF", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4MagicEffectTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(71, 'x'))), "ESM4::MGEF::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(73, 'x'))), "ESM4::MGEF::load - DATA has an unexpected size");
    }

    TEST(ESM4MagicEffectTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::MGEF::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4MagicEffectTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::MGEF::load - record has unread bytes after its last sub-record");
    }

}
