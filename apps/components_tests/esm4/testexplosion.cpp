#include <components/esm4/loadexpl.hpp>

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

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("OBND", dataBounds(2)) + zString("FULL", "Text of FULL")
            + zString("MODL", "Text of MODL") + subRecord("MODT", bytePattern(5, 15))
            + valueSubRecord<std::uint32_t>("EITM", 0x00010006) + valueSubRecord<std::uint32_t>("MNAM", 0x00010007)
            + valueSubRecord<std::uint32_t>("INAM", 0x00010008) + subRecord("DATA", bytePattern(52, 19));
    }

    void expectEverySubRecord(const ESM4::Explosion& result)
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
        EXPECT_EQ(result.mObjectEffect.toUint32(), 0x00010006u);
        EXPECT_EQ(result.mImageSpaceModifier.toUint32(), 0x00010007u);
        EXPECT_EQ(result.mPlacedImpactObject.toUint32(), 0x00010008u);
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(52, 19));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Explosion>("EXPL", record("EXPL", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ExplosionTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Explosion> records
            = loadRecords<ESM4::Explosion>("EXPL", record("EXPL", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ExplosionTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Explosion> records
            = loadRecords<ESM4::Explosion>("EXPL", compressedRecord("EXPL", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ExplosionTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(11, 'x'))), "ESM4::EXPL::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(13, 'x'))), "ESM4::EXPL::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EITM", std::string(3, 'x'))), "ESM4::EXPL::load - EITM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("EITM", std::string(5, 'x'))), "ESM4::EXPL::load - EITM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("MNAM", std::string(3, 'x'))), "ESM4::EXPL::load - MNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("MNAM", std::string(5, 'x'))), "ESM4::EXPL::load - MNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("INAM", std::string(3, 'x'))), "ESM4::EXPL::load - INAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("INAM", std::string(5, 'x'))), "ESM4::EXPL::load - INAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(51, 'x'))), "ESM4::EXPL::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(53, 'x'))), "ESM4::EXPL::load - DATA has an unexpected size");
    }

    TEST(ESM4ExplosionTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::EXPL::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ExplosionTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::EXPL::load - record has unread bytes after its last sub-record");
    }

}
