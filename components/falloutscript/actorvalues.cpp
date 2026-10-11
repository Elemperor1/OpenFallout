#include "actorvalues.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "actors.hpp"
#include "arguments.hpp"
#include "playerprogress.hpp"

namespace FalloutScript
{
    namespace
    {
        /// Further than any actor value goes, so that arithmetic on them stays a number
        constexpr double farthest = 1e9;

        double bounded(double amount)
        {
            return std::isnan(amount) ? 0.0 : std::clamp(amount, -farthest, farthest);
        }

        int whole(double amount)
        {
            return static_cast<int>(std::clamp(std::trunc(bounded(amount)), -2147483648.0, 2147483647.0));
        }
    }

    double ActorValues::startingValue(int value)
    {
        if (value >= ActorValue::strength && value <= ActorValue::luck)
            return 5;
        if (value == ActorValue::health || value == ActorValue::fatigue || value == ActorValue::speedMult)
            return 100;
        if (value >= ActorValue::firstCondition && value <= ActorValue::lastCondition)
            return 100;
        if (value >= ActorValue::firstSkill && value <= ActorValue::lastSkill)
            return 15;
        return 0;
    }

    double ActorValues::startOf(FormId actor, int value) const
    {
        if (mRecords != nullptr)
        {
            auto known = mFromRecords.find(actor);
            if (known == mFromRecords.end())
            {
                std::vector<std::pair<int, double>> values;
                if (mRecords->initialValues(actor, values))
                {
                    std::map<int, double>& own = mFromRecords[actor];
                    for (const auto& [number, amount] : values)
                        if (isValid(number))
                            own[number] = bounded(amount);
                    known = mFromRecords.find(actor);
                }
            }
            if (known != mFromRecords.end())
            {
                const auto found = known->second.find(value);
                if (found != known->second.end())
                    return found->second;
            }
        }
        return startingValue(value);
    }

    double ActorValues::baseOf(FormId actor, const Entry& entry, int value) const
    {
        return entry.mBase ? *entry.mBase : startOf(actor, value);
    }

    void ActorValues::clear()
    {
        const std::lock_guard lock(mMutex);
        mEntries.clear();
        mFromRecords.clear();
    }

