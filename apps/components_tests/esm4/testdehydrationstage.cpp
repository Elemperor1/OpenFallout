#include <components/esm4/loaddehy.hpp>

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
        append<ESM::FormId32>(data, 0x00010000u + offset + 1);
        return data;
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + subRecord("DATA", dataData(2));
    }

    void expectEverySubRecord(const ESM4::DehydrationStage& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mData.mTriggerThreshold, 100002u);
        EXPECT_EQ(result.mData.mActorEffect, 0x00010003u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::DehydrationStage>("DEHY", record("DEHY", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4DehydrationStageTest, readsEverySubrecord)
    {
        const std::vector<ESM4::DehydrationStage> records
            = loadRecords<ESM4::DehydrationStage>("DEHY", record("DEHY", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4DehydrationStageTest, readsACompressedRecord)
    {
        const std::vector<ESM4::DehydrationStage> records
            = loadRecords<ESM4::DehydrationStage>("DEHY", compressedRecord("DEHY", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4DehydrationStageTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(7, 'x'))), "ESM4::DEHY::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(9, 'x'))), "ESM4::DEHY::load - DATA has an unexpected size");
    }

    TEST(ESM4DehydrationStageTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::DEHY::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4DehydrationStageTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::DEHY::load - record has unread bytes after its last sub-record");
    }

}
