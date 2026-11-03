#include "caravan.hpp"

#include <algorithm>
#include <cstdlib>

namespace FalloutScript::Casino
{
    namespace
    {
        constexpr int opponentIndex = static_cast<int>(Caravan::Side::Opponent);
        constexpr int playerIndex = static_cast<int>(Caravan::Side::Player);

        int sideIndex(Caravan::Side side)
        {
            return static_cast<int>(side);
        }

        /// The first caravan of a side: the player's are 0 to 2, the opponent's 3 to 5; the one that faces track `i` is
        /// 5 - i
        bool ownTrack(Caravan::Side side, int track)
        {
            return side == Caravan::Side::Opponent ? track >= 3 && track <= 5 : track >= 0 && track <= 2;
        }
    }

    Caravan::Caravan(std::vector<Card> playerPile, std::vector<Card> opponentPile, int aiType, Random random)
        : mAiType(aiType)
        , mRandom(std::move(random))
    {
        mPiles[playerIndex] = std::move(playerPile);
        mPiles[opponentIndex] = std::move(opponentPile);
        // Each side takes eight cards from its own pile, one at a time, chosen at random (0x73EC44)
        for (int side = 0; side < 2; ++side)
        {
            for (int count = 0; count < handSize && !mPiles[side].empty(); ++count)
            {
                const int pick = mRandom(0, static_cast<int>(mPiles[side].size()) - 1);
                mHands[side].push_back(mPiles[side][pick]);
                mPiles[side].erase(mPiles[side].begin() + pick);
            }
        }
        mPhase = Phase::Start;
        // The opponent moves first (0x73B9FA)
        mTurn = Side::Opponent;
        recompute();
    }

    Caravan::Caravan(const Position& position, Random random)
        : mHands(position.mHands)
        , mPiles(position.mPiles)
        , mPhase(position.mPhase)
        , mTurn(position.mTurn)
        , mAiType(position.mAiType)
        , mRandom(std::move(random))
    {
        for (int track = 0; track < tracks; ++track)
            mTracks[track].mRows = position.mBoard[track];
        recompute();
    }

    int Caravan::rowCount(int track) const
    {
        int count = 0;
        while (count < rows && !mTracks[track].mRows[count].empty())
            ++count;
        return count;
    }

    int Caravan::rowValue(int track, int row) const
    {
        if (track < 0 || track >= tracks || row < 0 || row >= rows)
            return 0;
        const Row& cards = mTracks[track].mRows[row];
        if (cards.empty())
            return 0;
        int value = cards[0].mValue;
        for (std::size_t card = 1; card < cards.size(); ++card)
            if (cards[card].mValue == king)
                value *= 2;
        return value;
    }

    void Caravan::recomputeTrack(int index)
    {
        Track& track = mTracks[index];
        track.mDirection = 0;
        track.mSuit = noSuit;
        track.mTotal = 0;
        const int count = rowCount(index);
        for (int row = 0; row < count; ++row)
            track.mTotal += rowValue(index, row);
        if (count == 0)
            return;
        const Row& last = track.mRows[count - 1];
        track.mSuit = last[0].mSuit;
        if (count >= 2)
            track.mDirection = last[0].mValue > track.mRows[count - 2][0].mValue ? 1 : -1;
        // Only the Queens on the last row count: each gives the caravan its own suit and turns the direction round
        for (std::size_t card = 1; card < last.size(); ++card)
        {
            if (last[card].mValue != queen)
                continue;
            track.mSuit = last[card].mSuit;
            if (count >= 2)
                track.mDirection = -track.mDirection;
        }
    }

    void Caravan::recompute()
    {
        for (int track = 0; track < tracks; ++track)
            recomputeTrack(track);
    }

