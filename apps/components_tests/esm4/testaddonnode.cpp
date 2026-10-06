#include <components/esm4/loadaddn.hpp>

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

    std::string dataParticleData(int offset)
    {
        std::string data;
        append<std::uint16_t>(data, static_cast<std::uint16_t>((100 + offset + 0)));
        append<std::uint16_t>(data, static_cast<std::uint16_t>((100 + offset + 1)));
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("OBND", dataBounds(2)) + zString("MODL", "Text of MODL")
            + subRecord("MODT", bytePattern(5, 14)) + valueSubRecord<std::int32_t>("DATA", -100005)
            + valueSubRecord<std::uint32_t>("SNAM", 0x00010006) + subRecord("DNAM", dataParticleData(7));
    }

    void expectEverySubRecord(const ESM4::AddonNode& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mBounds.mX1, -102);
        EXPECT_EQ(result.mBounds.mY1, -103);
        EXPECT_EQ(result.mBounds.mZ1, -104);
        EXPECT_EQ(result.mBounds.mX2, -105);
        EXPECT_EQ(result.mBounds.mY2, -106);
        EXPECT_EQ(result.mBounds.mZ2, -107);
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(std::string(result.mModelTextures.begin(), result.mModelTextures.end()), bytePattern(5, 14));
        EXPECT_EQ(result.mNodeIndex, -100005);
        EXPECT_EQ(result.mSound.toUint32(), 0x00010006u);
        EXPECT_EQ(result.mParticleData.mMasterParticleSystemCap, 107);
        EXPECT_EQ(result.mParticleData.mUnknown, 108);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::AddonNode>("ADDN", record("ADDN", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4AddonNodeTest, readsEverySubrecord)
    {
        const std::vector<ESM4::AddonNode> records
            = loadRecords<ESM4::AddonNode>("ADDN", record("ADDN", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4AddonNodeTest, readsACompressedRecord)
    {
        const std::vector<ESM4::AddonNode> records
            = loadRecords<ESM4::AddonNode>("ADDN", compressedRecord("ADDN", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4AddonNodeTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(11, 'x'))), "ESM4::ADDN::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(13, 'x'))), "ESM4::ADDN::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(3, 'x'))), "ESM4::ADDN::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(5, 'x'))), "ESM4::ADDN::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(3, 'x'))), "ESM4::ADDN::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(5, 'x'))), "ESM4::ADDN::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(3, 'x'))), "ESM4::ADDN::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(5, 'x'))), "ESM4::ADDN::load - DNAM has an unexpected size");
    }

    TEST(ESM4AddonNodeTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::ADDN::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4AddonNodeTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::ADDN::load - record has unread bytes after its last sub-record");
    }

}
