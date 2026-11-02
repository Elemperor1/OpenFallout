#include <gtest/gtest.h>

#include <components/falloutscript/settingtext.hpp>

#include <cstddef>
#include <utility>

namespace
{
    using namespace FalloutScript;

    TEST(FalloutSettingText, a_label_of_the_menu_files_has_its_words)
    {
        EXPECT_EQ(SettingText::standIn("sCancel"), "Cancel");
        EXPECT_EQ(SettingText::standIn("sOK"), "OK");
        EXPECT_EQ(SettingText::standIn("sInventoryWeightUpper"), "WG");
    }

    TEST(FalloutSettingText, the_words_of_the_data_page_have_the_lengths_the_executables_give_them)
    {
        // Read from the default texts of FalloutNV.exe and Fallout3.exe (the same in both): the length of each, and
        // that only sData is upper case
        const std::pair<const char*, size_t> lengths[] = {
            { "sTravel", 6 },
            { "sMakeActiveQuest", 17 },
            { "sPlayAudio", 10 },
            { "sTune", 4 },
            { "sStopAudio", 10 },
            { "sPlaceMarker", 12 },
            { "sShowLocation", 13 },
            { "sShowAllNotes", 14 },
            { "sShowActiveNotes", 23 },
            { "sNextChallenge", 19 },
            { "sData", 4 },
            { "sLocalMapTabText", 9 },
            { "sWorldMapTabText", 9 },
            { "sQuestsTabText", 6 },
            { "sMiscTabText", 4 },
            { "sNotesTabText", 5 },
            { "sCommsTabText", 5 },
        };
        for (const auto& [name, length] : lengths)
            EXPECT_EQ(SettingText::standIn(name).size(), length) << name;
        EXPECT_EQ(SettingText::standIn("sData"), "DATA");
    }

    TEST(FalloutSettingText, the_counters_of_the_character_making_screens_are_formats_with_the_points_left)
    {
        // Read from the executables as format strings (19 and 12 characters, the same in both games)
        EXPECT_EQ(SettingText::standIn("sAttributesCount"), "Distribute %d Point");
        EXPECT_EQ(SettingText::standIn("sSkillsCount"), "Tag %d Skill");
        // And the titles, read as format strings too (33 and 23 characters, two %d each)
        EXPECT_EQ(SettingText::standIn("sAttributesTitle"), "ATTRIBUTES: %d/%d Points Assigned");
        EXPECT_EQ(SettingText::standIn("sSkillsTitle"), "SKILLS:  %d/%d Selected");
        // The trait menu (New Vegas): the title takes the most that can be picked, the counter the picks that are left
        EXPECT_EQ(SettingText::standIn("sTraitMenuTitleText"), "CHOOSE UP TO %d TRAITS");
        EXPECT_EQ(SettingText::standIn("sTraitMenuTitleTextSingular"), "CHOOSE %d TRAIT");
        EXPECT_EQ(SettingText::standIn("sTraitMenuCounter"), "%d TRAITS LEFT");
        EXPECT_EQ(SettingText::standIn("sTraitMenuCounterSingular"), "%d TRAIT LEFT");
    }

    TEST(FalloutSettingText, the_names_are_not_case_sensitive)
    {
        // The menu files spell some of the names two ways (sOK and sOk, sQuit and squit)
        EXPECT_EQ(SettingText::standIn("sOk"), SettingText::standIn("sOK"));
        EXPECT_EQ(SettingText::standIn("squit"), "Quit");
        EXPECT_EQ(SettingText::standIn("SQUIT"), "Quit");
    }

    TEST(FalloutSettingText, a_name_that_is_not_there_is_an_empty_text)
    {
        EXPECT_TRUE(SettingText::standIn("sNoSuchSetting").empty());
        EXPECT_TRUE(SettingText::standIn("").empty());
    }
}