    bool Caravan::canPlace(int track, int row, int handIndex, Side side) const
    {
        if (track < 0 || track >= tracks || row < 0 || row >= rows)
            return false;
        const std::vector<Card>& cards = mHands[sideIndex(side)];
        if (handIndex < 0 || handIndex >= static_cast<int>(cards.size()))
            return false;
        const Card& card = cards[handIndex];
        const Row& target = mTracks[track].mRows[row];

        if (mPhase == Phase::Start)
            return card.mValue <= 10 && row == 0 && target.empty() && ownTrack(side, track);
        if (mPhase != Phase::Play)
            return false;

        if (card.isFace())
        {
            // A row with a number card and room for one more; a Queen only on the last row of a caravan
            if (target.empty() || target.size() > 3)
                return false;
            if (card.mValue == queen && row < rows - 1 && !mTracks[track].mRows[row + 1].empty())
                return false;
            return true;
        }

        if (!target.empty() || !ownTrack(side, track))
            return false;
        if (row == 0)
            return true;
        const Row& before = mTracks[track].mRows[row - 1];
        if (before.empty())
            return false;
        const int previous = before[0].mValue;
        if (previous == card.mValue)
            return false;
        const Track& caravan = mTracks[track];
        // Against the way the caravan goes only a card of its suit may be put
        if (caravan.mDirection == 1 && previous > card.mValue)
            return caravan.mSuit == card.mSuit;
        if (caravan.mDirection == -1 && previous < card.mValue)
            return caravan.mSuit == card.mSuit;
        return true;
    }

    void Caravan::fill(Side side)
    {
        std::vector<Card>& pile = mPiles[sideIndex(side)];
        if (pile.empty())
            return;
        const int pick = mRandom(0, static_cast<int>(pile.size()) - 1);
        mHands[sideIndex(side)].push_back(pile[pick]);
        pile.erase(pile.begin() + pick);
    }

    void Caravan::removeRows(const std::array<std::array<bool, rows>, tracks>& marked)
    {
        for (int track = 0; track < tracks; ++track)
        {
            std::array<Row, rows> kept;
            int next = 0;
            for (int row = 0; row < rows; ++row)
            {
                if (marked[track][row] || mTracks[track].mRows[row].empty())
                    continue;
                kept[next++] = std::move(mTracks[track].mRows[row]);
            }
            mTracks[track].mRows = std::move(kept);
        }
    }

    bool Caravan::place(Side side, int track, int row, int handIndex)
    {
        if (mPhase == Phase::Over || side != mTurn || !canPlace(track, row, handIndex, side))
            return false;
        std::vector<Card>& cards = mHands[sideIndex(side)];
        const Card card = cards[handIndex];
        cards.erase(cards.begin() + handIndex);
        Row& target = mTracks[track].mRows[row];

        if (!card.isFace())
            target = { card };
        else if (card.mValue == jack)
        {
            // The whole row goes, with what is on it, and the rows after it move down (0x7448FE)
            std::array<std::array<bool, rows>, tracks> marked{};
            marked[track][row] = true;
            removeRows(marked);
        }
        else if (card.mValue == joker)
        {
            // It stays on its row; every other row (of either side) whose number card is alike goes: the same value,
            // and for an ace the same suit (0x744859)
            target.push_back(card);
            const int value = target[0].mValue;
            const int suit = target[0].mSuit;
            std::array<std::array<bool, rows>, tracks> marked{};
            for (int other = 0; other < tracks; ++other)
            {
                for (int at = 0; at < rows; ++at)
                {
                    const Row& candidate = mTracks[other].mRows[at];
                    if (candidate.empty() || (other == track && at == row))
                        continue;
                    marked[other][at] = value == 1 ? candidate[0].mSuit == suit : candidate[0].mValue == value;
                }
            }
            removeRows(marked);
        }
        else
            target.push_back(card);

        recompute();
        fill(side);
        if (mPhase == Phase::Start)
        {
            bool done = true;
            for (int at = 0; at < tracks; ++at)
                done = done && !mTracks[at].mRows[0].empty();
            if (done)
                mPhase = Phase::Play;
        }
        endTurn();
        return true;
    }