    double ActorValues::base(FormId actor, int value) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find({ actor, value });
        return found == mEntries.end() ? startOf(actor, value) : baseOf(actor, found->second, value);
    }

    double ActorValues::permanent(FormId actor, int value) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find({ actor, value });
        return found == mEntries.end() ? startOf(actor, value)
                                       : baseOf(actor, found->second, value) + found->second.mModifier;
    }

    double ActorValues::current(FormId actor, int value) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find({ actor, value });
        if (found == mEntries.end())
            return startOf(actor, value);
        const Entry& entry = found->second;
        return baseOf(actor, entry, value) + entry.mModifier - entry.mDamage;
    }

    void ActorValues::setBase(FormId actor, int value, double amount)
    {
        if (actor == 0 || !isValid(value))
            return;
        const std::lock_guard lock(mMutex);
        mEntries[{ actor, value }].mBase = bounded(amount);
    }

    void ActorValues::modify(FormId actor, int value, double amount)
    {
        if (actor == 0 || !isValid(value))
            return;
        const std::lock_guard lock(mMutex);
        Entry& entry = mEntries[{ actor, value }];
        entry.mModifier = bounded(entry.mModifier + bounded(amount));
    }

    void ActorValues::setCurrent(FormId actor, int value, double amount)
    {
        if (actor == 0 || !isValid(value))
            return;
        const std::lock_guard lock(mMutex);
        Entry& entry = mEntries[{ actor, value }];
        entry.mBase = bounded(bounded(amount) - entry.mModifier + entry.mDamage);
    }

    void ActorValues::damage(FormId actor, int value, double amount)
    {
        if (actor == 0 || !isValid(value))
            return;
        const std::lock_guard lock(mMutex);
        Entry& entry = mEntries[{ actor, value }];
        entry.mDamage = bounded(entry.mDamage + bounded(amount));
    }

    void ActorValues::restore(FormId actor, int value, double amount)
    {
        if (actor == 0 || !isValid(value))
            return;
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find({ actor, value });
        if (found == mEntries.end())
            return;
        Entry& entry = found->second;
        entry.mDamage = std::max(entry.mDamage - bounded(amount), std::min(entry.mDamage, 0.0));
    }

    void ActorValues::heal(FormId actor, int value)
    {
        if (actor == 0 || !isValid(value))
            return;
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find({ actor, value });
        if (found != mEntries.end())
            found->second.mDamage = 0;
    }

    void addActorValueCommands(
        Interpreter& interpreter, ActorValues& values, ActorState& actors, PlayerProgress& progress)
    {
        // The karma and the experience of the player are the ones the story rewards, which PlayerProgress keeps
        const auto isPlayersProgress = [](const CallContext& call, int value) {
            return call.mReference == call.mHost.player()
                && (value == ActorValue::karma || value == ActorValue::experience);
        };
        const auto progressValue
            = [&progress](int value) { return value == ActorValue::karma ? progress.karma() : progress.experience(); };
        const auto rewardProgress = [&progress](int value, int amount) {
            if (value == ActorValue::karma)
                progress.rewardKarma(amount);
            else
                progress.rewardExperience(amount);
        };
        // What the actor value is now, for a character that is dead its health is none
        const auto currentOf = [&values, &actors, isPlayersProgress, progressValue](
                                   const CallContext& call, int value) {
            if (isPlayersProgress(call, value))
                return static_cast<double>(progressValue(value));
            const double current = values.current(call.mReference, value);
            return value == ActorValue::health && actors.isDead(call.mReference) ? std::min(current, 0.0) : current;
        };

        interpreter.setHandler("GetActorValue", [currentOf](CallContext& call) {
            const int value = intArgument(call, 0);
            return call.mReference != 0 && ActorValues::isValid(value) ? currentOf(call, value) : 0.0;
        });
        interpreter.setHandler("GetBaseActorValue", [&values, isPlayersProgress, progressValue](CallContext& call) {
            const int value = intArgument(call, 0);
            if (call.mReference == 0 || !ActorValues::isValid(value))
                return 0.0;
            return isPlayersProgress(call, value) ? static_cast<double>(progressValue(value))
                                                  : values.base(call.mReference, value);
        });
        interpreter.setHandler(
            "GetPermanentActorValue", [&values, isPlayersProgress, progressValue](CallContext& call) {
                const int value = intArgument(call, 0);
                if (call.mReference == 0 || !ActorValues::isValid(value))
                    return 0.0;
                return isPlayersProgress(call, value) ? static_cast<double>(progressValue(value))
                                                      : values.permanent(call.mReference, value);
            });
        interpreter.setHandler(
            "SetActorValue", [&values, isPlayersProgress, progressValue, rewardProgress](CallContext& call) {
                const int value = intArgument(call, 0);
                if (call.mReference == 0 || !ActorValues::isValid(value))
                    return 0.0;
                if (isPlayersProgress(call, value))
                    rewardProgress(value, whole(argument(call, 1) - progressValue(value)));
                else
                    values.setBase(call.mReference, value, argument(call, 1));
                return 0.0;
            });
        interpreter.setHandler("ModActorValue", [&values, isPlayersProgress, rewardProgress](CallContext& call) {
            const int value = intArgument(call, 0);
            if (call.mReference == 0 || !ActorValues::isValid(value))
                return 0.0;
            if (isPlayersProgress(call, value))
                rewardProgress(value, whole(argument(call, 1)));
            else
                values.modify(call.mReference, value, argument(call, 1));
            return 0.0;
        });
        interpreter.setHandler(
            "ForceActorValue", [&values, isPlayersProgress, progressValue, rewardProgress](CallContext& call) {
                const int value = intArgument(call, 0);
                if (call.mReference == 0 || !ActorValues::isValid(value))
                    return 0.0;
                if (isPlayersProgress(call, value))
                    rewardProgress(value, whole(argument(call, 1) - progressValue(value)));
                else
                    values.setCurrent(call.mReference, value, argument(call, 1));
                return 0.0;
            });
        interpreter.setHandler(
            "DamageActorValue", [&values, &actors, isPlayersProgress, rewardProgress](CallContext& call) {
                const int value = intArgument(call, 0);
                if (call.mReference == 0 || !ActorValues::isValid(value))
                    return 0.0;
                if (isPlayersProgress(call, value))
                {
                    rewardProgress(value, -whole(argument(call, 1)));
                    return 0.0;
                }
                values.damage(call.mReference, value, argument(call, 1));
                // A character with no health left is dead
                if (value == ActorValue::health && values.current(call.mReference, value) <= 0)
                    actors.setDead(call.mReference, true);
                return 0.0;
            });
        interpreter.setHandler("RestoreActorValue", [&values, isPlayersProgress](CallContext& call) {
            const int value = intArgument(call, 0);
            // Nothing is damaged in what the story rewards, so there is nothing to take back
            if (call.mReference != 0 && ActorValues::isValid(value) && !isPlayersProgress(call, value))
                values.restore(call.mReference, value, argument(call, 1));
            return 0.0;
        });

        // The share of its health that a character that lives has left: damage lowers it, and a character with
        // no health at all (or none to begin with) has none
        interpreter.setHandler("GetHealthPercentage", [&values, &actors](CallContext& call) {
            if (call.mReference == 0 || actors.isDead(call.mReference))
                return 0.0;
            const double most = values.permanent(call.mReference, ActorValue::health);
            if (most <= 0)
                return 0.0;
            return std::clamp(values.current(call.mReference, ActorValue::health) / most, 0.0, 1.0);
        });
        interpreter.setHandler("ResurrectActor", [&values, &actors](CallContext& call) {
            actors.setDead(call.mReference, false);
            values.heal(call.mReference, ActorValue::health);
            return 0.0;
        });
        // Nothing wears a weapon out yet, so the weapon a character holds is as good as new
        interpreter.setHandler("GetWeaponHealthPerc", [](CallContext&) { return 100.0; });
    }
}
