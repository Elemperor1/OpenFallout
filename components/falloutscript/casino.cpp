#include "casino.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace FalloutScript::Casino
{
    Rules Rules::fromRecord(float reshuffleShare, float blackjackPayout, const std::array<std::uint32_t, 7>& slotStops,
        std::uint32_t decks, std::uint32_t maxWinnings, std::uint32_t flags)
    {
        Rules rules;
        if (reshuffleShare > 0.f)
            rules.mReshuffleShare = reshuffleShare;
        if (blackjackPayout > 0.f)
            rules.mBlackjackPayout = blackjackPayout;
        if (decks > 0)
            rules.mDecks = static_cast<int>(std::min<std::uint32_t>(decks, 64));
        rules.mMaxWinnings = static_cast<int>(std::min<std::uint32_t>(maxWinnings, 0x7fffffff));
        rules.mDealerStandsOnSoft17 = (flags & 1) != 0;
        for (std::size_t i = 0; i < slotStops.size(); ++i)
            rules.mSlotStops[i] = static_cast<int>(std::min<std::uint32_t>(slotStops[i], 0x7fffffff));
        return rules;
    }

    namespace
    {
        // The steps of blackjack: the bet below 10 goes by 1, below 40 by 5, below 100 by 10, below 400 by 25, below
        // 1500 by 100 and above by 500 (0x733FF0). Going down the step is the one of the bet itself (at 10 it is 1, at
        // 40 it is 5, and so on), so that the two walk the same stairs.
        int blackjackUp(int bet)
        {
            if (bet < 10)
                return 1;
            if (bet < 40)
                return 5;
            if (bet < 100)
                return 10;
            if (bet < 400)
                return 25;
            if (bet < 1500)
                return 100;
            return 500;
        }

        int blackjackDown(int bet)
        {
            if (bet <= 10)
                return 1;
            if (bet <= 40)
                return 5;
            if (bet <= 100)
                return 10;
            if (bet <= 400)
                return 25;
            if (bet <= 1500)
                return 100;
            return 500;
        }

        // Roulette and slots: 1 below 10, 5 below 40, 10 below 100, 25 below 500 and 100 above (0x7BC870, 0x7C2460)
        int tableUp(int bet)
        {
            if (bet < 10)
                return 1;
            if (bet < 40)
                return 5;
            if (bet < 100)
                return 10;
            if (bet < 500)
                return 25;
            return 100;
        }

        int tableDown(int bet)
        {
            if (bet <= 10)
                return 1;
            if (bet <= 40)
                return 5;
            if (bet <= 100)
                return 10;
            if (bet <= 500)
                return 25;
            return 100;
        }
    }

    int highestBet(int maxBet, int chips)
    {
        return std::max(0, std::min(maxBet, chips));
    }

    int increasedBet(Table table, int bet, int minBet, int maxBet, int chips)
    {
        const int step = table == Table::Blackjack ? blackjackUp(bet) : tableUp(bet);
        // To the next multiple of the step above the bet
        int result = (bet / step + 1) * step;
        result = std::min(result, highestBet(maxBet, chips));
        // The least is kept even when the chips are fewer (the games do not let the menu open then)
        return std::max(result, minBet);
    }

    int decreasedBet(Table table, int bet, int minBet)
    {
        const int step = table == Table::Blackjack ? blackjackDown(bet) : tableDown(bet);
        // To the previous multiple of the step below the bet
        const int result = ((bet - 1) / step) * step;
        return std::max(result, minBet);
    }
}

namespace FalloutScript
{
    template <>
    struct SaveAccess<Casino::Ledger>
    {
        static void write(Casino::Ledger& ledger, SaveWriter& out)
        {
            const std::lock_guard lock(ledger.mMutex);
            out.count(ledger.mEntries.size());
            for (const auto& [casino, entry] : ledger.mEntries)
            {
                out.form(casino);
                out.i32(entry.mEarnings);
                out.i32(entry.mLevel);
            }
        }

        static void read(Casino::Ledger& ledger, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Casino::Ledger::Entry> entries;
            const std::size_t count = in.count(12);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> casino = in.form();
                Casino::Ledger::Entry entry;
                entry.mEarnings = in.i32();
                entry.mLevel = std::clamp<std::int32_t>(in.i32(), 0, 4);
                // A casino of a plugin that is no longer loaded is forgotten
                if (casino)
                    entries[*casino] = entry;
            }
            const std::lock_guard lock(ledger.mMutex);
            ledger.mEntries = std::move(entries);
        }
    };
}

namespace FalloutScript
{
    template <>
    struct SaveAccess<Casino::CaravanRecord>
    {
        static void write(Casino::CaravanRecord& record, SaveWriter& out)
        {
            const std::lock_guard lock(record.mMutex);
            out.i32(record.mTotals.mMoneyWon);
            out.i32(record.mTotals.mMoneyLost);
            out.i32(record.mTotals.mGamesWon);
            out.i32(record.mTotals.mGamesLost);
            out.i32(record.mTotals.mBiggestAnte);
        }

