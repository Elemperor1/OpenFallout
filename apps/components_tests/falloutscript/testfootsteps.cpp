#include <components/falloutscript/footsteps.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <string_view>

namespace
{
    using namespace FalloutScript::Footsteps;

    TEST(FalloutScriptFootstepsTest, the_surface_of_a_material_is_the_executables_table)
    {
        // FalloutNV.exe 0x11A17F8
        EXPECT_EQ(surfaceOfMaterial(2, true), Surface::Dirt);
        EXPECT_EQ(surfaceOfMaterial(4, true), Surface::Grass);
        EXPECT_EQ(surfaceOfMaterial(5, true), Surface::MetalSolid);
        EXPECT_EQ(surfaceOfMaterial(8, true), Surface::Water);
        EXPECT_EQ(surfaceOfMaterial(9, true), Surface::Wood);
        EXPECT_EQ(surfaceOfMaterial(16, true), Surface::MetalHollow);
        EXPECT_EQ(surfaceOfMaterial(17, true), Surface::MetalSheet);
        EXPECT_EQ(surfaceOfMaterial(18, true), Surface::Sand);
        EXPECT_EQ(surfaceOfMaterial(19, true), Surface::ConcBroken);
        EXPECT_EQ(surfaceOfMaterial(32, true), Surface::ChainLink);
        EXPECT_EQ(surfaceOfMaterial(33, true), Surface::Tile);
        EXPECT_EQ(surfaceOfMaterial(34, true), Surface::Carpet);
        EXPECT_EQ(surfaceOfMaterial(35, true), Surface::Dirt);
    }

    TEST(FalloutScriptFootstepsTest, no_material_and_materials_past_the_table_are_concrete)
    {
        EXPECT_EQ(surfaceOfMaterial(0, true), Surface::ConcSolid);
        EXPECT_EQ(surfaceOfMaterial(-3, true), Surface::ConcSolid);
        EXPECT_EQ(surfaceOfMaterial(36, true), Surface::ConcSolid);
        EXPECT_EQ(surfaceOfMaterial(1000, false), Surface::ConcSolid);
    }

    TEST(FalloutScriptFootstepsTest, fallout_3_has_thirty_two_materials_and_no_chain_link_tile_or_carpet)
    {
        // Past its table, where New Vegas has three more surfaces
        EXPECT_EQ(surfaceOfMaterial(32, false), Surface::ConcSolid);
        EXPECT_EQ(surfaceOfMaterial(33, false), Surface::ConcSolid);
        EXPECT_EQ(surfaceOfMaterial(34, false), Surface::ConcSolid);
        EXPECT_EQ(surfaceOfMaterial(35, false), Surface::ConcSolid);
        // and the first thirty-two are those of New Vegas
        for (int material = 0; material < 32; ++material)
            EXPECT_EQ(surfaceOfMaterial(material, false), surfaceOfMaterial(material, true)) << material;
    }

    TEST(FalloutScriptFootstepsTest, every_surface_has_a_name_and_the_parents_are_the_executables)
    {
        std::set<std::string> names;
        for (int i = 0; i < surfaceCount; ++i)
            names.insert(std::string(surfaceName(static_cast<Surface>(i))));
        EXPECT_EQ(names.size(), static_cast<std::size_t>(surfaceCount));
        EXPECT_FALSE(parentOf(Surface::ConcSolid).has_value());
        EXPECT_EQ(parentOf(Surface::ConcBroken), Surface::ConcSolid);
        EXPECT_EQ(parentOf(Surface::MetalHollow), Surface::MetalSolid);
        EXPECT_EQ(parentOf(Surface::MetalSheet), Surface::MetalHollow);
        EXPECT_EQ(parentOf(Surface::Sand), Surface::Dirt);
        EXPECT_EQ(parentOf(Surface::Grass), Surface::Dirt);
        EXPECT_EQ(parentOf(Surface::ChainLink), Surface::Dirt);
        EXPECT_EQ(parentOf(Surface::Water), Surface::ConcSolid);
    }

