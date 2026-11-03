#include <components/falloutscript/slots.hpp>

#include <deque>
#include <memory>
#include <random>
#include <set>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutScript::Casino;
    using Class = SlotMachine::WinClass;

    // Answers with the numbers that are asked for, in that order (a spin draws the three stops, then a roll for the
    // Luck rig)
    Random queued(std::vector<int> numbers)
    {
        auto left = std::make_shared<std::deque<int>>(numbers.begin(), numbers.end());
        return [left](int low, int high) {
            if (left->empty())
                return low;
            const int value = left->front();
            left->pop_front();
            EXPECT_GE(value, low);
            EXPECT_LE(value, high);
            return value;
        };
    }

    Rules rulesOf(std::array<std::uint32_t, 7> stops)
    {
        return Rules::fromRecord(0.f, 0.f, stops, 0, 0, 0);
    }

    TEST(FalloutScriptSlotsTest, the_strip_is_filled_in_passes_of_a_symbol_and_the_blank_after_it)
    {
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 1, 25, 100, 5, queued({}));
        ASSERT_TRUE(machine.valid());
        const std::vector<int>& strip = machine.strip();
        ASSERT_EQ(strip.size(), 28u);
        for (int i = 0; i < 28; ++i)
            EXPECT_EQ(strip[static_cast<std::size_t>(i)], i % 14) << i;
    }

    TEST(FalloutScriptSlotsTest, a_symbol_that_is_not_on_the_record_leaves_its_place_empty_in_the_numbering)
    {
        // The second symbol has no stops: its two numbers are skipped, and one more pass is made over the others
        SlotMachine machine(rulesOf({ 3, 0, 3, 3, 3, 3, 3 }), 1, 25, 100, 5, queued({}));
        ASSERT_TRUE(machine.valid());
        const std::vector<int>& strip = machine.strip();
        // Pass one: 0, 1, then 3 (a gap of one), 4, 5, ...
        EXPECT_EQ(strip[0], 0);
        EXPECT_EQ(strip[1], 1);
        EXPECT_EQ(strip[2], 3);
        EXPECT_EQ(strip[3], 4);
        EXPECT_EQ(strip.size(), 28u);
    }

    TEST(FalloutScriptSlotsTest, counts_that_add_up_to_less_than_fourteen_make_no_machine)
    {
        SlotMachine few(rulesOf({ 1, 1, 1, 1, 1, 1, 1 }), 1, 25, 100, 5, queued({}));
        EXPECT_FALSE(few.valid());
        EXPECT_FALSE(few.canSpin());
        SlotMachine none(rulesOf({ 0, 0, 0, 0, 0, 0, 0 }), 1, 25, 100, 5, queued({}));
        EXPECT_FALSE(none.valid());
        // More than 14 is not turned away
        SlotMachine many(rulesOf({ 3, 3, 3, 3, 3, 3, 3 }), 1, 25, 100, 5, queued({}));
        EXPECT_TRUE(many.valid());
        EXPECT_EQ(many.strip().size(), 28u);
    }

    TEST(FalloutScriptSlotsTest, three_equal_symbols_two_specials_and_one_special_win_and_the_rest_lose)
    {
        EXPECT_EQ(SlotMachine::classOf({ 4, 4, 4 }), Class::ThreeAlike);
        EXPECT_EQ(SlotMachine::classOf({ 12, 12, 12 }), Class::ThreeAlike);
        EXPECT_EQ(SlotMachine::classOf({ 12, 12, 3 }), Class::TwoSpecial);
        EXPECT_EQ(SlotMachine::classOf({ 5, 12, 12 }), Class::TwoSpecial);
        EXPECT_EQ(SlotMachine::classOf({ 12, 3, 5 }), Class::OneSpecial);
        // One special wins even beside a pair of another symbol
        EXPECT_EQ(SlotMachine::classOf({ 4, 4, 12 }), Class::OneSpecial);
        // A pair of a symbol pays nothing, three blanks are not a line, and a pair of blanks is nothing
        EXPECT_EQ(SlotMachine::classOf({ 4, 4, 6 }), Class::Pair);
        EXPECT_EQ(SlotMachine::classOf({ 3, 3, 3 }), Class::Lose);
        EXPECT_EQ(SlotMachine::classOf({ 3, 3, 6 }), Class::Lose);
        EXPECT_EQ(SlotMachine::classOf({ 0, 6, 8 }), Class::Lose);
    }

    TEST(FalloutScriptSlotsTest, a_line_pays_by_the_symbol_of_the_first_reel)
    {
        const std::vector<std::pair<int, int>> pays{ { 0, 10 }, { 2, 20 }, { 4, 30 }, { 6, 40 }, { 8, 60 }, { 10, 100 },
            { 12, 10 }, { 1, 0 }, { 13, 0 } };
        for (const auto& [stop, pay] : pays)
            EXPECT_EQ(SlotMachine::threeAlikePays(stop), pay) << stop;
    }

    TEST(FalloutScriptSlotsTest, a_spin_takes_each_stop_from_the_first_entries_of_the_strip)
    {
        // Luck 5: the roll after the stops is made and does nothing. Entries 6, 6 and 6 of the strip are the symbol 3.
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 5, queued({ 6, 6, 6, 99 }));
        ASSERT_TRUE(machine.canSpin());
        const SlotMachine::Result result = machine.spin();
        EXPECT_EQ(result.mStops, (std::array<int, 3>{ 6, 6, 6 }));
        EXPECT_EQ(result.mClass, Class::ThreeAlike);
        EXPECT_EQ(result.mMultiplier, 40);
        EXPECT_FALSE(result.mLucky);
        // 5 on the bet pays 5 x 40 on top of the stake
        EXPECT_EQ(result.mNet, 200);
        EXPECT_EQ(machine.chips(), 300);
    }

    TEST(FalloutScriptSlotsTest, a_lost_spin_costs_the_bet_and_the_bet_follows_the_chips)
    {
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 100, 8, 5, queued({ 0, 3, 8, 50 }));
        const SlotMachine::Result result = machine.spin();
        EXPECT_EQ(result.mClass, Class::Lose);
        EXPECT_EQ(result.mNet, -5);
        EXPECT_EQ(machine.chips(), 3);
        // What is left is less than the least bet: the machine cannot be played any more
        EXPECT_TRUE(machine.broke());
        EXPECT_FALSE(machine.canSpin());
    }

    TEST(FalloutScriptSlotsTest, luck_above_five_picks_the_reels_again_when_nothing_came_up_and_sometimes_wins)
    {
        // Luck 10: the chance is 49 per cent; the roll 99 passes. Nothing came up (0, 3, 8): the three reels are picked
        // again (2, 2, 4), which is a pair (nothing), and the new line gets its own roll, which fails (0).
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 10, queued({ 0, 3, 8, 99, 2, 2, 4, 0 }));
        SlotMachine::Result result = machine.spin();
        EXPECT_EQ(result.mStops, (std::array<int, 3>{ 2, 2, 4 }));
        EXPECT_FALSE(result.mLucky);
        EXPECT_EQ(result.mClass, Class::Pair);
        // The same line with a roll that passes: the third reel is turned to the pair (distance 2, 4 < Luck 10)
        SlotMachine second(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 10, queued({ 0, 3, 8, 99, 2, 2, 4, 99 }));
        result = second.spin();
        EXPECT_EQ(result.mStops, (std::array<int, 3>{ 2, 2, 2 }));
        EXPECT_TRUE(result.mLucky);
        EXPECT_EQ(result.mClass, Class::ThreeAlike);
        EXPECT_EQ(result.mMultiplier, 20);
    }

    TEST(FalloutScriptSlotsTest, the_chance_of_the_rig_is_one_less_than_the_per_cent_luck_is_away_from_five)
    {
        // Luck 6: t = 10, so a roll of 91 or more passes and 90 does not
        SlotMachine fails(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 6, queued({ 4, 4, 5, 90 }));
        EXPECT_EQ(fails.spin().mStops, (std::array<int, 3>{ 4, 4, 5 }));
        SlotMachine passes(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 6, queued({ 4, 4, 5, 91 }));
        // The pair is two apart from the third reel: 1 squared is not more than Luck 6, so the third is turned
        EXPECT_EQ(passes.spin().mStops, (std::array<int, 3>{ 4, 4, 4 }));
        // Luck 5 never rigs
        SlotMachine fair(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 5, queued({ 4, 4, 5, 99 }));
        EXPECT_EQ(fair.spin().mStops, (std::array<int, 3>{ 4, 4, 5 }));
    }

    TEST(FalloutScriptSlotsTest, luck_below_five_turns_a_win_into_a_loss_and_says_nothing_of_it)
    {
        // Luck 0: three of the symbol at stop 4 become three of the next, which is the blank 5
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 0, queued({ 4, 4, 4, 99 }));
        SlotMachine::Result result = machine.spin();
        EXPECT_EQ(result.mStops, (std::array<int, 3>{ 5, 5, 5 }));
        EXPECT_EQ(result.mClass, Class::Lose);
        EXPECT_FALSE(result.mLucky);
        EXPECT_EQ(result.mNet, -5);
        // A line of the symbols above the sixth goes back to the first
        SlotMachine wrap(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 0, queued({ 8, 8, 8, 99 }));
        EXPECT_EQ(wrap.spin().mStops, (std::array<int, 3>{ 0, 0, 0 }));
        // A special symbol is turned to the blank after it
        SlotMachine special(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 5, 25, 100, 0, queued({ 12, 2, 4, 99 }));
        const SlotMachine::Result blank = special.spin();
        EXPECT_EQ(blank.mStops, (std::array<int, 3>{ 13, 2, 4 }));
        EXPECT_EQ(blank.mClass, Class::Lose);
    }

    TEST(FalloutScriptSlotsTest, the_bet_goes_by_the_steps_of_the_tables_and_stays_between_the_least_and_the_chips)
    {
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 1, 60, 45, 5, queued({}));
        EXPECT_EQ(machine.bet(), 1);
        for (int i = 0; i < 12; ++i)
            machine.increaseBet();
        EXPECT_EQ(machine.bet(), 25);
        machine.setHighestBet();
        // The chips are fewer than the most
        EXPECT_EQ(machine.bet(), 45);
        machine.decreaseBet();
        EXPECT_EQ(machine.bet(), 40);
        for (int i = 0; i < 40; ++i)
            machine.decreaseBet();
        EXPECT_EQ(machine.bet(), 1);
    }

    TEST(FalloutScriptSlotsTest, a_long_game_keeps_the_stops_on_the_strip)
    {
        std::mt19937 engine(7);
        Random random = [&engine](int low, int high) { return std::uniform_int_distribution<int>(low, high)(engine); };
        SlotMachine machine(rulesOf({ 2, 2, 2, 2, 2, 2, 2 }), 1, 25, 100000, 8, random);
        std::set<int> seen;
        for (int i = 0; i < 2000; ++i)
        {
            const SlotMachine::Result result = machine.spin();
            for (const int stop : result.mStops)
            {
                EXPECT_GE(stop, 0);
                EXPECT_LE(stop, 13);
                seen.insert(stop);
            }
        }
        EXPECT_GE(seen.size(), 10u);
    }
}
