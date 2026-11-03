#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SLOTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SLOTS_H

#include <array>
#include <vector>

#include "casino.hpp"

namespace FalloutScript::Casino
{
    /// The slot machine of the casinos (FalloutNV.exe, the menu 0x438, creator 0x7C0A40; the rules were read in the Mac
    /// rounds 12AO B7 and 12AQ B7). Three reels; a stop is a number from 0 to 13 on a strip: the even numbers 0 to 12
    /// are the seven symbols (12 is the special one, the last of the record's seven) and the odd ones are the blank
    /// after a symbol. The casino's record gives, for each symbol, how many times it is on the strip.
    class SlotMachine
    {
    public:
        static constexpr int reels = 3;
        static constexpr int symbols = 7;
        /// A stop number that is the special symbol
        static constexpr int special = 12;

        /// What the three stops come to (0x7C5760)
        enum class WinClass
        {
            /// Nothing: no two alike, or a pair that pays nothing
            Lose = 0,
            /// Two reels alike on a symbol: it pays nothing
            Pair = 1,
            /// One special symbol: twice the bet
            OneSpecial = 4,
            /// Two special symbols: five times the bet
            TwoSpecial = 5,
            /// Three alike on a symbol: what the first reel's symbol pays
            ThreeAlike = 6,
        };

        struct Result
        {
            std::array<int, reels> mStops{};
            WinClass mClass = WinClass::Lose;
            /// What it pays on top of the stake: the bet times this when the player wins (0 when it loses)
            int mMultiplier = 0;
            /// What the player has more (or less) than before
            int mNet = 0;
            /// Luck turned the stops to the player's good (the words of a lucky win)
            bool mLucky = false;
        };

        /// `minBet` and `maxBet` come from the script, `chips` is what the player has and `luck` its Luck
        SlotMachine(const Rules& rules, int minBet, int maxBet, int chips, int luck, Random random);

        /// False when the seven counts of the record add up to less than 14: the strip cannot be made and the machine
        /// does not open (the game says so and closes it)
        bool valid() const { return mValid; }
        int bet() const { return mBet; }
        int chips() const { return mChips; }
        int minBet() const { return mMinBet; }
        int maxBet() const { return mMaxBet; }
        /// The strip of 28 stops
        const std::vector<int>& strip() const { return mStrip; }
        /// The stops the reels show now (before the first pull: 0, 2 and 4)
        const std::array<int, reels>& stops() const { return mStops; }

        void increaseBet();
        void decreaseBet();
        void setHighestBet();
        bool canSpin() const { return mValid && mBet >= mMinBet && mBet <= mChips; }
        Result spin();
        /// The player has not the chips for the least bet any more
        bool broke() const { return mChips < mMinBet; }

        /// What a win of three alike pays on top of the stake, by the first reel's stop: 10, 20, 30, 40, 60, 100 for
        /// the symbols from the first to the sixth and 10 for the special one; 0 for a blank
        static int threeAlikePays(int stop);
        static WinClass classOf(const std::array<int, reels>& stops);

    private:
        void buildStrip();
        WinClass pickStops();
        /// The Luck rig (0x7C58F0); true when it turned the stops towards a win
        bool rig(WinClass winClass);

        Rules mRules;
        int mMinBet;
        int mMaxBet;
        int mChips;
        int mLuck;
        int mBet;
        Random mRandom;
        bool mValid = true;
        int mSymbols = 0;
        std::vector<int> mStrip;
        std::array<int, reels> mStops{ 0, 2, 4 };
    };
}

#endif
