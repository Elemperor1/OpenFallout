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
        // from the files. The caravan, card and roulette games are not here: their words are not needed yet.
        constexpr std::array entries = {
            Entry{ "sAccept", "Accept" },
            Entry{ "sBack", "Back" },
            Entry{ "sCWheelTitle", "Companion Wheel" },
            Entry{ "sCancel", "Cancel" },
            Entry{ "sCauterize", "Cauterize" },
            Entry{ "sChallengeToggle", "Challenges" },
            Entry{ "sCloseButton", "Close" },
            Entry{ "sContinue", "Continue" },
            Entry{ "sCrew", "Credits" },
            Entry{ "sCurrentObjective", "Current Objective" },
            Entry{ "sData", "DATA" },
            Entry{ "sDevice", "Device" },
            Entry{ "sDone", "Done" },
            Entry{ "sEnterName", "Enter Name" },
            Entry{ "sExit", "Exit" },
            Entry{ "sExplosive", "Explosive" },
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
            Entry{ "sLevelProgress", "Level Progress" },
            Entry{ "sLevelUp", "LEVEL UP" },
            Entry{ "sLoad", "Load" },
            Entry{ "sLoadSavedGame", "Load Saved Game" },
            Entry{ "sLockLevelText", "Lock Level" },
            Entry{ "sLockpickSkillText", "Lockpick Skill" },
            Entry{ "sMadeAt", "Made at" },
            Entry{ "sMakeActiveQuest", "Make Active" },
            Entry{ "sMedicineSkill", "Medicine" },
            Entry{ "sModffects", "Mod Effects" },
            Entry{ "sMorphine", "Morphine" },
            Entry{ "sNavigate", "Navigate" },
            Entry{ "sNew", "New" },
            Entry{ "sNext", "Next" },
            Entry{ "sNextChallenge", "Next Challenge" },
            Entry{ "sNoItemsToRepair", "No items to repair" },
            Entry{ "sOK", "OK" },
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
            Entry{ "sPlaceMarker", "Place Marker" },
            Entry{ "sPlayAudio", "Play" },
            Entry{ "sPressStart", "Press Start" },
            Entry{ "sPrevious", "Previous" },
            Entry{ "sQuit", "Quit" },
            Entry{ "sRecipes", "Recipes" },
            Entry{ "sRedeemCode", "Redeem Code" },
            Entry{ "sRepairAllItems", "Repair All Items" },
            Entry{ "sRepairItem", "Repair Item" },
            Entry{ "sRepairServicesTitle", "Repair Services" },
            Entry{ "sRepairSkill", "Repair Skill" },
            Entry{ "sReputationSwap", "Swap" },
            Entry{ "sReset", "Reset" },
            Entry{ "sReturn", "Return" },
            Entry{ "sSave", "Save" },
            Entry{ "sSaveGame", "Save Game" },
            Entry{ "sSelect", "Select" },
            Entry{ "sSettings", "Settings" },
            Entry{ "sShowActiveNotes", "Active Notes" },
            Entry{ "sShowAllNotes", "All Notes" },
            Entry{ "sShowLocation", "Show Location" },
            Entry{ "sSkillRequirement", "Skill Requirement" },
            Entry{ "sSpace", "Space" },
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
            Entry{ "sStopAudio", "Stop" },
            Entry{ "sSwitchLimbs", "Switch Limbs" },
            Entry{ "sTakeAll", "Take All" },
            Entry{ "sTravel", "Travel" },
            Entry{ "sTune", "Tune" },
            Entry{ "sVatsAiming", "Aiming" },
            Entry{ "sVatsBodyPart", "Body Part" },
            Entry{ "sVatsSelect", "Select" },
            Entry{ "sVatsTarget", "Target" },
            Entry{ "sXPProgressMarker", "XP" },
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
