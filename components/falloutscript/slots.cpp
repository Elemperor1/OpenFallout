#include "slots.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace FalloutScript::Casino
{
    namespace
    {
        constexpr std::size_t stripSize = 28;
        // The strip holds this for an entry that was not written
        constexpr int unwritten = 14;
    }

    SlotMachine::SlotMachine(const Rules& rules, int minBet, int maxBet, int chips, int luck, Random random)
        : mRules(rules)
        , mMinBet(std::max(1, minBet))
        , mMaxBet(std::max(mMinBet, maxBet))
        , mChips(std::max(0, chips))
        , mLuck(luck)
        , mBet(mMinBet)
        , mRandom(std::move(random))
    {
        buildStrip();
    }

    void SlotMachine::buildStrip()
    {
        // The strip is filled in passes over the seven symbols (0x7C16B0): in a pass each symbol that still has a count
        // writes two entries, its own number and the blank after it, and its count goes down by one. The counts of the
        // record decide only whether a symbol is on the strip, not how often it is picked.
        std::array<int, symbols> counts = mRules.mSlotStops;
        mSymbols = 0;
        for (const int count : counts)
            if (count != 0)
                ++mSymbols;
        mStrip.assign(stripSize, unwritten);
        std::size_t filled = 0;
        while (filled < stripSize)
        {
            std::size_t written = 0;
            int value = 0;
            for (int symbol = 0; symbol < symbols; ++symbol)
            {
                if (counts[static_cast<std::size_t>(symbol)] != 0)
                {
                    for (int part = 0; part < 2 && filled < stripSize; ++part)
                    {
                        mStrip[filled++] = value + part;
                        ++written;
                    }
                    --counts[static_cast<std::size_t>(symbol)];
                    value += 2;
                }
                else
                    value += 1;
            }
            // A pass that writes nothing before the strip is full: the counts add up to less than 14
            if (written == 0)
            {
                mValid = false;
                return;
            }
        }
    }

    void SlotMachine::increaseBet()
    {
        mBet = increasedBet(Table::Roulette, mBet, mMinBet, mMaxBet, mChips);
    }

    void SlotMachine::decreaseBet()
    {
        mBet = decreasedBet(Table::Roulette, mBet, mMinBet);
    }

    void SlotMachine::setHighestBet()
    {
        mBet = highestBet(mMaxBet, mChips);
    }

    int SlotMachine::threeAlikePays(int stop)
    {
        switch (stop)
        {
            case 0:
                return 10;
            case 2:
                return 20;
            case 4:
                return 30;
            case 6:
                return 40;
            case 8:
                return 60;
            case 10:
                return 100;
            case 12:
                return 10;
            default:
                return 0;
        }
    }

    SlotMachine::WinClass SlotMachine::classOf(const std::array<int, reels>& stops)
    {
        const auto even = [](int stop) { return stop % 2 == 0; };
        if (stops[0] == stops[1] && stops[1] == stops[2] && even(stops[0]))
            return WinClass::ThreeAlike;
        const int specials = static_cast<int>(std::count(stops.begin(), stops.end(), special));
        if (specials == 2)
            return WinClass::TwoSpecial;
        if (specials == 1)
            return WinClass::OneSpecial;
        for (std::size_t a = 0; a < stops.size(); ++a)
            for (std::size_t b = a + 1; b < stops.size(); ++b)
                if (stops[a] == stops[b] && even(stops[a]))
                    return WinClass::Pair;
        return WinClass::Lose;
    }

    SlotMachine::WinClass SlotMachine::pickStops()
    {
        // Each reel takes a stop from the first 2 N - 1 entries of the strip, N being the symbols that are on it
        const int last = std::max(0, 2 * mSymbols - 2);
        for (int& stop : mStops)
            stop = mStrip[static_cast<std::size_t>(mRandom(0, last))];
        return classOf(mStops);
    }

    bool SlotMachine::rig(WinClass winClass)
    {
        // The chance is (|Luck - 5| x 10 - 1) per cent
        const int t = std::abs(mLuck - 5) * 10;
        const int roll = mRandom(0, 99);
        if (roll + t <= 100)
            return false;
        const int kind = static_cast<int>(winClass);
        if (mLuck >= 5)
        {
            switch (kind)
            {
                case 0:
                    // Nothing came up: the reels are picked again once, and the new line gets its own try
                    return rig(pickStops());
                case 1:
                case 5:
                {
                    // Two reels alike: the third is turned to them when it is near enough
                    for (std::size_t odd = 0; odd < mStops.size(); ++odd)
                    {
                        const int a = mStops[(odd + 1) % 3];
                        const int b = mStops[(odd + 2) % 3];
                        if (a == b)
                        {
                            const int distance = mStops[odd] - a;
                            if (distance * distance <= mLuck)
                            {
                                mStops[odd] = a;
                                return true;
                            }
                            return false;
                        }
                    }
                    return false;
                }
                case 4:
                {
                    // One special symbol: the two other reels are made alike when they are near enough
                    std::size_t at = 0;
                    for (std::size_t i = 0; i < mStops.size(); ++i)
                        if (mStops[i] == special)
                            at = i;
                    int& first = mStops[(at + 1) % 3];
                    int& second = mStops[(at + 2) % 3];
                    if (std::abs(first - second) > mLuck || first == second)
                        return false;
                    // The symbol is copied over a blank when there is one
                    if (first % 2 != 0 || second % 2 == 0)
                        first = second;
                    else
                        second = first;
                    return true;
                }
                default:
                    return false;
            }
        }
        // Below 5 Luck turns the stops against the player, and says nothing of it (the caller's flag is overwritten)
        switch (kind)
        {
            case 4:
            case 5:
                for (int& stop : mStops)
                    if (stop == special)
                        stop = special + 1;
                break;
            case 6:
            {
                const int next = mStops[0] + 1 > 6 ? 0 : mStops[0] + 1;
                mStops = { next, next, next };
                break;
            }
            default:
                break;
        }
        return false;
    }

    SlotMachine::Result SlotMachine::spin()
    {
        Result result;
        WinClass winClass = pickStops();
        result.mLucky = rig(winClass);
        winClass = classOf(mStops);
        result.mStops = mStops;
        result.mClass = winClass;
        switch (winClass)
        {
            case WinClass::ThreeAlike:
                result.mMultiplier = threeAlikePays(mStops[0]);
                break;
            case WinClass::TwoSpecial:
                result.mMultiplier = 5;
                break;
            case WinClass::OneSpecial:
                result.mMultiplier = 2;
                break;
            default:
                break;
        }
        result.mNet = result.mMultiplier > 0 ? mBet * result.mMultiplier : -mBet;
        mChips = std::max(0, mChips + result.mNet);
        // What the player has left may be less than the bet
        mBet = std::min(mBet, mChips);
        return result;
    }
}
