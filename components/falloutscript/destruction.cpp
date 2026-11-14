#include "destruction.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"
#include "interpreter.hpp"
#include "referencestate.hpp"

namespace FalloutScript
{
    namespace DestructionRule
    {
        int percent(double health, double maxHealth)
        {
            // The executable multiplies the health (a float) by 100 and divides it by the most (an integer that it widens
            // without a sign) in double precision, keeps the result as a float, rounds it up, and keeps the low byte of
            // what comes out
            const double most = static_cast<double>(static_cast<std::uint32_t>(static_cast<std::int32_t>(
                std::clamp(maxHealth, -2147483648.0, 2147483647.0))));
            const double share = static_cast<double>(static_cast<float>(health)) * 100.0 / most;
            if (!std::isfinite(share))
                return 0;
            const float rounded = std::ceil(static_cast<float>(share));
            // (the x87 conversion of a number that does not fit gives the "indefinite" integer, whose low byte is 0)
            if (!(std::fabs(rounded) < 2147483648.0f))
                return 0;
            return static_cast<int>(static_cast<std::uint8_t>(static_cast<std::int32_t>(rounded) & 0xff));
        }

        int stage(const DestructionData& data, double health, double maxHealth)
        {
            const int current = percent(health, maxHealth);
            int result = static_cast<int>(data.mStages.size());
            for (std::size_t i = 0; i < data.mStages.size(); ++i)
                if (data.mStages[i].mHealthPercent < current)
                {
                    result = static_cast<int>(i);
                    break;
                }
            return result;
        }

        bool disables(const DestructionData& data, double health, double maxHealth)
        {
            if (health < 0)
                return false;
            // The stages that the health went down through, from the most to where it is, as `damage` crosses them: those
            // with a percentage below the one at the start and not below the one now, up to a stage that caps the health
            const int before = percent(maxHealth, maxHealth);
            int after = percent(health, maxHealth);
            if (after == 0 && health > 0)
                after = 1;
            for (const DestructionStage& stage : data.mStages)
            {
                const int percentOfStage = std::clamp(stage.mHealthPercent, 0, 255);
                if (percentOfStage >= before || percentOfStage < after)
                    continue;
                if ((stage.mFlags & DestructionStage::disable) != 0)
                    return true;
                if ((stage.mFlags & DestructionStage::capDamage) != 0)
                    break;
            }
            return false;
        }

        DestructionOutcome damage(const DestructionData& data, double maxHealth, double health, double amount,
            std::optional<double> resulting)
        {
            DestructionOutcome outcome;
            outcome.mHealth = health;
            outcome.mMaxHealth = maxHealth;
            if (!(amount > 0))
                return outcome;
            // A thing that was never damaged has all its health
            const bool untouched = health < 0;
            const double current = untouched ? maxHealth : health;
            if (current == 0)
                return outcome;

            const int before = percent(current, maxHealth);
            double result = resulting ? *resulting : current - amount;
            if (!(result > 0))
                result = 0;
            int after = percent(result, maxHealth);
            // What is left of a thing is never counted as nothing
            if (after == 0 && result > 0)
                after = 1;

            // The stages the health goes down through: those with a percentage below where it was, down to where it is. The
            // stage that was passed before is the last one at or above where it was.
            int passedCount = 0; // the number of the last stage passed before, plus one
            int crossedCount = 0; // the number of the last stage crossed now, plus one
            int passedDamageStage = 0;
            int damageStage = 0;
            int modelStage = -1;
            int selfDamage = 0;
            for (std::size_t i = 0; i < data.mStages.size(); ++i)
            {
                const DestructionStage& stage = data.mStages[i];
                const int percentOfStage = std::clamp(stage.mHealthPercent, 0, 255);
                if (percentOfStage < before && percentOfStage >= after)
                {
                    if (stage.mDamageStage != 0)
                        damageStage = stage.mDamageStage;
                    if (!stage.mModel.empty())
                        modelStage = static_cast<int>(i);
                    selfDamage = stage.mSelfDamage;
                    crossedCount = static_cast<int>(i) + 1;
                    if ((stage.mFlags & DestructionStage::disable) != 0)
                        outcome.mDisable = true;
                    if ((stage.mFlags & DestructionStage::destroy) != 0)
                        outcome.mDestroyed = true;
                    if ((stage.mFlags & DestructionStage::capDamage) != 0)
                    {
                        // The health stops here
                        result = percentOfStage * maxHealth / 100.0;
                        after = percentOfStage;
                        break;
                    }
                }
                else if (percentOfStage >= before && percentOfStage > after)
                {
                    passedCount = static_cast<int>(i) + 1;
                    if (stage.mDamageStage != 0)
                        passedDamageStage = stage.mDamageStage;
                }
            }

            if (!(result > 0))
                outcome.mDestroyed = true;
            outcome.mApplied = true;
            outcome.mHealth = result;
            outcome.mStageBefore = untouched ? -1 : stage(data, current, maxHealth);
            outcome.mStageAfter = stage(data, result, maxHealth);
            if (crossedCount > passedCount)
            {
                for (int i = passedCount; i < crossedCount; ++i)
                    outcome.mCrossed.push_back(i);
                outcome.mModelStage = modelStage;
                // A model brings its own damage stage; without one the model of the thing shows its nodes of the stage
                if (modelStage < 0 && damageStage > passedDamageStage)
                    outcome.mDamageStage = damageStage;
                outcome.mSelfDamage = selfDamage != 0 && result > 0 ? selfDamage : 0;
            }
            return outcome;
        }
    }

