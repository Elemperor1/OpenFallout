#include <components/falloutscript/caravan.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutScript::Casino;
    using Side = Caravan::Side;
    using Phase = Caravan::Phase;
    using Card = Caravan::Card;
    using MoveType = Caravan::MoveType;

    Card card(int value, int suit = 1)
    {
        Card made;
        made.mValue = value;
        made.mSuit = suit;
        return made;
    }

    Random first()
    {
        return [](int low, int) { return low; };
    }

    /// A position in the middle of a match: the player to move, hands and piles as the test gives them
    struct Table
    {
        Caravan::Position mPosition;

        Table()
        {
            mPosition.mPhase = Phase::Play;
            mPosition.mTurn = Side::Player;
            // Both sides have a card to play with and one in the pile, so that nobody has run out
            mPosition.mHands[0] = { card(2, 1) };
            mPosition.mHands[1] = { card(2, 1) };
            mPosition.mPiles[0] = { card(3, 1) };
            mPosition.mPiles[1] = { card(3, 1) };
        }

        Table& row(int track, int row, std::vector<Card> cards)
        {
            mPosition.mBoard[static_cast<std::size_t>(track)][static_cast<std::size_t>(row)] = std::move(cards);
            return *this;
        }
        /// The number cards of a caravan, one to a row, all of one suit
        Table& numbers(int track, std::vector<int> values, int suit = 1)
        {
            for (std::size_t i = 0; i < values.size(); ++i)
                row(track, static_cast<int>(i), { card(values[i], suit) });
            return *this;
        }
        Table& hand(Side side, std::vector<Card> cards)
        {
            mPosition.mHands[static_cast<std::size_t>(side)] = std::move(cards);
            return *this;
        }
        Table& pile(Side side, std::vector<Card> cards)
        {
            mPosition.mPiles[static_cast<std::size_t>(side)] = std::move(cards);
            return *this;
        }
        Caravan game(int aiType = 0) const
        {
            Caravan::Position position = mPosition;
            position.mAiType = aiType;
            return Caravan(position, first());
        }
    };

    TEST(FalloutScriptCaravanTest, a_caravan_adds_up_its_rows_and_a_king_doubles_the_row_it_is_on)
    {
        Table table;
        table.numbers(0, { 3, 5 }).row(0, 2, { card(4), card(14) }).row(0, 3, { card(2), card(14), card(14) });
        const Caravan game = table.game();
        // 3 + 5 + 4 x 2 + 2 x 4
        EXPECT_EQ(game.track(0).mTotal, 3 + 5 + 8 + 8);
        EXPECT_EQ(game.rowValue(0, 2), 8);
        EXPECT_EQ(game.rowValue(0, 3), 8);
        EXPECT_EQ(game.rowValue(0, 4), 0);
        EXPECT_EQ(game.rowCount(0), 4);
    }

    TEST(FalloutScriptCaravanTest, a_caravan_goes_the_way_its_last_two_number_cards_go_and_has_the_suit_of_the_last)
    {
        Table table;
        table.row(0, 0, { card(3, 2) }).row(0, 1, { card(7, 3) });
        table.row(1, 0, { card(9, 1) }).row(1, 1, { card(4, 4) });
        table.row(2, 0, { card(6, 2) });
        const Caravan game = table.game();
        EXPECT_EQ(game.track(0).mDirection, 1);
        EXPECT_EQ(game.track(0).mSuit, 3);
        EXPECT_EQ(game.track(1).mDirection, -1);
        EXPECT_EQ(game.track(1).mSuit, 4);
        // One row: no direction, only a suit; no row: no suit
        EXPECT_EQ(game.track(2).mDirection, 0);
        EXPECT_EQ(game.track(2).mSuit, 2);
        EXPECT_EQ(game.track(3).mSuit, Caravan::noSuit);
    }

    TEST(FalloutScriptCaravanTest, a_queen_on_the_last_row_changes_the_suit_and_turns_the_direction_round)
    {
        Table table;
        table.row(0, 0, { card(3, 1) }).row(0, 1, { card(7, 1), card(13, 4) });
        // A queen on a row that is not the last counts for nothing
        table.row(1, 0, { card(3, 1), card(13, 2) }).row(1, 1, { card(7, 1) });
        // One row: the queen changes the suit only
        table.row(2, 0, { card(5, 1), card(13, 3) });
        const Caravan game = table.game();
        EXPECT_EQ(game.track(0).mDirection, -1);
        EXPECT_EQ(game.track(0).mSuit, 4);
        EXPECT_EQ(game.track(1).mDirection, 1);
        EXPECT_EQ(game.track(1).mSuit, 1);
        EXPECT_EQ(game.track(2).mDirection, 0);
        EXPECT_EQ(game.track(2).mSuit, 3);
        // A face card adds nothing of its own
        EXPECT_EQ(game.track(0).mTotal, 10);
    }

    TEST(FalloutScriptCaravanTest, a_number_card_goes_on_the_next_row_of_a_caravan_of_its_own_and_not_the_value_below)
    {
        Table table;
        table.numbers(0, { 5 }).hand(Side::Player, { card(8), card(5), card(2) });
        const Caravan game = table.game();
        EXPECT_TRUE(game.canPlace(0, 1, 0, Side::Player));
        // Not the same value as the row below, whatever the suit
        EXPECT_FALSE(game.canPlace(0, 1, 1, Side::Player));
        EXPECT_TRUE(game.canPlace(0, 1, 2, Side::Player));
        // Not on a row that has a card, not further than the next row, not on the other side's caravan
        EXPECT_FALSE(game.canPlace(0, 0, 0, Side::Player));
        EXPECT_FALSE(game.canPlace(0, 2, 0, Side::Player));
        EXPECT_FALSE(game.canPlace(3, 0, 0, Side::Player));
        // A caravan with nothing on it takes a card on its first row
        EXPECT_TRUE(game.canPlace(1, 0, 0, Side::Player));
        EXPECT_FALSE(game.canPlace(1, 1, 0, Side::Player));
        // A card of the hand that is not there
        EXPECT_FALSE(game.canPlace(1, 0, 3, Side::Player));
        EXPECT_FALSE(game.canPlace(1, 0, -1, Side::Player));
    }

    TEST(FalloutScriptCaravanTest, a_card_goes_against_the_direction_of_a_caravan_only_in_its_suit)
    {
        Table table;
        // Going up, the suit of the last card is 2
        table.row(0, 0, { card(3, 1) }).row(0, 1, { card(7, 2) });
        // Going down, the suit of the last card is 3
        table.row(1, 0, { card(9, 1) }).row(1, 1, { card(4, 3) });
        table.hand(Side::Player, { card(9, 2), card(9, 1), card(2, 3), card(2, 1), card(5, 2), card(6, 3) });
        const Caravan game = table.game();
        // Up: a higher card is fine in any suit, a lower one only in the suit of the caravan
        EXPECT_TRUE(game.canPlace(0, 2, 0, Side::Player));
        EXPECT_TRUE(game.canPlace(0, 2, 1, Side::Player));
        EXPECT_TRUE(game.canPlace(0, 2, 4, Side::Player));
        EXPECT_FALSE(game.canPlace(0, 2, 3, Side::Player));
        // Down: a lower card is fine, a higher one only in the suit of the caravan
        EXPECT_TRUE(game.canPlace(1, 2, 2, Side::Player));
        EXPECT_TRUE(game.canPlace(1, 2, 3, Side::Player));
        EXPECT_TRUE(game.canPlace(1, 2, 5, Side::Player));
        EXPECT_FALSE(game.canPlace(1, 2, 0, Side::Player));
    }

    TEST(FalloutScriptCaravanTest, a_second_row_has_no_direction_to_keep)
    {
        Table table;
        table.row(0, 0, { card(5, 1) }).hand(Side::Player, { card(9, 4), card(1, 2) });
        const Caravan game = table.game();
        EXPECT_TRUE(game.canPlace(0, 1, 0, Side::Player));
        EXPECT_TRUE(game.canPlace(0, 1, 1, Side::Player));
    }

    TEST(
        FalloutScriptCaravanTest, a_face_card_goes_on_a_row_that_has_a_number_card_and_room_on_a_caravan_of_either_side)
    {
        Table table;
        table.numbers(0, { 3, 5 }).row(2, 0, { card(2), card(14), card(14), card(14) }).numbers(4, { 6 });
        table.hand(Side::Player, { card(12), card(13), card(14), card(15), card(4) });
        const Caravan game = table.game();
        // The player's caravan and the opponent's
        EXPECT_TRUE(game.canPlace(0, 0, 0, Side::Player));
        EXPECT_TRUE(game.canPlace(0, 1, 2, Side::Player));
        EXPECT_TRUE(game.canPlace(4, 0, 3, Side::Player));
        // Not on a row that has no card, and not on a row that has four
        EXPECT_FALSE(game.canPlace(0, 2, 0, Side::Player));
        EXPECT_FALSE(game.canPlace(2, 0, 0, Side::Player));
        // A queen only on the last row of a caravan
        EXPECT_FALSE(game.canPlace(0, 0, 1, Side::Player));
        EXPECT_TRUE(game.canPlace(0, 1, 1, Side::Player));
        EXPECT_TRUE(game.canPlace(4, 0, 1, Side::Player));
    }

    TEST(
        FalloutScriptCaravanTest, a_card_that_is_put_down_is_replaced_from_the_pile_and_the_turn_goes_to_the_other_side)
    {
        Table table;
        table.hand(Side::Player, { card(4), card(6) }).pile(Side::Player, { card(9), card(2) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 1, 0, 0));
        EXPECT_EQ(game.track(1).mRows[0], std::vector<Card>{ card(4) });
        ASSERT_EQ(game.hand(Side::Player).size(), 2u);
        EXPECT_EQ(game.hand(Side::Player)[0], card(6));
        // (the first of the pile: the random numbers here are the lowest)
        EXPECT_EQ(game.hand(Side::Player)[1], card(9));
        EXPECT_EQ(game.pile(Side::Player).size(), 1u);
        EXPECT_EQ(game.turn(), Side::Opponent);
        // A side that is not to move may not
        EXPECT_FALSE(game.place(Side::Player, 0, 0, 0));
    }

    TEST(FalloutScriptCaravanTest, with_no_pile_left_the_hand_shrinks)
    {
        Table table;
        table.hand(Side::Player, { card(4), card(6) }).pile(Side::Player, {});
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 0, 0, 0));
        EXPECT_EQ(game.hand(Side::Player).size(), 1u);
    }

    TEST(FalloutScriptCaravanTest,
        a_jack_takes_the_row_it_is_put_on_away_with_what_is_on_it_and_the_rows_after_it_move_down)
    {
        Table table;
        table.row(0, 0, { card(3) }).row(0, 1, { card(5), card(14) }).row(0, 2, { card(8) }).row(0, 3, { card(9) });
        table.hand(Side::Player, { card(12) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 0, 1, 0));
        EXPECT_EQ(game.rowCount(0), 3);
        EXPECT_EQ(game.track(0).mRows[0], std::vector<Card>{ card(3) });
        EXPECT_EQ(game.track(0).mRows[1], std::vector<Card>{ card(8) });
        EXPECT_EQ(game.track(0).mRows[2], std::vector<Card>{ card(9) });
        EXPECT_TRUE(game.track(0).mRows[3].empty());
        EXPECT_EQ(game.track(0).mTotal, 20);
    }

    TEST(FalloutScriptCaravanTest, a_joker_takes_the_rows_of_the_same_value_on_every_caravan_and_stays_where_it_was_put)
    {
        Table table;
        table.row(0, 0, { card(4, 1) }).row(0, 1, { card(7, 2) }).row(0, 2, { card(4, 3) });
        table.row(3, 0, { card(4, 4) }).row(3, 1, { card(9, 1) });
        table.row(5, 0, { card(1, 1) });
        table.hand(Side::Player, { card(15, 5) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 0, 0, 0));
        // The other rows with a 4 are gone, from both sides; the row the joker is on stays with it
        ASSERT_EQ(game.rowCount(0), 2);
        EXPECT_EQ(game.track(0).mRows[0], std::vector<Card>({ card(4, 1), card(15, 5) }));
        EXPECT_EQ(game.track(0).mRows[1], std::vector<Card>{ card(7, 2) });
        EXPECT_TRUE(game.track(0).mRows[2].empty());
        ASSERT_EQ(game.rowCount(3), 1);
        EXPECT_EQ(game.track(3).mRows[0], std::vector<Card>{ card(9, 1) });
        EXPECT_TRUE(game.track(3).mRows[1].empty());
        // Another value stays
        EXPECT_EQ(game.track(5).mRows[0], std::vector<Card>{ card(1, 1) });
    }

    TEST(FalloutScriptCaravanTest, a_joker_on_an_ace_takes_the_rows_of_the_same_suit)
    {
        Table table;
        table.row(0, 0, { card(1, 2) }).row(0, 1, { card(6, 2) }).row(0, 2, { card(8, 1) });
        table.row(4, 0, { card(1, 3) }).row(4, 1, { card(2, 2) });
        table.hand(Side::Player, { card(15, 5) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 0, 0, 0));
        // Only the ace's own row is left of the suit 2 (the joker is on it), then the 8
        ASSERT_EQ(game.rowCount(0), 2);
        EXPECT_EQ(game.track(0).mRows[0], std::vector<Card>({ card(1, 2), card(15, 5) }));
        EXPECT_EQ(game.track(0).mRows[1], std::vector<Card>{ card(8, 1) });
        EXPECT_EQ(game.track(4).mRows[0], std::vector<Card>{ card(1, 3) });
        EXPECT_TRUE(game.track(4).mRows[1].empty());
    }

    TEST(FalloutScriptCaravanTest, a_queen_and_a_king_stay_on_the_row_and_change_the_caravan)
    {
        Table table;
        table.numbers(0, { 3, 8 }, 2).hand(Side::Player, { card(13, 4), card(14, 1) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 0, 1, 0));
        EXPECT_EQ(game.track(0).mSuit, 4);
        EXPECT_EQ(game.track(0).mDirection, -1);
        EXPECT_EQ(game.track(0).mRows[1].size(), 2u);
    }

    TEST(FalloutScriptCaravanTest, a_caravan_is_sold_at_21_to_26)
    {
        EXPECT_FALSE(Caravan::sold(20));
        EXPECT_TRUE(Caravan::sold(21));
        EXPECT_TRUE(Caravan::sold(26));
        EXPECT_FALSE(Caravan::sold(27));
    }

    TEST(FalloutScriptCaravanTest, the_player_wins_when_two_of_three_pairs_are_its_and_the_third_is_decided)
    {
        Table table;
        // Pairs: 0 with 5, 1 with 4, 2 with 3. The player: 24 against 18 (sold, the other not), 22 against 25 (the
        // opponent's), 23 against 30 (the player's: the opponent is over)
        table.numbers(0, { 9, 8, 7 }).numbers(5, { 9, 9 });
        table.numbers(1, { 9, 7, 6 }).numbers(4, { 9, 9, 7 });
        table.numbers(2, { 9, 8, 6 }).numbers(3, { 9, 9, 6, 6 });
        table.hand(Side::Player, { card(2) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 0, 3, 0));
        EXPECT_TRUE(game.over());
        EXPECT_TRUE(game.playerWon());
    }

    TEST(FalloutScriptCaravanTest, pairs_that_are_not_decided_leave_the_match_going)
    {
        Table table;
        // Both sold with the same total, and neither sold
        table.numbers(0, { 9, 8, 5 }).numbers(5, { 9, 8, 5 });
        table.numbers(1, { 4, 3 }).numbers(4, { 5, 6 });
        table.numbers(2, { 9, 8, 7 }).numbers(3, { 9, 9, 9 });
        table.hand(Side::Player, { card(2) });
        Caravan game = table.game();
        // (the player puts a 2 on a caravan: the totals stay as they are for the tie and the unsold pair)
        ASSERT_TRUE(game.place(Side::Player, 1, 2, 0));
        EXPECT_FALSE(game.over());
        EXPECT_EQ(game.turn(), Side::Opponent);
    }

    TEST(FalloutScriptCaravanTest, the_side_with_two_pairs_wins_even_when_the_third_is_the_others)
    {
        Table table;
        table.numbers(0, { 9, 8, 7 }).numbers(5, { 9, 8 }); // 24 against 17: the player's
        table.numbers(1, { 9, 8, 6 }).numbers(4, { 4 }); // 23 against 4: the player's
        table.numbers(2, { 3 }).numbers(3, { 9, 7, 8 }); // 3 against 24: the opponent's
        table.hand(Side::Player, { card(2) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 2, 1, 0));
        EXPECT_TRUE(game.over());
        EXPECT_TRUE(game.playerWon());
    }

    TEST(FalloutScriptCaravanTest, the_opponent_wins_with_two_pairs)
    {
        Table table;
        table.numbers(0, { 3 }).numbers(5, { 9, 7, 8 });
        table.numbers(1, { 4 }).numbers(4, { 9, 8, 6 });
        table.numbers(2, { 9, 8, 7 }).numbers(3, { 5 });
        table.hand(Side::Player, { card(2) });
        Caravan game = table.game();
        ASSERT_TRUE(game.place(Side::Player, 2, 3, 0));
        EXPECT_TRUE(game.over());
        EXPECT_FALSE(game.playerWon());
    }

    TEST(FalloutScriptCaravanTest, a_side_that_has_no_cards_left_loses_and_the_player_is_looked_at_first)
    {
        // The player puts its last card down with no pile: it has none left, and the match is lost
        {
            Table table;
            table.hand(Side::Player, { card(4) }).pile(Side::Player, {});
            Caravan game = table.game();
            ASSERT_TRUE(game.place(Side::Player, 0, 0, 0));
            EXPECT_TRUE(game.over());
            EXPECT_FALSE(game.playerWon());
        }
        // The opponent has none left: the player wins
        {
            Table table;
            table.hand(Side::Opponent, {}).pile(Side::Opponent, {}).hand(Side::Player, { card(4), card(5) });
            Caravan game = table.game();
            ASSERT_TRUE(game.place(Side::Player, 0, 0, 0));
            EXPECT_TRUE(game.over());
            EXPECT_TRUE(game.playerWon());
        }
        // Both have none: the player loses
        {
            Table table;
            table.hand(Side::Opponent, {})
                .pile(Side::Opponent, {})
                .hand(Side::Player, { card(4) })
                .pile(Side::Player, {});
            Caravan game = table.game();
            ASSERT_TRUE(game.place(Side::Player, 0, 0, 0));
            EXPECT_TRUE(game.over());
            EXPECT_FALSE(game.playerWon());
        }
    }

    TEST(FalloutScriptCaravanTest, giving_up_is_a_loss)
    {
        Table table;
        Caravan game = table.game();
        game.forfeit();
        EXPECT_TRUE(game.over());
        EXPECT_FALSE(game.playerWon());
        EXPECT_FALSE(game.place(Side::Player, 0, 0, 0));
    }

    TEST(FalloutScriptCaravanTest, a_discarded_card_is_gone_and_costs_the_turn_in_the_play)
    {
        Table table;
        table.hand(Side::Player, { card(4), card(6) }).pile(Side::Player, { card(9) });
        Caravan game = table.game();
        ASSERT_TRUE(game.discardCard(Side::Player, 0));
        EXPECT_EQ(game.hand(Side::Player).size(), 2u);
        EXPECT_EQ(game.hand(Side::Player)[0], card(6));
        EXPECT_EQ(game.hand(Side::Player)[1], card(9));
        EXPECT_EQ(game.turn(), Side::Opponent);
    }

    TEST(FalloutScriptCaravanTest, the_player_may_not_discard_a_card_when_its_pile_is_empty)
    {
        Table table;
        table.hand(Side::Player, { card(4), card(6) }).pile(Side::Player, {});
        Caravan game = table.game();
        EXPECT_FALSE(game.discardCard(Side::Player, 0));
        EXPECT_EQ(game.turn(), Side::Player);
    }

    TEST(FalloutScriptCaravanTest, discarding_a_caravan_empties_it_and_costs_the_turn)
    {
        Table table;
        table.numbers(1, { 4, 6, 8 });
        Caravan game = table.game();
        // Not the other side's, and not a caravan out of range
        EXPECT_FALSE(game.discardTrack(Side::Player, 4));
        ASSERT_TRUE(game.discardTrack(Side::Player, 1));
        EXPECT_EQ(game.rowCount(1), 0);
        EXPECT_EQ(game.track(1).mTotal, 0);
        EXPECT_EQ(game.turn(), Side::Opponent);
    }

    TEST(FalloutScriptCaravanTest, a_new_match_deals_eight_cards_to_each_side_and_the_opponent_starts)
    {
        std::vector<Card> pile;
        for (int value = 1; value <= 10; ++value)
            pile.push_back(card(value, 1 + value % 4));
        for (int value = 1; value <= 10; ++value)
            pile.push_back(card(value, 1 + value % 3));
        Caravan game(pile, pile, 0, first());
        EXPECT_EQ(game.hand(Side::Player).size(), 8u);
        EXPECT_EQ(game.hand(Side::Opponent).size(), 8u);
        EXPECT_EQ(game.pile(Side::Player).size(), 12u);
        EXPECT_EQ(game.phase(), Phase::Start);
        EXPECT_EQ(game.turn(), Side::Opponent);
    }

    TEST(FalloutScriptCaravanTest, a_small_pile_gives_what_it_has)
    {
        Caravan game({ card(2), card(3) }, { card(4) }, 0, first());
        EXPECT_EQ(game.hand(Side::Player).size(), 2u);
        EXPECT_EQ(game.hand(Side::Opponent).size(), 1u);
        EXPECT_TRUE(game.pile(Side::Player).empty());
    }

    TEST(FalloutScriptCaravanTest,
        the_start_puts_a_number_card_on_the_first_row_of_each_caravan_of_a_side_and_then_the_play_begins)
    {
        std::vector<Card> pile;
        for (int i = 0; i < 20; ++i)
            pile.push_back(card(1 + i % 10, 1 + i % 4));
        Caravan game(pile, pile, 0, first());
        ASSERT_EQ(game.phase(), Phase::Start);
        for (int step = 0; step < 3; ++step)
        {
            ASSERT_EQ(game.turn(), Side::Opponent);
            // A card of a side's own caravan only, number cards only, on the first row
            const Caravan::Move move = game.opponentMove();
            EXPECT_EQ(move.mType, MoveType::Start);
            EXPECT_EQ(move.mTrack, 5 - step);
            ASSERT_EQ(game.turn(), Side::Player);
            EXPECT_FALSE(game.canPlace(3, 0, 0, Side::Player));
            EXPECT_FALSE(game.canPlace(step, 1, 0, Side::Player));
            ASSERT_TRUE(game.canPlace(step, 0, 0, Side::Player));
            ASSERT_TRUE(game.place(Side::Player, step, 0, 0));
        }
        EXPECT_EQ(game.phase(), Phase::Play);
        EXPECT_EQ(game.turn(), Side::Opponent);
        for (int track = 0; track < 6; ++track)
            EXPECT_EQ(game.rowCount(track), 1) << track;
    }

    TEST(FalloutScriptCaravanTest, a_discard_in_the_start_costs_nothing_and_the_side_moves_again)
    {
        Table table;
        table.mPosition.mPhase = Phase::Start;
        table.hand(Side::Player, { card(12), card(4) }).pile(Side::Player, { card(9) });
        Caravan game = table.game();
        EXPECT_FALSE(game.canPlace(0, 0, 0, Side::Player));
        ASSERT_TRUE(game.discardCard(Side::Player, 0));
        EXPECT_EQ(game.turn(), Side::Player);
        EXPECT_EQ(game.phase(), Phase::Start);
        // No caravan can be discarded in the start
        EXPECT_FALSE(game.discardTrack(Side::Player, 0));
    }

    // ---- The opponent ---------------------------------------------------------------------------------------------

    TEST(FalloutScriptCaravanOpponentTest,
        in_the_start_the_first_kind_puts_down_the_last_number_card_on_its_highest_empty_caravan)
    {
        Table table;
        table.mPosition.mPhase = Phase::Start;
        table.mPosition.mTurn = Side::Opponent;
        table.hand(Side::Opponent, { card(4), card(12), card(9), card(13) });
        table.row(5, 0, { card(2) });
        const Caravan game = table.game(0);
        const Caravan::Move move = game.chooseOpponentMove();
        EXPECT_EQ(move.mType, MoveType::Start);
        EXPECT_EQ(move.mTrack, 4);
        EXPECT_EQ(move.mRow, 0);
        EXPECT_EQ(move.mHand, 2);
    }

    TEST(FalloutScriptCaravanOpponentTest, in_the_start_a_hand_of_more_than_five_face_cards_loses_the_last_of_them)
    {
        Table table;
        table.mPosition.mPhase = Phase::Start;
        table.mPosition.mTurn = Side::Opponent;
        table.hand(Side::Opponent, { card(12), card(13), card(4), card(14), card(15), card(12), card(14), card(13) });
        const Caravan::Move move = table.game(0).chooseOpponentMove();
        EXPECT_EQ(move.mType, MoveType::DiscardCard);
        EXPECT_EQ(move.mHand, 7);
    }

    TEST(FalloutScriptCaravanOpponentTest,
        in_the_start_the_other_kinds_pick_a_high_or_a_low_card_by_which_there_are_more)
    {
        Table table;
        table.mPosition.mPhase = Phase::Start;
        table.mPosition.mTurn = Side::Opponent;
        // Two high (8, 9), one low (2): the first high card
        table.hand(Side::Opponent, { card(5), card(8), card(2), card(9), card(4) });
        EXPECT_EQ(table.game(1).chooseOpponentMove().mHand, 1);
        // One high, two low: the first low card
        table.hand(Side::Opponent, { card(5), card(8), card(2), card(1), card(4) });
        EXPECT_EQ(table.game(1).chooseOpponentMove().mHand, 2);
        // Neither: the first card
        table.hand(Side::Opponent, { card(5), card(4), card(5), card(4) });
        EXPECT_EQ(table.game(1).chooseOpponentMove().mHand, 0);
    }

    TEST(FalloutScriptCaravanOpponentTest,
        in_the_start_the_other_kinds_throw_a_joker_away_first_when_the_hand_is_full_of_the_rest)
    {
        Table table;
        table.mPosition.mPhase = Phase::Start;
        table.mPosition.mTurn = Side::Opponent;
        table.hand(Side::Opponent, { card(12), card(14), card(13), card(15), card(4), card(5), card(1), card(9) });
        const Caravan::Move move = table.game(1).chooseOpponentMove();
        EXPECT_EQ(move.mType, MoveType::DiscardCard);
        EXPECT_EQ(move.mHand, 3);
    }

    TEST(FalloutScriptCaravanOpponentTest, a_caravan_that_is_not_sold_takes_the_card_that_brings_it_the_nearest_to_26)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        table.numbers(3, { 9 }).numbers(4, { 6 }).numbers(5, { 7 });
        table.hand(Side::Opponent, { card(8), card(3), card(10) });
        const Caravan game = table.game();
        const Caravan::Move move = game.chooseOpponentMove();
        EXPECT_EQ(move.mType, MoveType::Place);
        // 9 + 10 = 19 is the highest that is not over; 6 + 10 = 16 and 7 + 10 = 17 are lower. A 10 on 9 is legal (going
        // up).
        EXPECT_EQ(move.mTrack, 3);
        EXPECT_EQ(move.mRow, 1);
        EXPECT_EQ(move.mHand, 2);
        EXPECT_EQ(move.mPriority, 19);
    }

    TEST(FalloutScriptCaravanOpponentTest, a_king_is_put_on_a_row_that_would_bring_the_caravan_to_a_good_total)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        table.numbers(3, { 8, 9 }).hand(Side::Opponent, { card(14) });
        const Caravan::Move move = table.game().chooseOpponentMove();
        // 17 + 9 (the row of the 9 doubled) = 26, worth one more than the total
        EXPECT_EQ(move.mType, MoveType::Place);
        EXPECT_EQ(move.mTrack, 3);
        EXPECT_EQ(move.mRow, 1);
        EXPECT_EQ(move.mPriority, 27);
    }

    TEST(FalloutScriptCaravanOpponentTest, a_caravan_over_26_is_brought_back_with_a_jack_on_one_of_its_own_rows)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        table.numbers(3, { 9, 10, 8 }).hand(Side::Opponent, { card(12), card(2) });
        const Caravan game = table.game();
        const Caravan::Move move = game.chooseOpponentMove();
        // 27; the jack on the 8 or the 9 or the 10 brings it to 19, 18 or 17; the highest is 19
        EXPECT_EQ(move.mType, MoveType::Jack);
        EXPECT_EQ(move.mTrack, 3);
        EXPECT_EQ(move.mRow, 2);
        EXPECT_EQ(move.mPriority, 74 + 19);
    }

    TEST(FalloutScriptCaravanOpponentTest, a_jack_on_the_row_that_takes_the_players_better_caravan_down_is_worth_it)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        // The opponent's caravan is 22, the player's across is 25; a jack on its 9 leaves 16
        table.numbers(5, { 10, 6, 6 }).numbers(0, { 9, 7, 9 }).hand(Side::Opponent, { card(12) });
        const Caravan::Move move = table.game().chooseOpponentMove();
        EXPECT_EQ(move.mType, MoveType::FaceCard);
        EXPECT_EQ(move.mTrack, 0);
        EXPECT_EQ(move.mHand, 0);
        EXPECT_GE(move.mPriority, 75);
    }

    TEST(FalloutScriptCaravanOpponentTest, with_nothing_to_play_the_highest_card_is_thrown_away)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        // Every caravan of its own is sold: a number card would only spoil it, and no face card has a use
        table.numbers(3, { 9, 8, 5 }).numbers(4, { 9, 8, 5 }).numbers(5, { 9, 8, 5 });
        table.hand(Side::Opponent, { card(10), card(7) });
        const Caravan::Move move = table.game().chooseOpponentMove();
        EXPECT_EQ(move.mType, MoveType::DiscardCard);
        EXPECT_EQ(move.mHand, 0);
    }

    TEST(FalloutScriptCaravanOpponentTest,
        a_caravan_that_is_far_over_is_emptied_when_it_is_worth_more_than_a_card_thrown_away)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        table.numbers(3, { 10, 9, 8, 7, 6 }, 1).numbers(4, { 9, 8, 5 }).numbers(5, { 9, 8, 5 });
        // The caravan has 40; discarding it is worth 14 (the first kind) against the hand index 0 of the card
        table.hand(Side::Opponent, { card(10) });
        const Caravan::Move first = table.game(0).chooseOpponentMove();
        EXPECT_EQ(first.mType, MoveType::DiscardTrack);
        EXPECT_EQ(first.mTrack, 3);
        EXPECT_EQ(first.mPriority, 40 - 26);
        EXPECT_EQ(table.game(1).chooseOpponentMove().mPriority, 2 * 40 - 52);
    }

    TEST(FalloutScriptCaravanOpponentTest, the_move_is_made_and_the_turn_goes_to_the_player)
    {
        Table table;
        table.mPosition.mTurn = Side::Opponent;
        table.numbers(3, { 5 }).hand(Side::Opponent, { card(9) }).pile(Side::Opponent, { card(2) });
        Caravan game = table.game();
        const Caravan::Move move = game.opponentMove();
        EXPECT_EQ(move.mType, MoveType::Place);
        EXPECT_EQ(game.track(3).mRows[1], std::vector<Card>{ card(9) });
        EXPECT_EQ(game.turn(), Side::Player);
        ASSERT_EQ(game.hand(Side::Opponent).size(), 1u);
        EXPECT_EQ(game.hand(Side::Opponent)[0], card(2));
    }

    // ---- The stake and the payment ----------------------------------------------------------------------------------

    TEST(FalloutScriptCaravanStakeTest, the_opponent_asks_for_its_share_and_the_player_matches_it)
    {
        CaravanStake stake(500, 400, 0.25f, 0);
        EXPECT_EQ(stake.ante(), 100);
        EXPECT_EQ(stake.bet(), 0);
        EXPECT_FALSE(stake.canAccept());
        ASSERT_TRUE(stake.canMatch());
        stake.match();
        EXPECT_EQ(stake.bet(), 100);
        EXPECT_FALSE(stake.canMatch());
        ASSERT_TRUE(stake.accept());
        EXPECT_EQ(stake.pot(), 100);
    }

    TEST(FalloutScriptCaravanStakeTest, a_player_with_less_than_the_ante_matches_with_all_it_has)
    {
        CaravanStake stake(60, 400, 0.25f, 0);
        stake.match();
        EXPECT_EQ(stake.bet(), 60);
        // Less than the ante and all the player has: no more to match
        EXPECT_FALSE(stake.canMatch());
        ASSERT_TRUE(stake.accept());
        EXPECT_EQ(stake.pot(), 60);
    }

    TEST(FalloutScriptCaravanStakeTest, a_raise_is_followed_up_to_what_the_opponent_will_stake)
    {
        // The opponent has 400 and puts up a quarter, more by the player's Barter: 100 + 100 x 50 / 100 = 150
        CaravanStake stake(500, 400, 0.25f, 50);
        EXPECT_EQ(stake.ceiling(), 150);
        stake.match();
        ASSERT_TRUE(stake.raise(30));
        EXPECT_EQ(stake.bet(), 130);
        EXPECT_EQ(stake.ante(), 130);
        // Further than the ceiling the ante stays at it
        ASSERT_TRUE(stake.raise(100));
        EXPECT_EQ(stake.bet(), 230);
        EXPECT_EQ(stake.ante(), 150);
        ASSERT_TRUE(stake.accept());
        EXPECT_EQ(stake.pot(), 150);
        // Not more than the player has, and not nothing
        EXPECT_EQ(stake.mostRaise(), 270);
        EXPECT_FALSE(stake.raise(271));
        EXPECT_FALSE(stake.raise(0));
    }

    TEST(FalloutScriptCaravanStakeTest, the_ceiling_is_never_more_than_the_opponent_has)
    {
        CaravanStake stake(500, 100, 0.9f, 100);
        EXPECT_EQ(stake.ceiling(), 100);
    }

    TEST(FalloutScriptCaravanStakeTest, a_game_with_no_ante_may_be_accepted_with_no_bet)
    {
        CaravanStake stake(500, 0, 0.5f, 0);
        EXPECT_EQ(stake.ante(), 0);
        ASSERT_TRUE(stake.accept());
        EXPECT_EQ(stake.pot(), 0);
    }

    TEST(FalloutScriptCaravanStakeTest, the_player_with_nothing_cannot_match)
    {
        CaravanStake stake(0, 400, 0.25f, 0);
        EXPECT_FALSE(stake.canMatch());
        EXPECT_FALSE(stake.canAccept());
    }

    TEST(FalloutScriptCaravanPaymentTest, caravan_money_is_paid_first_item_by_item_and_the_rest_in_caps)
    {
        // 100 is owed: two of the 40s are 80 (20 is left), two of the 8s are 16 (4 is left) and two of the 2s are 4, so
        // that nothing is left to pay in caps
        const CaravanPayment payment = settleCaravan(100, { { 0x10, 40, 3 }, { 0x11, 8, 5 }, { 0x12, 2, 5 } }, 50);
        ASSERT_EQ(payment.mItems.size(), 3u);
        EXPECT_EQ(payment.mItems[0], std::make_pair(FalloutScript::FormId(0x10), 2));
        EXPECT_EQ(payment.mItems[1], std::make_pair(FalloutScript::FormId(0x11), 2));
        EXPECT_EQ(payment.mItems[2], std::make_pair(FalloutScript::FormId(0x12), 2));
        EXPECT_EQ(payment.mPayerCaps, 0);
        EXPECT_EQ(payment.mReceiverCaps, 0);
    }

    TEST(FalloutScriptCaravanPaymentTest, a_stack_is_never_split_below_one_item_and_the_rest_is_caps)
    {
        const CaravanPayment payment = settleCaravan(50, { { 0x10, 40, 3 }, { 0x11, 8, 1 } }, 100);
        ASSERT_EQ(payment.mItems.size(), 2u);
        EXPECT_EQ(payment.mItems[0].second, 1);
        EXPECT_EQ(payment.mItems[1].second, 1);
        // 50 - 40 - 8 = 2 in caps
        EXPECT_EQ(payment.mPayerCaps, 2);
        EXPECT_EQ(payment.mReceiverCaps, 2);
    }

    TEST(FalloutScriptCaravanPaymentTest, a_payer_that_lacks_the_caps_pays_what_it_has_and_the_receiver_gets_it_all)
    {
        const CaravanPayment payment = settleCaravan(100, {}, 30);
        EXPECT_TRUE(payment.mItems.empty());
        EXPECT_EQ(payment.mPayerCaps, 30);
        EXPECT_EQ(payment.mReceiverCaps, 100);
    }
}
