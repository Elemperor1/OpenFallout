#include "blackjack.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace FalloutScript::Casino
{
    int cardPoints(int rank)
    {
        if (rank <= 8)
            return rank + 2;
        if (rank <= 11)
            return 10;
        return 11;
    }

    HandTotal handTotal(const std::vector<Card>& cards)
    {
        int total = 0;
        int aces = 0;
        for (const Card& card : cards)
        {
            total += cardPoints(card.mRank);
            if (card.mRank == 12)
                ++aces;
        }
        while (total > 21 && aces > 0)
        {
            total -= 10;
            --aces;
        }
        return { total, aces > 0 };
    }

    Blackjack::Blackjack(const Rules& rules, int minBet, int maxBet, int chips, int luck, Random random)
        : mRules(rules)
        , mMinBet(std::max(1, minBet))
        , mMaxBet(std::max(mMinBet, maxBet))
        , mChips(std::max(0, chips))
        , mLuck(luck)
        , mBet(mMinBet)
        , mRandom(std::move(random))
    {
        shuffle();
    }

    void Blackjack::shuffle()
    {
        // The shoe is the decks laid out in order; a draw takes a card at random, so no order is kept
        mShoe.clear();
        for (int deck = 0; deck < mRules.mDecks; ++deck)
            for (int index = 0; index < 52; ++index)
                mShoe.push_back({ (index + 12) % 13, index / 13 });
        mShoeCount = mShoe.size();
    }

    Card Blackjack::draw(const std::vector<Card>& hand, bool allowLuck, bool forDealer)
    {
        // The shoe starts again when it is nearly spent
        if (mShoeCount < 5)
            shuffle();
        // Luck helps the player when it is above 5 and the dealer when it is below: as many cards are drawn as Luck is
        // away from 5, and the one that brings the hand nearest to 21 without going over is the one that is taken
        int candidates = 1;
        if (allowLuck && (forDealer ? mLuck <= 4 : mLuck >= 6))
            candidates = std::min(std::max(1, std::abs(mLuck - 5)), static_cast<int>(mShoeCount));
        for (int k = 0; k < candidates; ++k)
        {
            const int other = mRandom(k, static_cast<int>(mShoeCount) - 1);
            std::swap(mShoe[static_cast<std::size_t>(k)], mShoe[static_cast<std::size_t>(other)]);
        }
        int pick = 0;
        if (candidates > 1)
        {
            int best = -1;
            for (int k = 0; k < candidates; ++k)
            {
                std::vector<Card> with = hand;
                with.push_back(mShoe[static_cast<std::size_t>(k)]);
                const int total = handTotal(with).mTotal;
                if (total <= 21 && total > best)
                {
                    best = total;
                    pick = k;
                }
            }
        }
        const Card card = mShoe[static_cast<std::size_t>(pick)];
        std::swap(mShoe[static_cast<std::size_t>(pick)], mShoe[mShoeCount - 1]);
        --mShoeCount;
        if (pick != 0)
            (forDealer ? mDealerHelped : mPlayerHelped) = true;
        return card;
    }

    void Blackjack::increaseBet()
    {
        if (mState != State::Playing)
            mBet = increasedBet(Table::Blackjack, mBet, mMinBet, mMaxBet, mChips);
    }

    void Blackjack::decreaseBet()
    {
        if (mState != State::Playing)
            mBet = decreasedBet(Table::Blackjack, mBet, mMinBet);
    }

    void Blackjack::setHighestBet()
    {
        if (mState != State::Playing)
            mBet = highestBet(mMaxBet, mChips);
    }

    bool Blackjack::canDeal() const
    {
        return mState != State::Playing && mBet >= mMinBet && mBet <= mChips;
    }

    bool Blackjack::deal()
    {
        if (!canDeal())
            return false;
        mHands.clear();
        mDealer.clear();
        mActive = 0;
        mActed = false;
        mHit = false;
        mSplit = false;
        mNet = 0;
        mDealerHelped = false;
        mPlayerHelped = false;
        mState = State::Playing;
        // The deal is the dealer's card, the player's, the dealer's second (face down) and the player's second (the
        // executable, 0x7393F0): Luck helps only with the second cards, and both of the dealer's cards are in his hand
        // from the start.
        Hand hand;
        hand.mBet = mBet;
        mDealer.push_back(draw(mDealer, false, true));
        hand.mCards.push_back(draw(hand.mCards, false, false));
        mDealer.push_back(draw(mDealer, true, true));
        hand.mCards.push_back(draw(hand.mCards, true, false));
        mHands.push_back(std::move(hand));
        const bool dealer21 = handTotal(mDealer).mTotal == 21;
        const bool player21 = handTotal(mHands[0].mCards).mTotal == 21;
        if (dealer21 || player21)
        {
            // A blackjack on the deal ends the round: both have one, it is a push
            mHands[0].mDone = true;
            mHands[0].mResult = dealer21 ? (player21 ? Result::Push : Result::Lose) : Result::Blackjack;
            finishRound();
        }
        return true;
    }

    bool Blackjack::canHit() const
    {
        return mState == State::Playing && mActive < static_cast<int>(mHands.size())
            && !mHands[static_cast<std::size_t>(mActive)].mDone
            && static_cast<int>(mHands[static_cast<std::size_t>(mActive)].mCards.size()) < maxCards;
    }

    bool Blackjack::hit()
    {
        if (!canHit())
            return false;
        Hand& hand = mHands[static_cast<std::size_t>(mActive)];
        hand.mCards.push_back(draw(hand.mCards, true, false));
        mActed = true;
        // After a hit the hand can be neither doubled nor split, and it cannot be given up
        mHit = true;
        const int total = handTotal(hand.mCards).mTotal;
        if (total > 21)
            hand.mResult = Result::Bust;
        // A hand that has reached 21 or more, or its seventh card, is finished
        if (total >= 21 || static_cast<int>(hand.mCards.size()) >= maxCards)
        {
            hand.mDone = true;
            finishHand();
        }
        return true;
    }

    bool Blackjack::canStay() const
    {
        return mState == State::Playing && mActive < static_cast<int>(mHands.size())
            && !mHands[static_cast<std::size_t>(mActive)].mDone;
    }

    bool Blackjack::stay()
    {
        if (!canStay())
            return false;
        // Stay ends the player's turn for both hands at once; a second hand that was never played counts with its two
        // cards
        for (Hand& hand : mHands)
            hand.mDone = true;
        mActed = true;
        finishHand();
        return true;
    }

    bool Blackjack::canDouble() const
    {
        // Any two cards of the first hand (the executable offers it on every total, 0x7343F2), with the chips for the
        // doubled bet and, after a split, for the second hand's bet too; a hit takes it away. Double works on the first
        // hand even when the second one is the one in play, as the executable does.
        if (mState != State::Playing || mHit || mHands.empty())
            return false;
        const Hand& hand = mHands[0];
        if (hand.mDone || hand.mDoubled || hand.mCards.size() != 2)
            return false;
        const int needed = 2 * hand.mBet + (mHands.size() == 2 ? mHands[1].mBet : 0);
        return mChips >= needed;
    }

    bool Blackjack::doubleDown()
    {
        if (!canDouble())
            return false;
        Hand& hand = mHands[0];
        hand.mBet *= 2;
        hand.mDoubled = true;
        hand.mCards.push_back(draw(hand.mCards, true, false));
        mActed = true;
        if (handTotal(hand.mCards).mTotal > 21)
            hand.mResult = Result::Bust;
        hand.mDone = true;
        finishHand();
        return true;
    }

    bool Blackjack::canSplit() const
    {
        // Two cards of the same rank, one split only, and the chips for the second bet
        if (mState != State::Playing || mSplit || mHands.size() != 1 || mActed)
            return false;
        const Hand& hand = mHands[0];
        return !hand.mDone && hand.mCards.size() == 2 && hand.mCards[0].mRank == hand.mCards[1].mRank
            && mChips >= 2 * hand.mBet;
    }

    bool Blackjack::split()
    {
        if (!canSplit())
            return false;
        Hand second;
        second.mBet = mHands[0].mBet;
        second.mCards.push_back(mHands[0].mCards[1]);
        mHands[0].mCards.pop_back();
        // The cards that come to a split hand are not helped by Luck
        mHands[0].mCards.push_back(draw(mHands[0].mCards, false, false));
        second.mCards.push_back(draw(second.mCards, false, false));
        mHands.push_back(std::move(second));
        mSplit = true;
        mActed = true;
        mActive = 0;
        // The executable adds up the cards of the first hand for both of its checks for 21 (0x73483F and 0x734854 both
        // take the list of hand one, Mac round 12AR D2): a first hand of 21 makes a blackjack of both hands, and a 21
        // that only the second hand has is not seen here (it is counted when it is played or scored)
        if (handTotal(mHands[0].mCards).mTotal == 21)
        {
            for (Hand& hand : mHands)
            {
                hand.mResult = Result::Blackjack;
                hand.mDone = true;
            }
            finishHand();
        }
        return true;
    }

    bool Blackjack::canSurrender() const
    {
        return mState == State::Playing && !mActed && mHands.size() == 1 && !mHands[0].mDone;
    }

    bool Blackjack::surrender()
    {
        if (!canSurrender())
            return false;
        mHands[0].mResult = Result::Surrender;
        mHands[0].mDone = true;
        mActed = true;
        finishHand();
        return true;
    }

    bool Blackjack::canSwitch() const
    {
        return mState == State::Playing && mHands.size() == 2 && !mHands[static_cast<std::size_t>(1 - mActive)].mDone;
    }

    bool Blackjack::switchHands()
    {
        if (!canSwitch())
            return false;
        mActive = 1 - mActive;
        return true;
    }

    void Blackjack::abandon()
    {
        if (mState != State::Playing)
            return;
        for (Hand& hand : mHands)
        {
            hand.mDone = true;
            if (hand.mResult == Result::None)
                hand.mResult = Result::Lose;
        }
        finishRound();
    }

    void Blackjack::finishHand()
    {
        // The turn is over when every hand is finished. It never moves on to the other hand by itself: the player
        // switches hands (or stays, which ends both).
        for (const Hand& hand : mHands)
            if (!hand.mDone)
                return;
        playDealer();
        settle();
        finishRound();
    }

    void Blackjack::playDealer()
    {
        // The dealer does not play for hands that are lost or given up, nor when both of two split hands are blackjacks
        const bool needed = std::any_of(mHands.begin(), mHands.end(),
            [](const Hand& hand) { return hand.mResult != Result::Bust && hand.mResult != Result::Surrender; });
        const bool blackjacks = std::all_of(
            mHands.begin(), mHands.end(), [](const Hand& hand) { return hand.mResult == Result::Blackjack; });
        if (!needed || blackjacks)
            return;
        const auto holdsAce = [](const std::vector<Card>& cards) {
            return std::any_of(cards.begin(), cards.end(), [](const Card& card) { return card.mRank == 12; });
        };
        while (static_cast<int>(mDealer.size()) < maxCards)
        {
            const int total = handTotal(mDealer).mTotal;
            // Below 17 the dealer draws. On 17 he draws when the hand holds an ace (any ace card, as the executable
            // tests, 0x7373E8), unless the casino's record says he stands.
            const bool draws = total < 17 || (total == 17 && holdsAce(mDealer) && !mRules.mDealerStandsOnSoft17);
            if (!draws)
                break;
            mDealer.push_back(draw(mDealer, true, true));
        }
    }

    void Blackjack::settle()
    {
        const int dealer = handTotal(mDealer).mTotal;
        for (Hand& hand : mHands)
        {
            if (hand.mResult != Result::None)
                continue;
            const int total = handTotal(hand.mCards).mTotal;
            if (hand.mCards.size() == 2 && total == 21)
                hand.mResult = Result::Blackjack;
            else if (dealer > 21 || total > dealer)
                hand.mResult = Result::Win;
            else if (total == dealer)
                hand.mResult = Result::Push;
            else
                hand.mResult = Result::Lose;
        }
    }

    int Blackjack::payout(Result result, int bet, bool secondHand) const
    {
        switch (result)
        {
            case Result::Lose:
            case Result::Bust:
                return -bet;
            case Result::Win:
                return bet;
            case Result::Blackjack:
                return static_cast<int>(static_cast<double>(bet) * mRules.mBlackjackPayout + (secondHand ? 0.5 : 0.51));
            case Result::Surrender:
                return -static_cast<int>(bet / 2.0 + 0.51);
            case Result::Push:
            case Result::None:
                break;
        }
        return 0;
    }

    void Blackjack::finishRound()
    {
        mNet = 0;
        for (std::size_t i = 0; i < mHands.size(); ++i)
            mNet += payout(mHands[i].mResult, mHands[i].mBet, i == 1);
        mChips = std::max(0, mChips + mNet);
        mState = State::Finished;
        // What the player has left may be less than the bet
        mBet = std::min(mBet, mChips);
        // The shoe is shuffled when what is left of it has fallen to the share of the record
        if (static_cast<double>(mShoeCount) <= mRules.mDecks * 52 * static_cast<double>(mRules.mReshuffleShare))
            shuffle();
    }
}