    Destruction::Destruction(DestructionRecords& records, ReferenceState& references)
        : mRecords(records)
        , mReferences(references)
    {
    }

    void Destruction::setListener(Listener listener)
    {
        const std::lock_guard lock(mMutex);
        mListener = std::move(listener);
    }

    void Destruction::setRestoreListener(std::function<void(FormId)> listener)
    {
        const std::lock_guard lock(mMutex);
        mRestoreListener = std::move(listener);
    }

    void Destruction::clear()
    {
        const std::lock_guard lock(mMutex);
        mStates.clear();
    }

    bool Destruction::dataOf(FormId reference, DestructionData& data)
    {
        return reference != 0 && mRecords.destructionData(reference, data);
    }

    DestructionOutcome Destruction::damage(FormId reference, double amount, std::optional<double> resulting)
    {
        DestructionData data;
        if (!dataOf(reference, data))
            return {};
        const double baseHealth = mRecords.destructionBaseHealth(reference);
        const bool actor = baseHealth >= 0;
        // The health of a character is the actor value, which the blow has changed already when the host says so; a blow of
        // a script that no one applied reads what the character has
        if (actor && !resulting)
            resulting = mRecords.destructionActorHealth(reference);
        const double maxHealth = actor ? baseHealth : static_cast<double>(data.mHealth);

        DestructionOutcome outcome;
        Listener listener;
        {
            const std::lock_guard lock(mMutex);
            const auto found = mStates.find(reference);
            const double health = found == mStates.end() ? -1.0 : found->second.mHealth;
            outcome = DestructionRule::damage(data, maxHealth, health, amount, resulting);
            if (!outcome.mApplied)
                return outcome;
            State& state = mStates[reference];
            state.mHealth = outcome.mHealth;
            if (outcome.mModelStage >= 0)
                state.mModelStage = outcome.mModelStage;
            if (outcome.mDamageStage != 0)
                state.mDamageStage = outcome.mDamageStage;
            if (!outcome.mCrossed.empty())
                state.mSelfDamage = outcome.mSelfDamage;
            // The mark is set with the state, under the same lock as a clear takes it away under: a clear that comes after
            // this blow also takes this mark away, and one that came before it is not undone (the state it cleared is gone)
            if (outcome.mDestroyed)
                mReferences.setDestroyed(reference, true);
            listener = mListener;
        }
        if (listener)
            listener(reference, outcome);
        return outcome;
    }

    int Destruction::stage(FormId reference)
    {
        DestructionData data;
        if (!dataOf(reference, data))
            return -1;
        const double baseHealth = mRecords.destructionBaseHealth(reference);
        const double maxHealth = baseHealth >= 0 ? baseHealth : static_cast<double>(data.mHealth);
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(reference);
        if (found == mStates.end() || found->second.mHealth < 0)
            return -1;
        return DestructionRule::stage(data, found->second.mHealth, maxHealth);
    }

    double Destruction::health(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(reference);
        return found == mStates.end() ? -1.0 : found->second.mHealth;
    }

