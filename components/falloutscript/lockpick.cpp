#include "lockpick.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Lockpick
{
    namespace
    {
        int clampClass(int lockClass)
        {
            return std::clamp(lockClass, 0, 4);
        }
    }

    bool mayTry(double skill, int lockClass, const Rules& rules)
    {
        return skill >= rules.mSkillGate[static_cast<size_t>(std::clamp(lockClass, 0, 5))];
    }

    SweetSpot sweetSpot(int lockClass, double meterWidth, double random, const Rules& rules)
    {
        const size_t index = static_cast<size_t>(clampClass(lockClass));
        SweetSpot spot;
        spot.mLength = rules.mSweetSpotLength[index];
        spot.mConcentric = rules.mConcentricLength[index];
        const double outer = spot.outer();
        const double wanted = std::clamp(random, 0.0, 1.0) * meterWidth;
        // A meter too narrow for the whole spot has it in the middle
        spot.mCenter = meterWidth > 2 * outer ? std::clamp(wanted, outer, meterWidth - outer) : meterWidth / 2;
        return spot;
    }

    double maxTurn(double pick, const SweetSpot& spot, double meterWidth)
    {
        const double distance = std::abs(pick - spot.mCenter);
        const double outer = spot.outer();
        if (distance <= outer)
        {
            for (int band = 0; band < 6; ++band)
                if (distance <= spot.halfWidth(band))
                    return (1.0 - band / 6.0) * 90.0;
            return 15.0;
        }
        const double span = 2 * outer - meterWidth;
        if (span == 0)
            return 0;
        return std::clamp(15.0 * (outer + distance - meterWidth) / span, 0.0, 15.0);
    }

    double pickHealth(double skill, const Rules& rules)
    {
        return 1.0 + skill * rules.mBonusHealth;
    }

    double forceChance(double skill, int lockClass, const Rules& rules)
    {
        return std::clamp(
            skill - rules.mSkillGate[static_cast<size_t>(std::clamp(lockClass, 0, 5))] + rules.mForceBase, 0.0, 100.0);
    }

    Game::Game(const Setup& setup, std::function<double()> random)
        : mSetup(setup)
        , mRandom(std::move(random))
        , mPicks(setup.mPicks)
    {
        mSpot = Lockpick::sweetSpot(mSetup.mLockClass, mSetup.mMeterWidth, mRandom ? mRandom() : 0.5, mSetup.mRules);
        mPick = mSetup.mMeterWidth / 2;
        mLimit = maxTurn(mPick, mSpot, mSetup.mMeterWidth);
        mHealth = pickHealth(mSetup.mSkill, mSetup.mRules);
    }

    void Game::crossSqueaks(double before, double after, std::vector<Event>& events)
    {
        const auto crosses = [&](double at) { return (before < at) != (after < at); };
        if (crosses(30))
            events.push_back(Event::SqueakA);
        if (crosses(60))
            events.push_back(Event::SqueakB);
    }

    std::vector<Event> Game::step(const Input& input)
    {
        std::vector<Event> events;
        const double seconds = std::max(input.mSeconds, 0.0);
        const double width = mSetup.mMeterWidth;
        const double rate = mSetup.mRules.mTurnDegrees;

        switch (mState)
        {
            case State::Intro:
                // The exe says so on the first frame of the intro: the entering sound, or, with no pick, the message
                // that there is none (and no sound)
                if (!mAnnounced)
                {
                    mAnnounced = true;
                    events.push_back(mPicks > 0 ? Event::Enter : Event::OutOfPicks);
                }
                mClock += seconds;
                if (mClock >= lockForward)
                {
                    mClock = 0;
                    mState = State::Idle;
                }
                break;
            case State::Idle:
            {
                const double moved = std::clamp(mPick + input.mMove, 0.0, width);
                const bool moving = input.mMove != 0;
                // A sound when the pick starts to move, and for each step of a hundred units, but not for every tenth
                if (moving && !mMoving)
                    events.push_back(Event::Move);
                else if (std::floor(mPick / 100) != std::floor(moved / 100) && (!mRandom || mRandom() >= 0.1))
                    events.push_back(Event::Move);
                mMoving = moving;
                mPick = moved;
                mLimit = maxTurn(mPick, mSpot, width);

                if (input.mTurn > 0 && mPicks > 0)
                {
                    mState = State::Turning;
                    events.push_back(Event::Turn);
                }
                else
                {
                    const double before = mAngle;
                    mAngle = std::max(0.0, mAngle - seconds * rate);
                    crossSqueaks(before, mAngle, events);
                    break;
                }
                [[fallthrough]];
            }
            case State::Turning:
            {
                if (input.mTurn <= 0 && !mForcing)
                {
                    mState = State::Idle;
                    break;
                }
                const double before = mAngle;
                const double reach = mForcing ? 90.0 : mLimit;
                mAngle = std::min(mAngle + seconds * rate * (mForcing ? 1.0 : std::min(input.mTurn, 1.0)), reach);
                crossSqueaks(before, mAngle, events);
                if (mAngle >= reach)
                {
                    if (reach >= 90.0)
                    {
                        mState = State::Opened;
                        events.push_back(Event::Unlock);
                    }
                    else
                    {
                        mState = State::Binding;
                        events.push_back(Event::Stop);
                        events.push_back(Event::TensionStart);
                    }
                }
                break;
            }
            case State::Binding:
            {
                if (input.mTurn <= 0)
                {
                    mState = State::Idle;
                    events.push_back(Event::TensionEnd);
                    break;
                }
                mShake = -mShake;
                mHealth -= seconds / std::max(mSetup.mRules.mBreakSeconds, 1e-6);
                if (mHealth <= 0)
                {
                    mHealth = 0;
                    mState = State::Snapping;
                    mClock = 0;
                    --mPicks;
                    events.push_back(Event::TensionEnd);
                    events.push_back(Event::Break);
                }
                break;
            }
            case State::Snapping:
                mClock += seconds;
                if (mClock >= resetSeconds)
                {
                    mClock = 0;
                    mAngle = 0;
                    mPick = width / 2;
                    mLimit = maxTurn(mPick, mSpot, width);
                    mHealth = pickHealth(mSetup.mSkill, mSetup.mRules);
                    mShake = 1;
                    mState = State::Waiting;
                    if (mPicks <= 0)
                        events.push_back(Event::OutOfPicks);
                }
                break;
            case State::Waiting:
                mClock += seconds;
                if (mClock >= pinForward)
                {
                    mClock = 0;
                    mState = State::Idle;
                }
                break;
            case State::Opened:
            case State::Failed:
                break;
        }
        return events;
    }

    std::vector<Event> Game::force(double roll)
    {
        std::vector<Event> events;
        // A force that was accepted runs to its end: a second request cannot turn a success into a failure
        if (mForcing)
            return events;
        if (mState != State::Idle && mState != State::Turning && mState != State::Binding)
            return events;
        if (mState == State::Binding)
            events.push_back(Event::TensionEnd);
        if (roll < forceChance(mSetup.mSkill, mSetup.mLockClass, mSetup.mRules))
        {
            // The cylinder turns by itself, all the way
            mForcing = true;
            mLimit = 90.0;
            mState = State::Turning;
            events.push_back(Event::Turn);
        }
        else
        {
            mState = State::Failed;
            events.push_back(Event::ForceFailed);
        }
        return events;
    }

    Game::View Game::view() const
    {
        View view;
        const bool strained = mState == State::Binding;
        view.mPick = mPick + (strained ? mShake * 2.5 : 0.0);
        view.mPickAngle = 90.0 - 180.0 * (view.mPick / std::max(mSetup.mMeterWidth, 1e-6));
        view.mAngle = mAngle;
        view.mHealth = mHealth;

        if (mState == State::Intro)
        {
            view.mLockSequence = "Forward";
            view.mLockTime = std::min(mClock, lockForward);
        }
        else
        {
            const double turn = std::clamp((mAngle + (strained ? mShake * 0.25 : 0.0)) / 90.0, 0.0, 1.0);
            view.mLockSequence = "Backward";
            view.mLockTime = lockBackward * turn;
        }

        switch (mState)
        {
            case State::Intro:
            case State::Waiting:
                view.mPinSequence = "Forward";
                view.mPinTime = std::min(mClock, pinForward);
                break;
            case State::Binding:
                view.mPinSequence = "Backward";
                view.mPinTime = pinBackward * std::clamp(1.0 - mHealth, 0.0, 1.0);
                break;
            case State::Snapping:
                view.mPinSequence = "Left";
                view.mPinTime = std::min(mClock, pinLeft);
                break;
            default:
                view.mPinSequence = "Forward";
                view.mPinTime = pinForward;
                break;
        }
        return view;
    }
}