    TEST(FalloutScriptFootstepsTest, the_slots_swap_left_and_right_as_the_executable_does)
    {
        // The key Left (side 0) walks on the foot named R, runs on the one named L
        EXPECT_EQ(chooseSlot(Foot::Left, false, false, false), Slot::WalkR);
        EXPECT_EQ(chooseSlot(Foot::Right, false, false, false), Slot::WalkL);
        EXPECT_EQ(chooseSlot(Foot::Left, false, false, true), Slot::RunL);
        EXPECT_EQ(chooseSlot(Foot::Right, false, false, true), Slot::RunR);
        EXPECT_EQ(chooseSlot(Foot::Left, true, false, false), Slot::SneakR);
        EXPECT_EQ(chooseSlot(Foot::Right, true, false, false), Slot::SneakL);
        EXPECT_EQ(chooseSlot(Foot::Left, false, true, false), Slot::StairsR);
        EXPECT_EQ(chooseSlot(Foot::Right, false, true, false), Slot::StairsL);
    }

    TEST(FalloutScriptFootstepsTest, the_back_feet_step_as_the_front_ones)
    {
        EXPECT_EQ(chooseSlot(Foot::BackLeft, false, false, false), chooseSlot(Foot::Left, false, false, false));
        EXPECT_EQ(chooseSlot(Foot::BackRight, false, false, true), chooseSlot(Foot::Right, false, false, true));
    }

    TEST(FalloutScriptFootstepsTest, sneaking_comes_before_stairs_before_running)
    {
        EXPECT_EQ(chooseSlot(Foot::Left, true, true, true), Slot::SneakR);
        EXPECT_EQ(chooseSlot(Foot::Left, false, true, true), Slot::StairsR);
        EXPECT_EQ(chooseSlot(Foot::Left, false, false, true), Slot::RunL);
    }

    TEST(FalloutScriptFootstepsTest, the_text_keys_name_the_feet)
    {
        EXPECT_EQ(footOfKey("left"), Foot::Left);
        EXPECT_EQ(footOfKey("right"), Foot::Right);
        EXPECT_EQ(footOfKey("backleft"), Foot::BackLeft);
        EXPECT_EQ(footOfKey("backright"), Foot::BackRight);
        EXPECT_FALSE(footOfKey("idle").has_value());
        EXPECT_FALSE(footOfKey("Left").has_value());
    }

    TEST(FalloutScriptFootstepsTest, sounds_are_named_for_surface_and_slot)
    {
        EXPECT_EQ(soundName(Surface::Dirt, Slot::WalkL), "FSTDirtWalkL");
        EXPECT_EQ(soundName(Surface::ConcSolid, Slot::RunR), "FSTConcSolidRunR");
        EXPECT_EQ(soundName(Surface::MetalHollow, Slot::SneakL), "FSTMetalHollowSneakL");
        EXPECT_EQ(soundName(Surface::Grass, Slot::Land), "FSTGrassLand");
        EXPECT_EQ(soundName(Surface::Sand, Slot::Jump), "FSTSandJump");
    }

    TEST(FalloutScriptFootstepsTest, a_surface_without_the_sound_uses_its_parents)
    {
        const std::set<std::string, std::less<>> sounds = { "FSTDirtWalkL", "FSTConcSolidSneakL", "FSTConcSolidWalkR" };
        const auto exists = [&](std::string_view name) { return sounds.contains(name); };
        EXPECT_EQ(resolveSound(Surface::Dirt, Slot::WalkL, exists), "FSTDirtWalkL");
        // Grass has no sound of its own: its parent, the dirt, has
        EXPECT_EQ(resolveSound(Surface::Grass, Slot::WalkL, exists), "FSTDirtWalkL");
        // Sand's parent is dirt, whose parent is concrete: three steps at most
        EXPECT_EQ(resolveSound(Surface::Sand, Slot::SneakL, exists), "FSTConcSolidSneakL");
        // a metal sheet goes through the hollow metal and the solid one, which has none either
        EXPECT_EQ(resolveSound(Surface::MetalSheet, Slot::WalkL, exists), "FSTConcSolidWalkR");
        // and where there is not even that, none
        EXPECT_TRUE(resolveSound(Surface::Dirt, Slot::WalkL, [](std::string_view) { return false; }).empty());
    }

