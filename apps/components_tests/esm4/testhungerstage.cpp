#include <components/esm4/loadhung.hpp>

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

    void expectEverySubRecord(const ESM4::HungerStage& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mData.mTriggerThreshold, 100002u);
        EXPECT_EQ(result.mData.mActorEffect, 0x00010003u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::HungerStage>("HUNG", record("HUNG", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4HungerStageTest, readsEverySubrecord)
    {
        const std::vector<ESM4::HungerStage> records
            = loadRecords<ESM4::HungerStage>("HUNG", record("HUNG", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4HungerStageTest, readsACompressedRecord)
    {
        const std::vector<ESM4::HungerStage> records
            = loadRecords<ESM4::HungerStage>("HUNG", compressedRecord("HUNG", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4HungerStageTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(7, 'x'))), "ESM4::HUNG::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(9, 'x'))), "ESM4::HUNG::load - DATA has an unexpected size");
    }

    TEST(ESM4HungerStageTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::HUNG::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4HungerStageTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::HUNG::load - record has unread bytes after its last sub-record");
    }

}
