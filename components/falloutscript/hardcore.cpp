#include "hardcore.hpp"

#include <algorithm>
#include <cmath>

#include "actorvalues.hpp"
#include "magic.hpp"

namespace FalloutScript
{
    HardcoreNeeds::HardcoreNeeds(StageFinder stages, ActorValues& values, Magic& magic)
        : mStages(std::move(stages))
        , mValues(values)
        , mMagic(magic)
    {
    }

    void HardcoreNeeds::setRates(double dehydration, double hunger, double sleep)
    {
        const std::lock_guard lock(mMutex);
        mRates = { dehydration, hunger, sleep };
    }

    void HardcoreNeeds::setStageListener(std::function<void(FormId actor)> listener)
    {
        const std::lock_guard lock(mMutex);
        mListener = std::move(listener);
    }

    void HardcoreNeeds::chooseStage(FormId player, int need)
    {
        const double level = mValues.current(player, actorValueOf(need));
        std::function<void(FormId)> listener;
        {
            const std::lock_guard lock(mMutex);
            // Nothing to do while the level is the one the stage was chosen for
            if (mChosen[need] == level)
                return;
            mChosen[need] = level;
            listener = mListener;
        }
        std::vector<Stage> stages = mStages ? mStages(need) : std::vector<Stage>();
        std::sort(
            stages.begin(), stages.end(), [](const Stage& a, const Stage& b) { return a.mThreshold < b.mThreshold; });
        FormId wanted = 0;
        for (const Stage& stage : stages)
            if (level >= stage.mThreshold)
                wanted = stage.mSpell;
        // The spells that are on the player are asked, not remembered: a game that was loaded has them without this
        // having put them there
        bool changed = false;
        for (const Stage& stage : stages)
            if (stage.mSpell != 0 && stage.mSpell != wanted && mMagic.hasSpell(player, stage.mSpell))
                changed = mMagic.removeSpell(player, stage.mSpell) || changed;
        if (wanted != 0)
            changed = mMagic.addSpell(player, wanted) || changed;
        if (changed && listener)
            listener(player);
    }

    void HardcoreNeeds::update(FormId player, double minutes, double timeScale, bool sleeping)
    {
        if (player == 0 || timeScale <= 0)
            return;
        for (int need = 0; need < needCount; ++need)
        {
            double points = 0;
            {
                const std::lock_guard lock(mMutex);
                const double interval = mRates[need] * timeScale / 60.0;
                if (mStamps[need] <= 0 || minutes < mStamps[need])
                    mStamps[need] = minutes;
                else if (interval > 0 && minutes - mStamps[need] >= interval)
                {
                    points = std::floor((minutes - mStamps[need]) / interval);
                    // What is left of the time is dropped; sleeping counts the time without adding to the need
                    mStamps[need] = minutes;
                    if (need == Sleep && sleeping)
                        points = 0;
                }
            }
            if (points > 0)
                mValues.addBase(player, actorValueOf(need), points, 0.0);
            chooseStage(player, need);
        }
    }

    void HardcoreNeeds::sleepHour(FormId player, double amount)
    {
        if (player == 0)
            return;
        const double reduced = std::min(mValues.current(player, actorValueOf(Sleep)), amount);
        if (reduced > 0)
            mValues.addBase(player, actorValueOf(Sleep), -reduced, 0.0);
        chooseStage(player, Sleep);
    }

    void HardcoreNeeds::end(FormId player)
    {
        if (player != 0)
        {
            std::function<void(FormId)> listener;
            {
                const std::lock_guard lock(mMutex);
                listener = mListener;
            }
            bool changed = false;
            for (int need = 0; need < needCount; ++need)
                for (const Stage& stage : mStages ? mStages(need) : std::vector<Stage>())
                    if (stage.mSpell != 0 && mMagic.hasSpell(player, stage.mSpell))
                        changed = mMagic.removeSpell(player, stage.mSpell) || changed;
            if (changed && listener)
                listener(player);
        }
        clear();
    }

    void HardcoreNeeds::clear()
    {
        const std::lock_guard lock(mMutex);
        mStamps = {};
        mChosen = { -1.0, -1.0, -1.0 };
    }
}
