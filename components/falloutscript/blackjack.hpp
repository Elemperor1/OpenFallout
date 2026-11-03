#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_BLACKJACK_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_BLACKJACK_H

#include <cstddef>
#include <vector>

#include "casino.hpp"

namespace FalloutScript::Casino
{
    /// A card of the shoe, as the executable numbers them (FalloutNV.exe, the blackjack menu 0x733630): ranks 0 to 8
    /// are the twos to the tens, 9, 10 and 11 the jack, queen and king (ten points each) and 12 the ace (11 or 1).
    struct Card
    {
        int mRank = 0;
        int mSuit = 0;
    };

    int cardPoints(int rank);

    /// The total of a hand: aces count 11 while that does not take it past 21 (soft: one of them still does).
    struct HandTotal
    {
        int mTotal = 0;
        bool mSoft = false;
    };

    HandTotal handTotal(const std::vector<Card>& cards);

    /// One round of blackjack at a casino table, and the chips the player has at it. The player bets, is dealt two
    /// cards (and the dealer two, one of them turned down), and hits, stays, doubles down, splits a pair or surrenders;
    /// the dealer plays when the player is done. The numbers of the results are those of the executable (0x73BA20); the
    /// rules of the turn are those of its state machine (0x734DB0).
    class Blackjack
    {
    public:
        enum class State
        {
            /// The player bets (the cards of the last round, if any, are still on the table)
            Betting,
            Playing,
            /// The round has a result; the cards stay on the table until the next deal
            Finished,
        };

        enum class Result
        {
            None = 0,
            Lose = 1,
            Push = 2,
            Win = 3,
            Bust = 4,
            Blackjack = 5,
            Surrender = 6,
        };

        /// What a hand of the player has at the moment
        struct Hand
        {
            std::vector<Card> mCards;
            int mBet = 0;
            Result mResult = Result::None;
            bool mDoubled = false;
            /// The player is done with the hand (stayed, finished at 21 or the seventh card, busted, doubled or
            /// surrendered)
            bool mDone = false;
        };

        /// The most cards a hand holds; a seventh card ends it
        static constexpr int maxCards = 7;

        Blackjack(const Rules& rules, int minBet, int maxBet, int chips, int luck, Random random);

        State state() const { return mState; }
        int bet() const { return mBet; }
        int chips() const { return mChips; }
        int minBet() const { return mMinBet; }
        int maxBet() const { return mMaxBet; }

        // The bet (while the player is betting)
        void increaseBet();
        void decreaseBet();
        void setHighestBet();

        bool canDeal() const;
        bool deal();

        // The moves of a round
        bool canHit() const;
        bool hit();
        bool canStay() const;
        bool stay();
        bool canDouble() const;
        bool doubleDown();
        bool canSplit() const;
        bool split();
        bool canSurrender() const;
        bool surrender();
        bool canSwitch() const;
        bool switchHands();
        /// The player gets up in the middle of a round: what is on the table is lost (nothing happens between rounds)
        void abandon();

        const std::vector<Hand>& hands() const { return mHands; }
        int activeHand() const { return mActive; }
        const std::vector<Card>& dealerCards() const { return mDealer; }
        /// The dealer's second card is face down until the round has a result
        bool holeCardHidden() const { return mState == State::Playing && mDealer.size() >= 2; }

        /// What the last round came to: what the player has more (or less) in chips, and which of Luck's helps was
        /// used (they choose the words of the result)
        int net() const { return mNet; }
        bool dealerHelped() const { return mDealerHelped; }
        bool playerHelped() const { return mPlayerHelped; }
        /// The player has not the chips for the least bet any more (the game ends)
        bool broke() const { return mChips < mMinBet; }

    private:
        Card draw(const std::vector<Card>& hand, bool allowLuck, bool forDealer);
        void shuffle();
        void finishHand();
        void finishRound();
        void playDealer();
        void settle();
        int payout(Result result, int bet, bool secondHand) const;

        Rules mRules;
        int mMinBet;
        int mMaxBet;
        int mChips;
        int mLuck;
        int mBet;
        Random mRandom;
        State mState = State::Betting;
        std::vector<Card> mShoe;
        std::size_t mShoeCount = 0;
        std::vector<Card> mDealer;
        std::vector<Hand> mHands;
        int mActive = 0;
        bool mActed = false;
        bool mHit = false;
        bool mSplit = false;
        int mNet = 0;
        bool mDealerHelped = false;
        bool mPlayerHelped = false;
    };
}

#endif
