#include <components/esm4/loadcdck.hpp>

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
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL")
            + valueSubRecord<std::uint32_t>("CARD", 0x00010003) + valueSubRecord<std::uint32_t>("CARD", 0x00020003)
            + valueSubRecord<std::uint32_t>("DATA", 100004);
    }

    void expectEverySubRecord(const ESM4::CaravanDeck& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        ASSERT_EQ(result.mCards.size(), 2u);
        EXPECT_EQ(result.mCards[0].toUint32(), 0x00010003u);
        EXPECT_EQ(result.mCards[1].toUint32(), 0x00020003u);
        EXPECT_EQ(result.mData, 100004u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::CaravanDeck>("CDCK", record("CDCK", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CaravanDeckTest, readsEverySubrecord)
    {
        const std::vector<ESM4::CaravanDeck> records
            = loadRecords<ESM4::CaravanDeck>("CDCK", record("CDCK", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CaravanDeckTest, readsACompressedRecord)
    {
        const std::vector<ESM4::CaravanDeck> records
            = loadRecords<ESM4::CaravanDeck>("CDCK", compressedRecord("CDCK", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CaravanDeckTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("CARD", std::string(3, 'x'))), "ESM4::CDCK::load - CARD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("CARD", std::string(5, 'x'))), "ESM4::CDCK::load - CARD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(3, 'x'))), "ESM4::CDCK::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(5, 'x'))), "ESM4::CDCK::load - DATA has an unexpected size");
    }

    TEST(ESM4CaravanDeckTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CDCK::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CaravanDeckTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CDCK::load - record has unread bytes after its last sub-record");
    }

}