    TEST(FalloutScriptFootstepsTest, the_chain_stops_after_two_parents_as_the_executables_table_does)
    {
        // Metal sheet -> hollow -> solid is the longest chain of the table that is looked at; a sound of the concrete
        // that the solid metal's parent would give is not asked for by name, but the last resort is the same sound
        const std::set<std::string, std::less<>> sounds = { "FSTConcSolidRunL" };
        const auto exists = [&](std::string_view name) { return sounds.contains(name); };
        EXPECT_TRUE(resolveSound(Surface::MetalSheet, Slot::RunL, exists).empty());
        const std::set<std::string, std::less<>> withSolid = { "FSTMetalSolidRunL" };
        EXPECT_EQ(resolveSound(Surface::MetalSheet, Slot::RunL, [&](std::string_view n) { return withSolid.contains(n); }),
            "FSTMetalSolidRunL");
    }

    TEST(FalloutScriptFootstepsTest, armor_adds_a_sound_of_its_own)
    {
        EXPECT_EQ(armorSoundName(false, Foot::Left), "FSTArmorR");
        EXPECT_EQ(armorSoundName(false, Foot::Right), "FSTArmorL");
        EXPECT_EQ(armorSoundName(true, Foot::Left), "FSTArmorPowerR");
        EXPECT_EQ(armorSoundName(true, Foot::BackRight), "FSTArmorPowerL");
        EXPECT_EQ(armorJumpSoundName(true), "FSTJumpPower");
        EXPECT_EQ(armorJumpSoundName(false), "FSTJumpArmor");
    }

    TEST(FalloutScriptFootstepsTest, a_step_is_heard_up_to_the_ini_distance)
    {
        const Settings settings;
        EXPECT_TRUE(isHeard(settings, 0.f));
        EXPECT_TRUE(isHeard(settings, 1100.f));
        EXPECT_FALSE(isHeard(settings, 1100.5f));
    }

    TEST(FalloutScriptFootstepsTest, the_volumes_are_the_ini_one_for_the_player_and_near_one_for_the_rest)
    {
        const Settings settings;
        EXPECT_FLOAT_EQ(stepVolume(settings, true, 0.3f), 0.65f);
        EXPECT_FLOAT_EQ(stepVolume(settings, false, 0.f), 0.9f);
        EXPECT_FLOAT_EQ(stepVolume(settings, false, 1.f), 1.f);
        EXPECT_FLOAT_EQ(armorVolume(true, settings, 0.7f), 0.65f);
        EXPECT_FLOAT_EQ(armorVolume(false, settings, 0.f), 1.f);
        EXPECT_FLOAT_EQ(armorVolume(false, settings, 1.f), 0.9f);
    }

    TEST(FalloutScriptFootstepsTest, a_step_reaches_a_hundred_units_for_each_byte_of_the_maximum)
    {
        EXPECT_TRUE(reachesListener(1600.f, 16));
        EXPECT_FALSE(reachesListener(1600.5f, 16));
        // a sound with no maximum reaches nowhere past the listener's own place
        EXPECT_FALSE(reachesListener(1.f, 0));
        EXPECT_TRUE(reachesListener(0.f, 0));
    }

