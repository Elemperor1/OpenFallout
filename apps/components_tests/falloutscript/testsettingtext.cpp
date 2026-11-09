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

    TEST(FalloutSettingText, the_pause_menu_has_the_words_its_list_its_questions_and_its_saves_ask_for)
    {
        // The items of the list, the choices and questions of the confirm box, and the rows of the list of saves (the
        // setting names are the ones round 12V read from the executable)
        for (const char* name : { "sContinue", "sSave", "sLoad", "sSettings", "sQuit", "sBack", "sConfirmQuit",
                 "sExitGameAffirm", "sCancel", "sYes", "sNo", "sConfirmSave", "sConfirmLoad", "sNewSave",
                 "sSavesRemaining", "sNoSaves", "sLevelAbbrev", "sPlayTime", "sQuicksaveAbbrev", "sAutosaveAbbrev" })
            EXPECT_FALSE(SettingText::standIn(name).empty()) << name;
    }

    TEST(FalloutSettingText, the_title_screen_has_the_words_of_its_rows_and_its_two_questions)
    {
        // Continue and New ask before they act, Credits is a row of the title list only (Mac rounds 12AI A2 and 12BN C)
        for (const char* name : { "sNew", "sCrew", "sConfirmContinue", "sConfirmNew" })
            EXPECT_FALSE(SettingText::standIn(name).empty()) << name;
    }

    TEST(FalloutSettingText, the_settings_screens_have_the_words_of_their_labels_and_values)
    {
        // The four submenus, the items of each, and the words a toggle goes through (the names are the ones rounds 12W
        // and 12Y read from the executable). The offline harness answers every name with a word of its own, so a name
        // that is missing here shows as the name itself in the running game (round 12AB)
        for (const char* name :
            { "sGameplay", "sDisplay", "sAudio", "sControls", "sKillCamMode", "sKillCamModeNone", "sKillCamModePlayer",
                "sKillCamModeCinematic", "sHardcore", "sDifficulty", "sLockLevelNameVeryEasy", "sLockLevelNameEasy",
                "sNormal", "sLockLevelNameHard", "sLockLevelNameVeryHard", "sSaveOnRest", "sSaveOnWait",
                "sSaveOnTravel", "sTrueIronSights", "sOn", "sOff", "sBrightness", "sHUDOpacity", "sHUDColor",
                "sPipboyColor", "sGreen", "sBlue", "sAmber", "sWhite", "sTextureSize", "sLarge", "sMedium", "sSmall",
                "sActorFade", "sItemFade", "sObjectFade", "sGrassFade", "sShadowFade", "sLightFade", "sSpecularityFade",
                "sObjectLODFade", "sTreeLODFade", "sCrosshair", "sDialogSubtitles", "sGeneralSubtitles",
                "sMasterVolume", "sMusicVolume", "sFootstepsVolume", "sVoiceVolume", "sEffectsVolume", "sRadioVolume",
                "sCameraPitch", "sMouseSensitivity", "sDisableXBoxController", "sRumble" })
            EXPECT_FALSE(SettingText::standIn(name).empty()) << name;
    }

    TEST(FalloutSettingText, the_words_of_the_settings_screens_have_the_lengths_the_executables_give_them)
    {
        // Read from the default texts of FalloutNV.exe (round 12AC; Fallout3.exe has the same lengths where it has the
        // setting): the length of each word. 37 of the words are the executable's text, byte for byte; the others are
        // words of that length in the executable's capitals
        const std::pair<const char*, size_t> lengths[] = { { "sGameplay", 8 }, { "sDisplay", 7 }, { "sAudio", 5 },
            { "sControls", 8 }, { "sKillCamMode", 13 }, { "sKillCamModeNone", 4 }, { "sKillCamModePlayer", 11 },
            { "sKillCamModeCinematic", 9 }, { "sHardcore", 13 }, { "sDifficulty", 10 }, { "sLockLevelNameVeryEasy", 9 },
            { "sLockLevelNameEasy", 4 }, { "sNormal", 6 }, { "sLockLevelNameHard", 4 }, { "sLockLevelNameVeryHard", 9 },
            { "sSaveOnRest", 12 }, { "sSaveOnWait", 12 }, { "sSaveOnTravel", 14 }, { "sTrueIronSights", 16 },
            { "sOn", 2 }, { "sOff", 3 }, { "sBrightness", 10 }, { "sHUDOpacity", 11 }, { "sHUDColor", 9 },
            { "sPipboyColor", 13 }, { "sGreen", 5 }, { "sBlue", 4 }, { "sAmber", 5 }, { "sWhite", 5 },
            { "sTextureSize", 12 }, { "sLarge", 5 }, { "sMedium", 6 }, { "sSmall", 5 }, { "sActorFade", 10 },
            { "sItemFade", 9 }, { "sObjectFade", 11 }, { "sGrassFade", 10 }, { "sShadowFade", 11 },
            { "sLightFade", 10 }, { "sSpecularityFade", 16 }, { "sObjectLODFade", 15 }, { "sTreeLODFade", 13 },
            { "sCrosshair", 9 }, { "sDialogSubtitles", 16 }, { "sGeneralSubtitles", 17 }, { "sMasterVolume", 6 },
            { "sMusicVolume", 5 }, { "sFootstepsVolume", 8 }, { "sVoiceVolume", 5 }, { "sEffectsVolume", 7 },
            { "sRadioVolume", 5 }, { "sCameraPitch", 8 }, { "sMouseSensitivity", 17 }, { "sDisableXBoxController", 14 },
            { "sRumble", 9 } };
        for (const auto& [name, length] : lengths)
            EXPECT_EQ(SettingText::standIn(name).size(), length) << name;
        // the switch words and two of the colours are in capitals, the volumes are one word each
        EXPECT_EQ(SettingText::standIn("sOn"), "ON");
        EXPECT_EQ(SettingText::standIn("sOff"), "OFF");
        EXPECT_EQ(SettingText::standIn("sAmber"), "AMBER");
        EXPECT_EQ(SettingText::standIn("sWhite"), "WHITE");
        EXPECT_EQ(SettingText::standIn("sSaveOnTravel"), "Save On Travel");
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

    TEST(FalloutSettingText, the_words_under_the_crosshair_have_the_lengths_the_executables_give_them)
    {
        // The verbs, the lock's words and the lock classes are string settings of FalloutNV.exe with no record behind
        // them (Mac rounds 12BA G1, 12BB G1 and 12BB H: the length of each default, and every word here equal to it)
        const std::pair<const char*, size_t> lengths[] = { { "sTargetTypeTake", 4 }, { "sTargetTypeOpen", 4 },
            { "sTargetTypeSit", 3 }, { "sTargetTypeActivate", 8 }, { "sTargetTypeSleep", 5 }, { "sTargetTypeRead", 4 },
            { "sTargetTypeTalk", 4 }, { "sTargetTypeOpenDoor", 4 }, { "sTargetTypeEquip", 5 },
            { "sTargetTypeUnequip", 7 }, { "sTargetTypeDrink", 5 }, { "sTargetTypeEat", 3 },
            { "sTargetTypeRecharge", 8 }, { "sTargetTypeBrew", 4 }, { "sTargetTypeApply", 5 },
            { "sTargetTypeRepair", 6 }, { "sSearch", 6 }, { "sPickpocket", 10 }, { "sSteal", 5 }, { "sPick", 4 },
            { "sCloseButton", 5 }, { "sDisarmMine", 11 }, { "sLocked", 6 }, { "sHUDUseKey", 7 }, { "sTo", 2 },
            { "sLockLevelNameVeryEasy", 9 }, { "sLockLevelNameEasy", 4 }, { "sLockLevelNameAverage", 7 },
            { "sLockLevelNameHard", 4 }, { "sLockLevelNameVeryHard", 9 }, { "sLockLevelNameImpossible", 12 },
            { "sBroken", 17 }, { "sInaccessible", 14 } };
        for (const auto& [name, length] : lengths)
            EXPECT_EQ(SettingText::standIn(name).size(), length) << name;
        // The word of a door is the word of a container
        EXPECT_EQ(SettingText::standIn("sTargetTypeOpenDoor"), SettingText::standIn("sTargetTypeOpen"));
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
