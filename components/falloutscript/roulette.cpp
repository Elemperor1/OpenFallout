#include "roulette.hpp"

#include <algorithm>

namespace FalloutScript::Casino
{
    namespace
    {
        // The pockets of the wheel as FalloutNV.exe stores them (the 38 numbers at the menu's +0x3E7C); 37 is the 00
        constexpr std::array<int, pocketCount> wheel = { 14, 2, 0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3,
            24, 36, 13, 1, 37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35 };

        int placeOnWheel(int pocket)
        {
            const auto found = std::find(wheel.begin(), wheel.end(), pocket);
            return static_cast<int>(found - wheel.begin());
        }

        void add(std::vector<RouletteArea>& areas, RouletteArea::Kind kind, std::vector<int> numbers, int multiplier)
        {
            areas.push_back({ kind, std::move(numbers), multiplier });
        }

        std::vector<RouletteArea> buildAreas()
        {
            using Kind = RouletteArea::Kind;
            std::vector<RouletteArea> areas;
            // The numbers (the double zero is 37), each pays 35
            for (int pocket = 0; pocket < pocketCount; ++pocket)
                add(areas, Kind::Straight, { pocket }, 35);
            // The numbers 1 to 36 lie in twelve rows of three: 3 * row + column + 1
            for (int row = 0; row < 12; ++row)
            {
                const int first = 3 * row + 1;
                add(areas, Kind::Split, { first, first + 1 }, 17);
                add(areas, Kind::Split, { first + 1, first + 2 }, 17);
            }
            for (int row = 0; row < 11; ++row)
                for (int column = 0; column < 3; ++column)
                {
                    const int number = 3 * row + column + 1;
                    add(areas, Kind::Split, { number, number + 3 }, 17);
                }
            // The zeroes lie above the first row. Three pairs touch them: 0 and 00, 0 and 1, 00 and 3 (there is no pair
            // of 0 or 00 with the 2: it is in the threes 0-00-2 and, with the 1 and the 3, in 0-1-2 and 00-2-3)
            add(areas, Kind::Split, { 0, doubleZero }, 17);
            add(areas, Kind::Split, { 0, 1 }, 17);
            add(areas, Kind::Split, { doubleZero, 3 }, 17);
            for (int row = 0; row < 12; ++row)
                add(areas, Kind::Street, { 3 * row + 1, 3 * row + 2, 3 * row + 3 }, 11);
            add(areas, Kind::Street, { 0, 1, 2 }, 11);
            add(areas, Kind::Street, { 0, doubleZero, 2 }, 11);
            add(areas, Kind::Street, { doubleZero, 2, 3 }, 11);
            for (int row = 0; row < 11; ++row)
                for (int column = 0; column < 2; ++column)
                {
                    const int number = 3 * row + column + 1;
                    add(areas, Kind::Corner, { number, number + 1, number + 3, number + 4 }, 8);
                }
            for (int row = 0; row < 11; ++row)
            {
                const int first = 3 * row + 1;
                add(areas, Kind::SixLine, { first, first + 1, first + 2, first + 3, first + 4, first + 5 }, 5);
            }
            add(areas, Kind::FiveNumber, { 0, doubleZero, 1, 2, 3 }, 6);

            // The outside bets, as the executable tests them in its payout loop (0x7BDC6F, 0x7BECD0), with nothing that
            // leaves the zeroes out of the last seven: the dozens and the halves are by the number and miss 0 and 00,
            // even is the remainder 0 (so 0 wins it), odd the remainder 1 (so 00, which is 37, wins it), red the even
            // place on the wheel (0 sits on one) and black the odd place (00 sits on one), and the columns by the
            // remainders of n - 1, n - 2 and n on division by three (00 is in the first column and 0 in the third).
            const auto pockets = [](auto&& keep) {
                std::vector<int> numbers;
                for (int number = 0; number < pocketCount; ++number)
                    if (keep(number))
                        numbers.push_back(number);
                return numbers;
            };
            add(areas, Kind::Dozen, pockets([](int n) { return n > 0 && n < 13; }), 2);
            add(areas, Kind::Dozen, pockets([](int n) { return n > 12 && n < 25; }), 2);
            add(areas, Kind::Dozen, pockets([](int n) { return n > 24 && n < 37; }), 2);
            add(areas, Kind::Half, pockets([](int n) { return n > 0 && n < 19; }), 1);
            add(areas, Kind::Half, pockets([](int n) { return n > 18 && n < 37; }), 1);
            add(areas, Kind::Even, pockets([](int n) { return n % 2 == 0; }), 1);
            add(areas, Kind::Odd, pockets([](int n) { return n % 2 == 1; }), 1);
            add(areas, Kind::Red, pockets([](int n) { return placeOnWheel(n) % 2 == 0; }), 1);
            add(areas, Kind::Black, pockets([](int n) { return placeOnWheel(n) % 2 == 1; }), 1);
            add(areas, Kind::Column, pockets([](int n) { return (n - 1) % 3 == 0; }), 2);
            add(areas, Kind::Column, pockets([](int n) { return (n - 2) % 3 == 0; }), 2);
            add(areas, Kind::Column, pockets([](int n) { return n % 3 == 0; }), 2);
            return areas;
        }
    }