    bool Caravan::discardCard(Side side, int handIndex)
    {
        if (mPhase == Phase::Over || side != mTurn)
            return false;
        std::vector<Card>& cards = mHands[sideIndex(side)];
        if (handIndex < 0 || handIndex >= static_cast<int>(cards.size()))
            return false;
        // The player may not throw a card away when its pile has none to give back
        if (side == Side::Player && pileEmpty(side))
            return false;
        cards.erase(cards.begin() + handIndex);
        fill(side);
        // In the start a card is thrown away for nothing and the side moves again
        if (mPhase == Phase::Play)
            endTurn();
        return true;
    }

    bool Caravan::discardTrack(Side side, int track)
    {
        if (mPhase != Phase::Play || side != mTurn || !ownTrack(side, track))
            return false;
        mTracks[track].mRows = {};
        recompute();
        endTurn();
        return true;
    }

    void Caravan::forfeit()
    {
        if (mPhase == Phase::Over)
            return;
        mPlayerWon = false;
        mPhase = Phase::Over;
    }

    bool Caravan::decide()
    {
        recompute();
        int player = 0;
        int opponent = 0;
        for (int track = 0; track < 3; ++track)
        {
            const int mine = mTracks[track].mTotal;
            const int theirs = mTracks[5 - track].mTotal;
            const bool mineSold = sold(mine);
            const bool theirsSold = sold(theirs);
            if (mineSold && (!theirsSold || mine > theirs))
                ++player;
            else if (theirsSold && (!mineSold || mine < theirs))
                ++opponent;
        }
        // All three pairs decided: the side with two or three of them wins
        if (player + opponent == 3)
        {
            mPlayerWon = player > opponent;
            return true;
        }
        // Otherwise a side that has no cards at all loses (the player is looked at first)
        if (mPiles[playerIndex].empty() && mHands[playerIndex].empty())
        {
            mPlayerWon = false;
            return true;
        }
        if (mPiles[opponentIndex].empty() && mHands[opponentIndex].empty())
        {
            mPlayerWon = true;
            return true;
        }
        return false;
    }

    void Caravan::endTurn()
    {
        if (decide())
        {
            mPhase = Phase::Over;
            return;
        }
        mTurn = mTurn == Side::Player ? Side::Opponent : Side::Player;
    }

    // ---- The opponent (0x74FDC0)
    // --------------------------------------------------------------------------------------

    Caravan::Move Caravan::startMove() const
    {
        const std::vector<Card>& cards = mHands[opponentIndex];
        Move move;
        move.mType = MoveType::Start;
        move.mRow = 0;
        // The highest of its caravans that has no card yet
        move.mTrack = 3;
        for (int track = 5; track >= 3; --track)
        {
            if (mTracks[track].mRows[0].empty())
            {
                move.mTrack = track;
                break;
            }
        }
        const int count = static_cast<int>(cards.size());
        const auto discard = [&](int index) {
            Move thrown;
            thrown.mType = MoveType::DiscardCard;
            thrown.mHand = index;
            return thrown;
        };
        const auto lastOf = [&](bool face) {
            for (int index = count - 1; index >= 0; --index)
                if (cards[index].isFace() == face)
                    return index;
            return -1;
        };
        int numbers = 0;
        for (const Card& card : cards)
            numbers += card.isFace() ? 0 : 1;

        if (mAiType <= 0)
        {
            // Too many face cards in the hand: the last one goes; else the last number card is put down
            if (count - numbers > 5)
                return discard(lastOf(true));
            const int index = lastOf(false);
            if (index < 0)
                return discard(count > 0 ? count - 1 : 0);
            move.mHand = index;
            return move;
        }

        // The other kinds count the high and the low cards (6 to 10 and 1 to 3) and throw a face card away when more
        // than five are neither
        int high = 0;
        int low = 0;
        for (const Card& card : cards)
        {
            if (!card.isFace() && card.mValue >= 6)
                ++high;
            else if (!card.isFace() && card.mValue <= 3)
                ++low;
        }
        if (count - high - low > 5)
        {
            // The one to throw away: a Joker first, then a Queen, a King, a Jack
            for (const int value : { joker, queen, king, jack })
                for (int index = 0; index < count; ++index)
                    if (cards[index].mValue == value)
                        return discard(index);
        }
        if (mAiType < 3)
        {
            if (high > low)
            {
                for (int index = 0; index < count; ++index)
                    if (!cards[index].isFace() && cards[index].mValue >= 6)
                    {
                        move.mHand = index;
                        return move;
                    }
            }
            else if (low != 0)
            {
                for (int index = 0; index < count; ++index)
                    if (!cards[index].isFace() && cards[index].mValue < 4)
                    {
                        move.mHand = index;
                        return move;
                    }
            }
            move.mHand = 0;
            return move;
        }
        // The card nearest to six
        int best = -1;
        for (int index = 0; index < count; ++index)
        {
            if (cards[index].isFace())
                continue;
            if (best < 0 || std::abs(cards[index].mValue - 6) < std::abs(cards[best].mValue - 6))
                best = index;
        }
        move.mHand = best < 0 ? 0 : best;
        return move;
    }

