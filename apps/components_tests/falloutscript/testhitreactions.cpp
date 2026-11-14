#include <components/falloutscript/hitreactions.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace
{
    using namespace FalloutScript::HitReactions;

    TEST(FalloutScriptHitReactionsTest, the_settings_keep_the_executables_defaults_when_the_plugins_say_nothing)
    {
        const Settings settings = Settings::fromGameSettings([](std::string_view) { return std::nullopt; });
        EXPECT_DOUBLE_EQ(settings.mOneCrippledLeg, 0.85);
        EXPECT_DOUBLE_EQ(settings.mTwoCrippledLegs, 0.75);
        EXPECT_DOUBLE_EQ(settings.mKnockdownBaseHealthThreshold, 75);
        EXPECT_DOUBLE_EQ(settings.mKnockdownCurrentHealthThreshold, 25);
        EXPECT_DOUBLE_EQ(settings.mKnockdownChance, 0.25);
        EXPECT_DOUBLE_EQ(settings.mKnockbackForceMax, 512);
        EXPECT_DOUBLE_EQ(settings.mKnockbackTime, 1);
    }

    TEST(FalloutScriptHitReactionsTest, a_game_setting_replaces_a_default)
    {
        const std::map<std::string, double, std::less<>> plugin = { { "fKnockbackForceMax", 300 } };
        const Settings settings = Settings::fromGameSettings([&](std::string_view name) -> std::optional<double> {
            const auto found = plugin.find(name);
            if (found == plugin.end())
                return std::nullopt;
            return found->second;
        });
        EXPECT_DOUBLE_EQ(settings.mKnockbackForceMax, 300);
        EXPECT_DOUBLE_EQ(settings.mKnockbackTime, 1);
    }

    TEST(FalloutScriptHitReactionsTest, a_leg_is_crippled_when_nothing_is_left_of_its_condition)
    {
        EXPECT_EQ(crippledLegs(100, 100), 0);
        EXPECT_EQ(crippledLegs(0.5, 100), 0);
        EXPECT_EQ(crippledLegs(0, 100), 1);
        EXPECT_EQ(crippledLegs(100, -3), 1);
        EXPECT_EQ(crippledLegs(0, 0), 2);
    }

    TEST(FalloutScriptHitReactionsTest, crippled_legs_slow_a_character_by_the_settings)
    {
        const Settings settings;
        EXPECT_DOUBLE_EQ(legSpeedMultiplier(settings, 0), 1);
        EXPECT_DOUBLE_EQ(legSpeedMultiplier(settings, 1), 0.85);
        EXPECT_DOUBLE_EQ(legSpeedMultiplier(settings, 2), 0.75);
    }

    TEST(FalloutScriptHitReactionsTest, nobody_dead_or_killed_by_the_blow_is_knocked_down)
    {
        const Settings settings;
        EXPECT_EQ(knockdownKind(settings, 10, 0, 100), Knockdown::Never);
        EXPECT_EQ(knockdownKind(settings, 100, 100, 100), Knockdown::Never);
        EXPECT_EQ(knockdownKind(settings, 150, 100, 100), Knockdown::Never);
    }

    TEST(FalloutScriptHitReactionsTest, a_blow_of_most_of_the_base_health_knocks_down_for_sure)
    {
        const Settings settings;
        // 80 of 100 base health, the character has 200 now: more than 75 percent of the base health
        EXPECT_EQ(knockdownKind(settings, 80, 200, 100), Knockdown::Always);
        // exactly 75 percent is not more
        EXPECT_NE(knockdownKind(settings, 75, 200, 100), Knockdown::Always);
    }

    TEST(FalloutScriptHitReactionsTest, a_smaller_blow_knocks_down_by_chance_only_when_it_takes_over_a_quarter_of_the_health)
    {
        const Settings settings;
        EXPECT_EQ(knockdownKind(settings, 20, 100, 100), Knockdown::Never);
        EXPECT_EQ(knockdownKind(settings, 25, 100, 100), Knockdown::Never);
        EXPECT_EQ(knockdownKind(settings, 26, 100, 100), Knockdown::ByChance);
    }

    TEST(FalloutScriptHitReactionsTest, the_chance_of_a_knockdown_goes_up_with_the_blow_and_down_with_agility)
    {
        const Settings settings;
        // damage 20 and agility 5: 20 * 0.3 / (5 * 10) = 0.12, 120 of a thousand
        EXPECT_EQ(knockdownPerMille(settings, 20, 5), 120);
        // the same blow on a character with agility 10: 0.06
        EXPECT_EQ(knockdownPerMille(settings, 20, 10), 60);
        // the chance is at most a quarter
        EXPECT_EQ(knockdownPerMille(settings, 200, 5), 250);
        // an agility of 0 has no divisor: the largest chance
        EXPECT_EQ(knockdownPerMille(settings, 20, 0), 250);
    }

    TEST(FalloutScriptHitReactionsTest, the_knockback_force_goes_up_with_the_blow_and_down_with_agility)
    {
        const Settings settings;
        // agility 5 is a factor of 1 - 0.008 * 50 = 0.6 and damage 20 is 20 * 10 + 50 = 250: a force of 150
        EXPECT_NEAR(knockbackForce(settings, 20, 5), 150, 1e-9);
        // agility 10: 1 - 0.8 = 0.2 of the same 250
        EXPECT_NEAR(knockbackForce(settings, 20, 10), 50, 1e-9);
    }

    TEST(FalloutScriptHitReactionsTest, the_knockback_force_has_a_ceiling)
    {
        const Settings settings;
        EXPECT_DOUBLE_EQ(knockbackForce(settings, 500, 1), 512);
    }

    TEST(FalloutScriptHitReactionsTest, the_knocked_answer_is_one_for_the_states_three_and_four_only)
    {
        for (int state = 0; state <= 6; ++state)
            EXPECT_EQ(knockedAnswer(state), state == 3 || state == 4) << state;
        EXPECT_FALSE(knockedAnswer(-1));
        EXPECT_FALSE(knockedAnswer(7));
    }

    TEST(FalloutScriptHitReactionsTest, a_knock_lasts_its_seconds_and_a_new_one_replaces_it)
    {
        KnockTracker knocks;
        EXPECT_EQ(knocks.stateOf(0x14), KnockState::none);
        knocks.start(0x14, 1.0);
        EXPECT_TRUE(knockedAnswer(knocks.stateOf(0x14)));
        EXPECT_EQ(knocks.stateOf(0x15), KnockState::none);
        knocks.advance(0.6);
        EXPECT_TRUE(knockedAnswer(knocks.stateOf(0x14)));
        knocks.start(0x14, 1.0);
        knocks.advance(0.6);
        EXPECT_TRUE(knockedAnswer(knocks.stateOf(0x14)));
        knocks.advance(0.5);
        EXPECT_EQ(knocks.stateOf(0x14), KnockState::none);
    }

    TEST(FalloutScriptHitReactionsTest, a_knock_of_no_time_is_no_knock_and_clear_forgets_them_all)
    {
        KnockTracker knocks;
        knocks.start(0x14, 0.0);
        EXPECT_EQ(knocks.stateOf(0x14), KnockState::none);
        knocks.start(0x14, 2.0);
        knocks.start(0x14, 0.0);
        EXPECT_EQ(knocks.stateOf(0x14), KnockState::none);
        knocks.start(0x16, 2.0);
        knocks.clear();
        EXPECT_EQ(knocks.stateOf(0x16), KnockState::none);
    }
}
