#include <components/esm4/loadcams.hpp>

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
        return zString("EDID", "Text of EDID") + zString("MODL", "Text of MODL") + valueSubRecord<float>("MODB", 4.5f)
            + subRecord("DATA", bytePattern(36, 14)) + valueSubRecord<std::uint32_t>("MNAM", 0x00010005);
    }

    void expectEverySubRecord(const ESM4::CameraShot& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mModel.getOriginal(), "Text of MODL");
        EXPECT_EQ(result.mBoundRadius, 4.5f);
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(36, 14));
        EXPECT_EQ(result.mImageSpaceModifier.toUint32(), 0x00010005u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::CameraShot>("CAMS", record("CAMS", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CameraShotTest, readsEverySubrecord)
    {
        const std::vector<ESM4::CameraShot> records
            = loadRecords<ESM4::CameraShot>("CAMS", record("CAMS", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CameraShotTest, readsACompressedRecord)
    {
        const std::vector<ESM4::CameraShot> records
            = loadRecords<ESM4::CameraShot>("CAMS", compressedRecord("CAMS", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CameraShotTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("MODB", std::string(3, 'x'))), "ESM4::CAMS::load - MODB has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("MODB", std::string(5, 'x'))), "ESM4::CAMS::load - MODB has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(41, 'x'))), "ESM4::CAMS::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("MNAM", std::string(3, 'x'))), "ESM4::CAMS::load - MNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("MNAM", std::string(5, 'x'))), "ESM4::CAMS::load - MNAM has an unexpected size");
    }

    TEST(ESM4CameraShotTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CAMS::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CameraShotTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CAMS::load - record has unread bytes after its last sub-record");
    }

}
