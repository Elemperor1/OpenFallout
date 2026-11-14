#include "hardcore.hpp"

#include <algorithm>
#include <cmath>

#include "actorvalues.hpp"
#include "arguments.hpp"
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

    void HardcoreNeeds::setNoticeListener(std::function<void(const Notice&)> listener)
    {
        const std::lock_guard lock(mMutex);
        mNotices = std::move(listener);
    }

    const char* HardcoreNeeds::settingName(int need)
    {
        switch (need)
        {
            case Dehydration:
                return "Dehydration";
            case Hunger:
                return "Hunger";
            default:
                return "SleepDeprevation";
        }
    }

    std::vector<HardcoreNeeds::NoticeLine> HardcoreNeeds::noticeLines(const Notice& notice)
    {
        constexpr int veryHappy = 1;
        constexpr int inPain = 3;
        const std::string base = std::string("s") + settingName(notice.mNeed);
        std::vector<NoticeLine> lines;
        lines.push_back({ base + (notice.mIncreased ? "Increase" : "Decrease"), false,
            notice.mIncreased ? inPain : veryHappy });
        if (notice.mStage != 0)
            lines.push_back({ "sRadiationSick", true, inPain });
        else
            lines.push_back({ base + "NotSick", false, veryHappy });
        return lines;
    }

    void HardcoreNeeds::chooseStage(FormId player, int need)
    {
        const double level = mValues.current(player, actorValueOf(need));
        std::function<void(FormId)> listener;
        std::function<void(const Notice&)> notices;
        double before = -1;
        {
            const std::lock_guard lock(mMutex);
            // Nothing to do while the level is the one the stage was chosen for
            if (mChosen[need] == level)
                return;
            before = mChosen[need];
            mChosen[need] = level;
            listener = mListener;
            notices = mNotices;
        }
        std::vector<Stage> stages = mStages ? mStages(need) : std::vector<Stage>();
        std::sort(
            stages.begin(), stages.end(), [](const Stage& a, const Stage& b) { return a.mThreshold < b.mThreshold; });
        const auto stageOf = [&stages](double value) {
            FormId found = 0;
            for (const Stage& stage : stages)
                if (value >= stage.mThreshold)
                    found = stage.mSpell;
            return found;
        };
        const FormId wanted = stageOf(level);
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
        // The player is told of a change of stage between two levels that are known: the first level seen (a game that
        // was just loaded) is not a change
        if (before >= 0 && notices && stageOf(before) != wanted)
            notices({ need, level > before, wanted });
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

    void HardcoreNeeds::reset(FormId player)
    {
        if (player != 0)
            for (int need = 0; need < needCount; ++need)
            {
                // The level is known before it goes, so that the stage it leaves is told
                chooseStage(player, need);
                const double level = mValues.current(player, actorValueOf(need));
                if (level != 0)
                    mValues.addBase(player, actorValueOf(need), -level, 0.0);
                chooseStage(player, need);
            }
        clear();
    }

    void HardcoreNeeds::refresh(FormId player)
    {
        if (player == 0)
            return;
        for (int need = 0; need < needCount; ++need)
            chooseStage(player, need);
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

    bool HardcoreMode::set(int value, bool loading)
    {
        const bool zero = value == 0 && !loading;
        if (zero)
            mTracking = false;
        mValue = value;
        return zero;
    }

    bool HardcoreMode::command(int value, bool& zero)
    {
        zero = false;
        if (value < 0 || value > 1)
            return false;
        zero = set(value, false);
        if (value != 1)
            mTracking = false;
        return true;
    }

    void HardcoreMode::reset()
    {
        mValue = 0;
        mTracking = true;
    }

    void HardcoreMode::write(SaveWriter& out) const
    {
        out.i32(mValue);
        out.boolean(mTracking);
    }

    void HardcoreMode::read(SaveReader& in, std::uint32_t)
    {
        const std::int32_t value = in.i32();
        const bool tracking = in.boolean();
        set(value, true);
        mTracking = tracking;
    }

    void addHardcoreCommands(Interpreter& interpreter, HardcoreMode& mode, std::function<void()> zeroNeeds)
    {
        interpreter.setHandler("SetHardcore", [&mode, zeroNeeds](CallContext& call) {
            // (the argument is the value; none is the first and only argument of the command)
            const int value = call.mArguments.empty() ? 1 : intArgument(call, 0);
            bool zero = false;
            if (!mode.command(value, zero))
            {
                call.mHost.log("SetHardcore: the value is not 0 or 1");
                return 0.0;
            }
            if (zero && zeroNeeds)
                zeroNeeds();
            return 1.0;
        });
        interpreter.setHandler("IsHardcore", [&mode](CallContext&) { return mode.on() ? 1.0 : 0.0; });
        interpreter.setHandler("IsAlwaysHardcore", [&mode](CallContext&) { return mode.always() ? 1.0 : 0.0; });
        interpreter.setHandler("DisableHardcoreTracking", [&mode](CallContext&) {
            mode.disableTracking();
            return 1.0;
        });
    }

    void NeedsDisplay::advance(double milliseconds)
    {
        // The first call only sets the time the next one counts from
        const double elapsed = mLast < 0 ? 0.0 : milliseconds - mLast;
        mLast = milliseconds;
        mFlash += (mFalling ? -elapsed : elapsed) * flashPerMillisecond;
        if (mFlash >= flashHigh)
            mFalling = true;
        if (mFlash <= flashLow)
            mFalling = false;
        mFlash = std::clamp(mFlash, flashLow, flashHigh);
    }

    NeedsDisplay::Tile NeedsDisplay::tile(double level) const
    {
        if (level >= mThresholds.mFlash)
            return { 2, static_cast<int>(mFlash) };
        if (level >= mThresholds.mRed)
            return { 2, fullAlpha };
        if (level >= mThresholds.mFullAlpha)
            return { 1, fullAlpha };
        if (level >= mThresholds.mHalfAlpha)
            return { 1, halfAlpha };
        return { 1, 0 };
    }

    double NeedsDisplay::limbLevel(int crippledParts) const
    {
        if (crippledParts >= 4)
            return mThresholds.mFlash + 1;
        if (crippledParts == 3)
            return mThresholds.mRed + 1;
        if (crippledParts == 2)
            return mThresholds.mFullAlpha + 1;
        if (crippledParts == 1)
            return mThresholds.mHalfAlpha + 1;
        return 0;
    }
}
