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

    TEST(FalloutScriptWearTest, a_shot_takes_the_setting)
    {
        Wear::Settings settings;
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings), 0.2);
        settings.mWeaponShot = 0.1;
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings), 0.1);
        settings.mWeaponShot = -1;
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings), 0);
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
        EXPECT_DOUBLE_EQ(settings.mMaxRating, 85);
    }

    Wear::Settings fallout3()
    {
        // The values of Fallout3.esm (12AY R4)
        const Wear::Settings settings = Wear::Settings::fromGameSettings([](std::string_view name) -> std::optional<double> {
            if (name == "fDamageToWeaponGunMult")
                return 0.03;
            if (name == "fDamageToWeaponEnergyMult")
                return 0.04;
            if (name == "fDamageToWeaponLauncherMult")
                return 0.06;
            if (name == "fDamageToWeaponMeleeMult")
                return 0.05;
            if (name == "fArmorRatingBase")
                return 0.2;
            if (name == "fArmorRatingMax")
                return 1.0;
            return std::nullopt;
        });
        Wear::Settings result = settings;
        result.mFallout3 = true;
        return result;
    }

    TEST(FalloutScriptWearTest, fallout_3_reads_the_numbers_of_its_kinds_of_weapon_and_its_armor_line)
    {
        const Wear::Settings settings = fallout3();
        EXPECT_DOUBLE_EQ(settings.mGunMult, 0.03);
        EXPECT_DOUBLE_EQ(settings.mEnergyMult, 0.04);
        EXPECT_DOUBLE_EQ(settings.mLauncherMult, 0.06);
        EXPECT_DOUBLE_EQ(settings.mMeleeMult, 0.05);
        EXPECT_DOUBLE_EQ(settings.mArmorBase, 0.2);
        EXPECT_DOUBLE_EQ(settings.mArmorMax, 1.0);
        // What the executable has when the data says nothing
        const Wear::Settings bare;
        EXPECT_DOUBLE_EQ(bare.mGunMult, 0.01);
        EXPECT_DOUBLE_EQ(bare.mArmorBase, 0.35);
        EXPECT_FALSE(bare.mFallout3);
    }

    TEST(FalloutScriptWearTest, fallout_3_takes_the_damage_of_the_record_times_the_number_of_the_kind_of_weapon)
    {
        const Wear::Settings settings = fallout3();
        // A 10 mm pistol of 20 damage (type 3), a plasma pistol (4), a rifle (5), a minigun (8), a launcher (9), a knife (1)
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 3, false, 1.0 }), 0.6, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 4, false, 1.0 }), 0.8, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 30, 5, false, 1.0 }), 0.9, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 30, 6, false, 1.0 }), 0.9, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 30, 7, false, 1.0 }), 1.2, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 10, 8, false, 1.0 }), 0.6, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 100, 9, false, 1.0 }), 6.0, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 1, false, 1.0 }), 1.0, 1e-9);
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 2, false, 1.0 }), 1.0, 1e-9);
        // A thing that is thrown or laid keeps the whole damage
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 10, false, 1.0 }), 20.0, 1e-9);
    }

    TEST(FalloutScriptWearTest, fallout_3_uses_the_number_of_the_record_when_the_record_has_one_of_its_own)
    {
        const Wear::Settings settings = fallout3();
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 3, true, 0.005 }), 0.1, 1e-9);
        // A weapon with 0 never wears
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, { 20, 3, true, 0.0 }), 0.0);
        // The number of the record is ignored when its flag is not set
        EXPECT_NEAR(Wear::shotPoints(settings, { 20, 3, false, 0.005 }), 0.6, 1e-9);
    }

    TEST(FalloutScriptWearTest, new_vegas_keeps_its_flat_shot_whatever_the_record_says)
    {
        Wear::Settings settings;
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, { 20, 3, false, 1.0 }), 0.2);
        EXPECT_DOUBLE_EQ(Wear::shotPoints(settings, { 200, 9, true, 0.5 }), 0.2);
    }

    TEST(FalloutScriptWearTest, fallout_3_armor_gives_a_line_of_its_rating_from_a_fifth_to_the_whole)
    {
        const Wear::Settings settings = fallout3();
        EXPECT_DOUBLE_EQ(Wear::armorFactor(settings, 1.0), 1.0);
        EXPECT_NEAR(Wear::armorFactor(settings, 0.5), 0.6, 1e-9);
        EXPECT_NEAR(Wear::armorFactor(settings, 0.0), 0.2, 1e-9);
        EXPECT_NEAR(Wear::armorFactor(settings, -3.0), 0.2, 1e-9);
        // The rating of a piece is rounded; the card shows the same whole number
        EXPECT_DOUBLE_EQ(Wear::armorValue(settings, 25, 0.5), 15);
        EXPECT_DOUBLE_EQ(Wear::armorValue(settings, 17, 0.5), 10); // 10.2
        EXPECT_DOUBLE_EQ(Wear::armorShown(settings, 17, 0.7), 13); // 17 * 0.76 = 12.92
        EXPECT_DOUBLE_EQ(Wear::armorShown(settings, 40, 0.0), 8);
    }

    TEST(FalloutScriptWearTest, new_vegas_armor_keeps_its_curve_and_gives_the_next_whole_number_up)
    {
        const Wear::Settings settings;
        EXPECT_DOUBLE_EQ(Wear::armorFactor(settings, 0.8), 1.0);
        EXPECT_DOUBLE_EQ(Wear::armorFactor(settings, 0.25), 0.75);
        // 17 * 0.75 = 12.75: a piece gives 13, and the card shows the same
        EXPECT_DOUBLE_EQ(Wear::armorValue(settings, 17, 0.25), 13);
        EXPECT_DOUBLE_EQ(Wear::armorShown(settings, 17, 0.25), 13);
        // the whole number of the rating is what the factor works on, and a product that is whole stays so
        EXPECT_DOUBLE_EQ(Wear::armorValue(settings, 10.9, 0.5), 10);
        EXPECT_DOUBLE_EQ(Wear::armorValue(settings, 20, 0.35), 20 * 0.85);
        EXPECT_DOUBLE_EQ(Wear::armorValue(settings, 20, 0.8), 20);
    }

    TEST(FalloutScriptWearTest, a_threshold_wears_armor_that_has_no_rating_by_the_smaller_of_itself_and_what_is_left)
    {
        const Wear::Settings settings;
        // (the first number of each: the blow, the resistance, the threshold, what is left of the blow after both, the
        // share of the blow that is the floor), from the worked cases of the executable
        EXPECT_NEAR(Wear::armorPoints(settings, 22, 0, 10, 12, 0.2), 0.35 * 7.6, 1e-9); // min(10, 12 - 4.4) = 7.6: 2.66
        EXPECT_NEAR(Wear::armorPoints(settings, 50, 0, 10, 40, 0.2), 0.35 * 10, 1e-9); // the threshold is the most: 3.5
        EXPECT_NEAR(Wear::armorPoints(settings, 14, 0, 10, 4, 0.2), 0.35 * (4 - 2.8), 1e-9); // 0.42
        // a blow that the threshold took down to under the floor takes from the share and the armor does not wear
        EXPECT_LT(Wear::armorPoints(settings, 12, 0, 10, 2, 0.2), 0);
        EXPECT_LT(Wear::armorPoints(settings, 22, 0, 30, -8, 0.2), 0);
        // the resistance and the threshold wear together
        EXPECT_NEAR(Wear::armorPoints(settings, 100, 50, 10, 40, 0.2), 0.35 * (100 * 0.5 + 10), 1e-9);
        // a negative threshold is none
        EXPECT_NEAR(Wear::armorPoints(settings, 20, 50, -4, 10, 0.2), 0.35 * 20 * 0.5, 1e-9);
        // armor with nothing (no rating and no threshold) never wears
        EXPECT_DOUBLE_EQ(Wear::armorPoints(settings, 22, 0, 0, 22, 0.2), 0);
        // Fallout 3 has no threshold and wears by the resistance only
        Wear::Settings fallout3;
        fallout3.mFallout3 = true;
        EXPECT_DOUBLE_EQ(Wear::armorPoints(fallout3, 22, 0, 10, 12, 0.2), 0);
        EXPECT_NEAR(Wear::armorPoints(fallout3, 20, 50, 10, 0, 0.2), 0.35 * 20 * 0.5, 1e-9);
    }
}