    int Caravan::jokerLoss(int track, int row, int lossTrack) const
    {
        // What a Joker on this row would take off the caravan `lossTrack`: the value of each of its rows that is alike
        const Row& target = mTracks[track].mRows[row];
        if (target.empty())
            return 0;
        const int value = target[0].mValue;
        const int suit = target[0].mSuit;
        int loss = 0;
        for (int at = 0; at < rows; ++at)
        {
            const Row& candidate = mTracks[lossTrack].mRows[at];
            if (candidate.empty() || (lossTrack == track && at == row))
                continue;
            if (value == 1 ? candidate[0].mSuit == suit : candidate[0].mValue == value)
                loss += rowValue(lossTrack, at);
        }
        return loss;
    }

    Caravan::Move Caravan::chooseOpponentMove() const
    {
        if (mPhase == Phase::Start)
            return startMove();

        const std::vector<Card>& cards = mHands[opponentIndex];
        const int count = static_cast<int>(cards.size());
        std::vector<Move> moves;
        const auto add = [&moves](MoveType type, int track, int row, int hand, int priority) {
            Move move;
            move.mType = type;
            move.mTrack = track;
            move.mRow = row;
            move.mHand = hand;
            move.mPriority = priority;
            moves.push_back(move);
        };

        for (int track = 3; track <= 5; ++track)
        {
            const int total = mTracks[track].mTotal;
            const int facing = 5 - track;
            const int facingTotal = mTracks[facing].mTotal;
            const int next = rowCount(track);
            for (int hand = 0; hand < count; ++hand)
            {
                const int value = cards[hand].mValue;
                const bool number = !cards[hand].isFace();
                if (total < 21)
                {
                    // Short of 21: a number card on the next row, a King on a row that has room
                    if (number && next < rows)
                    {
                        const int now = total + value;
                        if (now <= 26 && canPlace(track, next, hand, Side::Opponent))
                            add(MoveType::Place, track, next, hand, now);
                    }
                    if (value == king)
                    {
                        for (int row = 0; row < next; ++row)
                        {
                            if (mTracks[track].mRows[row].size() > 3)
                                continue;
                            const int now = total + rowValue(track, row);
                            if (now <= 26 && canPlace(track, row, hand, Side::Opponent))
                                add(MoveType::Place, track, row, hand, now + 1);
                        }
                    }
                }
                else if (total <= 26)
                {
                    if (total < facingTotal && facingTotal < 27)
                    {
                        // Sold, and the caravan across is better but not sold out of reach: top it, or cut it down
                        if (number && next < rows && canPlace(track, next, hand, Side::Opponent))
                        {
                            const int now = total + value;
                            const int priority = now >= 27 ? 0
                                : now > facingTotal        ? 100 - (now - facingTotal)
                                : now == facingTotal       ? 50
                                                           : now - 1;
                            add(MoveType::Place, track, next, hand, priority);
                        }
                        if (value == jack)
                        {
                            for (int row = 0; row < rowCount(facing); ++row)
                            {
                                const int delta = rowValue(facing, row);
                                if (delta <= facingTotal - total || !canPlace(facing, row, hand, Side::Opponent))
                                    continue;
                                const int d = total - (facingTotal - delta);
                                const int priority = facingTotal - delta > 21 ? 100 - d : 75 + d;
                                add(MoveType::FaceCard, facing, row, hand, priority);
                            }
                        }
                        if (value == joker && mAiType >= 1)
                        {
                            for (int row = 0; row < next; ++row)
                            {
                                if (!canPlace(track, row, hand, Side::Opponent))
                                    continue;
                                // As the executable has it: the loss of the caravan across is taken off a second time
                                const int difference = (total - jokerLoss(track, row, track)) - facingTotal
                                    - jokerLoss(track, row, facing);
                                if (difference > 0)
                                    add(MoveType::Joker, track, row, hand, difference + 50);
                            }
                        }
                    }
                    else
                    {
                        if (number && next < rows)
                        {
                            const int now = total + value;
                            if (now <= 26 && canPlace(track, next, hand, Side::Opponent))
                                add(MoveType::Place, track, next, hand, now);
                        }
                        if (value == king)
                        {
                            for (int row = 0; row < next; ++row)
                            {
                                if (mTracks[track].mRows[row].size() > 3)
                                    continue;
                                // (the row's value is added twice, as the executable has it)
                                const int now = total + 2 * rowValue(track, row);
                                if (now <= 26 && canPlace(track, row, hand, Side::Opponent))
                                    add(MoveType::Place, track, row, hand, now);
                            }
                        }
                    }
                }
                else if (value == jack)
                {
                    // Over 26: a Jack on a row of its own that brings it back
                    for (int row = 0; row < next; ++row)
                    {
                        const int now = total - rowValue(track, row);
                        if (now < 27 && canPlace(track, row, hand, Side::Opponent))
                            add(MoveType::Jack, track, row, hand, 74 + now);
                    }
                }
            }
        }

        // The face cards that go on the caravans of the player's, whatever the caravans of its own are
        for (int track = 0; track < 3; ++track)
        {
            const int total = mTracks[track].mTotal;
            for (int hand = 0; hand < count; ++hand)
            {
                const int value = cards[hand].mValue;
                if (value != jack && value != king)
                    continue;
                for (int row = 0; row < rowCount(track); ++row)
                {
                    if (mTracks[track].mRows[row].size() > 3)
                        continue;
                    const int delta = rowValue(track, row);
                    if (value == jack)
                    {
                        const int now = total - delta;
                        if (sold(total))
                            add(MoveType::FaceCard, track, row, hand, 51 - now);
                        else if (total < 21)
                            add(MoveType::FaceCard, track, row, hand, 26 - now);
                    }
                    else if (total + delta > 26)
                        add(MoveType::FaceCard, track, row, hand, total + delta - 1);
                }
            }
        }

        if (moves.empty())
        {
            // Nothing to be done with the cards: the highest card goes (the earlier of equals), with the number of its
            // place in the hand as its worth, and a caravan that is out of reach is emptied
            int highest = -1;
            for (int hand = 0; hand < count; ++hand)
                if (highest < 0 || cards[hand].mValue > cards[highest].mValue)
                    highest = hand;
            if (highest >= 0)
                add(MoveType::DiscardCard, 0, 0, highest, highest);
            int jacks = 0;
            for (const Card& card : mPiles[opponentIndex])
                jacks += card.mValue == jack ? 1 : 0;
            for (int track = 3; track <= 5; ++track)
            {
                const int total = mTracks[track].mTotal;
                if (sold(total))
                    continue;
                int priority = total - 26;
                if (mAiType == 1 || mAiType == 2)
                    priority = 2 * total - 52;
                else if (mAiType >= 3)
                    priority = 2 * total - 52 - 5 * jacks;
                add(MoveType::DiscardTrack, track, 0, 0, priority);
            }
        }

        if (moves.empty())
            return Move{ MoveType::DiscardCard, 0, 0, 0, 0 };
        // The highest worth wins, the first made among equals
        std::stable_sort(
            moves.begin(), moves.end(), [](const Move& a, const Move& b) { return a.mPriority > b.mPriority; });
        return moves.front();
    }

