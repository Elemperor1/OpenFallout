#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ITEMMODS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ITEMMODS_H

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "script.hpp"

namespace FalloutScript::ItemMods
{
    /// The rules of the item mods of New Vegas weapons, as FalloutNV.exe works them out (the item mod menu 0x425, its
    /// list builder 0x7840F0 and the apply 0x783AF0; Mac round 12AX R3 and R3b). A weapon record names up to three
    /// mods (the records of the type IMOD, one for each slot) and what each does: a kind of effect, and two values. A
    /// copy of a weapon keeps only the mask of the slots that have a mod installed (1 for the first slot, 2 for the second,
    /// 4 for the third); what the mods do is worked out from the weapon's record whenever it is asked. Nothing here
    /// knows of the menus or of the world: the host gives the numbers.

    constexpr int slotCount = 3;
    constexpr int allSlots = 7;

    /// The kinds of effect of a mod slot that the executable's code gives a meaning to (the number of its table)
    enum Effect : std::uint32_t
    {
        Damage = 1, // adds to the damage of the weapon (value 1)
        ClipSize = 2, // adds to the rounds the clip holds (value 1)
        Spread = 3, // takes away from the minimum and the maximum spread (value 1, never below 0)
        Weight = 4, // takes away from the weight of the weapon (value 1)
        Rate = 8, // adds to the rate of the weapon's animation (value 1)
        MaxCondition = 10, // adds to the health the weapon has at most (value 1)
        DamageMultiplied = 12, // the damage is multiplied by 1.3
    };

    /// What a weapon's record says about its three slots
    struct Slots
    {
        std::array<FormId, slotCount> mMod{};
        std::array<std::uint32_t, slotCount> mEffect{};
        std::array<double, slotCount> mValue1{};
        std::array<double, slotCount> mValue2{};
    };

    /// The slot (0 to 2) that takes the mod: the first free slot of the weapon that names that mod, -1 for none (the
    /// weapon does not take it, or every slot that does is taken). `installed` is the mask of the slots with a mod.
    int chooseSlot(int installed, const Slots& slots, FormId mod);

    /// The first installed slot with that effect: its first value (or the second), nothing when no installed slot has it
    std::optional<double> installedValue(int installed, const Slots& slots, std::uint32_t effect, bool second = false);

    /// The slots the mask has installed, in the order of the slots
    std::vector<int> installedSlots(int installed);

    /// One line of the list of the item mod menu
    struct Row
    {
        FormId mMod = 0;
        bool mInstalled = false; // a mod of the weapon that is in it (shown dim, with its marker, and it cannot be picked)
    };

    /// What the menu lists for a weapon: first the mods installed in it (they are listed even when the player has no
    /// copy of the mod), then one line for each unit of the player's mods that fit one of its slots and are not
    /// installed (three copies of one mod are three lines). `held` is how many of each mod the player has.
    std::vector<Row> listRows(int installed, const Slots& slots, const std::map<FormId, int>& held);

    /// The health of a weapon copy after a mod is installed, in tenths of a percent of the most it has. `baseHealth` is
    /// the weapon record's health, `condition` the copy's health (tenths of a percent of the most it had) and `installed`
    /// the mask before the mod. The most a weapon has is its record's health and the first value of an installed slot
    /// with the effect of maximum condition (cut to a whole number). A mod of that effect adds its value to the
    /// health in points, and a copy that has not lost any health (the executable keeps none for it) has the value less one
    /// point and not the whole of it afterwards; any other mod leaves the health as it was.
    int conditionAfterInstalling(double baseHealth, int condition, int installed, const Slots& slots, int slot);

    /// The most health a weapon copy has with those mods installed
    double mostHealth(double baseHealth, int installed, const Slots& slots);
}

#endif
