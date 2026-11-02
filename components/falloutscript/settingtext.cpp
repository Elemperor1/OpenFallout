#include "settingtext.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <unordered_map>

namespace FalloutScript::SettingText
{
    namespace
    {
        struct Entry
        {
            std::string_view mName;
            std::string_view mText;
        };

        // The labels the menu files of both games ask for (`&-sName;`), as they are on the screens that were drawn
        // from the files; sAttributesCount and sSkillsCount are formats of the character making screens (read from the
        // executables: the number of points left, then an 's' unless it is one), sAttributesTitle and sSkillsTitle the
        // formats of their titles (the points given and all that can be given; the capitals are in the text), and the
        // trait menu's title and counter have a plural and a singular format (upper case, as read). The words of the
        // pause menu's questions and save list (sConfirm*, sNewSave, sSavesRemaining, sNoSaves, the abbreviations of
        // the quick and automatic saves, sLevelAbbrev, sPlayTime) are the port's own. The caravan, card
        // and roulette games are not here: their words are not needed yet. The length and the case of the defaults of
        // the buttons and the tabs of the data page were read from both executables (sMakeActiveQuest 17, sPlayAudio
        // 10, sStopAudio 10, sShowAllNotes 14, sShowActiveNotes 23, sNextChallenge 19, the tab texts 9, 9, 6, 4 (Misc,
        // New Vegas), 5 (Notes, Fallout 3), 5); the words are the ones that fit, and the executables' texts were
        // compared with them: all of the data page's words are equal to them, sNextChallenge ("Next Challenge Type")
        // included. The words of the four settings screens (the items, their values, the colours and the sizes) were
        // compared with the executables' texts one by one (round 12AC): 37 are equal byte for byte, and the others
        // are the words that fit the length and the capitals the executables give them (the audio volumes are one
        // word each, the switch words and two of the colours are in capitals).
        constexpr std::array entries = {
            Entry{ "sAccept", "Accept" },
            Entry{ "sActorFade", "Actor Fade" },
            Entry{ "sAmber", "AMBER" },
            Entry{ "sAttributesCount", "Distribute %d Point" },
            Entry{ "sAttributesTitle", "ATTRIBUTES: %d/%d Points Assigned" },
            Entry{ "sAudio", "Audio" },
            Entry{ "sAutosaveAbbrev", "Auto" },
            Entry{ "sBack", "Back" },
            Entry{ "sBlue", "Blue" },
            Entry{ "sBrightness", "Brightness" },
            Entry{ "sCWheelTitle", "Companion Wheel" },
            Entry{ "sCameraPitch", "Invert Y" },
            Entry{ "sCancel", "Cancel" },
            Entry{ "sCauterize", "Cauterize" },
            Entry{ "sChallengeToggle", "Challenges" },
            Entry{ "sCloseButton", "Close" },
            Entry{ "sCommsTabText", "Radio" },
            Entry{ "sConfirmLoad", "Load this saved game? Any progress since your last save will be lost." },
            Entry{ "sConfirmQuit", "Are you sure you want to quit?" },
            Entry{ "sConfirmSave", "Overwrite this saved game?" },
            Entry{ "sContinue", "Continue" },
            Entry{ "sControls", "Controls" },
            Entry{ "sCrew", "Credits" },
            Entry{ "sCrosshair", "Crosshair" },
            Entry{ "sCurrentObjective", "Current Objective" },
            Entry{ "sData", "DATA" },
            Entry{ "sDevice", "Device" },
            Entry{ "sDialogSubtitles", "Dialog Subtitles" },
            Entry{ "sDifficulty", "Difficulty" },
            Entry{ "sDisableXBoxController", "360 Controller" },
            Entry{ "sDisplay", "Display" },
            Entry{ "sDone", "Done" },
            Entry{ "sEffectsVolume", "Effects" },
            Entry{ "sEnterName", "Enter Name" },
            Entry{ "sExit", "Exit" },
            Entry{ "sExitGameAffirm", "Exit Game" },
            Entry{ "sExplosive", "Explosive" },
            Entry{ "sFootstepsVolume", "Footstep" },
            Entry{ "sGameplay", "Gameplay" },
            Entry{ "sGeneralSubtitles", "General Subtitles" },
            Entry{ "sGrassFade", "Grass Fade" },
            Entry{ "sGreen", "Green" },
            Entry{ "sHUDColor", "HUD Color" },
            Entry{ "sHUDOpacity", "HUD Opacity" },
            Entry{ "sHardcore", "Hardcore Mode" },
            Entry{ "sHitPointsShort", "HP" },
            Entry{ "sHowMany", "How many?" },
            Entry{ "sIngredients", "Ingredients" },
            Entry{ "sInventoryCaps", "Caps" },
            Entry{ "sInventoryCondition", "CND" },
            Entry{ "sInventoryDamage", "DAM" },
            Entry{ "sInventoryDamagePerSecond", "DPS" },
            Entry{ "sInventoryDamageResistance", "DR" },
            Entry{ "sInventoryDamageThreshold", "DT" },
            Entry{ "sInventoryDrop", "Drop" },
            Entry{ "sInventoryEffects", "Effects" },
            Entry{ "sInventoryEquip", "Equip" },
            Entry{ "sInventoryHotKey", "Hot Key" },
            Entry{ "sInventoryItems", "ITEMS" },
            Entry{ "sInventoryMod", "Mod" },
            Entry{ "sInventoryRepair", "Repair" },
            Entry{ "sInventoryStrReq", "STR" },
            Entry{ "sInventoryValue", "VAL" },
            Entry{ "sInventoryWeight", "Wg" },
            Entry{ "sInventoryWeightUpper", "WG" },
            Entry{ "sItemFade", "Item Fade" },
            Entry{ "sKillCamMode", "Kill Cam Mode" },
            Entry{ "sKillCamModeCinematic", "Cinematic" },
            Entry{ "sKillCamModeNone", "None" },
            Entry{ "sKillCamModePlayer", "Player Only" },
            Entry{ "sLarge", "Large" },
            Entry{ "sLevelAbbrev", "Lvl" },
            Entry{ "sLevelProgress", "Level Progress" },
            Entry{ "sLevelUp", "LEVEL UP" },
            Entry{ "sLightFade", "Light Fade" },
            Entry{ "sLoad", "Load" },
            Entry{ "sLoadSavedGame", "Load Saved Game" },
            Entry{ "sLocalMapTabText", "Local Map" },
            Entry{ "sLockLevelNameEasy", "Easy" },
            Entry{ "sLockLevelNameHard", "Hard" },
            Entry{ "sLockLevelNameVeryEasy", "Very Easy" },
            Entry{ "sLockLevelNameVeryHard", "Very Hard" },
            Entry{ "sLockLevelText", "Lock Level" },
            Entry{ "sLockpickSkillText", "Lockpick Skill" },
            Entry{ "sMadeAt", "Made at" },
            Entry{ "sMakeActiveQuest", "Make Active Quest" },
            Entry{ "sMasterVolume", "Master" },
            Entry{ "sMedicineSkill", "Medicine" },
            Entry{ "sMedium", "Medium" },
            Entry{ "sMiscTabText", "Misc" },
            Entry{ "sModffects", "Mod Effects" },
            Entry{ "sMorphine", "Morphine" },
            Entry{ "sMouseSensitivity", "Mouse Sensitivity" },
            Entry{ "sMusicVolume", "Music" },
            Entry{ "sNavigate", "Navigate" },
            Entry{ "sNew", "New" },
            Entry{ "sNewSave", "New Save" },
            Entry{ "sNext", "Next" },
            Entry{ "sNextChallenge", "Next Challenge Type" },
            Entry{ "sNo", "No" },
            Entry{ "sNoItemsToRepair", "No items to repair" },
            Entry{ "sNoSaves", "No Saved Games" },
            Entry{ "sNormal", "Normal" },
            Entry{ "sNotesTabText", "Notes" },
            Entry{ "sNotesToggle", "Notes" },
            Entry{ "sOK", "OK" },
            Entry{ "sObjectFade", "Object Fade" },
            Entry{ "sObjectLODFade", "Object LOD Fade" },
            Entry{ "sOff", "OFF" },
            Entry{ "sOn", "ON" },
            Entry{ "sPCMenuHintA", "(A)" },
            Entry{ "sPCMenuHintE", "(E)" },
            Entry{ "sPCMenuHintF", "(F)" },
            Entry{ "sPCMenuHintQ", "(Q)" },
            Entry{ "sPCMenuHintR", "(R)" },
            Entry{ "sPCMenuHintRMB", "(RMB)" },
            Entry{ "sPCMenuHintS", "(S)" },
            Entry{ "sPCMenuHintW", "(W)" },
            Entry{ "sPCMenuHintX", "(X)" },
            Entry{ "sPicksRemainingText", "Picks Remaining" },
            Entry{ "sPipboyColor", "Pip-Boy Color" },
            Entry{ "sPlaceMarker", "Place Marker" },
            Entry{ "sPlayAudio", "Play Audio" },
            Entry{ "sPlayTime", "Play Time" },
            Entry{ "sPressStart", "Press Start" },
            Entry{ "sPrevious", "Previous" },
            Entry{ "sQuestsTabText", "Quests" },
            Entry{ "sQuicksaveAbbrev", "Quick" },
            Entry{ "sQuit", "Quit" },
            Entry{ "sRadioVolume", "Radio" },
            Entry{ "sRecipes", "Recipes" },
            Entry{ "sRedeemCode", "Redeem Code" },
            Entry{ "sRepairAllItems", "Repair All Items" },
            Entry{ "sRepairItem", "Repair Item" },
            Entry{ "sRepairServicesTitle", "Repair Services" },
            Entry{ "sRepairSkill", "Repair Skill" },
            Entry{ "sReputationSwap", "Swap" },
            Entry{ "sReset", "Reset" },
            Entry{ "sReturn", "Return" },
            Entry{ "sRumble", "Vibration" },
            Entry{ "sSave", "Save" },
            Entry{ "sSaveGame", "Save Game" },
            Entry{ "sSaveOnRest", "Save On Rest" },
            Entry{ "sSaveOnTravel", "Save On Travel" },
            Entry{ "sSaveOnWait", "Save On Wait" },
            Entry{ "sSavesRemaining", "Saves Remaining" },
            Entry{ "sSelect", "Select" },
            Entry{ "sSettings", "Settings" },
            Entry{ "sShadowFade", "Shadow Fade" },
            Entry{ "sShowActiveNotes", "Show Active Quest Notes" },
            Entry{ "sShowAllNotes", "Show All Notes" },
            Entry{ "sShowLocation", "Show Location" },
            Entry{ "sSkillRequirement", "Skill Requirement" },
            Entry{ "sSkillsCount", "Tag %d Skill" },
            Entry{ "sSkillsTitle", "SKILLS:  %d/%d Selected" },
            Entry{ "sSmall", "Small" },
            Entry{ "sSpace", "Space" },
            Entry{ "sSpecularityFade", "Specularity Fade" },
            Entry{ "sStats", "STATS" },
            Entry{ "sStatsAP", "AP" },
            Entry{ "sStatsCNDAbbrev", "CND" },
            Entry{ "sStatsCrippled", "CRIPPLED" },
            Entry{ "sStatsEFFAbbrev", "EFF" },
            Entry{ "sStatsFODAbbrev", "FOD" },
            Entry{ "sStatsGeneral", "General" },
            Entry{ "sStatsH20Abbrev", "H2O" },
            Entry{ "sStatsHP", "HP" },
            Entry{ "sStatsLMBAbbrev", "LMB" },
            Entry{ "sStatsLVLAbbrev", "LVL" },
            Entry{ "sStatsNoEffects", "NO STATUS EFFECTS" },
            Entry{ "sStatsPerks", "Perks" },
            Entry{ "sStatsRADAbbrev", "RAD" },
            Entry{ "sStatsRadResist", "RAD RESIST" },
            Entry{ "sStatsSLPAbbrev", "SLP" },
            Entry{ "sStatsSkills", "Skills" },
            Entry{ "sStatsSpecial", "S.P.E.C.I.A.L." },
            Entry{ "sStatsStatus", "Status" },
            Entry{ "sStatsXP", "XP" },
            Entry{ "sStimpak", "Stimpak" },
            Entry{ "sStopAudio", "Stop Audio" },
            Entry{ "sSwitchLimbs", "Switch Limbs" },
            Entry{ "sTakeAll", "Take All" },
            Entry{ "sTextureSize", "Texture Size" },
            Entry{ "sTraitMenuCounter", "%d TRAITS LEFT" },
            Entry{ "sTraitMenuCounterSingular", "%d TRAIT LEFT" },
            Entry{ "sTraitMenuTitleText", "CHOOSE UP TO %d TRAITS" },
            Entry{ "sTraitMenuTitleTextSingular", "CHOOSE %d TRAIT" },
            Entry{ "sTravel", "Travel" },
            Entry{ "sTreeLODFade", "Tree LOD Fade" },
            Entry{ "sTrueIronSights", "True Iron Sights" },
            Entry{ "sTune", "Tune" },
            Entry{ "sVatsAiming", "Aiming" },
            Entry{ "sVatsBodyPart", "Body Part" },
            Entry{ "sVatsSelect", "Select" },
            Entry{ "sVatsTarget", "Target" },
            Entry{ "sVoiceVolume", "Voice" },
            Entry{ "sWhite", "WHITE" },
            Entry{ "sWorldMapTabText", "World Map" },
            Entry{ "sXPProgressMarker", "XP" },
            Entry{ "sYes", "Yes" },
        };

        char lower(char c)
        {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        std::string lowered(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(), lower);
            return result;
        }

        const std::unordered_map<std::string, std::string_view>& table()
        {
            static const std::unordered_map<std::string, std::string_view> built = [] {
                std::unordered_map<std::string, std::string_view> result;
                for (const Entry& entry : entries)
                    result.emplace(lowered(entry.mName), entry.mText);
                return result;
            }();
            return built;
        }
    }

    std::string_view standIn(std::string_view name)
    {
        const auto found = table().find(lowered(name));
        return found != table().end() ? found->second : std::string_view();
    }
}