    Caravan::Move Caravan::opponentMove()
    {
        if (mPhase == Phase::Over || mTurn != Side::Opponent)
            return Move{ MoveType::DiscardCard, 0, 0, 0, 0 };
        const Move move = chooseOpponentMove();
        bool done = false;
        switch (move.mType)
        {
            case MoveType::DiscardTrack:
                done = discardTrack(Side::Opponent, move.mTrack);
                break;
            case MoveType::DiscardCard:
                done = discardCard(Side::Opponent, move.mHand);
                break;
            default:
                done = place(Side::Opponent, move.mTrack, move.mRow, move.mHand);
                break;
        }
        if (!done && !mHands[opponentIndex].empty())
        {
            // A move that cannot be made (a card that has nowhere to go) is thrown away instead
            discardCard(Side::Opponent, move.mHand < static_cast<int>(mHands[opponentIndex].size()) ? move.mHand : 0);
        }
        return move;
    }

    // ---- The stake and the payment ---------------------------------------------------------------------------------

    CaravanStake::CaravanStake(int funds, int opponentFunds, float fraction, int barter)
        : mFunds(std::max(0, funds))
        , mOpponentFunds(std::max(0, opponentFunds))
        , mFraction(fraction)
        , mBarter(barter)
    {
        mAnte = static_cast<int>(static_cast<float>(mOpponentFunds) * mFraction);
        updateCeiling();
    }

