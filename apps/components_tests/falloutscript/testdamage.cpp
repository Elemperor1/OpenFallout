#include <components/falloutscript/damage.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    using namespace FalloutScript;

    constexpr double noCritical = 100; // a roll that is never below the chance
    constexpr double alwaysCritical = 0;

    HitResult hit(const WeaponDamage& weapon, const Defense& defense = {}, double roll = noCritical, double chance = 10,
        double meleeBonus = 0, const DamageSettings& settings = {})
    {
        return Damage::resolve(settings, weapon, chance, meleeBonus, defense, roll);
    }

    TEST(FalloutScriptDamageTest, a_blow_without_armor_does_the_damage_of_the_weapon)
    {
        WeaponDamage weapon;
        weapon.mDamage = 20;
        const HitResult result = hit(weapon);
        EXPECT_DOUBLE_EQ(result.mDamage, 20);
        EXPECT_FALSE(result.mCritical);
    }

    TEST(FalloutScriptDamageTest, the_strength_of_the_attacker_adds_to_a_melee_blow_only)
    {
        WeaponDamage weapon;
        weapon.mDamage = 10;
        EXPECT_DOUBLE_EQ(hit(weapon, {}, noCritical, 10, 5).mDamage, 10);
        weapon.mMelee = true;
        EXPECT_DOUBLE_EQ(hit(weapon, {}, noCritical, 10, 5).mDamage, 15);

        DamageSettings none;
        none.mMeleeStrengthBonus = false;
        EXPECT_DOUBLE_EQ(hit(weapon, {}, noCritical, 10, 5, none).mDamage, 10);
    }

    TEST(FalloutScriptDamageTest, a_worn_weapon_does_less_by_the_settings_for_guns_and_for_melee_weapons)
    {
        WeaponDamage weapon;
        weapon.mDamage = 20;
        weapon.mCondition = 0.5;
        EXPECT_NEAR(hit(weapon).mDamage, 20 * (0.66 + 0.34 * 0.5), 1e-9);
        weapon.mMelee = true;
        EXPECT_NEAR(hit(weapon).mDamage, 20 * (0.5 + 0.5 * 0.5), 1e-9);

        weapon.mCondition = 3; // more than all is no more than all
        EXPECT_DOUBLE_EQ(hit(weapon).mDamage, 20);
        weapon.mMelee = false;
        EXPECT_DOUBLE_EQ(hit(weapon).mDamage, 20);
    }

    TEST(FalloutScriptDamageTest, the_skill_with_the_weapon_changes_the_blow)
    {
        WeaponDamage weapon;
        weapon.mDamage = 20;
        weapon.mSkill = 0;
        EXPECT_DOUBLE_EQ(hit(weapon).mDamage, 10); // half at the lowest skill
        weapon.mSkill = 50;
        EXPECT_DOUBLE_EQ(hit(weapon).mDamage, 15);
        weapon.mSkill = 300;
        EXPECT_DOUBLE_EQ(hit(weapon).mDamage, 20);
    }

    TEST(FalloutScriptDamageTest, a_critical_blow_adds_the_critical_damage_of_the_weapon)
    {
        WeaponDamage weapon;
        weapon.mDamage = 20;
        weapon.mCriticalDamage = 12;
        const HitResult result = hit(weapon, {}, alwaysCritical, 10);
        EXPECT_TRUE(result.mCritical);
        EXPECT_DOUBLE_EQ(result.mDamage, 32);
    }

    TEST(FalloutScriptDamageTest, armor_works_on_the_critical_bonus_too)
    {
        WeaponDamage weapon;
        weapon.mDamage = 40;
        weapon.mCriticalDamage = 10;
        Defense defense;
        defense.mThreshold = 10;
        defense.mResistance = 50;
        // (40 + 10) * 0.5 - 10 = 15, and without the bonus 40 * 0.5 - 10 = 10
        EXPECT_DOUBLE_EQ(hit(weapon, defense, alwaysCritical, 10).mDamage, 15);
        EXPECT_DOUBLE_EQ(hit(weapon, defense, noCritical, 10).mDamage, 10);
    }

    TEST(FalloutScriptDamageTest, the_chance_of_a_critical_blow_is_that_of_the_attacker_times_the_weapons)
    {
        WeaponDamage weapon;
        weapon.mCriticalChanceMultiplier = 2;
        EXPECT_DOUBLE_EQ(Damage::criticalChance(weapon, 10), 20);
        EXPECT_DOUBLE_EQ(Damage::criticalChance(weapon, 80), 100);
        weapon.mCriticalChanceMultiplier = -1;
        EXPECT_DOUBLE_EQ(Damage::criticalChance(weapon, 10), 0);

        weapon.mCriticalChanceMultiplier = 1;
        weapon.mDamage = 1;
        EXPECT_TRUE(hit(weapon, {}, 9.9, 10).mCritical);
        EXPECT_FALSE(hit(weapon, {}, 10, 10).mCritical);
    }

    TEST(FalloutScriptDamageTest, a_threshold_takes_points_but_leaves_a_share_of_the_blow)
    {
        WeaponDamage weapon;
        weapon.mDamage = 20;
        Defense defense;
        defense.mThreshold = 15;
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 5);
        defense.mThreshold = 18; // 2 would be left, a fifth of the blow is 4
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 4);
        defense.mThreshold = 500;
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 4);
    }

    TEST(FalloutScriptDamageTest, resistance_takes_a_share_up_to_the_most_armor_can)
    {
        WeaponDamage weapon;
        weapon.mDamage = 40;
        Defense defense;
        defense.mResistance = 50;
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 20);
        defense.mResistance = 100;
        DamageSettings fallout3; // no floor: only the most that armor can take shows
        fallout3.mMinimumDamage = false;
        EXPECT_DOUBLE_EQ(hit(weapon, defense, noCritical, 10, 0, fallout3).mDamage, 6); // 85 percent is the most
        defense.mResistance = -30;
        EXPECT_DOUBLE_EQ(
            hit(weapon, defense, noCritical, 10, 0, fallout3).mDamage, 40); // armor does not make a blow worse
    }

    TEST(FalloutScriptDamageTest, the_resistance_comes_before_the_threshold)
    {
        WeaponDamage weapon;
        weapon.mDamage = 100;
        Defense defense;
        defense.mThreshold = 20;
        defense.mResistance = 50;
        // 100 * 0.5 = 50, then 20 points less
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 30);
    }

    TEST(FalloutScriptDamageTest, new_vegas_keeps_a_share_of_the_blow_whatever_the_armor_takes_and_fallout_3_does_not)
    {
        WeaponDamage weapon;
        weapon.mDamage = 40;
        Defense defense;
        defense.mResistance = 100; // 85 percent is the most, which leaves 6
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 8); // a fifth of the blow
        DamageSettings fallout3;
        fallout3.mMinimumDamage = false;
        EXPECT_DOUBLE_EQ(hit(weapon, defense, noCritical, 10, 0, fallout3).mDamage, 6);
    }

    TEST(FalloutScriptDamageTest, a_threshold_below_nothing_takes_nothing)
    {
        WeaponDamage weapon;
        weapon.mDamage = 20;
        Defense defense;
        defense.mThreshold = -5; // the perks of an attacker can take off more than the armor has
        EXPECT_DOUBLE_EQ(hit(weapon, defense).mDamage, 20);
    }

    TEST(FalloutScriptDamageTest, a_weapon_without_damage_does_none)
    {
        WeaponDamage weapon;
        weapon.mDamage = -4;
        EXPECT_DOUBLE_EQ(hit(weapon).mDamage, 0);
    }

    TEST(FalloutScriptDamageTest, the_settings_come_from_the_game_settings_that_the_files_have)
    {
        const std::map<std::string, double> values{ { "fMaxArmorRating", 75 }, { "fDamageSkillBase", 1 },
            { "fDamageSkillMult", 0 }, { "fDamageGunWeapCondBase", 0.7 }, { "fDamageMeleeWeapCondMult", 0.25 },
            { "fMinDamMultiplier", 0.1 }, { "fFriendHitTimer", 20 } };
        const DamageSettings settings
            = DamageSettings::fromGameSettings([&values](std::string_view name) -> std::optional<double> {
                  const auto found = values.find(std::string(name));
                  if (found == values.end())
                      return std::nullopt;
                  return found->second;
              });
        EXPECT_DOUBLE_EQ(settings.mMaxResistance, 75);
        EXPECT_DOUBLE_EQ(settings.mMinimumThroughThreshold, 0.1);
        EXPECT_DOUBLE_EQ(settings.mSkillBase, 1);
        EXPECT_DOUBLE_EQ(settings.mSkillMult, 0);
        EXPECT_DOUBLE_EQ(settings.mGunConditionBase, 0.7);
        EXPECT_DOUBLE_EQ(settings.mGunConditionMult, 0.34);
        EXPECT_DOUBLE_EQ(settings.mMeleeConditionBase, 0.5);
        EXPECT_DOUBLE_EQ(settings.mMeleeConditionMult, 0.25);
        EXPECT_DOUBLE_EQ(settings.mFriendHitSeconds, 20);
        EXPECT_DOUBLE_EQ(settings.mFriendHitSpacingSeconds, 0.5);

        EXPECT_DOUBLE_EQ(DamageSettings::fromGameSettings({}).mMaxResistance, 85);
        EXPECT_DOUBLE_EQ(DamageSettings::fromGameSettings({}).mMinimumThroughThreshold, 0.2);
        EXPECT_DOUBLE_EQ(DamageSettings::fromGameSettings({}).mFriendHitSeconds, 10);
    }
}