        static void read(Casino::CaravanRecord& record, SaveReader& in, std::uint32_t)
        {
            Casino::CaravanTotals totals;
            totals.mMoneyWon = std::max<std::int32_t>(in.i32(), 0);
            totals.mMoneyLost = std::max<std::int32_t>(in.i32(), 0);
            totals.mGamesWon = std::max<std::int32_t>(in.i32(), 0);
            totals.mGamesLost = std::max<std::int32_t>(in.i32(), 0);
            totals.mBiggestAnte = std::max<std::int32_t>(in.i32(), 0);
            const std::lock_guard lock(record.mMutex);
            record.mTotals = totals;
        }
    };
}

namespace FalloutScript::Casino
{
    CaravanTotals CaravanRecord::add(bool won, int pot)
    {
        const std::lock_guard lock(mMutex);
        pot = std::max(pot, 0);
        // The totals do not wrap around, as the winnings of a casino do not (Ledger::add)
        const auto sum = [](int total, int more) {
            return static_cast<int>(std::min<long long>(static_cast<long long>(total) + more, std::numeric_limits<int>::max()));
        };
        if (won)
        {
            mTotals.mMoneyWon = sum(mTotals.mMoneyWon, pot);
            mTotals.mGamesWon = sum(mTotals.mGamesWon, 1);
            mTotals.mBiggestAnte = std::max(mTotals.mBiggestAnte, pot);
        }
        else
        {
            mTotals.mMoneyLost = sum(mTotals.mMoneyLost, pot);
            mTotals.mGamesLost = sum(mTotals.mGamesLost, 1);
        }
        return mTotals;
    }

    CaravanTotals CaravanRecord::totals() const
    {
        const std::lock_guard lock(mMutex);
        return mTotals;
    }

    void CaravanRecord::reset()
    {
        const std::lock_guard lock(mMutex);
        mTotals = CaravanTotals();
    }

    void writeCaravanRecord(CaravanRecord& record, SaveWriter& out)
    {
        SaveAccess<CaravanRecord>::write(record, out);
    }

    void readCaravanRecord(CaravanRecord& record, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<CaravanRecord>::read(record, in, version);
    }

    int winningsLevel(int earnings, int maxWinnings)
    {
        int level = 0;
        for (int step = 1; step <= 4; ++step)
            if (static_cast<double>(earnings) >= static_cast<double>(maxWinnings) * step * 0.25)
                level = step;
        return level;
    }

    int Ledger::earnings(FormId casino) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(casino);
        return found == mEntries.end() ? 0 : found->second.mEarnings;
    }

    bool Ledger::known(FormId casino) const
    {
        const std::lock_guard lock(mMutex);
        return mEntries.find(casino) != mEntries.end();
    }

    void Ledger::open(FormId casino)
    {
        if (casino == 0)
            return;
        const std::lock_guard lock(mMutex);
        mEntries.try_emplace(casino);
    }

    void Ledger::add(FormId casino, int net)
    {
        if (casino == 0)
            return;
        const std::lock_guard lock(mMutex);
        int& won = mEntries.try_emplace(casino).first->second.mEarnings;
        // Winnings do not wrap around
        const long long sum = static_cast<long long>(won) + net;
        won = static_cast<int>(
            std::clamp<long long>(sum, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    }

    int Ledger::level(FormId casino, int maxWinnings) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(casino);
        return found == mEntries.end() ? 0 : winningsLevel(found->second.mEarnings, maxWinnings);
    }

    int Ledger::storedLevel(FormId casino) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(casino);
        return found == mEntries.end() ? 0 : found->second.mLevel;
    }

    bool Ledger::raiseLevel(FormId casino, int maxWinnings)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(casino);
        if (found == mEntries.end())
            return false;
        Entry& entry = found->second;
        bool raised = false;
        for (int step = 1; step <= 4; ++step)
        {
            if (step <= entry.mLevel)
                raised = false;
            else if (static_cast<double>(entry.mEarnings) >= static_cast<double>(maxWinnings) * step * 0.25)
            {
                entry.mLevel = step;
                raised = true;
            }
        }
        return raised;
    }

    bool Ledger::setLevel(FormId casino, int level, int maxWinnings)
    {
        if (level < 0 || level > 4)
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(casino);
        if (found == mEntries.end())
            return false;
        found->second.mLevel = level;
        found->second.mEarnings = static_cast<int>(level * 0.25 * static_cast<double>(maxWinnings));
        return true;
    }

    void Ledger::reset()
    {
        const std::lock_guard lock(mMutex);
        mEntries.clear();
    }

    void writeLedger(Ledger& ledger, SaveWriter& out)
    {
        SaveAccess<Ledger>::write(ledger, out);
    }

    void readLedger(Ledger& ledger, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Ledger>::read(ledger, in, version);
    }
}
