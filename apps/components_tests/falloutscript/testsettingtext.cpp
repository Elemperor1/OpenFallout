#include <gtest/gtest.h>

#include <components/falloutscript/settingtext.hpp>

namespace
{
    using namespace FalloutScript;

    TEST(FalloutSettingText, a_label_of_the_menu_files_has_its_words)
    {
        EXPECT_EQ(SettingText::standIn("sCancel"), "Cancel");
        EXPECT_EQ(SettingText::standIn("sOK"), "OK");
        EXPECT_EQ(SettingText::standIn("sInventoryWeightUpper"), "WG");
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
