#include "pipboy.hpp"

#include <algorithm>

namespace FalloutScript::Pipboy
{
    namespace
    {
        double settingOr(const Lookup& setting, std::string_view name, double fallback)
        {
            if (setting)
                if (const std::optional<double> value = setting(name))
                    return *value;
            return fallback;
        }
    }

    KarmaRules karmaRules(const Lookup& setting)
    {
        KarmaRules rules;
        rules.mEvilMax = settingOr(setting, "fAlignEvilMaxKarma", rules.mEvilMax);
        rules.mVeryEvilMax = settingOr(setting, "fAlignVeryEvilMaxKarma", rules.mVeryEvilMax);
        rules.mGoodMin = settingOr(setting, "fAlignGoodMinKarma", rules.mGoodMin);
        rules.mVeryGoodMin = settingOr(setting, "fAlignVeryGoodMinKarma", rules.mVeryGoodMin);
        return rules;
    }

    KarmaClass karmaClass(const KarmaRules& rules, double karma)
    {
        if (karma <= rules.mVeryEvilMax)
            return KarmaClass::VeryEvil;
        if (karma <= rules.mEvilMax)
            return KarmaClass::Evil;
        if (karma >= rules.mVeryGoodMin)
            return KarmaClass::VeryGood;
        if (karma >= rules.mGoodMin)
            return KarmaClass::Good;
        return KarmaClass::Neutral;
    }

    int karmaSide(KarmaClass karma)
    {
        switch (karma)
        {
            case KarmaClass::VeryEvil:
            case KarmaClass::Evil:
                return -1;
            case KarmaClass::Good:
            case KarmaClass::VeryGood:
                return 1;
            case KarmaClass::Neutral:
                break;
        }
        return 0;
    }

    std::string karmaTitleSetting(int level, KarmaClass karma)
    {
        const int number = std::clamp(level, 1, lastKarmaTitle);
        const char* kind = "Neutral";
        switch (karmaSide(karma))
        {
            case -1:
                kind = "Evil";
                break;
            case 1:
                kind = "Good";
                break;
            default:
                break;
        }
        std::string name = "sKarmicTitle";
        name += kind;
        if (number < 10)
            name += '0';
        name += std::to_string(number);
        return name;
    }

    std::string skillLevelSetting(const Lookup& setting, double value)
    {
        if (value >= settingOr(setting, "iSkillMasterMin", 100))
            return "sSkillLevelMaster";
        if (value >= settingOr(setting, "iSkillExpertMin", 75))
            return "sSkillLevelExpert";
        if (value >= settingOr(setting, "iSkillJourneymanMin", 50))
            return "sSkillLevelJourneyman";
        if (value >= settingOr(setting, "iSkillApprenticeMin", 25))
            return "sSkillLevelApprentice";
        return "sSkillLevelNovice";
    }

    ItemCategory categoryOf(ItemKind kind)
    {
        switch (kind)
        {
            case ItemKind::Weapon:
                return ItemCategory::Weapons;
            case ItemKind::Armor:
                return ItemCategory::Apparel;
            case ItemKind::Aid:
                return ItemCategory::Aid;
            case ItemKind::Ammo:
                return ItemCategory::Ammo;
            case ItemKind::Other:
                break;
        }
        return ItemCategory::Misc;
    }

    const char* categorySetting(ItemCategory category)
    {
        switch (category)
        {
            case ItemCategory::Weapons:
                return "sInventoryWeapons";
            case ItemCategory::Apparel:
                return "sInventoryApparel";
            case ItemCategory::Aid:
                return "sInventoryAid";
            case ItemCategory::Misc:
                return "sInventoryMisc";
            case ItemCategory::Ammo:
                return "sInventoryAmmo";
        }
        return "";
    }

    const char* categoryName(ItemCategory category)
    {
        switch (category)
        {
            case ItemCategory::Weapons:
                return "weapons";
            case ItemCategory::Apparel:
                return "apparel";
            case ItemCategory::Aid:
                return "aid";
            case ItemCategory::Misc:
                return "misc";
            case ItemCategory::Ammo:
                return "ammo";
        }
        return "";
    }
}
