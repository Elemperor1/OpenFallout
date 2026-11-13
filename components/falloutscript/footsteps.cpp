#include "footsteps.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace FalloutScript::Footsteps
{
    namespace
    {
        constexpr std::array<std::string_view, surfaceCount> surfaceNames = { "ConcSolid", "ConcBroken", "MetalSolid",
            "MetalHollow", "MetalSheet", "Wood", "Sand", "Dirt", "Grass", "Water", "ChainLink", "Tile", "Carpet" };

        constexpr std::array<std::string_view, slotCount> slotNames
            = { "WalkL", "WalkR", "RunR", "RunL", "SneakL", "SneakR", "StairsL", "StairsR", "Land", "Jump" };

        // FalloutNV.exe 0x11A17F8: the surface index for each Havok material number (Fallout 3's table, 0xF6C538, is the
        // first 32 of it)
        constexpr std::array<int, 36> materialSurface = { 0, 0, 7, 0, 8, 2, 0, 0, 9, 5, 0, 2, 5, 2, 0, 0, 3, 4, 6, 1, 2,
            2, 3, 4, 6, 4, 2, 2, 5, 5, 2, 5, 10, 11, 12, 7 };
        constexpr int materialsFallout3 = 32;
        constexpr int materialsNewVegas = 36;

        constexpr std::string_view lastResort = "FSTConcSolidWalkR";
    }

    std::string_view surfaceName(Surface surface)
    {
        return surfaceNames[static_cast<std::size_t>(surface)];
    }

    std::string_view slotName(Slot slot)
    {
        return slotNames[static_cast<std::size_t>(slot)];
    }

    std::optional<Foot> footOfKey(std::string_view word)
    {
        if (word == "left")
            return Foot::Left;
        if (word == "right")
            return Foot::Right;
        if (word == "backleft")
            return Foot::BackLeft;
        if (word == "backright")
            return Foot::BackRight;
        return std::nullopt;
    }

    std::optional<int> enumKeyIndex(std::string_view word)
    {
        static constexpr std::array<std::string_view, 31> words = { "left", "right", "backleft", "backright", "idle",
            "aware", "attack", "hit", "death", "equip", "unequip", "camerashake", "stopsounds", "hitshader",
            "ridersoft", "riderloud", "weapon1", "foleysneak", "foleywalk", "foleyrun", "armorsneak", "armorwalk",
            "armorrun", "auxiliary1", "auxiliary2", "auxiliary3", "auxiliary4", "auxiliary5", "auxiliary6",
            "auxiliary7", "auxiliary8" };
        const auto found = std::find(words.begin(), words.end(), word);
        if (found == words.end())
            return std::nullopt;
        return static_cast<int>(found - words.begin());
    }

    int pickByChance(const std::vector<int>& chances, float unit)
    {
        int total = 0;
        for (const int chance : chances)
            total += std::max(chance, 0);
        if (total <= 0)
            return -1;
        const float roll = std::clamp(unit, 0.f, 1.f) * static_cast<float>(std::max(total, 100));
        float passed = 0.f;
        for (std::size_t i = 0; i < chances.size(); ++i)
        {
            passed += static_cast<float>(std::max(chances[i], 0));
            if (roll < passed)
                return static_cast<int>(i);
        }
        return -1;
    }

    Surface surfaceOfMaterial(int material, bool newVegas)
    {
        // The table is used from 1, and 0 and what is past its end are the first surface (0x83A910)
        if (material <= 0 || material >= (newVegas ? materialsNewVegas : materialsFallout3))
            return Surface::ConcSolid;
        return static_cast<Surface>(materialSurface[static_cast<std::size_t>(material)]);
    }

    std::optional<Surface> parentOf(Surface surface)
    {
        switch (surface)
        {
            case Surface::ConcBroken:
            case Surface::MetalSolid:
            case Surface::Wood:
            case Surface::Dirt:
            case Surface::Water:
            case Surface::Tile:
            case Surface::Carpet:
                return Surface::ConcSolid;
            case Surface::MetalHollow:
                return Surface::MetalSolid;
            case Surface::MetalSheet:
                return Surface::MetalHollow;
            case Surface::Sand:
            case Surface::Grass:
            case Surface::ChainLink:
                return Surface::Dirt;
            case Surface::ConcSolid:
                return std::nullopt;
        }
        return std::nullopt;
    }

    namespace
    {
        // The left and right of the key are swapped in the names (side 0 is the key Left), and the back feet are the
        // front ones (0x838200: a value above 1 loses 2)
        int side(Foot foot)
        {
            const int value = static_cast<int>(foot);
            return value > 1 ? value - 2 : value;
        }
    }

    Slot chooseSlot(Foot foot, bool sneaking, bool onStairs, bool running)
    {
        const bool leftKey = side(foot) == 0;
        if (sneaking)
            return leftKey ? Slot::SneakR : Slot::SneakL;
        if (onStairs)
            return leftKey ? Slot::StairsR : Slot::StairsL;
        if (running)
            return leftKey ? Slot::RunL : Slot::RunR;
        return leftKey ? Slot::WalkR : Slot::WalkL;
    }

    std::string soundName(Surface surface, Slot slot)
    {
        std::string name = "FST";
        name += surfaceName(surface);
        name += slotName(slot);
        return name;
    }

    std::string resolveSound(Surface surface, Slot slot, const std::function<bool(std::string_view)>& exists)
    {
        std::optional<Surface> candidate = surface;
        // The surface, its parent and the parent's parent: no surface of the table is deeper than that
        for (int depth = 0; depth < 3 && candidate.has_value(); ++depth)
        {
            const std::string name = soundName(*candidate, slot);
            if (exists(name))
                return name;
            candidate = parentOf(*candidate);
        }
        if (exists(lastResort))
            return std::string(lastResort);
        return {};
    }

    bool isStairs(const StairSettings& settings, float normalZ, float height, std::optional<float> aheadHeight,
        std::optional<float> behindHeight)
    {
        if (normalZ < settings.mFlatNormal)
            return false;
        // What a slope of that tilt rises over the reach is not a step: a ramp is ground that goes up evenly, a tread is a
        // flat that the ground goes up from
        const float tilt = std::sqrt(std::max(0.f, 1.f - normalZ * normalZ)) / std::max(normalZ, 1e-3f);
        const float slopeRise = settings.mReach * tilt;
        const auto isStep = [&](std::optional<float> other) {
            if (!other)
                return false;
            const float rise = std::abs(*other - height);
            return rise - slopeRise >= settings.mMinStep && rise <= settings.mMaxStep;
        };
        return isStep(aheadHeight) || isStep(behindHeight);
    }

    std::string armorSoundName(bool powerArmor, Foot foot)
    {
        const bool leftKey = side(foot) == 0;
        if (powerArmor)
            return leftKey ? "FSTArmorPowerR" : "FSTArmorPowerL";
        return leftKey ? "FSTArmorR" : "FSTArmorL";
    }

    std::string armorJumpSoundName(bool powerArmor)
    {
        return powerArmor ? "FSTJumpPower" : "FSTJumpArmor";
    }

    bool isHeard(const Settings& settings, float distance)
    {
        return distance <= settings.mMaxDistance;
    }

    float stepVolume(const Settings& settings, bool playerInFirstPerson, float unit)
    {
        if (playerInFirstPerson)
            return settings.mPlayerVolume;
        return 0.9f + 0.1f * std::clamp(unit, 0.f, 1.f);
    }

    float armorVolume(bool playerInFirstPerson, const Settings& settings, float unit)
    {
        if (playerInFirstPerson)
            return settings.mPlayerVolume;
        return 1.f - 0.1f * std::clamp(unit, 0.f, 1.f);
    }

    bool reachesListener(float distance, int maxAttenuationByte)
    {
        return distance <= static_cast<float>(maxAttenuationByte) * 100.f;
    }
}