    TEST(FalloutScriptFootstepsTest, the_words_of_the_enum_keys_are_the_indexes_of_the_executables_table)
    {
        EXPECT_EQ(enumKeyIndex("left"), 0);
        EXPECT_EQ(enumKeyIndex("backright"), 3);
        EXPECT_EQ(enumKeyIndex("idle"), 4);
        EXPECT_EQ(enumKeyIndex("death"), 8);
        EXPECT_EQ(enumKeyIndex("foleywalk"), static_cast<int>(EnumKey::FoleyWalk));
        EXPECT_EQ(enumKeyIndex("armorrun"), 22);
        EXPECT_EQ(enumKeyIndex("auxiliary8"), 30);
        EXPECT_EQ(enumKeyIndex("Left"), std::nullopt); // the keys are kept in lower case
        EXPECT_EQ(enumKeyIndex("bip01"), std::nullopt);
        EXPECT_EQ(enumKeyIndex(""), std::nullopt);
    }

    TEST(FalloutScriptFootstepsTest, a_list_of_chances_picks_by_shares_of_a_hundred)
    {
        EXPECT_EQ(pickByChance({ 100 }, 0.f), 0);
        EXPECT_EQ(pickByChance({ 100 }, 0.999f), 0);
        EXPECT_EQ(pickByChance({ 50, 50 }, 0.25f), 0);
        EXPECT_EQ(pickByChance({ 50, 50 }, 0.75f), 1);
        // A list that comes to less than a hundred leaves the rest as no sound
        EXPECT_EQ(pickByChance({ 40 }, 0.3f), 0);
        EXPECT_EQ(pickByChance({ 40 }, 0.6f), -1);
        // One that comes to more is shared out by its total
        EXPECT_EQ(pickByChance({ 100, 100 }, 0.4f), 0);
        EXPECT_EQ(pickByChance({ 100, 100 }, 0.6f), 1);
        EXPECT_EQ(pickByChance({}, 0.5f), -1);
        EXPECT_EQ(pickByChance({ 0, 0 }, 0.5f), -1);
        EXPECT_EQ(pickByChance({ -5, 100 }, 0.5f), 1);
    }

    TEST(FalloutScriptFootstepsTest, a_flat_tread_with_a_step_ahead_or_behind_is_stairs)
    {
        const StairSettings settings;
        // a tread of a flight going up: the next one is 14 units higher, the one before is 14 lower
        EXPECT_TRUE(isStairs(settings, 1.f, 100.f, 114.f, 86.f));
        // going down: only the tread ahead is lower
        EXPECT_TRUE(isStairs(settings, 1.f, 100.f, 86.f, std::nullopt));
        // at the top of a flight: only the one behind is lower
        EXPECT_TRUE(isStairs(settings, 0.99f, 100.f, std::nullopt, 86.f));
    }

    TEST(FalloutScriptFootstepsTest, level_ground_a_slope_and_a_wall_are_not_stairs)
    {
        const StairSettings settings;
        // a floor with a floor on both sides
        EXPECT_FALSE(isStairs(settings, 1.f, 100.f, 100.f, 101.f));
        // a gentle slope changes a few units over a stride: less than a step
        EXPECT_FALSE(isStairs(settings, 1.f, 100.f, 103.f, 97.f));
        // a ramp has a tilted normal even where it changes as much as a step
        EXPECT_FALSE(isStairs(settings, 0.8f, 100.f, 114.f, 86.f));
        // a ledge higher than a step is a wall, not a stair
        EXPECT_FALSE(isStairs(settings, 1.f, 100.f, 160.f, std::nullopt));
        // nothing found around: not stairs
        EXPECT_FALSE(isStairs(settings, 1.f, 100.f, std::nullopt, std::nullopt));
    }

    TEST(FalloutScriptFootstepsTest, the_limits_of_a_step_are_inclusive)
    {
        StairSettings settings;
        settings.mMinStep = 6.f;
        settings.mMaxStep = 20.f;
        EXPECT_TRUE(isStairs(settings, 1.f, 0.f, 6.f, std::nullopt));
        EXPECT_TRUE(isStairs(settings, 1.f, 0.f, -20.f, std::nullopt));
        EXPECT_FALSE(isStairs(settings, 1.f, 0.f, 5.9f, std::nullopt));
        EXPECT_FALSE(isStairs(settings, 1.f, 0.f, 20.1f, std::nullopt));
    }
}
