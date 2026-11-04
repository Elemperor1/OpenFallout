#include "itemmods.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::ItemMods
{
    int chooseSlot(int installed, const Slots& slots, FormId mod)
    {
        if (mod == 0)
            return -1;
        for (int slot = 0; slot < slotCount; ++slot)
            if ((installed & (1 << slot)) == 0 && slots.mMod[slot] == mod)
                return slot;
        return -1;
    }

    std::optional<double> installedValue(int installed, const Slots& slots, std::uint32_t effect, bool second)
    {
        for (int slot = 0; slot < slotCount; ++slot)
            if ((installed & (1 << slot)) != 0 && slots.mEffect[slot] == effect)
                return second ? slots.mValue2[slot] : slots.mValue1[slot];
        return std::nullopt;
    }

    std::vector<int> installedSlots(int installed)
    {
        std::vector<int> result;
        for (int slot = 0; slot < slotCount; ++slot)
            if ((installed & (1 << slot)) != 0)
                result.push_back(slot);
        return result;
    }

    std::vector<Row> listRows(int installed, const Slots& slots, const std::map<FormId, int>& held)
    {
        std::vector<Row> rows;
        std::array<FormId, slotCount> inside{};
        for (const int slot : installedSlots(installed))
        {
            inside[slot] = slots.mMod[slot];
            rows.push_back({ slots.mMod[slot], true });
        }
        // The mods that fit any slot, once each, in the order of the slots; one that is installed is not offered again
        std::vector<FormId> offered;
        for (int slot = 0; slot < slotCount; ++slot)
        {
            const FormId mod = slots.mMod[slot];
            if (mod == 0 || std::find(offered.begin(), offered.end(), mod) != offered.end())
                continue;
            offered.push_back(mod);
            if (std::find(inside.begin(), inside.end(), mod) != inside.end())
                continue;
            const auto have = held.find(mod);
            for (int unit = 0; have != held.end() && unit < have->second; ++unit)
                rows.push_back({ mod, false });
        }
        return rows;
    }

    double mostHealth(double baseHealth, int installed, const Slots& slots)
    {
        const std::optional<double> bonus = installedValue(installed, slots, MaxCondition);
        return baseHealth + (bonus ? std::trunc(*bonus) : 0.0);
    }

    int conditionAfterInstalling(double baseHealth, int condition, int installed, const Slots& slots, int slot)
    {
        if (slot < 0 || slot >= slotCount || baseHealth <= 0.0)
            return condition;
        // Only the mod of the effect of maximum condition changes the health
        if (slots.mEffect[slot] != MaxCondition)
            return condition;
        const double before = mostHealth(baseHealth, installed, slots);
        // The health of a copy that has lost none is not kept: the executable then adds to "no health" (-1)
        const double points = condition >= 1000 ? -1.0 : condition / 1000.0 * before;
        const double bonus = slots.mValue1[slot];
        const double after = mostHealth(baseHealth, installed | (1 << slot), slots);
        if (after <= 0.0)
            return condition;
        const double health = points + bonus;
        return std::clamp(static_cast<int>(std::lround(health / after * 1000.0)), 0, 1000);
    }
}
