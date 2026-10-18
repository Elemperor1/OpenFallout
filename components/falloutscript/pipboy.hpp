#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PIPBOY_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PIPBOY_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "equipment.hpp"
#include "vanillacommands.hpp"

namespace FalloutScript
{
    /// What the Pip-Boy tells the player, which the games take from settings of their files (see the notes of the M6
    /// mac round 4A): the title that goes with the karma and the level, the word for how good a skill is, and the tab
    /// an item is listed under. The texts are the files' own; this only says which setting has which.
    namespace Pipboy
    {
        /// The kinds of karma that the titles tell apart
        enum class KarmaClass
        {
            VeryEvil,
            Evil,
            Neutral,
            Good,
            VeryGood,
        };

        /// The karma at which the player is evil, very evil, good and very good (fAlignEvilMaxKarma and the like)
        struct KarmaRules
        {
            double mEvilMax = -250;
            double mVeryEvilMax = -750;
            double mGoodMin = 250;
            double mVeryGoodMin = 750;
        };

        using Lookup = std::function<std::optional<double>(std::string_view)>;

        /// The ranges of karma, from the settings of the files
        KarmaRules karmaRules(const Lookup& setting);

        KarmaClass karmaClass(const KarmaRules& rules, double karma);

        /// Whether the karma is on the side of the good or the evil, or neither: -1, 1 or 0
        int karmaSide(KarmaClass karma);

        /// The name of the setting that has the title of a player of the level and the karma: sKarmicTitleGood05.
        /// The very good and the very evil have the titles of the good and the evil; a level above the last title
        /// has the last.
        std::string karmaTitleSetting(int level, KarmaClass karma);

        /// The highest number a title has
        constexpr int lastKarmaTitle = 30;

        /// The name of the setting that has the word for a skill of that value: sSkillLevelNovice up to
        /// sSkillLevelMaster (the values from which the words are for are iSkillApprenticeMin and the like)
        std::string skillLevelSetting(const Lookup& setting, double value);

        /// The tabs of the inventory
        enum class ItemCategory
        {
            Weapons,
            Apparel,
            Aid,
            Misc,
            Ammo,
        };
        constexpr int categoryCount = 5;

        ItemCategory categoryOf(ItemKind kind);
        /// The name of the setting that has the name of the tab (sInventoryWeapons, ...)
        const char* categorySetting(ItemCategory category);
        /// The short name of the tab for scripts of the interface (weapons, apparel, aid, misc, ammo)
        const char* categoryName(ItemCategory category);
    }
}

#endif
