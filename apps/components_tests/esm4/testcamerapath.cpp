#include <components/esm4/loadcpth.hpp>

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

    std::string dataRelated(int offset)
    {
        std::string data;
        append<ESM::FormId32>(data, 0x00010000u + offset + 0);
        append<ESM::FormId32>(data, 0x00010000u + offset + 1);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("CTDA", conditionData(0x42, 2.5f, 14, 0x00010002))
            + subRecord("ANAM", dataRelated(3)) + valueSubRecord<std::uint8_t>("DATA", 7)
            + valueSubRecord<std::uint32_t>("SNAM", 0x00010005) + valueSubRecord<std::uint32_t>("SNAM", 0x00020005);
    }

    void expectEverySubRecord(const ESM4::CameraPath& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        ASSERT_EQ(result.mConditions.size(), 1u);
        EXPECT_EQ(result.mConditions[0].condition, 0x42u);
        EXPECT_EQ(result.mConditions[0].comparison, 2.5f);
        EXPECT_EQ(result.mConditions[0].functionIndex, 14u);
        EXPECT_EQ(result.mConditions[0].reference, 0x00010002u);
        EXPECT_EQ(result.mRelated.mParent, 0x00010003u);
        EXPECT_EQ(result.mRelated.mPrevious, 0x00010004u);
        EXPECT_EQ(result.mZoom, 7);
        ASSERT_EQ(result.mCameraShots.size(), 2u);
        EXPECT_EQ(result.mCameraShots[0].toUint32(), 0x00010005u);
        EXPECT_EQ(result.mCameraShots[1].toUint32(), 0x00020005u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::CameraPath>("CPTH", record("CPTH", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CameraPathTest, readsEverySubrecord)
    {
        const std::vector<ESM4::CameraPath> records
            = loadRecords<ESM4::CameraPath>("CPTH", record("CPTH", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CameraPathTest, readsACompressedRecord)
    {
        const std::vector<ESM4::CameraPath> records
            = loadRecords<ESM4::CameraPath>("CPTH", compressedRecord("CPTH", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CameraPathTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(24, 'x'))), "ESM4::CPTH::load - CTDA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CTDA", std::string(32, 'x'))), "ESM4::CPTH::load - CTDA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ANAM", std::string(7, 'x'))), "ESM4::CPTH::load - ANAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ANAM", std::string(9, 'x'))), "ESM4::CPTH::load - ANAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(0, 'x'))), "ESM4::CPTH::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(2, 'x'))), "ESM4::CPTH::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(3, 'x'))), "ESM4::CPTH::load - SNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("SNAM", std::string(5, 'x'))), "ESM4::CPTH::load - SNAM has an unexpected size");
    }

    TEST(ESM4CameraPathTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CPTH::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CameraPathTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CPTH::load - record has unread bytes after its last sub-record");
    }

}
