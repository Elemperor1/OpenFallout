#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REPAIR_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_REPAIR_H

#include <functional>
#include <optional>
#include <string_view>

namespace FalloutScript::Repair
{
    /// The rules of the repair screens of New Vegas, as FalloutNV.exe works them out (the repair menu 0x7B5520 with the
    /// formula 0x648090, and the repair services 0x7B7570 with the cost 0x648230; Mac rounds 12AL and 12AM, B3). The
    /// health of an item is its condition in tenths of a percent (1000 is full), as ActorState keeps it. Nothing here
    /// knows of the menus or of the world: the host gives the numbers.
    ///
    /// The executable calls the health "in tenths" (a full item is 10 in the formula's own unit, so the condition over
    /// 100); the reading of its units is not confirmed by a running game.

    /// The game settings the rules go by, with the defaults of the executable (the plugins of New Vegas give 10.0 to
    /// the skill maximum of repair services and 2.0 to the cost)
    struct Settings
    {
        double mMin = 0.5; // fRepairMin
        double mMax = 2.0; // fRepairMax
        double mScavengeMult = 0.05; // fRepairScavengeMult
        double mSkillBase = 4.0; // fRepairSkillBase
        double mSkillMax = 9.0; // fRepairSkillMax
        double mCostMult = 2.0; // fItemRepairCostMult

        using Lookup = std::function<std::optional<double>(std::string_view)>;
        /// The settings the game's data gives (a setting it does not give keeps the default)
        static Settings fromGameSettings(const Lookup& lookup);
    };

    /// The condition of the broken item after it is repaired with a spare copy of the same item (the spare is used up):
    /// the better of the two, `fRepairMin`, the repair skill (0 to 100) between `fRepairMin` and `fRepairMax`, and a
    /// little of the worse; at most full, never below the better of the two
    int repairedCondition(double skill, int brokenCondition, int spareCondition, const Settings& settings);

    /// The condition a repairing vendor leaves an item in, from the vendor's Repair skill (0 to 100): the skill between
    /// `fRepairSkillBase` and `fRepairSkillMax`, as tenths of the full health, at most full
    int serviceCondition(double vendorSkill, const Settings& settings);

    /// What a vendor asks to bring an item with that condition and base value to its best (the executable's 0x648230):
    /// nothing (0) when there is nothing to repair (what would be asked is half a cap or less), else the sum rounded up
    /// and at least 1
    int serviceCost(double vendorSkill, int condition, int value, const Settings& settings);
}

#endif
