#include <components/esm4/loadcsno.hpp>

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

    std::string data(std::uint32_t currency, std::uint32_t quest)
    {
        std::string result;
        append<float>(result, 0.25f);
        append<float>(result, 1.5f);
        for (std::uint32_t stop = 1; stop <= 7; ++stop)
            append(result, stop);
        append<std::uint32_t>(result, 6); // number of decks
        append<std::uint32_t>(result, 900); // max winnings
        append(result, currency);
        append(result, quest);
        append<std::uint32_t>(result, 1); // flags
        return subRecord("DATA", result);
    }

    std::string everySubRecord()
    {
        return zString("EDID", "Text of EDID") + zString("FULL", "Text of FULL") + data(0x00010001, 0x00010002)
            + zString("MODL", "First of MODL") + zString("MODL", "Second of MODL") + zString("MOD2", "Text of MOD2")
            + zString("MOD3", "Text of MOD3") + zString("MOD4", "Text of MOD4") + zString("ICON", "First of ICON")
            + zString("ICON", "Second of ICON") + zString("ICO2", "First of ICO2") + zString("ICO2", "Second of ICO2");
    }

    void expectEverySubRecord(const ESM4::Casino& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(result.mFullName, "Text of FULL");
        EXPECT_EQ(result.mData.mDecksPercentBeforeShuffle, 0.25f);
        EXPECT_EQ(result.mData.mBlackjackPayoutRatio, 1.5f);
        EXPECT_THAT(result.mData.mSlotReelStops, ElementsAre(1, 2, 3, 4, 5, 6, 7));
        EXPECT_EQ(result.mData.mNumberOfDecks, 6u);
        EXPECT_EQ(result.mData.mMaxWinnings, 900u);
        EXPECT_EQ(result.mData.mCurrency, 0x00010001u);
        EXPECT_EQ(result.mData.mWinningsQuest, 0x00010002u);
        EXPECT_EQ(result.mData.mFlags, 1u);
        EXPECT_THAT(result.mModels, ElementsAre("First of MODL", "Second of MODL"));
        EXPECT_EQ(result.mModel2, "Text of MOD2");
        EXPECT_EQ(result.mModel3, "Text of MOD3");
        EXPECT_EQ(result.mModel4, "Text of MOD4");
        EXPECT_THAT(result.mIcons, ElementsAre("First of ICON", "Second of ICON"));
        EXPECT_THAT(result.mIcons2, ElementsAre("First of ICO2", "Second of ICO2"));
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::Casino>("CSNO", record("CSNO", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4CasinoTest, readsEverySubrecord)
    {
        const std::vector<ESM4::Casino> records
            = loadRecords<ESM4::Casino>("CSNO", record("CSNO", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CasinoTest, readsACompressedRecord)
    {
        const std::vector<ESM4::Casino> records
            = loadRecords<ESM4::Casino>("CSNO", compressedRecord("CSNO", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4CasinoTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(55, 'x'))), "ESM4::CSNO::load - DATA has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("DATA", std::string(57, 'x'))), "ESM4::CSNO::load - DATA has an unexpected size");
    }

    TEST(ESM4CasinoTest, adjustsTheFormIdsOfTheDataToTheLoadOrderAndKeepsNullOnesNull)
    {
        const std::vector<ESM4::Casino> records = loadRecords<ESM4::Casino>(
            "CSNO", record("CSNO", 1, data(0x00000123, 0x00000456)) + record("CSNO", 2, data(0, 0)), 0, nullptr, 3);

        ASSERT_EQ(records.size(), 2u);
        EXPECT_EQ(records[0].mData.mCurrency, 0x03000123u);
        EXPECT_EQ(records[0].mData.mWinningsQuest, 0x03000456u);
        EXPECT_EQ(records[1].mData.mCurrency, 0u);
        EXPECT_EQ(records[1].mData.mWinningsQuest, 0u);
    }

    TEST(ESM4CasinoTest, keepsTheModelSubrecordsTheReferenceDoesNotListForACasino)
    {
        const std::vector<ESM4::Casino> records = loadRecords<ESM4::Casino>("CSNO",
            record("CSNO", 1,
                zString("MODL", "First of MODL") + valueSubRecord<float>("MODB", 2.5f)
                    + subRecord("MODT", bytePattern(5, 10)) + subRecord("MODS", alternateTextureData({}))
                    + valueSubRecord<std::uint8_t>("MODD", 3) + zString("MOD2", "Text of MOD2")
                    + subRecord("MO2T", bytePattern(4, 20)) + subRecord("MO2S", alternateTextureData({}))
                    + zString("MOD3", "Text of MOD3") + subRecord("MO3T", bytePattern(3, 30))
                    + subRecord("MO3S", alternateTextureData({})) + valueSubRecord<std::uint8_t>("MOSD", 1)
                    + zString("MOD4", "Text of MOD4") + subRecord("MO4T", bytePattern(2, 40))
                    + subRecord("MO4S", alternateTextureData({}))));

        ASSERT_EQ(records.size(), 1u);
        const std::vector<ESM4::RawSubRecord>& data = records[0].mModelData;
        ASSERT_EQ(data.size(), 11u);
        EXPECT_EQ(data[0].mType, ESM::fourCC("MODB"));
        EXPECT_EQ(std::string(data[1].mData.begin(), data[1].mData.end()), bytePattern(5, 10));
        EXPECT_EQ(data[4].mType, ESM::fourCC("MO2T"));
        EXPECT_EQ(data[8].mType, ESM::fourCC("MOSD"));
        EXPECT_EQ(data[10].mType, ESM::fourCC("MO4S"));
    }

    TEST(ESM4CasinoTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::CSNO::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4CasinoTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::CSNO::load - record has unread bytes after its last sub-record");
    }

}
