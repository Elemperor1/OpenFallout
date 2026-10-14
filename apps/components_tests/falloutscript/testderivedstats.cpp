#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/calendar.hpp>
#include <components/falloutscript/derivedstats.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::ActorValues;
    using FalloutScript::Calendar;
    using FalloutScript::DerivedStatSettings;
    using FalloutScript::FormId;
    using FalloutScript::GameDate;
    using FalloutScript::DerivedStats::actionPoints;
    using FalloutScript::DerivedStats::carryWeight;
    using FalloutScript::DerivedStats::criticalChance;
    using FalloutScript::DerivedStats::meleeDamage;
    using FalloutScript::DerivedStats::npcHealth;
    using FalloutScript::DerivedStats::playerHealth;
    namespace ActorValue = FalloutScript::ActorValue;

    constexpr FormId player = 0x00000014;
    constexpr FormId guard = 0x01000100;

    DerivedStatSettings settingsOf(const std::map<std::string, double>& names)
    {
        return DerivedStatSettings::fromGameSettings([&names](std::string_view name) -> std::optional<double> {
            const auto found = names.find(std::string(name));
            return found == names.end() ? std::nullopt : std::optional<double>(found->second);
        });
    }

    TEST(FalloutScriptDerivedStatsTest, the_settings_are_those_of_new_vegas_until_the_files_say_otherwise)
    {
        const DerivedStatSettings none = settingsOf({});
        EXPECT_EQ(none.mHealthLevelMult, 5);
        EXPECT_EQ(none.mActionPointsMult, 3);

        const DerivedStatSettings fallout3 = settingsOf({ { "fAVDHealthLevelMult", 10 }, { "fAVDActionPointsMult", 2 },
            { "fAVDCarryWeightsBase", 150 }, { "fAVDCarryWeightMult", 10 } });
        EXPECT_EQ(fallout3.mHealthLevelMult, 10);
        EXPECT_EQ(fallout3.mActionPointsMult, 2);
        EXPECT_EQ(fallout3.mHealthEnduranceMult, 20);
        EXPECT_EQ(DerivedStatSettings::fromGameSettings({}).mCarryWeightMult, 10);
    }

    TEST(FalloutScriptDerivedStatsTest, the_player_has_200_health_at_the_first_level_with_5_endurance_in_both_games)
    {
        // The record of the player has 100 in both games; each level after the first gives 5 in New Vegas, 10 in
        // Fallout 3
        EXPECT_EQ(playerHealth(DerivedStatSettings(), 100, 5, 1), 200);
        EXPECT_EQ(playerHealth(settingsOf({ { "fAVDHealthLevelMult", 10 } }), 100, 5, 1), 200);
        EXPECT_EQ(playerHealth(DerivedStatSettings(), 100, 5, 2), 205);
        EXPECT_EQ(playerHealth(settingsOf({ { "fAVDHealthLevelMult", 10 } }), 100, 5, 2), 210);
        // more endurance and more levels give more
        EXPECT_EQ(playerHealth(DerivedStatSettings(), 100, 8, 10), 100 + 160 + 45);
        // a level below the first is the first
        EXPECT_EQ(playerHealth(DerivedStatSettings(), 100, 5, 0), 200);
    }

    TEST(FalloutScriptDerivedStatsTest, characters_and_creatures_have_less_health_for_their_endurance_and_level)
    {
        EXPECT_EQ(npcHealth(DerivedStatSettings(), 50, 5, 1), 50 + 25);
        EXPECT_EQ(npcHealth(DerivedStatSettings(), 500, 6, 20), 500 + 30 + 95);
        // never below a point of health
        EXPECT_EQ(npcHealth(DerivedStatSettings(), -100, 1, 1), 1);
    }

    TEST(FalloutScriptDerivedStatsTest, the_offsets_of_the_files_move_the_attribute_before_it_is_multiplied)
    {
        // GECK: Starting Health = END * mult + (the base + mult * offset) + the share of the level
        const DerivedStatSettings settings = settingsOf({ { "fAVDHealthEnduranceOffset", 1 },
            { "fAVDNPCHealthEnduranceOffset", -1 }, { "fAVDMeleeDamageStrengthOffset", -5 }, { "fAVDCritLuckBase", 3 },
            { "fAVDCritLuckMult", 2 } });
        EXPECT_EQ(settings.mHealthEnduranceOffset, 1);
        EXPECT_EQ(settings.mNpcHealthEnduranceOffset, -1);
        EXPECT_EQ(playerHealth(settings, 100, 5, 1), 100 + 6 * 20);
        EXPECT_EQ(npcHealth(settings, 50, 5, 2), 50 + 4 * 5 + 5);
        EXPECT_EQ(meleeDamage(settings, 7), 1);
        EXPECT_EQ(meleeDamage(settings, 2), 0); // never below nothing
        EXPECT_EQ(criticalChance(settings, 5), 13);
        EXPECT_EQ(criticalChance(settings, 60), 100);

        // The files that do not name them keep the offsets at nothing, the critical chance that of the luck
        const DerivedStatSettings none = settingsOf({});
        EXPECT_EQ(none.mCritLuckBase, 0);
        EXPECT_EQ(none.mCritLuckMult, 1);
        EXPECT_EQ(none.mMeleeDamageStrengthOffset, 0);
    }

    TEST(FalloutScriptDerivedStatsTest, the_other_stats_follow_the_attributes)
    {
        EXPECT_EQ(actionPoints(DerivedStatSettings(), 5), 80);
        EXPECT_EQ(actionPoints(settingsOf({ { "fAVDActionPointsMult", 2 } }), 5), 75);
        EXPECT_EQ(carryWeight(DerivedStatSettings(), 5), 200);
        EXPECT_EQ(carryWeight(DerivedStatSettings(), 10), 250);
        EXPECT_EQ(meleeDamage(DerivedStatSettings(), 7), 3.5);
        EXPECT_EQ(criticalChance(DerivedStatSettings(), 5), 5);
        EXPECT_EQ(criticalChance(DerivedStatSettings(), 500), 100);
        EXPECT_EQ(criticalChance(DerivedStatSettings(), -3), 0);
    }

    // Records that work the derived values out the way the host does, from the attributes the actor value store tells
    class DerivingRecords : public FalloutScript::ActorValueRecords
    {
    public:
        bool initialValues(FormId actor, std::vector<std::pair<int, double>>& values) override
        {
            if (actor == guard)
                values = { { ActorValue::endurance, 8 }, { ActorValue::agility, 7 } };
            return true;
        }

        std::optional<double> derivedValue(
            FormId actor, int value, const std::function<double(int)>& permanent) override
        {
            ++mAsked;
            switch (value)
            {
                case ActorValue::health:
                    return playerHealth(
                        DerivedStatSettings(), actor == player ? 100 : 50, permanent(ActorValue::endurance), mLevel);
                case ActorValue::actionPoints:
                    return actionPoints(DerivedStatSettings(), permanent(ActorValue::agility));
                default:
                    return std::nullopt;
            }
        }

        int mLevel = 1;
        int mAsked = 0;
    };

    TEST(FalloutScriptDerivedStatsTest, health_and_action_points_are_worked_out_from_the_attributes_of_the_actor)
    {
        DerivingRecords records;
        ActorValues values(&records);

        EXPECT_EQ(values.current(player, ActorValue::health), 200);
        EXPECT_EQ(values.current(guard, ActorValue::health), 50 + 8 * 20);
        EXPECT_EQ(values.current(guard, ActorValue::actionPoints), 65 + 7 * 3);
        // what the records do not work out stays what it was
        EXPECT_EQ(values.current(guard, ActorValue::fatigue), 100);
        EXPECT_EQ(values.current(guard, ActorValue::carryWeight), 0);
    }

    TEST(FalloutScriptDerivedStatsTest, a_worked_out_value_follows_the_attributes_and_the_level_that_change)
    {
        DerivingRecords records;
        ActorValues values(&records);

        EXPECT_EQ(values.current(player, ActorValue::health), 200);
        // a perk that adds endurance, and a new level
        values.modify(player, ActorValue::endurance, 1);
        EXPECT_EQ(values.current(player, ActorValue::health), 220);
        values.setBase(player, ActorValue::endurance, 10);
        records.mLevel = 3;
        EXPECT_EQ(values.current(player, ActorValue::health), 100 + 11 * 20 + 2 * 5);
    }

    TEST(FalloutScriptDerivedStatsTest, damage_comes_off_a_worked_out_value_and_a_set_value_replaces_it)
    {
        DerivingRecords records;
        ActorValues values(&records);

        values.damage(player, ActorValue::health, 60);
        EXPECT_EQ(values.current(player, ActorValue::health), 140);
        EXPECT_EQ(values.permanent(player, ActorValue::health), 200);

        values.setBase(guard, ActorValue::health, 12);
        EXPECT_EQ(values.current(guard, ActorValue::health), 12);
        records.mLevel = 9;
        EXPECT_EQ(values.current(guard, ActorValue::health), 12);
        // a new game works everything out again
        values.clear();
        EXPECT_EQ(values.current(player, ActorValue::health), 100 + 100 + 8 * 5);
    }

    TEST(FalloutScriptCalendarTest, a_day_follows_the_day_before_it)
    {
        const Calendar calendar(1);
        const GameDate start{ 2277, 10, 23 };

        EXPECT_EQ(calendar.after(start, 0).mDay, 23);
        const GameDate next = calendar.after(start, 1);
        EXPECT_EQ(next.mYear, 2277);
        EXPECT_EQ(next.mMonth, 10);
        EXPECT_EQ(next.mDay, 24);
    }

    TEST(FalloutScriptCalendarTest, a_month_ends_after_its_last_day_and_a_year_after_december)
    {
        const Calendar calendar(1);
        const GameDate october31 = calendar.after({ 2277, 10, 23 }, 8);
        EXPECT_EQ(october31.mMonth, 10);
        EXPECT_EQ(october31.mDay, 31);

        const GameDate november = calendar.after({ 2277, 10, 23 }, 9);
        EXPECT_EQ(november.mMonth, 11);
        EXPECT_EQ(november.mDay, 1);

        // November has 30 days
        const GameDate december = calendar.after({ 2277, 10, 23 }, 9 + 30);
        EXPECT_EQ(december.mMonth, 12);
        EXPECT_EQ(december.mDay, 1);

        const GameDate newYear = calendar.after({ 2277, 10, 23 }, 9 + 30 + 31);
        EXPECT_EQ(newYear.mYear, 2278);
        EXPECT_EQ(newYear.mMonth, 1);
        EXPECT_EQ(newYear.mDay, 1);
    }

    TEST(FalloutScriptCalendarTest, february_has_28_days_and_a_year_has_365)
    {
        const Calendar calendar(1);
        const GameDate march = calendar.after({ 2280, 2, 1 }, 28);
        EXPECT_EQ(march.mMonth, 3);
        EXPECT_EQ(march.mDay, 1);

        const GameDate later = calendar.after({ 2277, 10, 23 }, 365 * 3 + 40);
        const GameDate step = calendar.after({ 2280, 10, 23 }, 40);
        EXPECT_EQ(later.mYear, step.mYear);
        EXPECT_EQ(later.mMonth, step.mMonth);
        EXPECT_EQ(later.mDay, step.mDay);
        EXPECT_EQ(calendar.after({ 2277, 10, 23 }, 365).mYear, 2278);
        EXPECT_EQ(calendar.after({ 2277, 10, 23 }, 365).mDay, 23);
    }

    TEST(FalloutScriptCalendarTest, the_months_can_be_counted_from_zero)
    {
        const Calendar calendar(0);
        EXPECT_EQ(calendar.daysInMonth(0), 31);
        EXPECT_EQ(calendar.daysInMonth(1), 28);
        EXPECT_EQ(calendar.daysInMonth(11), 31);

        const GameDate december = calendar.after({ 2277, 9, 23 }, 9 + 30);
        EXPECT_EQ(december.mMonth, 11);
        EXPECT_EQ(december.mDay, 1);
        const GameDate newYear = calendar.after({ 2277, 9, 23 }, 9 + 30 + 31);
        EXPECT_EQ(newYear.mYear, 2278);
        EXPECT_EQ(newYear.mMonth, 0);
        EXPECT_EQ(newYear.mDay, 1);
    }

    TEST(FalloutScriptCalendarTest, a_date_that_is_none_is_taken_to_the_nearest_and_a_negative_time_changes_nothing)
    {
        const Calendar calendar(1);
        const GameDate nearest = calendar.after({ 2277, 14, 99 }, 0);
        EXPECT_EQ(nearest.mMonth, 12);
        EXPECT_EQ(nearest.mDay, 31);
        EXPECT_EQ(calendar.after({ 2277, 2, 10 }, -5).mDay, 10);
    }

    TEST(FalloutScriptCalendarTest, a_date_is_its_day_in_the_count_of_days_and_back)
    {
        for (const int base : { 0, 1 })
        {
            const Calendar calendar(base);
            const GameDate start{ 2277, base + 9, 23 };
            for (const long long days : { 0ll, 1ll, 8ll, 9ll, 39ll, 70ll, 365ll, 1000ll })
            {
                const GameDate date = calendar.after(start, days);
                EXPECT_EQ(calendar.dayNumber(date), calendar.dayNumber(start) + days) << "base " << base;
                const GameDate again = calendar.dateOf(calendar.dayNumber(date));
                EXPECT_EQ(again.mYear, date.mYear);
                EXPECT_EQ(again.mMonth, date.mMonth);
                EXPECT_EQ(again.mDay, date.mDay);
            }
        }
    }

    TEST(FalloutScriptCalendarTest, a_day_before_the_first_one_is_the_first_one)
    {
        const Calendar calendar(0);
        const GameDate first = calendar.dateOf(-12);
        EXPECT_EQ(first.mYear, 0);
        EXPECT_EQ(first.mMonth, 0);
        EXPECT_EQ(first.mDay, 1);
        EXPECT_EQ(calendar.dayNumber({ 0, 0, 1 }), 0);
        EXPECT_EQ(calendar.dayNumber({ 1, 0, 1 }), 365);
        EXPECT_EQ(calendar.dayNumber({ 0, 11, 31 }), 364);
    }
}