    int Destruction::modelStage(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(reference);
        return found == mStates.end() ? -1 : found->second.mModelStage;
    }

    int Destruction::damageStage(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(reference);
        return found == mStates.end() ? 0 : found->second.mDamageStage;
    }

    int Destruction::selfDamage(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(reference);
        return found == mStates.end() ? 0 : found->second.mSelfDamage;
    }

    bool Destruction::isDamaged(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        return mStates.count(reference) != 0;
    }

    bool Destruction::disabledByStage(FormId reference)
    {
        DestructionData data;
        if (!dataOf(reference, data))
            return false;
        const double baseHealth = mRecords.destructionBaseHealth(reference);
        const double maxHealth = baseHealth >= 0 ? baseHealth : static_cast<double>(data.mHealth);
        const std::lock_guard lock(mMutex);
        const auto found = mStates.find(reference);
        return found != mStates.end() && DestructionRule::disables(data, found->second.mHealth, maxHealth);
    }

    std::vector<FormId> Destruction::damaged()
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> result;
        for (const auto& [reference, state] : mStates)
            result.push_back(reference);
        return result;
    }

    bool Destruction::restore(FormId reference)
    {
        std::function<void(FormId)> listener;
        {
            const std::lock_guard lock(mMutex);
            if (mStates.erase(reference) == 0)
                return false;
            mReferences.setDestroyed(reference, false);
            listener = mRestoreListener;
        }
        if (listener)
            listener(reference);
        return true;
    }

    void Destruction::update(double seconds)
    {
        if (!(seconds > 0))
            return;
        std::vector<std::pair<FormId, int>> burning;
        {
            const std::lock_guard lock(mMutex);
            for (const auto& [reference, state] : mStates)
                if (state.mSelfDamage != 0 && state.mHealth > 0)
                    burning.emplace_back(reference, state.mSelfDamage);
        }
        for (const auto& [reference, perSecond] : burning)
        {
            // (a character takes the damage of its own from the actor values: nothing here does that)
            if (mRecords.destructionBaseHealth(reference) >= 0)
                continue;
            damage(reference, perSecond * seconds);
        }
    }

    void Destruction::forget(FormId reference)
    {
        const std::lock_guard lock(mMutex);
        mStates.erase(reference);
    }

    template <>
    struct SaveAccess<Destruction>
    {
        static void write(Destruction& destruction, SaveWriter& out)
        {
            const std::lock_guard lock(destruction.mMutex);
            out.count(destruction.mStates.size());
            for (const auto& [reference, state] : destruction.mStates)
            {
                out.form(reference);
                out.f64(state.mHealth);
                out.i32(state.mModelStage);
                out.i32(state.mDamageStage);
                out.i32(state.mSelfDamage);
            }
        }

        static void read(Destruction& destruction, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Destruction::State> states;
            const std::size_t count = in.count(24);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> reference = in.form();
                Destruction::State state;
                state.mHealth = in.f64();
                state.mModelStage = in.i32();
                state.mDamageStage = in.i32();
                state.mSelfDamage = in.i32();
                if (reference)
                    states[*reference] = state;
            }
            const std::lock_guard lock(destruction.mMutex);
            destruction.mStates = std::move(states);
        }
    };

    void writeDestruction(Destruction& destruction, SaveWriter& out)
    {
        SaveAccess<Destruction>::write(destruction, out);
    }

    void readDestruction(Destruction& destruction, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Destruction>::read(destruction, in, version);
    }

    void addDestructionCommands(Interpreter& interpreter, Destruction& destruction)
    {
        // The executable's DamageObject is a blow to the reference of the amount given (NV 0x5D4B90 calls the reference's
        // damage, which goes to 0x475B20)
        interpreter.setHandler("DamageObject", [&destruction](CallContext& call) {
            const double amount = argument(call, 0);
            if (call.mReference != 0 && !std::isnan(amount))
                destruction.damage(call.mReference, amount);
            return 0.0;
        });
        interpreter.setHandler("GetDestructionStage", [&destruction](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(destruction.stage(call.mReference)) : -1.0;
        });
        interpreter.setHandler("ClearDestruction", [&destruction](CallContext& call) {
            if (call.mReference != 0)
                destruction.restore(call.mReference);
            return 0.0;
        });
    }
}
