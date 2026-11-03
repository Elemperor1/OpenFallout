#include <components/falloutscript/casino.hpp>
#include <components/falloutscript/roulette.hpp>
#include <components/falloutscript/savestate.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutScript::Casino;

    Random fixed(std::vector<int> draws)
    {
        auto index = std::make_shared<std::size_t>(0);
        return [draws = std::move(draws), index](int low, int high) {
            const int value = draws.at((*index)++ % draws.size());
            EXPECT_GE(value, low);
            EXPECT_LE(value, high);
            return value;
        };
    }

    TEST(FalloutScriptCasinoTest, a_record_with_zeroes_plays_by_the_defaults)
    {
        const Rules rules = Rules::fromRecord(0.f, 0.f, { 2, 2, 2, 2, 2, 2, 2 }, 0, 0, 0);
        EXPECT_FLOAT_EQ(rules.mReshuffleShare, 0.25f);
        EXPECT_FLOAT_EQ(rules.mBlackjackPayout, 1.5f);
        EXPECT_EQ(rules.mDecks, 1);
        EXPECT_FALSE(rules.mDealerStandsOnSoft17);
        const Rules set = Rules::fromRecord(0.5f, 1.2f, { 1, 2, 3, 4, 2, 1, 1 }, 6, 3000, 1);
        EXPECT_FLOAT_EQ(set.mReshuffleShare, 0.5f);
        EXPECT_FLOAT_EQ(set.mBlackjackPayout, 1.2f);
        EXPECT_EQ(set.mDecks, 6);
        EXPECT_EQ(set.mMaxWinnings, 3000);
        EXPECT_TRUE(set.mDealerStandsOnSoft17);
        EXPECT_EQ(set.mSlotStops[3], 4);
    }

    TEST(FalloutScriptCasinoTest, the_blackjack_bet_walks_its_stairs)
    {
        // 1 to 10 by one, then by 5 to 40, by 10 to 100, by 25 to 400, by 100 to 1500, then by 500
        int bet = 1;
        std::vector<int> seen{ bet };
        while (bet < 3000)
        {
            bet = increasedBet(Table::Blackjack, bet, 1, 100000, 100000);
            seen.push_back(bet);
        }
        const std::vector<int> expected{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 35, 40, 50, 60, 70, 80, 90, 100,
            125, 150, 175, 200, 225, 250, 275, 300, 325, 350, 375, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300,
            1400, 1500, 2000, 2500, 3000 };
        EXPECT_EQ(seen, expected);
        // Down the same stairs
        std::vector<int> down{ bet };
        while (bet > 1)
        {
            bet = decreasedBet(Table::Blackjack, bet, 1);
            down.push_back(bet);
        }
        std::reverse(down.begin(), down.end());
        EXPECT_EQ(down, expected);
    }

    TEST(FalloutScriptCasinoTest, the_roulette_and_slot_bet_steps_are_their_own)
    {
        EXPECT_EQ(increasedBet(Table::Roulette, 9, 1, 1000, 1000), 10);
        EXPECT_EQ(increasedBet(Table::Roulette, 10, 1, 1000, 1000), 15);
        EXPECT_EQ(increasedBet(Table::Roulette, 100, 1, 1000, 1000), 125);
        // Blackjack goes by 10 from 100 on up to 400 by 25, roulette by 25 up to 500
        EXPECT_EQ(increasedBet(Table::Roulette, 400, 1, 1000, 1000), 425);
        EXPECT_EQ(increasedBet(Table::Roulette, 500, 1, 1000, 1000), 600);
        EXPECT_EQ(decreasedBet(Table::Roulette, 600, 1), 500);
        EXPECT_EQ(decreasedBet(Table::Roulette, 125, 1), 100);
        EXPECT_EQ(decreasedBet(Table::Roulette, 11, 1), 10);
        EXPECT_EQ(decreasedBet(Table::Roulette, 10, 1), 9);
    }

    TEST(FalloutScriptCasinoTest, the_bet_is_held_between_the_least_and_the_most_and_the_chips)
    {
        EXPECT_EQ(increasedBet(Table::Blackjack, 195, 1, 200, 5000), 200);
        EXPECT_EQ(increasedBet(Table::Blackjack, 200, 1, 200, 5000), 200);
        // The player has 120 chips: that is the most
        EXPECT_EQ(increasedBet(Table::Blackjack, 100, 1, 200, 120), 120);
        EXPECT_EQ(decreasedBet(Table::Blackjack, 5, 5), 5);
        EXPECT_EQ(decreasedBet(Table::Blackjack, 2, 2), 2);
        EXPECT_EQ(highestBet(200, 120), 120);
        EXPECT_EQ(highestBet(200, 800), 200);
        EXPECT_EQ(highestBet(200, 0), 0);
    }

    TEST(FalloutScriptCasinoTest, the_wheel_has_every_pocket_once_and_the_colours_are_those_of_a_table)
    {
        const auto& wheel = rouletteWheel();
        const std::set<int> pockets(wheel.begin(), wheel.end());
        EXPECT_EQ(pockets.size(), 38u);
        EXPECT_EQ(*pockets.begin(), 0);
        EXPECT_EQ(*pockets.rbegin(), doubleZero);
        const std::set<int> red{ 1, 3, 5, 7, 9, 12, 14, 16, 18, 19, 21, 23, 25, 27, 30, 32, 34, 36 };
        for (int pocket = 0; pocket < 38; ++pocket)
        {
            EXPECT_EQ(pocketIsRed(pocket), red.count(pocket) == 1) << pocket;
            EXPECT_EQ(pocketIsBlack(pocket), pocket >= 1 && pocket <= 36 && red.count(pocket) == 0) << pocket;
        }
    }

    TEST(FalloutScriptCasinoTest, the_table_offers_the_bets_of_the_american_layout_with_their_payouts)
    {
        const auto& areas = rouletteAreas();
        std::map<RouletteArea::Kind, int> count;
        for (const RouletteArea& area : areas)
            ++count[area.mKind];
        EXPECT_EQ(count[RouletteArea::Kind::Straight], 38);
        // The executable's table has 147 inside bets: 38 numbers, 60 pairs (33 + 24 and the three of the zeroes), 15
        // threes (12 rows and 0-1-2, 0-00-2, 00-2-3), 22 squares, 11 lines of six and the five numbers
        EXPECT_EQ(count[RouletteArea::Kind::Split], 24 + 33 + 3);
        EXPECT_EQ(count[RouletteArea::Kind::Street], 12 + 3);
        EXPECT_EQ(count[RouletteArea::Kind::Corner], 22);
        EXPECT_EQ(count[RouletteArea::Kind::SixLine], 11);
        EXPECT_EQ(count[RouletteArea::Kind::FiveNumber], 1);
        EXPECT_EQ(count[RouletteArea::Kind::Dozen], 3);
        EXPECT_EQ(count[RouletteArea::Kind::Column], 3);
        EXPECT_EQ(count[RouletteArea::Kind::Half], 2);
        std::set<std::vector<int>> different;
        for (const RouletteArea& area : areas)
        {
            std::vector<int> numbers = area.mNumbers;
            std::sort(numbers.begin(), numbers.end());
            different.insert(numbers);
        }
        // 147 inside bets and no two the same
        EXPECT_EQ(areas.size(), 147u + 12u);
        EXPECT_EQ(different.size(), areas.size());
        // Twelve bets on the outside, as the executable's table of them
        EXPECT_EQ(count[RouletteArea::Kind::Dozen] + count[RouletteArea::Kind::Column] + count[RouletteArea::Kind::Half]
                + count[RouletteArea::Kind::Even] + count[RouletteArea::Kind::Odd] + count[RouletteArea::Kind::Red]
                + count[RouletteArea::Kind::Black],
            12);
        for (const RouletteArea& area : areas)
        {
            const std::size_t size = area.mNumbers.size();
            switch (area.mKind)
            {
                case RouletteArea::Kind::Straight:
                    EXPECT_EQ(size, 1u);
                    EXPECT_EQ(area.mMultiplier, 35);
                    break;
                case RouletteArea::Kind::Split:
                    EXPECT_EQ(size, 2u);
                    EXPECT_EQ(area.mMultiplier, 17);
                    break;
                case RouletteArea::Kind::Street:
                    EXPECT_EQ(size, 3u);
                    EXPECT_EQ(area.mMultiplier, 11);
                    break;
                case RouletteArea::Kind::Corner:
                    EXPECT_EQ(size, 4u);
                    EXPECT_EQ(area.mMultiplier, 8);
                    break;
                case RouletteArea::Kind::SixLine:
                    EXPECT_EQ(size, 6u);
                    EXPECT_EQ(area.mMultiplier, 5);
                    break;
                case RouletteArea::Kind::FiveNumber:
                    EXPECT_EQ(size, 5u);
                    EXPECT_EQ(area.mMultiplier, 6);
                    break;
                case RouletteArea::Kind::Dozen:
                    EXPECT_EQ(size, 12u);
                    EXPECT_EQ(area.mMultiplier, 2);
                    break;
                case RouletteArea::Kind::Column:
                    // The columns hold their twelve numbers, and the first or the third also a zero (see below)
                    EXPECT_GE(size, 12u);
                    EXPECT_LE(size, 13u);
                    EXPECT_EQ(area.mMultiplier, 2);
                    break;
                case RouletteArea::Kind::Half:
                    EXPECT_EQ(size, 18u);
                    EXPECT_EQ(area.mMultiplier, 1);
                    break;
                default:
                    // Even, odd, red and black: 18 numbers and one of the zeroes
                    EXPECT_EQ(size, 19u);
                    EXPECT_EQ(area.mMultiplier, 1);
                    break;
            }
            // No bet is twice on a number and every number is a pocket
            const std::set<int> distinct(area.mNumbers.begin(), area.mNumbers.end());
            EXPECT_EQ(distinct.size(), size);
            for (int number : area.mNumbers)
            {
                EXPECT_GE(number, 0);
                EXPECT_LE(number, doubleZero);
            }
        }
    }

    TEST(FalloutScriptCasinoTest, the_outside_bets_are_matched_as_the_executables_payout_loop_does)
    {
        // The dozens and the halves are by the number: the zeroes lose them. Nothing in the loop leaves the zeroes out
        // of the others: 0 wins even, red and the third column, 00 (37) wins odd, black and the first column.
        for (const RouletteArea& area : rouletteAreas())
        {
            switch (area.mKind)
            {
                case RouletteArea::Kind::Dozen:
                case RouletteArea::Kind::Half:
                    EXPECT_FALSE(area.wins(0));
                    EXPECT_FALSE(area.wins(doubleZero));
                    break;
                case RouletteArea::Kind::Even:
                    EXPECT_TRUE(area.wins(0));
                    EXPECT_FALSE(area.wins(doubleZero));
                    break;
                case RouletteArea::Kind::Odd:
                    EXPECT_FALSE(area.wins(0));
                    EXPECT_TRUE(area.wins(doubleZero));
                    break;
                case RouletteArea::Kind::Red:
                    EXPECT_TRUE(area.wins(0));
                    EXPECT_FALSE(area.wins(doubleZero));
                    break;
                case RouletteArea::Kind::Black:
                    EXPECT_FALSE(area.wins(0));
                    EXPECT_TRUE(area.wins(doubleZero));
                    break;
                default:
                    break;
            }
        }
        const auto column = [](int remainder) {
            for (const RouletteArea& area : rouletteAreas())
                if (area.mKind == RouletteArea::Kind::Column && area.wins(remainder == 0 ? 3 : remainder))
                    return area;
            return RouletteArea{};
        };
        EXPECT_TRUE(column(1).wins(doubleZero));
        EXPECT_FALSE(column(1).wins(0));
        EXPECT_TRUE(column(0).wins(0));
        EXPECT_FALSE(column(0).wins(doubleZero));
        EXPECT_FALSE(column(2).wins(0));
        EXPECT_FALSE(column(2).wins(doubleZero));
    }

    TEST(FalloutScriptCasinoTest, the_zeroes_have_their_pairs_threes_and_the_five_numbers)
    {
        const auto& areas = rouletteAreas();
        const auto has = [&](RouletteArea::Kind kind, std::vector<int> numbers) {
            for (const RouletteArea& area : areas)
            {
                std::vector<int> have = area.mNumbers;
                std::sort(have.begin(), have.end());
                std::sort(numbers.begin(), numbers.end());
                if (area.mKind == kind && have == numbers)
                    return true;
            }
            return false;
        };
        EXPECT_TRUE(has(RouletteArea::Kind::Split, { 0, doubleZero }));
        EXPECT_TRUE(has(RouletteArea::Kind::Split, { 0, 1 }));
        EXPECT_TRUE(has(RouletteArea::Kind::Split, { doubleZero, 3 }));
        EXPECT_FALSE(has(RouletteArea::Kind::Split, { 0, 2 }));
        EXPECT_FALSE(has(RouletteArea::Kind::Split, { doubleZero, 2 }));
        EXPECT_TRUE(has(RouletteArea::Kind::Street, { 0, 1, 2 }));
        EXPECT_TRUE(has(RouletteArea::Kind::Street, { 0, doubleZero, 2 }));
        EXPECT_TRUE(has(RouletteArea::Kind::Street, { doubleZero, 2, 3 }));
    }

    int areaOf(RouletteArea::Kind kind, std::vector<int> numbers)
    {
        const auto& areas = rouletteAreas();
        for (std::size_t i = 0; i < areas.size(); ++i)
        {
            std::vector<int> have = areas[i].mNumbers;
            std::sort(have.begin(), have.end());
            std::sort(numbers.begin(), numbers.end());
            if (areas[i].mKind == kind && have == numbers)
                return static_cast<int>(i);
        }
        return -1;
    }

    TEST(FalloutScriptCasinoTest, a_spin_pays_the_stake_times_one_more_than_the_multiplier_for_each_bet_that_won)
    {
        // A number that comes up: 17 straight (35 to 1) with 10 in all
        Roulette table(5, 200, 500, 5, fixed({ 17, 1 }));
        const int straight = areaOf(RouletteArea::Kind::Straight, { 17 });
        ASSERT_GE(straight, 0);
        EXPECT_EQ(table.amount(), 5);
        table.increaseAmount();
        EXPECT_EQ(table.amount(), 6);
        table.decreaseAmount();
        table.decreaseAmount();
        EXPECT_EQ(table.amount(), 5);
        EXPECT_TRUE(table.place(straight));
        // A place that has a stake takes no more: the second click does nothing
        EXPECT_FALSE(table.canPlace(straight));
        EXPECT_FALSE(table.place(straight));
        EXPECT_EQ(table.stake(straight), 5);
        const int other = areaOf(RouletteArea::Kind::Straight, { 18 });
        EXPECT_TRUE(table.place(other));
        EXPECT_EQ(table.total(), 10);
        EXPECT_EQ(table.betCount(), 2);
        const Roulette::Result result = table.spin();
        EXPECT_EQ(result.mPocket, 17);
        EXPECT_EQ(result.mSpinType, 1);
        // 5 on the 17 pays 36 x 5 = 180; the 5 on the 18 is lost
        EXPECT_EQ(result.mPayout, 180);
        EXPECT_EQ(result.mNet, 170);
        EXPECT_FALSE(result.mLucky);
        EXPECT_FALSE(result.mUnlucky);
        EXPECT_EQ(table.chips(), 670);
        // The table is clear after the spin
        EXPECT_EQ(table.total(), 0);
        EXPECT_EQ(table.betCount(), 0);
        EXPECT_FALSE(table.canSpin());
    }

    TEST(FalloutScriptCasinoTest, a_bet_that_lost_costs_its_stake_and_the_chips_follow_the_net)
    {
        Roulette table(10, 200, 100, 8, fixed({ 0, 0 }));
        const int split = areaOf(RouletteArea::Kind::Split, { 1, 2 });
        const int five = areaOf(RouletteArea::Kind::FiveNumber, { 0, 1, 2, 3, doubleZero });
        ASSERT_GE(split, 0);
        ASSERT_GE(five, 0);
        EXPECT_TRUE(table.place(split));
        EXPECT_TRUE(table.place(five));
        EXPECT_EQ(table.total(), 20);
        const Roulette::Result result = table.spin();
        // The pocket is 0: the five numbers pay 6 to 1 (70 with the stake) and the pair loses
        EXPECT_EQ(result.mPayout, 70);
        EXPECT_EQ(result.mNet, 50);
        EXPECT_TRUE(result.mLucky);
        EXPECT_EQ(table.chips(), 150);
    }

    TEST(FalloutScriptCasinoTest, the_luck_of_the_player_names_the_words_but_not_the_pocket)
    {
        Roulette unlucky(5, 100, 100, 3, fixed({ 4, 0 }));
        ASSERT_TRUE(unlucky.place(0));
        const Roulette::Result result = unlucky.spin();
        EXPECT_EQ(result.mPocket, 4);
        EXPECT_TRUE(result.mUnlucky);
        EXPECT_FALSE(result.mLucky);
        EXPECT_EQ(result.mNet, -5);
    }

    TEST(FalloutScriptCasinoTest, a_table_takes_ten_bets_and_no_more_than_the_most_in_all)
    {
        Roulette table(10, 95, 1000, 5, fixed({ 0 }));
        for (int area = 0; area < 9; ++area)
            EXPECT_TRUE(table.place(area));
        EXPECT_EQ(table.total(), 90);
        // 5 are left of the most: the amount is what is left
        EXPECT_TRUE(table.place(9));
        EXPECT_EQ(table.stake(9), 5);
        EXPECT_EQ(table.total(), 95);
        EXPECT_EQ(table.amount(), 5);
        EXPECT_FALSE(table.canPlace(10));
        EXPECT_FALSE(table.place(10));
        // Taking one off makes room
        EXPECT_TRUE(table.remove(9));
        EXPECT_EQ(table.total(), 90);
        EXPECT_FALSE(table.remove(9));
        EXPECT_TRUE(table.canPlace(10));

        Roulette crowded(1, 1000, 1000, 5, fixed({ 0 }));
        for (int area = 0; area < 10; ++area)
            EXPECT_TRUE(crowded.place(area));
        EXPECT_EQ(crowded.betCount(), 10);
        EXPECT_FALSE(crowded.canPlace(10));
        EXPECT_FALSE(crowded.canPlace(0));
    }

    TEST(FalloutScriptCasinoTest, remove_bet_takes_off_the_whole_stake_and_leaves_the_amount)
    {
        Roulette table(5, 500, 1000, 5, fixed({ 0 }));
        for (int i = 0; i < 4; ++i)
            table.increaseAmount();
        const int amount = table.amount();
        ASSERT_GT(amount, 5);
        ASSERT_TRUE(table.place(3));
        ASSERT_TRUE(table.place(4));
        EXPECT_EQ(table.total(), 2 * amount);
        table.decreaseAmount();
        const int lowered = table.amount();
        ASSERT_LT(lowered, amount);
        EXPECT_TRUE(table.remove(3));
        // The whole stake of that place is gone, not one step of the ladder, and the amount stays where it was
        EXPECT_EQ(table.total(), amount);
        EXPECT_EQ(table.stake(3), 0);
        EXPECT_EQ(table.amount(), lowered);
        EXPECT_EQ(table.betCount(), 1);
    }

    TEST(FalloutScriptCasinoTest, after_a_spin_the_amount_is_the_least_bet_and_a_player_short_of_it_is_broke)
    {
        Roulette table(10, 500, 25, 5, fixed({ 5, 0 }));
        for (int i = 0; i < 3; ++i)
            table.increaseAmount();
        ASSERT_GT(table.amount(), 10);
        EXPECT_FALSE(table.broke());
        ASSERT_TRUE(table.place(3));
        const Roulette::Result result = table.spin();
        EXPECT_LT(result.mNet, 0);
        EXPECT_EQ(table.amount(), 10);
        // 25 less the amount that was lost is under the least bet of 10
        EXPECT_EQ(table.chips() < 10, table.broke());
    }

    TEST(FalloutScriptCasinoTest, a_player_cannot_bet_more_than_the_chips_in_hand)
    {
        Roulette table(10, 500, 25, 5, fixed({ 0 }));
        EXPECT_TRUE(table.place(0));
        EXPECT_TRUE(table.place(1));
        // 5 chips are left
        EXPECT_TRUE(table.place(2));
        EXPECT_EQ(table.stake(2), 5);
        EXPECT_EQ(table.total(), 25);
        EXPECT_FALSE(table.canPlace(3));
        // A place that is not on the table is refused
        Roulette other(10, 500, 500, 5, fixed({ 0 }));
        EXPECT_FALSE(other.place(-1));
        EXPECT_FALSE(other.place(static_cast<int>(rouletteAreas().size())));
    }

    TEST(FalloutScriptCasinoTest, the_ledger_keeps_the_winnings_of_each_casino)
    {
        constexpr FalloutScript::FormId gomorrah = 0x00100001;
        constexpr FalloutScript::FormId tops = 0x00100002;
        Ledger ledger;
        EXPECT_FALSE(ledger.known(gomorrah));
        EXPECT_EQ(ledger.earnings(gomorrah), 0);
        ledger.open(gomorrah);
        EXPECT_TRUE(ledger.known(gomorrah));
        EXPECT_EQ(ledger.earnings(gomorrah), 0);
        ledger.add(gomorrah, 120);
        ledger.add(gomorrah, -20);
        ledger.add(tops, -40);
        EXPECT_EQ(ledger.earnings(gomorrah), 100);
        EXPECT_EQ(ledger.earnings(tops), -40);
        // No casino, no entry
        ledger.add(0, 10);
        ledger.open(0);
        EXPECT_FALSE(ledger.known(0));
        // The sum does not wrap
        ledger.add(gomorrah, std::numeric_limits<int>::max());
        EXPECT_EQ(ledger.earnings(gomorrah), std::numeric_limits<int>::max());
        ledger.reset();
        EXPECT_FALSE(ledger.known(tops));
    }

    TEST(FalloutScriptCasinoTest, the_ledger_goes_through_a_saved_game)
    {
        constexpr FalloutScript::FormId gomorrah = 0x00100001;
        constexpr FalloutScript::FormId tops = 0x00100002;
        Ledger ledger;
        ledger.add(gomorrah, 250);
        ledger.add(tops, -75);
        ledger.raiseLevel(gomorrah, 400);
        FalloutScript::SaveWriter out;
        writeLedger(ledger, out);
        const std::string bytes = out.take();

        Ledger again;
        again.add(0x00100003, 5);
        FalloutScript::SaveReader in(bytes, [](FalloutScript::FormId form) -> std::optional<FalloutScript::FormId> {
            if (form == tops)
                return std::nullopt; // its plugin is gone
            return form;
        });
        readLedger(again, in, ledgerSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(again.earnings(gomorrah), 250);
        EXPECT_EQ(again.storedLevel(gomorrah), 2);
        EXPECT_FALSE(again.known(tops));
        EXPECT_FALSE(again.known(0x00100003));
    }

    TEST(FalloutScriptCasinoTest, the_caravan_totals_count_the_pots_the_matches_and_the_biggest_pot_won)
    {
        CaravanRecord record;
        EXPECT_EQ(record.totals().mGamesWon, 0);
        record.add(true, 40);
        record.add(false, 25);
        const CaravanTotals after = record.add(true, 15);
        EXPECT_EQ(after.mMoneyWon, 55);
        EXPECT_EQ(after.mMoneyLost, 25);
        EXPECT_EQ(after.mGamesWon, 2);
        EXPECT_EQ(after.mGamesLost, 1);
        // The biggest ante is the biggest pot of a win: a bigger pot that was lost does not count
        EXPECT_EQ(after.mBiggestAnte, 40);
        record.add(false, 500);
        EXPECT_EQ(record.totals().mBiggestAnte, 40);
        record.reset();
        EXPECT_EQ(record.totals().mMoneyWon, 0);
        EXPECT_EQ(record.totals().mBiggestAnte, 0);
    }

    TEST(FalloutScriptCasinoTest, the_caravan_totals_go_through_a_saved_game)
    {
        CaravanRecord record;
        record.add(true, 30);
        record.add(false, 12);
        FalloutScript::SaveWriter out;
        writeCaravanRecord(record, out);
        const std::string bytes = out.take();

        CaravanRecord again;
        again.add(true, 999);
        FalloutScript::SaveReader in(bytes, [](FalloutScript::FormId form) -> std::optional<FalloutScript::FormId> {
            return form;
        });
        readCaravanRecord(again, in, caravanRecordSectionVersion);
        EXPECT_TRUE(in.atEnd());
        const CaravanTotals totals = again.totals();
        EXPECT_EQ(totals.mMoneyWon, 30);
        EXPECT_EQ(totals.mMoneyLost, 12);
        EXPECT_EQ(totals.mGamesWon, 1);
        EXPECT_EQ(totals.mGamesLost, 1);
        EXPECT_EQ(totals.mBiggestAnte, 30);
    }

    TEST(FalloutScriptCasinoTest, the_level_of_the_winnings_is_the_highest_quarter_they_reach)
    {
        // A quarter, a half, three quarters and all of the most (400): 100, 200, 300 and 400
        EXPECT_EQ(winningsLevel(-50, 400), 0);
        EXPECT_EQ(winningsLevel(0, 400), 0);
        EXPECT_EQ(winningsLevel(99, 400), 0);
        EXPECT_EQ(winningsLevel(100, 400), 1);
        EXPECT_EQ(winningsLevel(199, 400), 1);
        EXPECT_EQ(winningsLevel(200, 400), 2);
        EXPECT_EQ(winningsLevel(300, 400), 3);
        EXPECT_EQ(winningsLevel(399, 400), 3);
        EXPECT_EQ(winningsLevel(400, 400), 4);
        EXPECT_EQ(winningsLevel(9000, 400), 4);
        // No rounding: a quarter of 10 is 2.5
        EXPECT_EQ(winningsLevel(2, 10), 0);
        EXPECT_EQ(winningsLevel(3, 10), 1);
        // With a most of 0 every winning of 0 or more is on the fourth step
        EXPECT_EQ(winningsLevel(0, 0), 4);
        EXPECT_EQ(winningsLevel(-1, 0), 0);
    }

    TEST(FalloutScriptCasinoTest, the_commands_of_the_level_work_on_the_entry_of_the_casino)
    {
        constexpr FalloutScript::FormId gomorrah = 0x00100001;
        Ledger ledger;
        // No entry: the level is 0, nothing is set and none is made
        EXPECT_EQ(ledger.level(gomorrah, 400), 0);
        EXPECT_FALSE(ledger.setLevel(gomorrah, 2, 400));
        EXPECT_FALSE(ledger.known(gomorrah));
        ledger.open(gomorrah);
        ledger.add(gomorrah, 250);
        EXPECT_EQ(ledger.level(gomorrah, 400), 2);
        // The stored level is not the one that is told: it moves only when a table raises it
        EXPECT_EQ(ledger.storedLevel(gomorrah), 0);
        // Set writes the stored level and the winnings that make it, for 0 to 4 only
        EXPECT_FALSE(ledger.setLevel(gomorrah, 5, 400));
        EXPECT_FALSE(ledger.setLevel(gomorrah, -1, 400));
        EXPECT_EQ(ledger.earnings(gomorrah), 250);
        EXPECT_TRUE(ledger.setLevel(gomorrah, 3, 400));
        EXPECT_EQ(ledger.storedLevel(gomorrah), 3);
        EXPECT_EQ(ledger.earnings(gomorrah), 300);
        EXPECT_EQ(ledger.level(gomorrah, 400), 3);
        EXPECT_TRUE(ledger.setLevel(gomorrah, 0, 400));
        EXPECT_EQ(ledger.earnings(gomorrah), 0);
        EXPECT_EQ(ledger.level(gomorrah, 400), 0);
    }

    TEST(FalloutScriptCasinoTest, a_table_closes_when_a_round_takes_the_winnings_past_a_new_quarter)
    {
        constexpr FalloutScript::FormId gomorrah = 0x00100001;
        Ledger ledger;
        ledger.open(gomorrah);
        EXPECT_FALSE(ledger.raiseLevel(gomorrah, 400));
        ledger.add(gomorrah, 120);
        // The first quarter is crossed: the level is raised and the table closes
        EXPECT_TRUE(ledger.raiseLevel(gomorrah, 400));
        EXPECT_EQ(ledger.storedLevel(gomorrah), 1);
        // The same winnings do not close it again
        EXPECT_FALSE(ledger.raiseLevel(gomorrah, 400));
        ledger.add(gomorrah, -60);
        EXPECT_FALSE(ledger.raiseLevel(gomorrah, 400));
        EXPECT_EQ(ledger.storedLevel(gomorrah), 1);
        // Two quarters in one round are one close, and the level goes to the highest
        ledger.add(gomorrah, 300);
        EXPECT_TRUE(ledger.raiseLevel(gomorrah, 400));
        EXPECT_EQ(ledger.storedLevel(gomorrah), 3);
        // Setting a high level keeps the table open until a higher quarter is crossed
        ledger.setLevel(gomorrah, 4, 400);
        ledger.add(gomorrah, -400);
        EXPECT_FALSE(ledger.raiseLevel(gomorrah, 400));
        EXPECT_EQ(ledger.storedLevel(gomorrah), 4);
    }
}