    const std::array<int, pocketCount>& rouletteWheel()
    {
        return wheel;
    }

    bool pocketIsRed(int pocket)
    {
        if (pocket < 1 || pocket > 36)
            return false;
        return placeOnWheel(pocket) % 2 == 0;
    }

    bool pocketIsBlack(int pocket)
    {
        if (pocket < 1 || pocket > 36)
            return false;
        return placeOnWheel(pocket) % 2 == 1;
    }

    bool RouletteArea::wins(int pocket) const
    {
        return std::find(mNumbers.begin(), mNumbers.end(), pocket) != mNumbers.end();
    }

    const std::vector<RouletteArea>& rouletteAreas()
    {
        static const std::vector<RouletteArea> areas = buildAreas();
        return areas;
    }

    Roulette::Roulette(int minBet, int maxTotal, int chips, int luck, Random random)
        : mMinBet(std::max(1, minBet))
        , mMaxTotal(std::max(mMinBet, maxTotal))
        , mChips(std::max(0, chips))
        , mLuck(luck)
        , mAmount(mMinBet)
        , mRandom(std::move(random))
    {
    }

    int Roulette::stake(int area) const
    {
        for (const Bet& bet : mBets)
            if (bet.mArea == area)
                return bet.mStake;
        return 0;
    }

    void Roulette::increaseAmount()
    {
        mAmount = increasedBet(Table::Roulette, mAmount, mMinBet, mMaxTotal, mChips);
    }

    void Roulette::decreaseAmount()
    {
        mAmount = decreasedBet(Table::Roulette, mAmount, mMinBet);
    }

    bool Roulette::canPlace(int area) const
    {
        if (area < 0 || area >= static_cast<int>(rouletteAreas().size()))
            return false;
        // Ten bets fill the table, and a spin takes no more than the most in all; a place that has a stake cannot be
        // added to (the second click does nothing, 0x7BED70): the stake is taken off with Remove Bet and put on again
        if (betCount() >= maxBets || mTotal >= mMaxTotal || mTotal >= mChips || stake(area) != 0)
            return false;
        return mAmount > 0;
    }

    bool Roulette::place(int area)
    {
        if (!canPlace(area))
            return false;
        // What does not fit under the most, or the chips, is not put down
        const int left = std::min(mMaxTotal, mChips) - mTotal;
        const int put = std::min(mAmount, left);
        if (put <= 0)
            return false;
        mAmount = put;
        mBets.push_back({ area, put });
        mTotal += put;
        return true;
    }

    bool Roulette::remove(int area)
    {
        const auto existing
            = std::find_if(mBets.begin(), mBets.end(), [area](const Bet& bet) { return bet.mArea == area; });
        if (existing == mBets.end())
            return false;
        mTotal -= existing->mStake;
        mBets.erase(existing);
        return true;
    }

    Roulette::Result Roulette::spin()
    {
        Result result;
        result.mPocket = mRandom(0, pocketCount - 1);
        result.mSpinType = mRandom(0, 2);
        const auto& areas = rouletteAreas();
        for (const Bet& bet : mBets)
            if (areas[static_cast<std::size_t>(bet.mArea)].wins(result.mPocket))
                result.mPayout += (areas[static_cast<std::size_t>(bet.mArea)].mMultiplier + 1) * bet.mStake;
        result.mNet = result.mPayout - mTotal;
        result.mLucky = mLuck > 5;
        result.mUnlucky = mLuck < 5;
        mChips = std::max(0, mChips + result.mNet);
        mBets.clear();
        mTotal = 0;
        // The amount to bet goes back to the least bet after a spin (0x7BE3A7)
        mAmount = mMinBet;
        return result;
    }
}
