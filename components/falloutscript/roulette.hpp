#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ROULETTE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ROULETTE_H

#include <array>
#include <vector>

#include "casino.hpp"

namespace FalloutScript::Casino
{
    /// One thing that can be bet on at the table of the American roulette of the games (FalloutNV.exe, the menu
    /// 0x43A, creator 0x7BBE20). The pockets are the numbers 1 to 36, 0, and 37 for the double zero.
    struct RouletteArea
    {
        enum class Kind
        {
            Straight,
            Split,
            Street,
            Corner,
            SixLine,
            FiveNumber,
            Dozen,
            Column,
            Half,
            Even,
            Odd,
            Red,
            Black,
        };

        Kind mKind = Kind::Straight;
        std::vector<int> mNumbers;
        /// What it pays on top of the stake: 35 for a number, 17 for two, 11 for three, 8 for four, 6 for the five of
        /// the zeroes and the first three, 5 for six, 2 for a dozen or a column and 1 for the halves and the rest
        int mMultiplier = 0;

        bool wins(int pocket) const;
    };

    constexpr int doubleZero = 37;
    constexpr int pocketCount = 38;
    /// The pockets of the wheel in the order they are on it (the double zero is 37)
    const std::array<int, pocketCount>& rouletteWheel();
    /// Whether the pocket is red (on the wheel the red ones are at the even places) and black (at the odd ones); the
    /// zeroes are neither
    bool pocketIsRed(int pocket);
    bool pocketIsBlack(int pocket);

    /// Everything that can be bet on: the 147 inside bets of the executable's table (the 38 numbers, the 60 pairs, the
    /// 15 rows of three, the 22 squares of four, the 11 lines of six and the five numbers of the start; 0x7B9C40), then
    /// the twelve outside bets (three dozens, the two halves, even, odd, red, black and three columns). The order is
    /// the one of this port. The numbers of the outside bets are the ones its payout loop matches, which let the zeroes
    /// win some of them (RouletteArea::wins).
    const std::vector<RouletteArea>& rouletteAreas();

    /// The roulette game of a player. The bets are placed one amount at a time; a spin pays them and clears the table.
    class Roulette
    {
    public:
        static constexpr int maxBets = 10;

        /// `minBet` is the least one amount, `maxTotal` the most that is bet in one spin (not on one area) and `chips`
        /// what the player has. `luck` is the player's Luck.
        Roulette(int minBet, int maxTotal, int chips, int luck, Random random);

        int amount() const { return mAmount; }
        int total() const { return mTotal; }
        int chips() const { return mChips; }
        int betCount() const { return static_cast<int>(mBets.size()); }
        int stake(int area) const;
        int minBet() const { return mMinBet; }
        int maxTotal() const { return mMaxTotal; }

        /// The amount to bet goes up or down a step (the least is the least bet, the most is what the table takes in
        /// one spin and what the player has)
        void increaseAmount();
        void decreaseAmount();
        /// The player has not the chips for the least bet any more (the game closes the table)
        bool broke() const { return mChips < mMinBet; }
        /// Whether an amount can be put on the area now: it exists, the table has room (ten different bets, and the
        /// most for one spin) and the player has the chips for the total
        bool canPlace(int area) const;
        /// Puts the amount on the area. When the amount would take the total past the most, the amount is what is left
        /// of it, as the game does. False when it cannot be placed, and so when the area has a stake already.
        bool place(int area);
        /// Takes the whole bet of the area off the table (not a step of the amount). False when there is none.
        bool remove(int area);

        struct Result
        {
            int mPocket = 0;
            /// The turn of the wheel: which of the three the picture shows
            int mSpinType = 0;
            /// What the bets that won are paid (stake included), and what the player has more (or less) than before
            int mPayout = 0;
            int mNet = 0;
            /// The Luck of the player is above 5 (the words of a lucky win) or below (those of an unlucky loss); it
            /// does not change where the ball stops
            bool mLucky = false;
            bool mUnlucky = false;
        };

        /// Spins the wheel with the bets that are on the table (at least one). The chips change by the net, the table
        /// is cleared and the amount to bet goes back to the least bet.
        bool canSpin() const { return !mBets.empty(); }
        Result spin();

    private:
        struct Bet
        {
            int mArea;
            int mStake;
        };

        int mMinBet;
        int mMaxTotal;
        int mChips;
        int mLuck;
        int mAmount;
        int mTotal = 0;
        Random mRandom;
        std::vector<Bet> mBets;
    };
}

#endif
