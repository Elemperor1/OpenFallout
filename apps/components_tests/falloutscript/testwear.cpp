#include <gtest/gtest.h>

#include <components/falloutscript/wear.hpp>

namespace
{
    using namespace FalloutScript;

    TEST(FalloutScriptWearTest, a_weapon_does_its_damage_down_to_three_quarters_and_less_below_that)
    {
        EXPECT_DOUBLE_EQ(Wear::weaponDamageFactor(1.0), 1.0);
        EXPECT_DOUBLE_EQ(Wear::weaponDamageFactor(0.76), 1.0);
        EXPECT_NEAR(Wear::weaponDamageFactor(0.5), 0.8325, 1e-9);
        EXPECT_NEAR(Wear::weaponDamageFactor(0.25), 0.665, 1e-9);
        EXPECT_NEAR(Wear::weaponDamageFactor(0.0), 0.4975, 1e-9);
        EXPECT_NEAR(Wear::weaponDamageFactor(-1.0), 0.4975, 1e-9);
    }

    TEST(FalloutScriptWearTest, armor_keeps_its_rating_down_to_a_half_and_loses_up_to_a_half_below_that)
    {
        EXPECT_DOUBLE_EQ(Wear::armorFactor(1.0), 1.0);
        EXPECT_DOUBLE_EQ(Wear::armorFactor(0.51), 1.0);
        EXPECT_DOUBLE_EQ(Wear::armorFactor(0.25), 0.75);
        EXPECT_DOUBLE_EQ(Wear::armorFactor(0.0), 0.5);
    }

    TEST(FalloutScriptWearTest, the_effects_of_ammunition_add_multiply_and_subtract)
    {
        EXPECT_DOUBLE_EQ(Wear::applyAmmoEffect(0.2, 0, 0.1), 0.3);
        EXPECT_DOUBLE_EQ(Wear::applyAmmoEffect(0.2, 1, 3.0), 0.6000000000000001);
        EXPECT_DOUBLE_EQ(Wear::applyAmmoEffect(0.5, 2, 0.25), 0.25);
        EXPECT_DOUBLE_EQ(Wear::applyAmmoEffect(0.5, 9, 4), 0.5);
    }

    TEST(FalloutScriptWearTest, a_shot_takes_the_setting_and_a_shot_of_vats_takes_more)
    {
        Wear::Settings settings;
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, false), 0.2);
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, true), 0.2);
        settings.mVatsMultiplier = 2.0;
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, true), 0.4);
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, false), 0.2);
    }

    TEST(FalloutScriptWearTest, armor_takes_a_share_of_the_blow_that_it_stops_and_no_more_than_it_can_stop)
    {
        Wear::Settings settings;
        EXPECT_NEAR(Wear::armorPoints(settings, 20, 50), 0.35 * 20 * 0.5, 1e-9);
        EXPECT_NEAR(Wear::armorPoints(settings, 20, 300), 0.35 * 20 * 0.85, 1e-9);
        EXPECT_DOUBLE_EQ(Wear::armorPoints(settings, -5, 50), 0);
    }

    TEST(FalloutScriptWearTest, points_of_health_are_tenths_of_a_percent_and_the_rest_is_kept)
    {
        double rest = 0;
        // 0.2 of 500 is 0.4 of a tenth of a percent, so the first two shots lose none and the third loses one
        EXPECT_EQ(Wear::tenthsLost(0.2, 500, rest), 0);
        EXPECT_NEAR(rest, 0.4, 1e-9);
        EXPECT_EQ(Wear::tenthsLost(0.2, 500, rest), 0);
        EXPECT_EQ(Wear::tenthsLost(0.2, 500, rest), 1);
        EXPECT_NEAR(rest, 0.2, 1e-9);
        EXPECT_EQ(Wear::tenthsLost(50, 500, rest), 100);
        EXPECT_EQ(Wear::tenthsLost(0, 500, rest), 0);
        EXPECT_EQ(Wear::tenthsLost(10, 0, rest), 0);
    }

    TEST(FalloutScriptWearTest, the_warning_comes_when_the_condition_goes_below_a_quarter)
    {
        EXPECT_TRUE(Wear::crossedLow(250, 249));
        EXPECT_TRUE(Wear::crossedLow(1000, 100));
        EXPECT_FALSE(Wear::crossedLow(249, 100));
        EXPECT_FALSE(Wear::crossedLow(400, 250));
        EXPECT_FALSE(Wear::crossedLow(300, 400));
    }

    TEST(FalloutScriptWearTest, the_settings_come_from_the_game_settings_that_the_files_have)
    {
        const Wear::Settings settings = Wear::Settings::fromGameSettings([](std::string_view name) -> std::optional<double> {
            if (name == "fDamageToWeaponValue")
                return 0.1;
            if (name == "fDamageToArmorPercentage")
                return 0.5;
            return std::nullopt;
        });
        EXPECT_DOUBLE_EQ(settings.mWeaponShot, 0.1);
        EXPECT_DOUBLE_EQ(settings.mArmorShare, 0.5);
        EXPECT_DOUBLE_EQ(settings.mVatsMultiplier, 1.0);
        EXPECT_DOUBLE_EQ(settings.mMaxRating, 85);
    }
}
