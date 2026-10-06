#include <components/esm4/loadrgdl.hpp>

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
        return zString("EDID", "Text of EDID") + valueSubRecord<std::uint32_t>("NVER", 100002)
            + subRecord("DATA", bytePattern(14, 13)) + valueSubRecord<std::uint32_t>("XNAM", 0x00010004)
            + valueSubRecord<std::uint32_t>("TNAM", 0x00010005) + subRecord("RAFD", bytePattern(60, 16))
            + subRecord("RAFB", bytePattern(5, 17)) + subRecord("RAPS", bytePattern(24, 18))
            + zString("ANAM", "Text of ANAM");
    }

    void expectEverySubRecord(const ESM4::Ragdoll& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mVersion, 100002u);
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(14, 13));
        EXPECT_EQ(result.mActorBase.toUint32(), 0x00010004u);
        EXPECT_EQ(result.mBodyPartData.toUint32(), 0x00010005u);
        EXPECT_EQ(std::string(result.mFeedbackData.begin(), result.mFeedbackData.end()), bytePattern(60, 16));
        EXPECT_EQ(
            std::string(result.mFeedbackDynamicBones.begin(), result.mFeedbackDynamicBones.end()), bytePattern(5, 17));
        EXPECT_EQ(std::string(result.mPoseMatching.begin(), result.mPoseMatching.end()), bytePattern(24, 18));
        EXPECT_EQ(result.mDeathPose, "Text of ANAM");
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Ragdoll>("RGDL", record("RGDL", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4RagdollTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Ragdoll> records
            = loadRecords<ESM4::Ragdoll>("RGDL", record("RGDL", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4RagdollTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Ragdoll> records
            = loadRecords<ESM4::Ragdoll>("RGDL", compressedRecord("RGDL", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4RagdollTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("NVER", std::string(3, 'x'))), "ESM4::RGDL::load - NVER has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("NVER", std::string(5, 'x'))), "ESM4::RGDL::load - NVER has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(13, 'x'))), "ESM4::RGDL::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(15, 'x'))), "ESM4::RGDL::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("XNAM", std::string(3, 'x'))), "ESM4::RGDL::load - XNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("XNAM", std::string(5, 'x'))), "ESM4::RGDL::load - XNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TNAM", std::string(3, 'x'))), "ESM4::RGDL::load - TNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("TNAM", std::string(5, 'x'))), "ESM4::RGDL::load - TNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RAFD", std::string(59, 'x'))), "ESM4::RGDL::load - RAFD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RAFD", std::string(61, 'x'))), "ESM4::RGDL::load - RAFD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RAPS", std::string(23, 'x'))), "ESM4::RGDL::load - RAPS has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RAPS", std::string(25, 'x'))), "ESM4::RGDL::load - RAPS has an unexpected size");
    }

    TEST(ESM4RagdollTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::RGDL::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4RagdollTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::RGDL::load - record has unread bytes after its last sub-record");
    }

}
