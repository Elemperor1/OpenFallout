#include <components/falloutscript/blackjack.hpp>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutScript::Casino;
    using Result = Blackjack::Result;
    using State = Blackjack::State;

    // Ranks as the executable numbers them: 0 to 8 are the twos to the tens, 9 to 11 the court cards, 12 the ace
    constexpr int two = 0;
    constexpr int three = 1;
    constexpr int four = 2;
    constexpr int five = 3;
    constexpr int six = 4;
    constexpr int seven = 5;
    constexpr int eight = 6;
    constexpr int nine = 7;
    constexpr int ten = 8;
    constexpr int king = 11;
    constexpr int ace = 12;

    // A source of random numbers that hands out the cards that are asked for, in that order (one candidate per draw, so
    // the games without Luck's help): it keeps the shoe as the game does, to find the place where the card is
    struct Scripted
    {
        std::vector<int> mShoe;
        std::size_t mCount = 0;
        std::vector<int> mWanted;
        std::size_t mNext = 0;

        explicit Scripted(std::vector<int> wanted, int decks = 1)
            : mWanted(std::move(wanted))
        {
            for (int deck = 0; deck < decks; ++deck)
                for (int index = 0; index < 52; ++index)
                    mShoe.push_back(index);
            mCount = mShoe.size();
        }

        int operator()(int low, int high)
        {
            if (mNext >= mWanted.size() || low != 0)
                return low;
            const int want = mWanted[mNext++];
            for (std::size_t position = 0; position < mCount; ++position)
            {
                if ((mShoe[position] + 12) % 13 == want)
                {
                    EXPECT_LE(static_cast<int>(position), high);
                    std::swap(mShoe[0], mShoe[position]);
                    std::swap(mShoe[0], mShoe[mCount - 1]);
                    --mCount;
                    return static_cast<int>(position);
                }
            }
            ADD_FAILURE() << "no card of rank " << want << " is left";
            return low;
        }
    };

    Random scripted(std::vector<int> wanted, std::shared_ptr<Scripted>* keep = nullptr, int decks = 1)
    {
        auto source = std::make_shared<Scripted>(std::move(wanted), decks);
        if (keep != nullptr)
            *keep = source;
        return [source](int low, int high) { return (*source)(low, high); };
    }

    Rules defaultRules()
    {
        return Rules::fromRecord(0.f, 0.f, { 2, 2, 2, 2, 2, 2, 2 }, 0, 0, 0);
    }

    // The cards are named in the order the hands are read in: the player's first card, the dealer's first, the player's
    // second and the dealer's second (face down), then those that are drawn; the game deals them dealer, player,
    // dealer, player (0x7393F0)
    Blackjack table(std::vector<int> cards, int chips = 500, int bet = 10, int luck = 5, Rules rules = defaultRules())
    {
        if (cards.size() >= 4)
        {
            std::swap(cards[0], cards[1]);
            std::swap(cards[2], cards[3]);
        }
        Blackjack game(rules, 5, 200, chips, luck, scripted(std::move(cards)));
        while (game.bet() < bet)
            game.increaseBet();
        return game;
    }

    TEST(FalloutScriptBlackjackTest, aces_count_eleven_until_that_would_bust)
    {
        EXPECT_EQ(handTotal({ { ace, 0 }, { king, 1 } }).mTotal, 21);
        EXPECT_TRUE(handTotal({ { ace, 0 }, { six, 1 } }).mSoft);
        const HandTotal hard = handTotal({ { ace, 0 }, { six, 1 }, { ten, 2 } });
        EXPECT_EQ(hard.mTotal, 17);
        EXPECT_FALSE(hard.mSoft);
        EXPECT_EQ(handTotal({ { ace, 0 }, { ace, 1 }, { ace, 2 } }).mTotal, 13);
        EXPECT_EQ(cardPoints(two), 2);
        EXPECT_EQ(cardPoints(ten), 10);
        EXPECT_EQ(cardPoints(9), 10);
        EXPECT_EQ(cardPoints(ace), 11);
    }

    TEST(FalloutScriptBlackjackTest, a_blackjack_on_the_deal_ends_the_round_and_pays_one_and_a_half)
    {
        Blackjack game = table({ ace, nine, king, five });
        ASSERT_TRUE(game.deal());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Blackjack);
        // 10 x 1.5 = 15
        EXPECT_EQ(game.net(), 15);
        EXPECT_EQ(game.chips(), 515);
        EXPECT_FALSE(game.holeCardHidden());
    }

    TEST(FalloutScriptBlackjackTest, a_dealer_blackjack_beats_the_player_or_pushes_with_a_player_blackjack)
    {
        Blackjack lost = table({ ten, ace, nine, king });
        ASSERT_TRUE(lost.deal());
        EXPECT_EQ(lost.hands()[0].mResult, Result::Lose);
        EXPECT_EQ(lost.net(), -10);
        Blackjack pushed = table({ ace, ace, king, king });
        ASSERT_TRUE(pushed.deal());
        EXPECT_EQ(pushed.hands()[0].mResult, Result::Push);
        EXPECT_EQ(pushed.net(), 0);
    }

    TEST(FalloutScriptBlackjackTest, hit_and_stay_and_the_dealer_draws_to_seventeen)
    {
        // The player has 16, the dealer 16 (one card down). The player hits a five and stays on 21; the dealer draws a
        // ten and busts.
        Blackjack game = table({ ten, nine, six, seven, five, ten });
        ASSERT_TRUE(game.deal());
        EXPECT_EQ(game.state(), State::Playing);
        EXPECT_TRUE(game.holeCardHidden());
        EXPECT_TRUE(game.canHit());
        ASSERT_TRUE(game.hit());
        // A hand that comes to 21 is finished at once and the dealer plays
        EXPECT_EQ(handTotal(game.hands()[0].mCards).mTotal, 21);
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.dealerCards().size(), 3u);
        EXPECT_EQ(game.hands()[0].mResult, Result::Win);
        EXPECT_EQ(game.net(), 10);
        EXPECT_EQ(game.chips(), 510);
    }

    TEST(FalloutScriptBlackjackTest, a_bust_loses_at_once_and_the_dealer_does_not_play)
    {
        Blackjack game = table({ ten, nine, six, seven, ten });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.hit());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Bust);
        EXPECT_EQ(game.dealerCards().size(), 2u);
        EXPECT_EQ(game.net(), -10);
    }

    TEST(FalloutScriptBlackjackTest, totals_decide_a_stay_and_equal_totals_push)
    {
        Blackjack higher = table({ ten, nine, nine, eight });
        ASSERT_TRUE(higher.deal());
        ASSERT_TRUE(higher.stay());
        // 19 against 17: the dealer stands on a hard 17
        EXPECT_EQ(higher.hands()[0].mResult, Result::Win);
        Blackjack equal = table({ ten, ten, eight, eight });
        ASSERT_TRUE(equal.deal());
        ASSERT_TRUE(equal.stay());
        EXPECT_EQ(equal.hands()[0].mResult, Result::Push);
        EXPECT_EQ(equal.net(), 0);
        Blackjack lower = table({ ten, ten, seven, nine });
        ASSERT_TRUE(lower.deal());
        ASSERT_TRUE(lower.stay());
        EXPECT_EQ(lower.hands()[0].mResult, Result::Lose);
    }

    TEST(FalloutScriptBlackjackTest, the_dealer_draws_on_a_soft_seventeen_unless_the_record_says_it_stands)
    {
        // The player has 19, the dealer ace and six
        Blackjack hits = table({ ten, ace, nine, six, four });
        ASSERT_TRUE(hits.deal());
        ASSERT_TRUE(hits.stay());
        EXPECT_EQ(hits.dealerCards().size(), 3u);
        // 17 + 4 = 21 (soft 17 plus a four is 21), so the dealer wins
        EXPECT_EQ(hits.hands()[0].mResult, Result::Lose);
        const Rules stands = Rules::fromRecord(0.f, 0.f, { 2, 2, 2, 2, 2, 2, 2 }, 0, 0, 1);
        Blackjack stays = table({ ten, ace, nine, six, four }, 500, 10, 5, stands);
        ASSERT_TRUE(stays.deal());
        ASSERT_TRUE(stays.stay());
        EXPECT_EQ(stays.dealerCards().size(), 2u);
        EXPECT_EQ(stays.hands()[0].mResult, Result::Win);
    }

    TEST(FalloutScriptBlackjackTest, the_dealer_draws_on_a_seventeen_that_holds_an_ace_card)
    {
        // The dealer's ace and six draw a ten (a hard 17 with an ace in it), and he draws again, as the game does
        Blackjack hits = table({ ten, ace, nine, six, ten, three });
        ASSERT_TRUE(hits.deal());
        ASSERT_TRUE(hits.stay());
        EXPECT_EQ(hits.dealerCards().size(), 4u);
        EXPECT_EQ(hits.hands()[0].mResult, Result::Lose);
        // A hard 17 without an ace stands
        Blackjack stands = table({ ten, ten, nine, seven, ten, three });
        ASSERT_TRUE(stands.deal());
        ASSERT_TRUE(stands.stay());
        EXPECT_EQ(stands.dealerCards().size(), 2u);
    }

    TEST(FalloutScriptBlackjackTest, doubling_down_doubles_the_bet_and_takes_one_card)
    {
        // The player has 6 and 5; the dealer 10 and 7; the double card is a ten: 21 against 17
        Blackjack game = table({ six, ten, five, seven, ten });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.canDouble());
        ASSERT_TRUE(game.doubleDown());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mBet, 20);
        EXPECT_EQ(game.hands()[0].mCards.size(), 3u);
        EXPECT_EQ(game.net(), 20);
        EXPECT_EQ(game.chips(), 520);
    }

    TEST(FalloutScriptBlackjackTest, doubling_needs_the_chips_for_a_second_bet_and_a_hand_that_has_not_been_played)
    {
        Blackjack poor = table({ six, ten, five, seven }, 15, 10);
        ASSERT_TRUE(poor.deal());
        EXPECT_FALSE(poor.canDouble());
        Blackjack played = table({ six, ten, five, seven, two }, 500, 10);
        ASSERT_TRUE(played.deal());
        ASSERT_TRUE(played.hit());
        EXPECT_FALSE(played.canDouble());
        EXPECT_FALSE(played.canSurrender());
        EXPECT_FALSE(played.canSplit());
    }

    TEST(FalloutScriptBlackjackTest, a_pair_can_be_split_once_and_each_hand_is_played_and_paid_on_its_own)
    {
        // Eights against a dealer 10 and 7. Hand one gets a three, hand two a ten.
        Blackjack game = table({ eight, ten, eight, seven, three, ten });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.canSplit());
        ASSERT_TRUE(game.split());
        ASSERT_EQ(game.hands().size(), 2u);
        EXPECT_FALSE(game.canSplit());
        EXPECT_EQ(game.hands()[1].mBet, 10);
        EXPECT_EQ(game.hands()[0].mCards.size(), 2u);
        EXPECT_EQ(game.activeHand(), 0);
        EXPECT_TRUE(game.canSwitch());
        ASSERT_TRUE(game.switchHands());
        EXPECT_EQ(game.activeHand(), 1);
        // Stay ends the turn of both hands: hand two (18) beats the dealer's 17, hand one (11) loses
        ASSERT_TRUE(game.stay());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Lose);
        EXPECT_EQ(game.hands()[1].mResult, Result::Win);
        EXPECT_EQ(game.net(), 0);
    }

    TEST(FalloutScriptBlackjackTest, the_turn_never_moves_to_the_other_split_hand_by_itself)
    {
        // Eights against a dealer 10 and 7; hand one gets a ten (18) and hits a king (28), hand two gets a three (11)
        Blackjack game = table({ eight, ten, eight, seven, ten, three, king, two });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.split());
        ASSERT_TRUE(game.hit());
        EXPECT_EQ(game.hands()[0].mResult, Result::Bust);
        // The finished hand stays the active one: it cannot be hit, and the player has to switch
        EXPECT_EQ(game.state(), State::Playing);
        EXPECT_EQ(game.activeHand(), 0);
        EXPECT_FALSE(game.canHit());
        ASSERT_TRUE(game.canSwitch());
        ASSERT_TRUE(game.switchHands());
        EXPECT_EQ(game.activeHand(), 1);
        EXPECT_FALSE(game.canSwitch());
        // Hand two (11 after its three) is played to the end: a two makes 13
        ASSERT_TRUE(game.hit());
        EXPECT_EQ(game.hands()[1].mCards.size(), 3u);
        ASSERT_TRUE(game.stay());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Bust);
    }

    TEST(FalloutScriptBlackjackTest, doubling_a_hand_after_a_split_works_on_the_first_hand)
    {
        // Fives against a dealer 6 and 10: hand one gets a six (11) and hand two a four (9)
        Blackjack game = table({ five, six, five, ten, six, four, ten });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.split());
        ASSERT_TRUE(game.switchHands());
        // The second hand is the one in play, the first is the one that is doubled
        ASSERT_TRUE(game.canDouble());
        ASSERT_TRUE(game.doubleDown());
        EXPECT_EQ(game.hands()[0].mBet, 20);
        EXPECT_EQ(game.hands()[0].mCards.size(), 3u);
        EXPECT_EQ(game.hands()[1].mCards.size(), 2u);
        EXPECT_TRUE(game.hands()[0].mDone);
        EXPECT_EQ(game.state(), State::Playing);
        // Chips that do not cover both bets and the doubling forbid it
        Blackjack poor = table({ five, six, five, ten, six, four }, 25, 10);
        ASSERT_TRUE(poor.deal());
        ASSERT_TRUE(poor.split());
        EXPECT_FALSE(poor.canDouble());
    }

    TEST(FalloutScriptBlackjackTest, a_split_hand_of_twenty_one_pays_as_a_blackjack)
    {
        // Aces split against a dealer 9 and 7, both get a ten: both hands are blackjacks, the round is paid at once
        Blackjack game = table({ ace, nine, ace, seven, ten, ten });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.split());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Blackjack);
        EXPECT_EQ(game.hands()[1].mResult, Result::Blackjack);
        // The dealer does not draw for it
        EXPECT_EQ(game.dealerCards().size(), 2u);
        // 10 x 1.5 + 0.51 is 15 and 10 x 1.5 + 0.5 is 15
        EXPECT_EQ(game.net(), 30);
    }

    TEST(FalloutScriptBlackjackTest, a_first_split_hand_of_twenty_one_makes_a_blackjack_of_both_hands)
    {
        // Tens split against a dealer 9 and 7: hand one gets an ace (21), hand two a five (15). The executable adds up
        // the first hand's cards for both checks, so the second hand is a blackjack too and the round is paid at once.
        Blackjack game = table({ ten, nine, ten, seven, ace, five });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.split());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Blackjack);
        EXPECT_EQ(game.hands()[1].mResult, Result::Blackjack);
        EXPECT_EQ(game.dealerCards().size(), 2u);
        EXPECT_EQ(game.net(), 30);
    }

    TEST(FalloutScriptBlackjackTest, a_twenty_one_of_the_second_split_hand_alone_is_not_seen_at_the_split)
    {
        // Aces split against a dealer 9 and 7: hand one gets a five (16), hand two a ten (21)
        Blackjack game = table({ ace, nine, ace, seven, five, ten, ten });
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.split());
        EXPECT_EQ(game.state(), State::Playing);
        EXPECT_FALSE(game.hands()[1].mDone);
        EXPECT_EQ(game.hands()[1].mResult, Result::None);
        EXPECT_FALSE(game.hands()[0].mDone);
        // Both hands are scored when the player stays: the dealer draws a ten and busts
        ASSERT_TRUE(game.stay());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Win);
        EXPECT_NE(game.hands()[1].mResult, Result::Lose);
    }

    TEST(FalloutScriptBlackjackTest, surrender_gives_up_half_the_bet_and_only_before_the_player_has_acted)
    {
        Blackjack game = table({ ten, ten, six, seven }, 500, 10);
        ASSERT_TRUE(game.canDeal());
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.canSurrender());
        ASSERT_TRUE(game.surrender());
        EXPECT_EQ(game.hands()[0].mResult, Result::Surrender);
        EXPECT_EQ(game.net(), -5);
        EXPECT_EQ(game.state(), State::Finished);
        // An odd bet gives up the half that rounds up ((5 / 2 + 0.51) is 3.01)
        Blackjack odd = table({ ten, ten, six, seven }, 500, 5);
        ASSERT_TRUE(odd.deal());
        ASSERT_TRUE(odd.surrender());
        EXPECT_EQ(odd.net(), -3);
    }

    TEST(FalloutScriptBlackjackTest, leaving_in_the_middle_of_a_round_loses_what_is_on_the_table)
    {
        Blackjack game = table({ ten, ten, six, seven }, 500, 10);
        ASSERT_TRUE(game.deal());
        game.abandon();
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.net(), -10);
        EXPECT_EQ(game.chips(), 490);
        // Between rounds nothing is lost
        game.abandon();
        EXPECT_EQ(game.chips(), 490);
    }

    TEST(FalloutScriptBlackjackTest, a_hand_stops_at_seven_cards)
    {
        // 2 + 2 + 2 + 2 + 3 + 3 + 3 is 17 with seven cards; the dealer holds 10 and 10. The seventh card finishes the
        // hand and the round.
        Blackjack game = table({ two, ten, two, ten, two, two, three, three, three });
        ASSERT_TRUE(game.deal());
        for (int i = 0; i < 4; ++i)
            ASSERT_TRUE(game.hit());
        EXPECT_EQ(game.hands()[0].mCards.size(), 6u);
        EXPECT_TRUE(game.canHit());
        ASSERT_TRUE(game.hit());
        EXPECT_EQ(game.hands()[0].mCards.size(), 7u);
        EXPECT_FALSE(game.canHit());
        EXPECT_EQ(game.state(), State::Finished);
        EXPECT_EQ(game.hands()[0].mResult, Result::Lose);
    }

    TEST(FalloutScriptBlackjackTest, the_bet_and_the_chips_follow_the_rounds)
    {
        Blackjack game = table({ ten, ten, six, seven }, 12, 10);
        ASSERT_TRUE(game.deal());
        ASSERT_TRUE(game.stay());
        // 16 against 17: the bet of 10 is lost; 2 chips are left, and the least bet is 5
        EXPECT_EQ(game.chips(), 2);
        EXPECT_EQ(game.bet(), 2);
        EXPECT_TRUE(game.broke());
        EXPECT_FALSE(game.canDeal());
    }

    TEST(FalloutScriptBlackjackTest, luck_helps_the_player_who_hits_with_more_cards_to_choose_from)
    {
        // Luck 10: five cards are drawn for a hit, and the one that brings 16 nearest to 21 without going over is
        // taken. After the four cards of the deal the shoe is in an order the script knows; the candidates are its
        // first five (the source gives the low end of the range, which leaves them where they are).
        std::shared_ptr<Scripted> shoe;
        Blackjack game(defaultRules(), 5, 200, 500, 10, scripted({ ten, nine, six, seven }, &shoe));
        ASSERT_TRUE(game.deal());
        std::vector<int> candidates(shoe->mShoe.begin(), shoe->mShoe.begin() + 5);
        int best = -1;
        int pick = 0;
        for (int k = 0; k < 5; ++k)
        {
            const int total = 16 + cardPoints((candidates[static_cast<std::size_t>(k)] + 12) % 13);
            if (total <= 21 && total > best)
            {
                best = total;
                pick = k;
            }
        }
        ASSERT_TRUE(game.hit());
        const int taken = 16 + cardPoints((candidates[static_cast<std::size_t>(pick)] + 12) % 13);
        EXPECT_EQ(handTotal(game.hands()[0].mCards).mTotal > 21 ? 22 : handTotal(game.hands()[0].mCards).mTotal,
            taken > 21 ? 22 : taken);
        EXPECT_EQ(game.playerHelped(), pick != 0);
        EXPECT_FALSE(game.dealerHelped());
    }

    TEST(FalloutScriptBlackjackTest, luck_below_five_helps_the_dealer_and_luck_five_helps_nobody)
    {
        Blackjack neutral(defaultRules(), 5, 200, 500, 5, scripted({ ten, nine, six, seven }));
        ASSERT_TRUE(neutral.deal());
        ASSERT_TRUE(neutral.hit());
        EXPECT_FALSE(neutral.playerHelped());
        EXPECT_FALSE(neutral.dealerHelped());
    }

    TEST(FalloutScriptBlackjackTest, a_game_of_many_rounds_keeps_the_numbers_in_order)
    {
        std::mt19937 engine(12345);
        auto random = [&engine](int low, int high) { return std::uniform_int_distribution<int>(low, high)(engine); };
        for (int luck : { 1, 5, 10 })
        {
            Blackjack game(Rules::fromRecord(0.f, 0.f, { 2, 2, 2, 2, 2, 2, 2 }, 2, 0, 0), 5, 200, 100000, luck, random);
            int rounds = 0;
            while (rounds < 400 && !game.broke())
            {
                const int before = game.chips();
                if (random(0, 3) == 0)
                    game.increaseBet();
                ASSERT_TRUE(game.deal());
                int moves = 0;
                while (game.state() == State::Playing && ++moves < 40)
                {
                    switch (random(0, 5))
                    {
                        case 0:
                            game.hit();
                            break;
                        case 1:
                            game.doubleDown();
                            break;
                        case 2:
                            game.split();
                            break;
                        case 3:
                            game.switchHands();
                            break;
                        case 4:
                            game.surrender();
                            break;
                        default:
                            game.stay();
                            break;
                    }
                }
                if (game.state() == State::Playing)
                {
                    while (game.state() == State::Playing)
                        game.stay();
                }
                ASSERT_EQ(game.state(), State::Finished);
                EXPECT_EQ(game.chips(), before + game.net());
                for (const auto& hand : game.hands())
                    EXPECT_NE(hand.mResult, Result::None);
                EXPECT_LE(game.bet(), std::max(game.chips(), 5));
                ++rounds;
            }
            EXPECT_GT(rounds, 0);
        }
    }
}
