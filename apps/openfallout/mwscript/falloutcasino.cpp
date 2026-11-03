#include "falloutscripts.hpp"

#include <algorithm>
#include <random>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcsno.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;
        using namespace FalloutScript::Casino;

        /// The form of a record of the files, as the number the scripts use (nothing for a form without a file)
        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        const char* gameName(FalloutScripts::CasinoGame game)
        {
            switch (game)
            {
                case FalloutScripts::CasinoGame::Blackjack:
                    return "blackjack";
                case FalloutScripts::CasinoGame::Roulette:
                    return "roulette";
                case FalloutScripts::CasinoGame::Slots:
                    return "slots";
            }
            return "";
        }

        /// A path of a record under the folder of its kind ("meshes", "textures"), as the virtual file system has it
        std::string filePath(std::string_view folder, const std::string& path)
        {
            if (path.empty())
                return {};
            std::string full = VFS::Path::normalizeFilename(path);
            const std::string prefix = std::string(folder) + "/";
            if (!full.starts_with(prefix))
                full = prefix + full;
            return full;
        }

        /// The miscellaneous statistic that a pull of the slot machine adds one to (FalloutNV.exe 0x7C2460)
        constexpr int slotsGamesPlayedStat = 0x2A;

        // The word for a chip in the notices: "1 Chip", "10 Chips" (sCasinoChipText, and sPlural for more than one)
        std::string chipsText(int count, const std::string& chip, const std::string& plural)
        {
            return std::to_string(count) + " " + chip + (count == 1 ? std::string() : plural);
        }
    }

    bool FalloutScripts::openCasino(CasinoGame game, FalloutScript::FormId casino, int minBet, int maxBet, int needed)
    {
        const ESM::FormId form = ESM::FormId::fromUint32(casino);
        const ESM4::Casino* record = form.isZeroOrUnset() ? nullptr : mStore.get<ESM4::Casino>().search(form);
        if (record == nullptr)
        {
            Log(Debug::Warning) << "Casino: the " << gameName(game) << " menu is asked for " << std::hex << casino
                                << std::dec << ", which is not a casino of the files";
            return false;
        }
        const Rules rules = Rules::fromRecord(record->mData.mDecksPercentBeforeShuffle,
            record->mData.mBlackjackPayoutRatio, record->mData.mSlotReelStops, record->mData.mNumberOfDecks,
            record->mData.mMaxWinnings, record->mData.mFlags);
        const FalloutScript::FormId currency = numberOf(ESM::FormId::fromUint32(record->mData.mCurrency));
        minBet = std::max(1, minBet);
        maxBet = std::max(minBet, maxBet);

        // The games make the entry of the casino for the player the first time, before they say no
        mCasinoLedger.open(casino);
        const int earnings = mCasinoLedger.earnings(casino);
        const int chips = currency != 0 ? mActors.itemCount(mPlayer, currency) : 0;
        const auto refuse = [this](const char* setting, const char* fallback) {
            const std::string text = gameSettingText(setting);
            showNotice(text.empty() ? fallback : text, 0);
            return false;
        };
        // The casino plays with a player no more once it has won that much from it (the winnings are the casino's
        // losses: the number of the record is what the player may take out)
        if (rules.mMaxWinnings > 0 && earnings >= rules.mMaxWinnings)
            return refuse("sGamblingBannedText", "The casino will not play with you any more.");
        if (chips < minBet)
            return refuse("sGamblingBrokeText", "You do not have enough chips.");
        if (needed > 0 && earnings < needed)
            return refuse("sGamblingMinWinText", "You have not won enough to play here.");

        CasinoSession session;
        session.mScreen.mGame = game;
        session.mScreen.mCasino = casino;
        session.mScreen.mName = record->mFullName;
        session.mScreen.mMinBet = minBet;
        session.mScreen.mMaxBet = maxBet;
        session.mScreen.mEarnings = earnings;
        session.mScreen.mStartChips = chips;
        session.mCurrency = currency;
        session.mMaxWinnings = rules.mMaxWinnings;
        session.mWinningsQuest = numberOf(ESM::FormId::fromUint32(record->mData.mWinningsQuest));
        const int luck = static_cast<int>(mActorValues.current(mPlayer, AV::luck));
        {
            const std::lock_guard lock(mCasinoMutex);
            session.mScreen.mSerial = ++mCasinoSerial;
            if (game == CasinoGame::Blackjack)
            {
                session.mBlackjack.emplace(rules, minBet, maxBet, chips, luck,
                    [this](int low, int high) { return std::uniform_int_distribution<int>(low, high)(mCasinoRandom); });
            }
            if (game == CasinoGame::Slots)
            {
                session.mSlots.emplace(rules, minBet, maxBet, chips, luck,
                    [this](int low, int high) { return std::uniform_int_distribution<int>(low, high)(mCasinoRandom); });
                // Seven counts that add up to less than 14 are an error of the casino's record: the machine does not
                // open (FalloutNV.exe 0x7C16B0)
                if (!session.mSlots->valid())
                {
                    Log(Debug::Warning) << "Casino: the slot machine of " << record->mEditorId
                                        << " has fewer than 14 stops on its reels and does not open";
                    return false;
                }
            }
            if (game == CasinoGame::Roulette)
            {
                // The table draws with the one random engine of the casinos; it is only asked while the lock is held
                session.mRoulette.emplace(minBet, maxBet, chips, luck,
                    [this](int low, int high) { return std::uniform_int_distribution<int>(low, high)(mCasinoRandom); });
            }
            mCasino = std::move(session);
        }
        return true;
    }

    std::optional<FalloutScripts::CasinoScreen> FalloutScripts::casinoScreen()
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino)
            return std::nullopt;
        CasinoScreen screen = mCasino->mScreen;
        screen.mEarnings = mCasinoLedger.earnings(screen.mCasino);
        return screen;
    }

    std::optional<FalloutScripts::RouletteView> FalloutScripts::rouletteView()
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mRoulette)
            return std::nullopt;
        const Roulette& table = *mCasino->mRoulette;
        RouletteView view;
        view.mAmount = table.amount();
        view.mTotal = table.total();
        view.mChips = table.chips();
        view.mBets = table.betCount();
        view.mMinBet = table.minBet();
        view.mMaxTotal = table.maxTotal();
        view.mCanSpin = table.canSpin();
        view.mEarnings = mCasinoLedger.earnings(mCasino->mScreen.mCasino);
        view.mBroke = table.broke();
        view.mCloses = mCasino->mNewLevel || view.mBroke;
        const int count = static_cast<int>(rouletteAreas().size());
        for (int area = 0; area < count; ++area)
            if (const int stake = table.stake(area); stake > 0)
                view.mStakes.emplace_back(area, stake);
        return view;
    }

    std::optional<FalloutScripts::SlotsView> FalloutScripts::slotsView()
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mSlots)
            return std::nullopt;
        const SlotMachine& machine = *mCasino->mSlots;
        SlotsView view;
        view.mBet = machine.bet();
        view.mChips = machine.chips();
        view.mMinBet = machine.minBet();
        view.mMaxBet = machine.maxBet();
        view.mEarnings = mCasinoLedger.earnings(mCasino->mScreen.mCasino);
        view.mStops = machine.stops();
        view.mCanSpin = machine.canSpin();
        // A machine closes when the player has no chips left (FalloutNV.exe, Mac round 12AR D1)
        view.mBroke = machine.chips() == 0;
        view.mCloses = mCasino->mNewLevel || view.mBroke;
        // The machine of the screen and the pictures on the reels are the record's (FalloutNV.exe 0x7C1490: the eighth
        // model, and the seven pictures of the record's symbols under Textures)
        if (const ESM4::Casino* record
            = mStore.get<ESM4::Casino>().search(ESM::FormId::fromUint32(mCasino->mScreen.mCasino)))
        {
            if (record->mModels.size() > 7)
                view.mScene = filePath("meshes", record->mModels[7]);
            for (std::size_t symbol = 0; symbol < view.mSymbols.size() && symbol < record->mIcons.size(); ++symbol)
                view.mSymbols[symbol] = filePath("textures", record->mIcons[symbol]);
        }
        return view;
    }

    bool FalloutScripts::slotsMove(SlotsMove move)
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mSlots)
            return false;
        SlotMachine& machine = *mCasino->mSlots;
        const int before = machine.bet();
        switch (move)
        {
            case SlotsMove::Increase:
                machine.increaseBet();
                break;
            case SlotsMove::Decrease:
                machine.decreaseBet();
                break;
            case SlotsMove::Highest:
                machine.setHighestBet();
                break;
        }
        return machine.bet() != before;
    }

    std::optional<SlotMachine::Result> FalloutScripts::slotsSpin()
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mSlots || !mCasino->mSlots->canSpin() || mCasino->mNewLevel)
            return std::nullopt;
        const SlotMachine::Result result = mCasino->mSlots->spin();
        // The pull counts as a game played in the Pip-Boy's miscellaneous statistics ("Slots Games Played", 0x2A)
        mProgress.modifyMiscStat(slotsGamesPlayedStat, 1);
        // What the pull came to is in the winnings of the casino at once. The level is looked at only after a pull that
        // won, and a machine that finds the winnings on a new quarter of the most closes (0x7C3367)
        mCasinoLedger.add(mCasino->mScreen.mCasino, result.mNet);
        if (result.mNet > 0 && mCasino->mMaxWinnings > 0
            && mCasinoLedger.raiseLevel(mCasino->mScreen.mCasino, mCasino->mMaxWinnings))
            mCasino->mNewLevel = true;
        return result;
    }

    std::optional<FalloutScripts::BlackjackView> FalloutScripts::blackjackView()
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mBlackjack)
            return std::nullopt;
        const Blackjack& table = *mCasino->mBlackjack;
        BlackjackView view;
        switch (table.state())
        {
            case Blackjack::State::Betting:
                view.mState = BlackjackView::State::Betting;
                break;
            case Blackjack::State::Playing:
                view.mState = BlackjackView::State::Playing;
                break;
            case Blackjack::State::Finished:
                view.mState = BlackjackView::State::Finished;
                break;
        }
        view.mBet = table.bet();
        view.mChips = table.chips();
        view.mMinBet = table.minBet();
        view.mMaxBet = table.maxBet();
        view.mEarnings = mCasinoLedger.earnings(mCasino->mScreen.mCasino);
        for (const Card& card : table.dealerCards())
            view.mDealer.push_back({ card.mRank, card.mSuit });
        view.mHoleHidden = table.holeCardHidden();
        for (const Blackjack::Hand& hand : table.hands())
        {
            BlackjackView::Hand shown;
            for (const Card& card : hand.mCards)
                shown.mCards.push_back({ card.mRank, card.mSuit });
            shown.mBet = hand.mBet;
            shown.mResult = static_cast<int>(hand.mResult);
            shown.mDoubled = hand.mDoubled;
            shown.mDone = hand.mDone;
            view.mHands.push_back(std::move(shown));
        }
        view.mActive = table.activeHand();
        view.mCanDeal = table.canDeal();
        view.mCanHit = table.canHit();
        view.mCanStay = table.canStay();
        view.mCanDouble = table.canDouble();
        view.mCanSplit = table.canSplit();
        view.mCanSurrender = table.canSurrender();
        view.mCanSwitch = table.canSwitch();
        view.mNet = table.net();
        view.mDealerHelped = table.dealerHelped();
        view.mPlayerHelped = table.playerHelped();
        view.mBroke = table.broke();
        view.mCloses = mCasino->mNewLevel && table.state() == Blackjack::State::Finished;
        return view;
    }

    bool FalloutScripts::blackjackMove(BlackjackMove move)
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mBlackjack)
            return false;
        Blackjack& table = *mCasino->mBlackjack;
        const int before = table.bet();
        bool done = false;
        switch (move)
        {
            case BlackjackMove::Increase:
                table.increaseBet();
                done = table.bet() != before;
                break;
            case BlackjackMove::Decrease:
                table.decreaseBet();
                done = table.bet() != before;
                break;
            case BlackjackMove::Highest:
                table.setHighestBet();
                done = table.bet() != before;
                break;
            case BlackjackMove::Deal:
                done = table.deal();
                // A new round: its result is not in the winnings yet
                if (done)
                    mCasino->mRoundSettled = false;
                break;
            case BlackjackMove::Hit:
                done = table.hit();
                break;
            case BlackjackMove::Stay:
                done = table.stay();
                break;
            case BlackjackMove::Double:
                done = table.doubleDown();
                break;
            case BlackjackMove::Split:
                done = table.split();
                break;
            case BlackjackMove::Surrender:
                done = table.surrender();
                break;
            case BlackjackMove::Switch:
                done = table.switchHands();
                break;
        }
        // What a round came to is in the winnings of the casino from the moment it has a result
        if (table.state() == Blackjack::State::Finished && !mCasino->mRoundSettled)
        {
            mCasino->mRoundSettled = true;
            mCasinoLedger.add(mCasino->mScreen.mCasino, table.net());
            // A round that takes the winnings to a new quarter of the most closes the table (0x736B56)
            if (mCasino->mMaxWinnings > 0 && mCasinoLedger.raiseLevel(mCasino->mScreen.mCasino, mCasino->mMaxWinnings))
                mCasino->mNewLevel = true;
        }
        return done;
    }

    bool FalloutScripts::rouletteAmount(bool increase)
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mRoulette)
            return false;
        const int before = mCasino->mRoulette->amount();
        if (increase)
            mCasino->mRoulette->increaseAmount();
        else
            mCasino->mRoulette->decreaseAmount();
        return mCasino->mRoulette->amount() != before;
    }

    bool FalloutScripts::roulettePlace(int area)
    {
        const std::lock_guard lock(mCasinoMutex);
        return mCasino && mCasino->mRoulette && mCasino->mRoulette->place(area);
    }

    bool FalloutScripts::rouletteRemove(int area)
    {
        const std::lock_guard lock(mCasinoMutex);
        return mCasino && mCasino->mRoulette && mCasino->mRoulette->remove(area);
    }

    std::optional<Roulette::Result> FalloutScripts::rouletteSpin()
    {
        const std::lock_guard lock(mCasinoMutex);
        if (!mCasino || !mCasino->mRoulette || !mCasino->mRoulette->canSpin())
            return std::nullopt;
        const Roulette::Result result = mCasino->mRoulette->spin();
        // What was won or lost is in the winnings of the casino from the spin on; a spin that takes them to a new
        // quarter of the most closes the table (0x7BE412)
        mCasinoLedger.add(mCasino->mScreen.mCasino, result.mNet);
        if (mCasino->mMaxWinnings > 0 && mCasinoLedger.raiseLevel(mCasino->mScreen.mCasino, mCasino->mMaxWinnings))
            mCasino->mNewLevel = true;
        return result;
    }

    void FalloutScripts::closeCasino()
    {
        CasinoSession session;
        int chips = 0;
        {
            const std::lock_guard lock(mCasinoMutex);
            if (!mCasino)
                return;
            session = std::move(*mCasino);
            mCasino.reset();
        }
        if (session.mRoulette)
        {
            chips = session.mRoulette->chips();
        }
        else if (session.mSlots)
        {
            chips = session.mSlots->chips();
        }
        else if (session.mBlackjack)
        {
            // A round that is not over when the player gets up is lost
            const bool playing = session.mBlackjack->state() == Blackjack::State::Playing;
            session.mBlackjack->abandon();
            if (playing)
                mCasinoLedger.add(session.mScreen.mCasino, session.mBlackjack->net());
            chips = session.mBlackjack->chips();
        }
        else
        {
            return;
        }
        // The casino's quest starts when a table closes with the winnings on a new quarter of the most (0x735330)
        if (session.mNewLevel && session.mWinningsQuest != 0)
            mQuests.start(session.mWinningsQuest);
        // The chips move when the player gets up: those it has more or less than it sat down with
        const int difference = chips - session.mScreen.mStartChips;
        if (difference == 0 || session.mCurrency == 0)
            return;
        std::string chip = gameSettingText("sCasinoChipText");
        if (chip.empty())
            chip = "Chip";
        std::string plural = gameSettingText("sPlural");
        if (plural.empty())
            plural = "s";
        if (difference < 0)
        {
            // What the player lost is taken from its pack (it can have less now than when it sat down)
            const int taken = mActors.removeItem(mPlayer, session.mCurrency, -difference);
            if (taken > 0)
            {
                std::string word = gameSettingText("sRemoveItemfromInventory");
                if (word.empty())
                    word = "removed";
                tellPlayer(chipsText(taken, chip, plural) + " " + word);
            }
            return;
        }
        // The casino pays no more to a player who has won what it is ready to lose
        if (session.mMaxWinnings > 0 && mCasinoLedger.earnings(session.mScreen.mCasino) >= session.mMaxWinnings)
        {
            const std::string text = gameSettingText("sGamblingBannedText");
            showNotice(text.empty() ? "The casino will not play with you any more." : text, 0);
        }
        mActors.addItem(mPlayer, session.mCurrency, difference);
        std::string word = gameSettingText("sAddItemtoInventory");
        if (word.empty())
            word = "added";
        tellPlayer(chipsText(difference, chip, plural) + " " + word);
    }

    int FalloutScripts::casinoWinningsLevel(FalloutScript::FormId casino)
    {
        const ESM::FormId form = ESM::FormId::fromUint32(casino);
        const ESM4::Casino* record = form.isZeroOrUnset() ? nullptr : mStore.get<ESM4::Casino>().search(form);
        // The command works out the level from the most of the casino's record, as the executable does (0x5DEC30); a
        // casino that is not a record or has no entry for the player has none
        if (record == nullptr)
            return 0;
        return mCasinoLedger.level(casino, static_cast<int>(record->mData.mMaxWinnings));
    }

    bool FalloutScripts::setCasinoWinningsLevel(FalloutScript::FormId casino, int level)
    {
        const ESM::FormId form = ESM::FormId::fromUint32(casino);
        const ESM4::Casino* record = form.isZeroOrUnset() ? nullptr : mStore.get<ESM4::Casino>().search(form);
        if (record == nullptr)
            return false;
        return mCasinoLedger.setLevel(casino, level, static_cast<int>(record->mData.mMaxWinnings));
    }
}
