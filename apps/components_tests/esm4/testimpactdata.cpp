#include <components/esm4/loadipct.hpp>

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
        return zString("EDID", "Text of EDID") + zString("MODL", "Text of MODL") + subRecord("MODT", bytePattern(5, 13))
            + subRecord("DATA", bytePattern(24, 14)) + subRecord("DODT", bytePattern(36, 15))
            + valueSubRecord<std::uint32_t>("DNAM", 0x00010006) + valueSubRecord<std::uint32_t>("SNAM", 0x00010007)
            + valueSubRecord<std::uint32_t>("NAM1", 0x00010008);
    }

    void expectEverySubRecord(const ESM4::ImpactData& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(std::string(result.mModelTextures.begin(), result.mModelTextures.end()), bytePattern(5, 13));
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(24, 14));
        EXPECT_EQ(std::string(result.mDecalData.begin(), result.mDecalData.end()), bytePattern(36, 15));
        EXPECT_EQ(result.mTextureSet.toUint32(), 0x00010006u);
        EXPECT_EQ(result.mSound1.toUint32(), 0x00010007u);
        EXPECT_EQ(result.mSound2.toUint32(), 0x00010008u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::ImpactData>("IPCT", record("IPCT", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ImpactDataTest, readsEverySubrecord)
    {
        const std::vector<ESM4::ImpactData> records
            = loadRecords<ESM4::ImpactData>("IPCT", record("IPCT", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImpactDataTest, readsACompressedRecord)
    {
        const std::vector<ESM4::ImpactData> records
            = loadRecords<ESM4::ImpactData>("IPCT", compressedRecord("IPCT", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImpactDataTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(23, 'x'))), "ESM4::IPCT::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(25, 'x'))), "ESM4::IPCT::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DODT", std::string(35, 'x'))), "ESM4::IPCT::load - DODT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DODT", std::string(37, 'x'))), "ESM4::IPCT::load - DODT has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(3, 'x'))), "ESM4::IPCT::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(5, 'x'))), "ESM4::IPCT::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(3, 'x'))), "ESM4::IPCT::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(5, 'x'))), "ESM4::IPCT::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("NAM1", std::string(3, 'x'))), "ESM4::IPCT::load - NAM1 has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("NAM1", std::string(5, 'x'))), "ESM4::IPCT::load - NAM1 has an unexpected size");
    }

    TEST(ESM4ImpactDataTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::IPCT::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ImpactDataTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::IPCT::load - record has unread bytes after its last sub-record");
    }

}
