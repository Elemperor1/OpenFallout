#include <components/esm4/loadnote.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    TEST(ESM4NoteTest, readsTheTypeAndTheTextOfANoteOfText)
    {
        const std::vector<ESM4::Note> records = loadRecords<ESM4::Note>("NOTE",
            record("NOTE", 7,
                zString("EDID", "Text of EDID") + zString("FULL", "A note") + valueSubRecord<std::uint8_t>("DATA", 1)
                    + zString("TNAM", "Press &sUActnJump; to jump.")));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mEditorId, "Text of EDID");
        EXPECT_EQ(records[0].mFullName, "A note");
        EXPECT_EQ(records[0].mType, 1);
        EXPECT_EQ(records[0].mText, "Press &sUActnJump; to jump.");
    }

    TEST(ESM4NoteTest, keepsTheTopicOfANoteOfVoiceOutOfTheText)
    {
        const std::vector<ESM4::Note> records = loadRecords<ESM4::Note>("NOTE",
            record("NOTE", 7,
                valueSubRecord<std::uint8_t>("DATA", 3) + valueSubRecord<std::uint32_t>("TNAM", 0x00010001)));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mType, 3);
        EXPECT_TRUE(records[0].mText.empty());
    }

    TEST(ESM4NoteTest, readsATextThatComesBeforeItsType)
    {
        const std::vector<ESM4::Note> records = loadRecords<ESM4::Note>(
            "NOTE", record("NOTE", 7, zString("TNAM", "Early.") + valueSubRecord<std::uint8_t>("DATA", 1)));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mText, "Early.");
    }

    TEST(ESM4NoteTest, dropsATopicThatComesBeforeTheTypeOfAVoiceNote)
    {
        const std::vector<ESM4::Note> records = loadRecords<ESM4::Note>("NOTE",
            record("NOTE", 7,
                valueSubRecord<std::uint32_t>("TNAM", 0x00010001) + valueSubRecord<std::uint8_t>("DATA", 3)));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mType, 3);
        EXPECT_TRUE(records[0].mText.empty());
    }

    TEST(ESM4NoteTest, hasNoTypeAndNoTextWithoutThem)
    {
        const std::vector<ESM4::Note> records
            = loadRecords<ESM4::Note>("NOTE", record("NOTE", 7, zString("EDID", "x")));

        ASSERT_EQ(records.size(), 1u);
        EXPECT_EQ(records[0].mType, 255);
        EXPECT_TRUE(records[0].mText.empty());
    }
}
