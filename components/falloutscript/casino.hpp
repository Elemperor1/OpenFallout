#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CASINO_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CASINO_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>

#include "saveaccess.hpp"
#include "savestate.hpp"
#include "script.hpp"

namespace FalloutScript::Casino
{
    using FalloutScript::FormId;

    /// A whole number between the two given, both included. The games ask the engine for one; the tables here ask this
    /// so that a test can name the draw.
    using Random = std::function<int(int low, int high)>;

    /// What a casino record (CSNO) says about how its games are played, with the defaults the executable uses for a
    /// number that is 0 (FalloutNV.exe, the menu creators 0x733630, 0x7BBE20 and 0x7C0A40 read them from the record).
    struct Rules
    {
        /// The share of the shoe that is left when the cards are shuffled again (the record says 0: a quarter)
        float mReshuffleShare = 0.25f;
        /// What a blackjack pays on top of the bet (the record says 0: one and a half)
        float mBlackjackPayout = 1.5f;
        int mDecks = 1;
        /// The winnings of the player in the casino after which it plays no more with the player (0: no limit is
        /// kept here, the record's number is what the games compare with)
        int mMaxWinnings = 0;
        /// The dealer stands on a soft 17 (the record's flag 1)
        bool mDealerStandsOnSoft17 = false;
        /// How many stops each of the seven symbols of the slot machine has on a reel (they add up to 14)
        std::array<int, 7> mSlotStops{};

        /// The record's numbers (a number of 0 becomes the default)
        static Rules fromRecord(float reshuffleShare, float blackjackPayout,
            const std::array<std::uint32_t, 7>& slotStops, std::uint32_t decks, std::uint32_t maxWinnings,
            std::uint32_t flags);
    };

    /// How much the bet goes up or down by a step. Blackjack has its own steps; the roulette and the slot machine share
    /// theirs. The bet goes to the next (or the previous) multiple of the step, is never more than the most the table
    /// takes or the chips of the player, and never less than the least.
    enum class Table
    {
        Blackjack,
        Roulette, // and the slot machine
    };

    int increasedBet(Table table, int bet, int minBet, int maxBet, int chips);
    int decreasedBet(Table table, int bet, int minBet);
    /// The most that can be bet: the most the table takes, or the chips when there are fewer
    int highestBet(int maxBet, int chips);

    /// The level of the winnings: the highest of the four steps (a quarter, a half, three quarters and all of the most
    /// the casino lets the player win) that they reach, 0 below the first. The executable compares as floating point,
    /// with no rounding (0x5DEC30); for a most of 0 every winning of 0 or more is on the fourth step.
    int winningsLevel(int earnings, int maxWinnings);

    /// What the player has won or lost in each casino (the entry the menus of the games make the first time they are
    /// opened there: the form of the casino, the winnings and the level they last reached, FalloutNV.exe 0x733F50). A
    /// casino that has played no game with the player has none. It can be used from any thread.
    class Ledger
    {
        template <class T>
        friend struct FalloutScript::SaveAccess;

    public:
        /// The winnings in the casino, negative for losses; 0 for a casino with no entry
        int earnings(FormId casino) const;
        /// Whether the player has played in the casino
        bool known(FormId casino) const;
        /// Makes the entry (with nothing won) when there is none
        void open(FormId casino);
        /// Adds to the winnings of the casino (the entry is made when there is none)
        void add(FormId casino, int net);
        /// The level the winnings are on now (GetCasinoWinningsLevel and the condition GetCasinoWinningStage tell this,
        /// not the stored one); 0 for a casino with no entry
        int level(FormId casino, int maxWinnings) const;
        /// The level the games last saw the winnings reach (the word of the entry): a table closes when it is raised
        int storedLevel(FormId casino) const;
        /// What a table does after a round: the stored level goes up to the highest step that the winnings have
        /// reached, when that is higher (0x736B56). True when it went up, and then the table closes.
        bool raiseLevel(FormId casino, int maxWinnings);
        /// SetCasinoWinningsLevel: for a level from 0 to 4 and a casino with an entry, the stored level is that and the
        /// winnings are what makes it, level x a quarter of the most. False when nothing was set.
        bool setLevel(FormId casino, int level, int maxWinnings);
        void reset();

    private:
        mutable std::mutex mMutex;
        struct Entry
        {
            int mEarnings = 0;
            int mLevel = 0;
        };
        std::map<FormId, Entry> mEntries;
    };

    constexpr std::uint32_t ledgerSectionVersion = 1;
    void writeLedger(Ledger& ledger, SaveWriter& out);
    void readLedger(Ledger& ledger, SaveReader& in, std::uint32_t version);

    /// What the player has won and lost at Caravan, which the result screen shows: the caps of the pots won and lost,
    /// the matches won and lost, and the biggest pot won (fields of the player's data in FalloutNV.exe: 0x61C, 0x620,
    /// 0x624, 0x628 and 0x62C, set by the result code at 0x740980; Mac round 12AV S2)
    struct CaravanTotals
    {
        int mMoneyWon = 0;
        int mMoneyLost = 0;
        int mGamesWon = 0;
        int mGamesLost = 0;
        int mBiggestAnte = 0;
    };

    /// The totals of the player's matches. It can be used from any thread.
    class CaravanRecord
    {
        template <class T>
        friend struct FalloutScript::SaveAccess;

    public:
        /// A match is over with `pot` at stake: a win adds the pot to the money won, one to the matches won and raises
        /// the biggest ante to the pot when that is more; a loss adds the pot to the money lost and one to the matches
        /// lost. The totals after it.
        CaravanTotals add(bool won, int pot);
        CaravanTotals totals() const;
        void reset();

    private:
        mutable std::mutex mMutex;
        CaravanTotals mTotals;
    };

    constexpr std::uint32_t caravanRecordSectionVersion = 1;
    void writeCaravanRecord(CaravanRecord& record, SaveWriter& out);
    void readCaravanRecord(CaravanRecord& record, SaveReader& in, std::uint32_t version);
}

#endif
