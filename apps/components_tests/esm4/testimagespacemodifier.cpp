#include <components/esm4/loadimad.hpp>

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
        return zString("EDID", "Text of EDID") + subRecord("DNAM", bytePattern(188, 12))
            + subRecord("BNAM", bytePattern(3, 20)) + subRecord("VNAM", bytePattern(4, 21))
            + subRecord(std::string("\005IAD", 4), bytePattern(5, 22))
            + subRecord(std::string("\016IAD", 4), bytePattern(6, 23)) + subRecord("SIAD", bytePattern(7, 24))
            + subRecord("TIAD", bytePattern(3, 25)) + valueSubRecord<std::uint32_t>("RDSD", 0x00010004)
            + valueSubRecord<std::uint32_t>("RDSI", 0x00010005);
    }

    void expectEverySubRecord(const ESM4::ImageSpaceModifier& result)
    {
        EXPECT_EQ(result.mEditorId, "Text of EDID");
        EXPECT_EQ(std::string(result.mData.begin(), result.mData.end()), bytePattern(188, 12));
        ASSERT_EQ(result.mTracks.size(), 6);
        EXPECT_EQ(result.mTracks[0].mType, ESM::fourCC("BNAM"));
        EXPECT_EQ(std::string(result.mTracks[0].mData.begin(), result.mTracks[0].mData.end()), bytePattern(3, 20));
        EXPECT_EQ(result.mTracks[1].mType, ESM::fourCC("VNAM"));
        EXPECT_EQ(std::string(result.mTracks[1].mData.begin(), result.mTracks[1].mData.end()), bytePattern(4, 21));
        EXPECT_EQ(result.mTracks[2].mType, ESM::fourCC("\005IAD"));
        EXPECT_EQ(std::string(result.mTracks[2].mData.begin(), result.mTracks[2].mData.end()), bytePattern(5, 22));
        EXPECT_EQ(result.mTracks[3].mType, ESM::fourCC("\016IAD"));
        EXPECT_EQ(std::string(result.mTracks[3].mData.begin(), result.mTracks[3].mData.end()), bytePattern(6, 23));
        EXPECT_EQ(result.mTracks[4].mType, ESM::fourCC("SIAD"));
        EXPECT_EQ(std::string(result.mTracks[4].mData.begin(), result.mTracks[4].mData.end()), bytePattern(7, 24));
        EXPECT_EQ(result.mTracks[5].mType, ESM::fourCC("TIAD"));
        EXPECT_EQ(std::string(result.mTracks[5].mData.begin(), result.mTracks[5].mData.end()), bytePattern(3, 25));
        EXPECT_EQ(result.mRdsd.toUint32(), 0x00010004u);
        EXPECT_EQ(result.mRdsi.toUint32(), 0x00010005u);
    }

    std::string loadFailure(const std::string& data)
    {
        try
        {
            loadRecords<ESM4::ImageSpaceModifier>("IMAD", record("IMAD", 1, data));
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(ESM4ImageSpaceModifierTest, readsEverySubrecord)
    {
        const std::vector<ESM4::ImageSpaceModifier> records
            = loadRecords<ESM4::ImageSpaceModifier>("IMAD", record("IMAD", 7, everySubRecord(), 0x20));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mId.toUint32(), 7u);
        EXPECT_EQ(records[0].mFlags, 0x20u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImageSpaceModifierTest, readsACompressedRecord)
    {
        const std::vector<ESM4::ImageSpaceModifier> records
            = loadRecords<ESM4::ImageSpaceModifier>("IMAD", compressedRecord("IMAD", 7, everySubRecord()));

        ASSERT_EQ(records.size(), 1u);
        expectEverySubRecord(records[0]);
    }

    TEST(ESM4ImageSpaceModifierTest, rejectsASizeThatNoGameUses)
    {
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(245, 'x'))), "ESM4::IMAD::load - DNAM has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RDSD", std::string(3, 'x'))), "ESM4::IMAD::load - RDSD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RDSD", std::string(5, 'x'))), "ESM4::IMAD::load - RDSD has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RDSI", std::string(3, 'x'))), "ESM4::IMAD::load - RDSI has an unexpected size");
        EXPECT_EQ(
            loadFailure(subRecord("RDSI", std::string(5, 'x'))), "ESM4::IMAD::load - RDSI has an unexpected size");
    }

    TEST(ESM4ImageSpaceModifierTest, readsDataThatEndsAfterAnyMember)
    {
        for (const std::size_t size : { 188u, 192u, 225u, 226u, 232u, 236u, 240u, 244u })
        {
            const std::vector<ESM4::ImageSpaceModifier> records = loadRecords<ESM4::ImageSpaceModifier>(
                "IMAD", record("IMAD", 1, subRecord("DNAM", bytePattern(size, 1))));
            ASSERT_EQ(records.size(), 1u) << size;
            EXPECT_EQ(records[0].mData.size(), size);
        }
        EXPECT_EQ(
            loadFailure(subRecord("DNAM", std::string(187, 'x'))), "ESM4::IMAD::load - DNAM has an unexpected size");
    }

    TEST(ESM4ImageSpaceModifierTest, rejectsAnUnknownSubrecord)
    {
        EXPECT_EQ(
            loadFailure(zString("EDID", "x") + subRecord("ZZZZ", "1")), "ESM4::IMAD::load - Unknown subrecord ZZZZ");
    }

    TEST(ESM4ImageSpaceModifierTest, rejectsBytesNoSubrecordAccountsFor)
    {
        EXPECT_EQ(loadFailure(everySubRecord() + std::string(3, '\0')),
            "ESM4::IMAD::load - record has unread bytes after its last sub-record");
    }

}