    void CaravanStake::updateCeiling()
    {
        // What the opponent follows a raise to: its share of what it has, more by the player's Barter, never more than
        // it has (0x73D5B4)
        const float held = static_cast<float>(mOpponentFunds) * mFraction;
        const float more = held + held * static_cast<float>(mBarter) / 100.f;
        mCeiling = std::min(static_cast<int>(more), mOpponentFunds);
    }

    bool CaravanStake::canMatch() const
    {
        return mFunds > 0 && (mBet == 0 || (mBet < mAnte && mBet < mFunds));
    }

    void CaravanStake::match()
    {
        if (canMatch())
            mBet = std::min(mAnte, mFunds);
    }

    bool CaravanStake::raise(int amount)
    {
        if (amount < 1 || amount > mostRaise())
            return false;
        mBet += amount;
        updateCeiling();
        if (mBet > mAnte)
            mAnte = std::min(mBet, mCeiling);
        return true;
    }

    bool CaravanStake::accept()
    {
        if (!canAccept())
            return false;
        mPot = std::min(mBet, mAnte);
        return true;
    }

    CaravanPayment settleCaravan(int pot, const std::vector<CaravanPayment::Item>& money, int caps)
    {
        CaravanPayment payment;
        int owed = std::max(0, pot);
        for (const CaravanPayment::Item& item : money)
        {
            if (item.mValue <= 0 || item.mCount <= 0 || owed <= 0)
                continue;
            const int count = std::min(item.mCount, owed / item.mValue);
            if (count <= 0)
                continue;
            payment.mItems.emplace_back(item.mForm, count);
            owed -= count * item.mValue;
        }
        payment.mReceiverCaps = owed;
        payment.mPayerCaps = std::min(owed, std::max(0, caps));
        return payment;
    }
}
