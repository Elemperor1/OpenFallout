#include "falloutscripts.hpp"

#include <algorithm>
#include <numeric>
#include <random>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadccrd.hpp>
#include <components/esm4/loadcdck.hpp>
#include <components/esm4/loadcmny.hpp>

#include "../mwbase/environment.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;
        using FalloutScript::Casino::Caravan;
        using FalloutScript::Casino::CaravanPayment;
        using FalloutScript::Casino::CaravanStake;

        /// The form of the caps (the games keep them as form 0xF)
        constexpr FalloutScript::FormId capsNumber = 0x0000000F;
        /// The least number of cards a deck has: the menu does not open for a player with fewer cards, and the deck
        /// is not accepted with fewer (FalloutNV.exe 0x741060 and the menu's Play)
        constexpr int leastDeck = 30;
        /// The most cards the deck takes: with this many in it the executable blocks adding another (0x749740)
        constexpr int mostDeck = 108;
        /// The miscellaneous statistics that a match of Caravan adds one to when it is won and when it is lost
        constexpr int caravanGamesWonStat = 0x25;
        constexpr int caravanGamesLostStat = 0x26;

        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        ESM::FormId formOf(FalloutScript::FormId number)
        {
            return ESM::FormId::fromUint32(number);
        }

        /// A card of the record: the two numbers INTV holds are its suit and its value (Mac round 12AS E3)
        Caravan::Card cardOf(const ESM4::CaravanCard& record, FalloutScript::FormId form)
        {
            Caravan::Card card;
            card.mForm = form;
            if (record.mIntegers.size() >= 2)
            {
                card.mSuit = static_cast<int>(record.mIntegers[0]);
                card.mValue = static_cast<int>(record.mIntegers[1]);
            }
            return card;
        }

        FalloutScripts::CaravanBoardCard boardCard(const Caravan::Card& card, std::string face)
        {
            return { card.mValue, card.mSuit, std::move(face) };
        }
    }

    namespace
    {
        /// What a holder has in caps and in caravan money (the form, the worth of one and how many), in the order of
        /// the forms
        struct Money
        {
            int mCaps = 0;
            std::vector<CaravanPayment::Item> mItems;
            int total() const
            {
                int sum = mCaps;
                for (const CaravanPayment::Item& item : mItems)
                    sum += item.mValue * item.mCount;
                return sum;
            }
        };
    }

    // FalloutNV.exe 0x73D650: the money of a side is its caps and the worth of the caravan money in its pack
    static Money moneyOf(
        FalloutScript::ActorState& actors, const OFWorld::ESMStore& store, FalloutScript::FormId holder)
    {
        Money money;
        if (holder == 0)
            return money;
        money.mCaps = actors.itemCount(holder, capsNumber);
        std::vector<std::pair<FalloutScript::FormId, int>> items;
        if (!actors.items(holder, items))
            return money;
        for (const auto& [form, count] : items)
        {
            if (count <= 0)
                continue;
            if (const ESM4::CaravanMoney* record = store.get<ESM4::CaravanMoney>().search(formOf(form)))
                money.mItems.push_back({ form, static_cast<int>(record->mAbsoluteValue), count });
        }
        return money;
    }

    bool FalloutScripts::openCaravan(
        FalloutScript::FormId opponent, FalloutScript::FormId deck, int aiType, float fraction)
    {
        const ESM4::CaravanDeck* record = deck == 0 ? nullptr : mStore.get<ESM4::CaravanDeck>().search(formOf(deck));
        if (record == nullptr)
        {
            Log(Debug::Warning) << "Caravan: the menu is asked for " << std::hex << deck << std::dec
                                << ", which is not a deck of the files";
            return false;
        }
        CaravanSession session;
        session.mOpponent = opponent;
        session.mDeck = deck;
        session.mAiType = aiType;
        // The opponent plays the cards of the deck's record, each of them once (no limit, no least number)
        for (const ESM::FormId& cardForm : record->mCards)
            if (const ESM4::CaravanCard* card = mStore.get<ESM4::CaravanCard>().search(cardForm))
                session.mOpponentCards.push_back(cardOf(*card, numberOf(cardForm)));
        // The player's cards are the ones of its pack, each card of a stack by itself
        std::vector<std::pair<FalloutScript::FormId, int>> items;
        mActors.items(mPlayer, items);
        for (const auto& [form, count] : items)
        {
            const ESM4::CaravanCard* card = count > 0 ? mStore.get<ESM4::CaravanCard>().search(formOf(form)) : nullptr;
            if (card == nullptr)
                continue;
            for (int copy = 0; copy < count; ++copy)
                session.mOwned.push_back({ cardOf(*card, form), card->mFullName, false });
        }
        // The cards of the screen go by the card's value, the smallest first, and by nothing else (the executable sorts its one
        // list that way, 0x73CED0; cards of one value stay in the order the pack has them)
        std::stable_sort(session.mOwned.begin(), session.mOwned.end(),
            [](const CaravanSession::Owned& left, const CaravanSession::Owned& right) {
                return left.mCard.mValue < right.mCard.mValue;
            });
        if (static_cast<int>(session.mOwned.size()) < leastDeck)
        {
            showNotice("You need at least " + std::to_string(leastDeck) + " Caravan cards to play.", 0);
            return false;
        }
        const int barter = static_cast<int>(mActorValues.current(mPlayer, AV::barter));
        session.mStake.emplace(
            moneyOf(mActors, mStore, mPlayer).total(), moneyOf(mActors, mStore, opponent).total(), fraction, barter);
        const std::lock_guard lock(mCaravanMutex);
        session.mSerial = ++mCaravanSerial;
        mCaravan = std::move(session);
        return true;
    }

    std::optional<FalloutScripts::CaravanScreen> FalloutScripts::caravanScreen()
    {
        CaravanScreen screen;
        FalloutScript::FormId lookup = 0;
        {
            const std::lock_guard lock(mCaravanMutex);
            if (!mCaravan)
                return std::nullopt;
            const CaravanSession& session = *mCaravan;
            screen.mStage = session.mStage;
            screen.mSerial = session.mSerial;
            screen.mOpponent = session.mOpponentName;
            if (screen.mOpponent.empty())
                lookup = session.mOpponent;
            if (session.mStake)
            {
                const CaravanStake& stake = *session.mStake;
                screen.mFunds = stake.funds();
                screen.mAnte = stake.ante();
                screen.mBet = stake.bet();
                screen.mCeiling = stake.ceiling();
                screen.mMostRaise = stake.mostRaise();
                screen.mPot = stake.pot();
                screen.mCanMatch = stake.canMatch();
                screen.mCanAccept = stake.canAccept();
            }
            screen.mOwned = static_cast<int>(session.mOwned.size());
            screen.mInDeck = static_cast<int>(std::count_if(
                session.mOwned.begin(), session.mOwned.end(), [](const auto& card) { return card.mInDeck; }));
            screen.mCanPlay = screen.mInDeck >= leastDeck;
            screen.mWon = session.mWon;
            screen.mForfeited = session.mForfeited;
            screen.mTotals = mCaravanRecord.totals();
        }
        // The name is the one of the opponent's reference, which only the thread of the frame may look up
        if (lookup != 0)
        {
            const OFWorld::Ptr ptr = findReference(lookup);
            if (!ptr.isEmpty())
                screen.mOpponent = std::string(ptr.getClass().getName(ptr));
            const std::lock_guard lock(mCaravanMutex);
            if (mCaravan && mCaravan->mSerial == screen.mSerial)
                mCaravan->mOpponentName = screen.mOpponent;
        }
        return screen;
    }

    bool FalloutScripts::caravanMatch()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || mCaravan->mStage != CaravanStage::Bet || !mCaravan->mStake || !mCaravan->mStake->canMatch())
            return false;
        mCaravan->mStake->match();
        return true;
    }

    bool FalloutScripts::caravanRaise(int amount)
    {
        const std::lock_guard lock(mCaravanMutex);
        return mCaravan && mCaravan->mStage == CaravanStage::Bet && mCaravan->mStake && mCaravan->mStake->raise(amount);
    }

    bool FalloutScripts::caravanAccept()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || mCaravan->mStage != CaravanStage::Bet || !mCaravan->mStake || !mCaravan->mStake->accept())
            return false;
        mCaravan->mStage = CaravanStage::Deck;
        return true;
    }

    std::vector<FalloutScripts::CaravanCardView> FalloutScripts::caravanCards()
    {
        std::vector<CaravanCardView> cards;
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan)
            return cards;
        cards.reserve(mCaravan->mOwned.size());
        for (const CaravanSession::Owned& owned : mCaravan->mOwned)
            cards.push_back({ owned.mCard.mForm, owned.mCard.mValue, owned.mCard.mSuit, owned.mName, owned.mInDeck,
                caravanFace(owned.mCard.mForm) });
        return cards;
    }

    std::string FalloutScripts::caravanFace(FalloutScript::FormId card) const
    {
        if (card == 0)
            return {};
        const ESM4::CaravanCard* record = mStore.get<ESM4::CaravanCard>().search(formOf(card));
        return record != nullptr ? record->mFrontFace : std::string();
    }

    bool FalloutScripts::caravanSetInDeck(int index, bool inDeck)
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || mCaravan->mStage != CaravanStage::Deck || index < 0
            || index >= static_cast<int>(mCaravan->mOwned.size()))
            return false;
        CaravanSession::Owned& owned = mCaravan->mOwned[static_cast<std::size_t>(index)];
        if (owned.mInDeck == inDeck)
            return false;
        if (inDeck
            && std::count_if(mCaravan->mOwned.begin(), mCaravan->mOwned.end(), [](const auto& card) { return card.mInDeck; })
                >= mostDeck)
            return false;
        owned.mInDeck = inDeck;
        return true;
    }

    bool FalloutScripts::caravanRandomize()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || mCaravan->mStage != CaravanStage::Deck)
            return false;
        // As many cards as the least deck for a player that has no more than that, else a number from that to the
        // number of cards it has (the game picks it as 0x944460(30, count), the top excluded; 0x748A5B)
        std::vector<CaravanSession::Owned>& owned = mCaravan->mOwned;
        const int count = static_cast<int>(owned.size());
        const int take
            = count <= leastDeck ? leastDeck : std::uniform_int_distribution<int>(leastDeck, count - 1)(mCaravanRandom);
        std::vector<int> order(owned.size());
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), mCaravanRandom);
        for (CaravanSession::Owned& card : owned)
            card.mInDeck = false;
        for (int pick = 0; pick < take && pick < count && pick < mostDeck; ++pick)
            owned[static_cast<std::size_t>(order[static_cast<std::size_t>(pick)])].mInDeck = true;
        return true;
    }

    bool FalloutScripts::caravanPlay()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || mCaravan->mStage != CaravanStage::Deck)
            return false;
        std::vector<Caravan::Card> pile;
        for (const CaravanSession::Owned& owned : mCaravan->mOwned)
            if (owned.mInDeck)
                pile.push_back(owned.mCard);
        if (static_cast<int>(pile.size()) < leastDeck)
            return false;
        mCaravan->mGame.emplace(std::move(pile), mCaravan->mOpponentCards, mCaravan->mAiType,
            [this](int low, int high) { return std::uniform_int_distribution<int>(low, high)(mCaravanRandom); });
        mCaravan->mStage = CaravanStage::Game;
        return true;
    }

    std::optional<FalloutScripts::CaravanBoard> FalloutScripts::caravanBoard()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || !mCaravan->mGame)
            return std::nullopt;
        const Caravan& game = *mCaravan->mGame;
        CaravanBoard board;
        for (int index = 0; index < Caravan::tracks; ++index)
        {
            const Caravan::Track& track = game.track(index);
            CaravanBoardTrack& shown = board.mTracks[static_cast<std::size_t>(index)];
            shown.mTotal = track.mTotal;
            shown.mDirection = track.mDirection;
            shown.mSuit = track.mSuit;
            for (int row = 0; row < game.rowCount(index); ++row)
            {
                std::vector<CaravanBoardCard>& cards = shown.mRows.emplace_back();
                for (const Caravan::Card& card : track.mRows[static_cast<std::size_t>(row)])
                    cards.push_back(boardCard(card, caravanFace(card.mForm)));
            }
        }
        for (const Caravan::Card& card : game.hand(Caravan::Side::Player))
            board.mHand.push_back(boardCard(card, caravanFace(card.mForm)));
        board.mOpponentHand = static_cast<int>(game.hand(Caravan::Side::Opponent).size());
        board.mPile = static_cast<int>(game.pile(Caravan::Side::Player).size());
        board.mOpponentPile = static_cast<int>(game.pile(Caravan::Side::Opponent).size());
        board.mPhase = game.phase() == Caravan::Phase::Start ? "start"
            : game.phase() == Caravan::Phase::Play           ? "play"
                                                             : "over";
        board.mPlayersTurn = !game.over() && game.turn() == Caravan::Side::Player;
        board.mOver = game.over();
        board.mWon = game.over() && game.playerWon();
        return board;
    }

    bool FalloutScripts::caravanCanPlace(int track, int row, int hand)
    {
        const std::lock_guard lock(mCaravanMutex);
        return mCaravan && mCaravan->mGame && !mCaravan->mGame->over()
            && mCaravan->mGame->turn() == Caravan::Side::Player
            && mCaravan->mGame->canPlace(track, row, hand, Caravan::Side::Player);
    }

    bool FalloutScripts::caravanPlace(int track, int row, int hand)
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || !mCaravan->mGame || mCaravan->mStage != CaravanStage::Game)
            return false;
        if (!mCaravan->mGame->place(Caravan::Side::Player, track, row, hand))
            return false;
        if (mCaravan->mGame->over())
            payCaravan(*mCaravan);
        return true;
    }

    bool FalloutScripts::caravanDiscardCard(int hand)
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || !mCaravan->mGame || mCaravan->mStage != CaravanStage::Game)
            return false;
        if (!mCaravan->mGame->discardCard(Caravan::Side::Player, hand))
            return false;
        if (mCaravan->mGame->over())
            payCaravan(*mCaravan);
        return true;
    }

    bool FalloutScripts::caravanDiscardTrack(int track)
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || !mCaravan->mGame || mCaravan->mStage != CaravanStage::Game)
            return false;
        if (!mCaravan->mGame->discardTrack(Caravan::Side::Player, track))
            return false;
        if (mCaravan->mGame->over())
            payCaravan(*mCaravan);
        return true;
    }

    bool FalloutScripts::caravanForfeit()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || !mCaravan->mGame || mCaravan->mStage != CaravanStage::Game)
            return false;
        mCaravan->mGame->forfeit();
        mCaravan->mForfeited = true;
        payCaravan(*mCaravan);
        return true;
    }

    FalloutScripts::CaravanOpponentMove FalloutScripts::caravanOpponentMove()
    {
        CaravanOpponentMove made;
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan || !mCaravan->mGame || mCaravan->mStage != CaravanStage::Game)
            return made;
        Caravan& game = *mCaravan->mGame;
        if (game.over() || game.turn() != Caravan::Side::Opponent)
            return made;
        // The move is chosen without any chance, so the card it uses can be read before it is made
        const Caravan::Move move = game.chooseOpponentMove();
        const std::vector<Caravan::Card>& hand = game.hand(Caravan::Side::Opponent);
        if (move.mHand >= 0 && move.mHand < static_cast<int>(hand.size()))
            {
            const Caravan::Card& used = hand[static_cast<std::size_t>(move.mHand)];
            made.mCard = boardCard(used, caravanFace(used.mForm));
        }
        game.opponentMove();
        made.mMade = true;
        made.mType = static_cast<int>(move.mType);
        made.mTrack = move.mTrack;
        made.mRow = move.mRow;
        if (game.over())
            payCaravan(*mCaravan);
        return made;
    }

    void FalloutScripts::payCaravan(CaravanSession& session)
    {
        if (session.mStage == CaravanStage::Results || !session.mGame)
            return;
        session.mStage = CaravanStage::Results;
        session.mWon = session.mGame->playerWon();
        const int pot = session.mStake ? session.mStake->pot() : 0;
        // The result code adds the pot to the money won or lost and one to the matches won or lost, and one to the Pip-Boy
        // statistic "Caravan games won" or "lost" (0x740980, stats 0x25 and 0x26; Mac round 12AV S2)
        mCaravanRecord.add(session.mWon, pot);
        mProgress.modifyMiscStat(session.mWon ? caravanGamesWonStat : caravanGamesLostStat, 1);
        if (pot <= 0)
            return;
        // The side that lost pays the pot: its caravan money first, item by item, and the rest in caps (0x74CC70)
        const FalloutScript::FormId payer = session.mWon ? session.mOpponent : mPlayer;
        const FalloutScript::FormId receiver = session.mWon ? mPlayer : session.mOpponent;
        const Money money = moneyOf(mActors, mStore, payer);
        const CaravanPayment payment = FalloutScript::Casino::settleCaravan(pot, money.mItems, money.mCaps);
        for (const auto& [form, count] : payment.mItems)
        {
            if (payer != 0)
                mActors.removeItem(payer, form, count);
            if (receiver != 0)
                mActors.addItem(receiver, form, count);
        }
        if (payer != 0 && payment.mPayerCaps > 0)
            mActors.removeItem(payer, capsNumber, payment.mPayerCaps);
        if (receiver != 0 && payment.mReceiverCaps > 0)
            mActors.addItem(receiver, capsNumber, payment.mReceiverCaps);
    }

    void FalloutScripts::closeCaravan()
    {
        const std::lock_guard lock(mCaravanMutex);
        if (!mCaravan)
            return;
        // A match that is not over when the player gets up is a forfeit, which is a loss (0x74A270)
        if (mCaravan->mGame && mCaravan->mStage == CaravanStage::Game)
        {
            mCaravan->mGame->forfeit();
            mCaravan->mForfeited = true;
            payCaravan(*mCaravan);
        }
        mCaravan.reset();
    }
}
