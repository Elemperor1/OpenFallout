#include <components/esm4/loadchip.hpp>

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
            + zString("MODL", "Text of MODL") + zString("ICON", "Text of ICON")
            + valueSubRecord<std::uint32_t>("YNAM", 0x00010006) + valueSubRecord<std::uint32_t>("ZNAM", 0x00010007);
    }

    void expectEverySubRecord(const ESM4::PokerChip& result)
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
        EXPECT_EQ(result.mIcon, "Text of ICON");
        EXPECT_EQ(result.mPickUpSound.toUint32(), 0x00010006u);
        EXPECT_EQ(result.mDropSound.toUint32(), 0x00010007u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::PokerChip>("CHIP", record("CHIP", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4PokerChipTest, readsEverySubrecord)
    {
        const std::vector<ESM4::PokerChip> records
            = loadRecords<ESM4::PokerChip>("CHIP", record("CHIP", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4PokerChipTest, readsACompressedRecord)
    {
        const std::vector<ESM4::PokerChip> records
            = loadRecords<ESM4::PokerChip>("CHIP", compressedRecord("CHIP", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4PokerChipTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(11, 'x'))), "ESM4::CHIP::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("OBND", std::string(13, 'x'))), "ESM4::CHIP::load - OBND has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("YNAM", std::string(3, 'x'))), "ESM4::CHIP::load - YNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("YNAM", std::string(5, 'x'))), "ESM4::CHIP::load - YNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ZNAM", std::string(3, 'x'))), "ESM4::CHIP::load - ZNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("ZNAM", std::string(5, 'x'))), "ESM4::CHIP::load - ZNAM has an unexpected size");
    }

    TEST(ESM4PokerChipTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CHIP::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4PokerChipTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CHIP::load - record has unread bytes after its last sub-record");
    }

}
