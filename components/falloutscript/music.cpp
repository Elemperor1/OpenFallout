#include "music.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Music
{
    bool CombatMusic::update(float seconds, float playerStrength, std::span<const Fighter> fighters)
    {
        if (mStopLeft)
            *mStopLeft -= seconds;
        mUntilLook -= seconds;
        if (mUntilLook > 0.f)
            return mOn;
        mUntilLook = mSettings.mUpdateTime;

        // The look at the fights (0x992E4B-0x9930BD): the first fighter that makes the music be on ends it
        float enemyStrength = 0.f;
        float nearStrength = 0.f;
        bool allyNear = false;
        bool shouldBeOn = false;
        for (const Fighter& fighter : fighters)
        {
            if (fighter.mHuntsPlayer)
            {
                // While the music plays, a hunter that has not lost the player keeps it on
                if (mOn)
                {
                    shouldBeOn = true;
                    break;
                }
                enemyStrength += std::max(fighter.mStrength, 0.f);
            }
            else if (fighter.mInCombat)
            {
                if (mOn)
                {
                    if (fighter.mDistance <= mSettings.mNearOuter)
                    {
                        shouldBeOn = true;
                        break;
                    }
                }
                else if (fighter.mDistance <= mSettings.mNearInner)
                {
                    allyNear = allyNear || fighter.mAllyNear;
                    nearStrength += std::max(fighter.mStrength, 0.f);
                }
            }
        }
        if (!mOn && !shouldBeOn)
        {
            // The enemies that hunt the player are a threat when the player is not much stronger than all of them
            if (enemyStrength > 0.f && playerStrength / enemyStrength < mSettings.mTargetedThreatRatio)
                shouldBeOn = true;
            // A fight near the player that a friend of the player is in, and that is stronger than the player
            else if (allyNear && playerStrength * mSettings.mNearStrengthMult < nearStrength)
                shouldBeOn = true;
        }

        if (shouldBeOn != mOn)
        {
            if (shouldBeOn)
            {
                mOn = true;
                mStopLeft.reset();
            }
            else if (!mStopLeft)
                mStopLeft = mSettings.mStopTime;
            else if (*mStopLeft <= 0.f)
            {
                mOn = false;
                mStopLeft.reset();
            }
        }
        return mOn;
    }

    void CombatMusic::reset()
    {
        mOn = false;
        mUntilLook = 0.f;
        mStopLeft.reset();
    }

    std::size_t firstTrack(std::size_t count, const Random& random)
    {
        return count == 0 ? 0 : static_cast<std::size_t>(random(0, static_cast<int>(count) - 1));
    }

    std::optional<std::size_t> nextTrack(std::size_t count, std::size_t& last, const Random& random)
    {
        if (count == 0)
            return std::nullopt;
        if (count == 1)
            return 0;
        std::size_t pick = static_cast<std::size_t>(random(0, static_cast<int>(count) - 1));
        if (last != 0 && last == pick)
            pick = 0;
        else if (last == 0 && pick == 0)
            pick = 1;
        last = pick;
        return pick;
    }

    Attenuation attenuationOf(float anam)
    {
        Attenuation result;
        result.mLoop = anam > 0.f;
        result.mDb = anam > 0.f ? -anam : anam;
        return result;
    }

    float gainOfDb(float db)
    {
        return std::pow(10.f, db / 20.f);
    }

    bool isDay(float hour, std::uint32_t dayStart, std::uint32_t nightStart, bool useDefaults)
    {
        const std::uint32_t day = useDefaults ? 64 : dayStart;
        const std::uint32_t night = useDefaults ? 255 : nightStart;
        const float dayHour = static_cast<float>(day) * 24.f / 256.f;
        const float nightHour = static_cast<float>(night) * 24.f / 256.f;
        if (dayHour <= nightHour)
            return hour >= dayHour && hour < nightHour;
        // A night that starts before the day (the day wraps around midnight)
        return hour >= dayHour || hour < nightHour;
    }

    std::optional<int> layerAt(float fraction, std::uint8_t enabled, const std::array<float, 3>& boundaryPercent)
    {
        for (int layer = 2; layer >= 0; --layer)
        {
            if ((enabled & (1u << layer)) == 0)
                continue;
            if (fraction * 100.f <= boundaryPercent[static_cast<std::size_t>(layer)] + 1e-3f)
                return layer;
        }
        return std::nullopt;
    }

    DirectorAction Director::update(const DirectorInput& input, const std::function<double()>& unit)
    {
        DirectorAction action;
        if (input.mBlocked)
        {
            // A script's track, or a screen, has the music: what played is gone, and the music of the place comes
            // back a little after
            mCurrent = 0;
            mFading = false;
            mGrace = false;
            mWait = mPacing.mStartDelay;
            return action;
        }
        const bool battle = input.mCombat && input.mBattle != 0;
        const std::uint32_t target = battle ? input.mBattle : input.mExplore;

        if (mCurrent != 0 && input.mPlaying)
        {
            mGrace = false;
            if (target != mCurrent && !mFading)
            {
                mFading = true;
                action.mKind = DirectorAction::Kind::FadeOut;
                action.mFade = mCurrentBattle ? mPacing.mBattleFade : mPacing.mTransition;
            }
            return action;
        }
        if (mCurrent != 0 && mGrace)
        {
            // The track was started in the last call and the output does not say that it plays yet
            mGrace = false;
            return action;
        }
        if (mCurrent != 0)
        {
            // The track stopped: it ended, or it faded out because the place or the fight changed
            const bool wasBattle = mCurrentBattle;
            if (mFading)
                mWait = battle ? 0.f : (wasBattle ? mPacing.mBattleWait : mPacing.mStartDelay);
            else if (wasBattle)
                mWait = battle ? 0.f : mPacing.mBattleWait;
            else if (input.mLoops && target == mCurrent)
                mWait = 0.f;
            else
            {
                const double rest = unit();
                mWait = mPacing.mSilenceMin + static_cast<float>(rest) * (mPacing.mSilenceMax - mPacing.mSilenceMin);
            }
            mCurrent = 0;
            mFading = false;
            // What the wait was chosen for is the place that is wanted now
            mLastTarget = target;
        }
        if (mWait < 0.f)
            mWait = mPacing.mStartDelay;
        if (target != mLastTarget)
        {
            // The place changed while nothing played: it does not wait out a long rest for the new place's music, and a
            // fight does not wait at all
            mWait = battle ? 0.f : std::min(mWait, mPacing.mStartDelay);
            mLastTarget = target;
        }
        if (target == 0)
            return action;
        mWait -= input.mSeconds;
        if (mWait > 0.f)
            return action;
        action.mKind = DirectorAction::Kind::Start;
        action.mMusic = target;
        action.mFade = battle ? mPacing.mBattleFade : mPacing.mTransition;
        mCurrent = target;
        mCurrentBattle = battle;
        mFading = false;
        mGrace = true;
        mWait = -1.f;
        return action;
    }

    void Director::reset()
    {
        mCurrent = 0;
        mCurrentBattle = false;
        mFading = false;
        mGrace = false;
        mLastTarget = 0;
        mWait = -1.f;
    }

    std::uint32_t pickSet(std::vector<std::uint32_t>& list, const Random& random)
    {
        if (list.empty())
            return 0;
        std::size_t picked = 0;
        for (std::size_t i = 1; i < list.size(); ++i)
            if (random(0, 3) == 0)
                picked = i;
        const std::uint32_t set = list[picked];
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(picked));
        list.push_back(set);
        return set;
    }

    Incidental::Sound Incidental::update(
        float seconds, bool day, const IncidentalTimes& times, const std::function<double()>& unit)
    {
        mWait -= seconds;
        if (mWait > 0.f)
            return Sound::None;
        float shortest = day ? times.mDayShortest : times.mNightShortest;
        float longest = day ? times.mDayLongest : times.mNightLongest;
        if (shortest > longest)
            std::swap(shortest, longest);
        mWait = std::max(0.f, shortest + static_cast<float>(unit()) * (longest - shortest));
        return day ? Sound::Open : Sound::Close;
    }
}
