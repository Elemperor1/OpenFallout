#include <components/esm4/loadproj.hpp>

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

    std::string dataBounds(int offset)
    {
        std::string data;
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 0)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 1)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 2)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 3)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 4)));
        append<std::int16_t>(data, static_cast<std::int16_t>(-(100 + offset + 5)));
        return data;
    }

    std::string dataDestruction(int offset)
    {
        std::string data;
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 0)));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 1) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 2) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 3) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 4) % 50));
        return data;
    }

    std::string dataStages(int offset)
    {
        std::string data;
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 0) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 1) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 2) % 50));
        append<std::uint8_t>(data, static_cast<std::uint8_t>(3 + (offset + 3) % 50));
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 4)));
        append<ESM::FormId32>(data, 0x00010000u + offset + 5);
        append<ESM::FormId32>(data, 0x00010000u + offset + 6);
        append<std::int32_t>(data, static_cast<std::int32_t>(-(100000 + offset + 7)));
        return data;
    }

    // DATA with a form ID at each of the offsets that hold one: two lights, the explosion, three sounds and the default
    // weapon source.
    std::string projectileData(std::uint32_t id, std::size_t size = 68)
    {
        return bytesWithFormIds(68, 19, { 16, 20, 36, 40, 56, 60, 64 }, id).substr(0, size);
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("OBND", dataBounds(2)) + zString("FULL", "Text of FULL")
            + zString("MODL", "Text of MODL") + subRecord("MODT", bytePattern(5, 15))
            + subRecord("DEST", dataDestruction(6)) + subRecord("DSTD", dataStages(7))
            + subRecord("DSTD", dataStages(17)) + subRecord("DSTF", "") + subRecord("DATA", projectileData(0x00010001))
            + zString("NAM1", "Text of NAM1") + subRecord("NAM2", bytePattern(48, 21))
            + valueSubRecord<std::uint32_t>("VNAM", 100012);
    }

    void expectEverySubRecord(const ESM4::Projectile& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mBounds.mX1, -102);
        EXPECT_EQ(result.mBounds.mY1, -103);
        EXPECT_EQ(result.mBounds.mZ1, -104);
        EXPECT_EQ(result.mBounds.mX2, -105);
        EXPECT_EQ(result.mBounds.mY2, -106);
        EXPECT_EQ(result.mBounds.mZ2, -107);
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(std::string(result.mModelTextures.begin(), result.mModelTextures.end()), bytePattern(5, 15));
        EXPECT_EQ(result.mDestruction.mHealth, -100006);
        EXPECT_EQ(result.mDestruction.mStageCount, 10);
        EXPECT_EQ(result.mDestruction.mFlags, 11);
        EXPECT_EQ(result.mDestruction.mUnused1, 12);
        EXPECT_EQ(result.mDestruction.mUnused2, 13);
        ASSERT_EQ(result.mStages.size(), 2u);
        EXPECT_EQ(result.mStages[0].mHealthPercent, 10);
        EXPECT_EQ(result.mStages[0].mIndex, 11);
        EXPECT_EQ(result.mStages[0].mDamageStage, 12);
        EXPECT_EQ(result.mStages[0].mFlags, 13);
        EXPECT_EQ(result.mStages[0].mSelfDamagePerSecond, -100011);
        EXPECT_EQ(result.mStages[0].mExplosion, 0x0001000cu);
        EXPECT_EQ(result.mStages[0].mDebris, 0x0001000du);
        EXPECT_EQ(result.mStages[0].mDebrisCount, -100014);
        EXPECT_EQ(result.mStages[1].mHealthPercent, 20);
        EXPECT_EQ(result.mStages[1].mIndex, 21);
        EXPECT_EQ(result.mStages[1].mDamageStage, 22);
        EXPECT_EQ(result.mStages[1].mFlags, 23);
        EXPECT_EQ(result.mStages[1].mSelfDamagePerSecond, -100021);
        EXPECT_EQ(result.mStages[1].mExplosion, 0x00010016u);
        EXPECT_EQ(result.mStages[1].mDebris, 0x00010017u);
        EXPECT_EQ(result.mStages[1].mDebrisCount, -100024);
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), projectileData(0x00010001));
        EXPECT_EQ(result.mMuzzleFlashModel, "Text of NAM1");
        EXPECT_EQ(
            std::string(result.mMuzzleFlashTextures.begin(), result.mMuzzleFlashTextures.end()), bytePattern(48, 21));
        EXPECT_EQ(result.mSoundLevel, 100012u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Projectile>("PROJ", record("PROJ", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ProjectileTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Projectile> records
            = loadRecords<ESM4::Projectile>("PROJ", record("PROJ", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ProjectileTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Projectile> records
            = loadRecords<ESM4::Projectile>("PROJ", compressedRecord("PROJ", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ProjectileTest, adjustsTheFormIdsOfItsDataAndReadsTheLongerForms)
    {
        for (const std::size_t size : { 68u, 80u, 84u })
        {
            const std::string data
                = bytesWithFormIds(84, 19, { 16, 20, 36, 40, 56, 60, 64 }, 0x00000123).substr(0, size);
            const std::vector<ESM4::Projectile> result = loadRecords<ESM4::Projectile>("PROJ",
                record("PROJ", 1, subRecord("DATA", data) + subRecord("NAM2", bytePattern(24 * 5, 1))), 0, nullptr, 3);
            ASSERT_EQ(result.size(), 1u) << size;
            EXPECT_EQ(result[0].mData.size(), size);
            EXPECT_EQ(std::string(result[0].mData.begin(), result[0].mData.end()),
                bytesWithFormIds(84, 19, { 16, 20, 36, 40, 56, 60, 64 }, 0x03000123).substr(0, size));
            EXPECT_EQ(result[0].mMuzzleFlashTextures.size(), 24u * 5);
        }
        // Null form IDs stay null.
        const std::vector<ESM4::Projectile> result = loadRecords<ESM4::Projectile>(
            "PROJ", record("PROJ", 1, subRecord("DATA", projectileData(0))), 0, nullptr, 3);
        ASSERT_EQ(result.size(), 1u);
        EXPECT_EQ(std::string(result[0].mData.begin(), result[0].mData.end()), projectileData(0));
    }

    TEST(ESM4ProjectileTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(11, 'x'))), "ESM4::PROJ::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(13, 'x'))), "ESM4::PROJ::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DEST", std::string(7, 'x'))), "ESM4::PROJ::load - DEST has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DEST", std::string(9, 'x'))), "ESM4::PROJ::load - DEST has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DSTD", std::string(19, 'x'))), "ESM4::PROJ::load - DSTD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DSTD", std::string(21, 'x'))), "ESM4::PROJ::load - DSTD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DSTF", std::string(1, 'x'))), "ESM4::PROJ::load - DSTF has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(85, 'x'))), "ESM4::PROJ::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("VNAM", std::string(3, 'x'))), "ESM4::PROJ::load - VNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("VNAM", std::string(5, 'x'))), "ESM4::PROJ::load - VNAM has an unexpected size");
    }

    TEST(ESM4ProjectileTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::PROJ::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ProjectileTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::PROJ::load - record has unread bytes after its last sub-record");
    }

}
