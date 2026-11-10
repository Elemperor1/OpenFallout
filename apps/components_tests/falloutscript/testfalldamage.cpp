#include <components/falloutscript/falldamage.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace
{
    using namespace FalloutScript::FallDamage;

    // What the plugins of both games say: only the minimum height and the exponent
    Settings pluginSettings()
    {
        const std::map<std::string, double, std::less<>> plugin = { { "fJumpFallHeightMin", 600 },
            { "fJumpFallHeightExponent", 1.65 }, { "fJumpFallVelocityMin", 2785 } };
        return Settings::fromGameSettings([&](std::string_view name) -> std::optional<double> {
            const auto found = plugin.find(name);
            if (found == plugin.end())
                return std::nullopt;
            return found->second;
        });
    }

    TEST(FalloutScriptFallDamageTest, the_settings_keep_the_executables_defaults_for_what_the_plugins_do_not_say)
    {
        const Settings settings = pluginSettings();
        EXPECT_DOUBLE_EQ(settings.mMinHeight, 600);
        EXPECT_DOUBLE_EQ(settings.mExponent, 1.65);
        // not in either plugin
        EXPECT_DOUBLE_EQ(settings.mMultiplier, 0.025);
        EXPECT_EQ(settings.mLegChance, 50);
        EXPECT_DOUBLE_EQ(settings.mLegMultiplier, 0.5);
        EXPECT_DOUBLE_EQ(settings.mHardLanding, 500);

        const Settings defaults = Settings::fromGameSettings([](std::string_view) { return std::nullopt; });
        EXPECT_DOUBLE_EQ(defaults.mMinHeight, 256);
        EXPECT_DOUBLE_EQ(defaults.mExponent, 2);
    }

    TEST(FalloutScriptFallDamageTest, a_fall_of_no_more_than_the_minimum_height_does_no_damage)
    {
        const Settings settings = pluginSettings();
        EXPECT_EQ(damage(settings, 0), 0);
        EXPECT_EQ(damage(settings, 599.9), 0);
        EXPECT_EQ(damage(settings, 600), 0);
        EXPECT_GT(damage(settings, 600.5), 0);
    }

    TEST(FalloutScriptFallDamageTest, the_damage_grows_as_a_power_of_the_excess_with_no_limit)
    {
        const Settings settings = pluginSettings();
        // 0.025 * 1000^1.65
        EXPECT_NEAR(damage(settings, 1600), 0.025 * std::pow(1000.0, 1.65), 1e-9);
        // The fall of 11,700 units of the round that found nothing happening: about 118,229 before the difficulty
        EXPECT_NEAR(damage(settings, 11700), 118229, 20);
        EXPECT_GT(damage(settings, 2000), 2 * damage(settings, 1300));
    }

    TEST(FalloutScriptFallDamageTest, the_difficulty_changes_what_the_player_takes_and_what_the_player_deals)
    {
        EXPECT_EQ(damageToPlayerMultiplier(0), 0.5);
        EXPECT_EQ(damageToPlayerMultiplier(normalDifficulty), 1);
        EXPECT_EQ(damageToPlayerMultiplier(4), 2);
        EXPECT_EQ(damageByPlayerMultiplier(0), 2);
        EXPECT_EQ(damageByPlayerMultiplier(normalDifficulty), 1);
        EXPECT_EQ(damageByPlayerMultiplier(4), 0.5);
        // a difficulty out of range is the nearest one
        EXPECT_EQ(damageToPlayerMultiplier(-3), 0.5);
        EXPECT_EQ(damageToPlayerMultiplier(9), 2);
    }

    TEST(FalloutScriptFallDamageTest, a_heavy_landing_is_one_above_the_threshold)
    {
        const Settings settings = pluginSettings();
        EXPECT_FALSE(isHardLanding(settings, 500));
        EXPECT_TRUE(isHardLanding(settings, 500.5));
        EXPECT_FALSE(isHardLanding(settings, 12));
    }

    TEST(FalloutScriptFallDamageTest, each_leg_is_hurt_by_a_roll_strictly_below_the_chance)
    {
        const Settings settings = pluginSettings();
        EXPECT_TRUE(legHurt(settings, 0));
        EXPECT_TRUE(legHurt(settings, 49));
        EXPECT_FALSE(legHurt(settings, 50));
        EXPECT_FALSE(legHurt(settings, 99));
    }
}
