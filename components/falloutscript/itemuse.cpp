#include "itemuse.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "actors.hpp"
#include "actorvalues.hpp"
#include "perks.hpp"
#include "playerprogress.hpp"

namespace FalloutScript
{
    namespace
    {
        // The stats of the Pip-Boy that using an item counts (the table of the executables)
        constexpr int statStimpaksTaken = 6;
        constexpr int statRadXTaken = 7;
        constexpr int statRadAwayTaken = 8;
        constexpr int statChemsTaken = 9;
        constexpr int statTimesAddicted = 10;
        constexpr int statBooksRead = 15;

        constexpr int radiationValue = 54;
        constexpr int radiationResistance = 20;
    }

    bool isLevelValue(int actorValue)
    {
        return actorValue == radiationValue || (actorValue >= 73 && actorValue <= 75);
    }

    ItemUse::ItemUse(UseRecords& records, ActorState& state, ActorValues& values, PlayerProgress& progress)
        : mRecords(records)
        , mState(state)
        , mValues(values)
        , mProgress(progress)
    {
        mRandom = [engine = std::mt19937(std::random_device{}())]() mutable {
            return std::uniform_real_distribution<double>(0.0, 1.0)(engine);
        };
    }

    void ItemUse::setPerks(PerkEngine* perks)
    {
        const std::lock_guard lock(mMutex);
        mPerks = perks;
    }

    void ItemUse::setConditions(
        std::function<bool(const std::vector<ESM4::TargetCondition>&, const ConditionContext&)> check)
    {
        const std::lock_guard lock(mMutex);
        mConditions = std::move(check);
    }

    void ItemUse::setRandom(std::function<double()> random)
    {
        const std::lock_guard lock(mMutex);
        mRandom = std::move(random);
    }

    void ItemUse::setBookRules(double points)
    {
        const std::lock_guard lock(mMutex);
        mBookPoints = points;
    }

    void ItemUse::setChemDurationNeedsAddiction(bool needsAddiction)
    {
        const std::lock_guard lock(mMutex);
        mChemDurationNeedsAddiction = needsAddiction;
    }

    void ItemUse::clear()
    {
        const std::lock_guard lock(mMutex);
        mActive.clear();
        mAddictions.clear();
    }

    void ItemUse::instant(FormId actor, int actorValue, double amount, bool detrimental)
    {
        if (!ActorValues::isValid(actorValue) || amount <= 0)
            return;
        if (isLevelValue(actorValue))
        {
            // A level goes up when it is hurt and down when it is restored, and does not go below none. Its modifiers
            // and damage stay as they are, and a change made at the same time counts too.
            mValues.addBase(actor, actorValue, detrimental ? amount : -amount, 0.0);
        }
        else if (detrimental && actorValue == ActorValue::health)
        {
            // No blow comes between the damage to the health and the death or fall that follows
            const auto transitions = mState.holdTransitions();
            mValues.damage(actor, actorValue, amount);
            dieIfSpent(actor);
        }
        else if (detrimental)
            mValues.damage(actor, actorValue, amount);
        else
        {
            // The health that an effect gives back is the value of an entry point (12, Modify Recovered Health), for
            // what happens at once and for each second of an effect that lasts
            if (actorValue == ActorValue::health)
            {
                PerkEngine* perks = nullptr;
                {
                    const std::lock_guard lock(mMutex);
                    perks = mPerks;
                }
                if (perks != nullptr)
                    amount = std::max(perks->apply(EntryPoint::recoveredHealth, actor, amount), 0.0);
            }
            mValues.restore(actor, actorValue, amount);
        }
    }

    void ItemUse::dieIfSpent(FormId actor)
    {
        // A character with no health left is dead, unless it is one that falls (an essential one)
        if (mValues.current(actor, ActorValue::health) <= 0 && !mState.isDead(actor) && !mState.falls(actor))
            mState.setDead(actor, true);
    }

    void ItemUse::checkHealth(FormId actor)
    {
        if (actor == 0)
            return;
        const auto transitions = mState.holdTransitions();
        dieIfSpent(actor);
    }

    void ItemUse::applyEffect(FormId actor, const ConsumableEffect& effect, double durationScale)
    {
        if (effect.mArchetype == 35)
        {
            // Restores the limbs
            for (int limb = ActorValue::firstCondition; limb <= ActorValue::lastCondition; ++limb)
                mValues.heal(actor, limb);
            return;
        }
        if (effect.mArchetype != 0 && effect.mArchetype != 34)
            return;
        if (!ActorValues::isValid(effect.mActorValue))
            return;
        const double duration = effect.mDuration * durationScale;
        if (duration <= 0)
        {
            instant(actor, effect.mActorValue, effect.mMagnitude, effect.mDetrimental);
            return;
        }
        Active active;
        active.mActor = actor;
        active.mActorValue = effect.mActorValue;
        active.mRemaining = duration;
        if (effect.mRecover)
        {
            // A boost that holds while the effect lasts (a drop, for a detrimental one)
            active.mTemporary = true;
            // A level (radiation, thirst, hunger, sleep) goes up when it is hurt: the drop of a detrimental effect is
            // a rise of the level
            active.mMagnitude
                = effect.mDetrimental == isLevelValue(effect.mActorValue) ? effect.mMagnitude : -effect.mMagnitude;
            if (effect.mActorValue == ActorValue::health && active.mMagnitude < 0)
            {
                // A drop of the health can take all of it: no blow comes between the drop and the death or fall
                const auto transitions = mState.holdTransitions();
                mValues.modify(actor, effect.mActorValue, active.mMagnitude);
                dieIfSpent(actor);
            }
            else
                mValues.modify(actor, effect.mActorValue, active.mMagnitude);
        }
        else
        {
            // Points each second; a detrimental effect hurts
            active.mTemporary = false;
            active.mMagnitude = effect.mDetrimental ? -effect.mMagnitude : effect.mMagnitude;
        }
        const std::lock_guard lock(mMutex);
        mActive.push_back(active);
    }

    void ItemUse::setPlayer(FormId player)
    {
        const std::lock_guard lock(mMutex);
        mPlayer = player;
    }

    ItemUse::Result ItemUse::use(FormId actor, FormId item)
    {
        Result result;
        if (actor == 0 || item == 0 || mState.itemCount(actor, item) <= 0)
            return result;

        std::function<bool(const std::vector<ESM4::TargetCondition>&, const ConditionContext&)> check;
        PerkEngine* perks = nullptr;
        double bookPoints = 1;
        bool durationNeedsAddiction = false;
        FormId player = 0;
        {
            const std::lock_guard lock(mMutex);
            check = mConditions;
            perks = mPerks;
            bookPoints = mBookPoints;
            durationNeedsAddiction = mChemDurationNeedsAddiction;
            player = mPlayer;
        }
        const bool counts = actor == player;

        Consumable consumable;
        if (mRecords.consumable(item, consumable))
        {
            // Of two calls for the last copy, the one that takes it is the one that uses it
            if (mState.removeItem(actor, item, 1) < 1)
                return result;
            result.mOutcome = Outcome::Consumed;
            bool stimpak = false;
            bool restoresRadiation = false;
            bool resistsRadiation = false;
            // How long the effects last may be changed by a perk (Chemist doubles the time of a chem), but not those of
            // a hostile effect, and in Fallout 3 only of an item that can make one addicted
            double durationScale = 1.0;
            if (perks != nullptr && (!durationNeedsAddiction || consumable.mWithdrawal != 0))
                durationScale = perks->apply(EntryPoint::chemDuration, actor, 1.0);
            for (const ConsumableEffect& effect : consumable.mEffects)
            {
                if (!effect.mConditions.empty())
                {
                    ConditionContext context;
                    context.mSubject = actor;
                    if (!check || !check(effect.mConditions, context))
                        continue;
                }
                stimpak = stimpak || effect.mArchetype == 34;
                restoresRadiation = restoresRadiation
                    || (effect.mArchetype == 0 && effect.mActorValue == radiationValue && !effect.mDetrimental);
                resistsRadiation = resistsRadiation
                    || (effect.mArchetype == 0 && effect.mActorValue == radiationResistance && !effect.mDetrimental);
                applyEffect(actor, effect, effect.mHostile ? 1.0 : durationScale);
            }

            const bool addictive = consumable.mWithdrawal != 0 && consumable.mAddictionChance > 0;
            if (addictive)
            {
                double chance = consumable.mAddictionChance;
                if (perks != nullptr)
                    chance = perks->apply(EntryPoint::addictionChance, actor, chance);
                double roll = 1;
                {
                    const std::lock_guard lock(mMutex);
                    roll = mRandom();
                }
                if (roll < chance)
                {
                    const std::lock_guard lock(mMutex);
                    const auto already = std::find(mAddictions.begin(), mAddictions.end(), std::make_pair(actor, item));
                    // One who is addicted already does not become addicted again
                    if (already == mAddictions.end())
                    {
                        mAddictions.emplace_back(actor, item);
                        result.mAddicted = true;
                    }
                }
            }

            if (counts)
            {
                if (stimpak)
                    mProgress.modifyMiscStat(statStimpaksTaken, 1);
                if (resistsRadiation)
                    mProgress.modifyMiscStat(statRadXTaken, 1);
                if (restoresRadiation)
                    mProgress.modifyMiscStat(statRadAwayTaken, 1);
                if (consumable.mWithdrawal != 0)
                    mProgress.modifyMiscStat(statChemsTaken, 1);
                if (result.mAddicted)
                    mProgress.modifyMiscStat(statTimesAddicted, 1);
            }
            return result;
        }

        BookInfo book;
        if (mRecords.book(item, book))
        {
            result.mSkill = book.mSkill;
            if (book.mSkill < 0 || !ActorValues::isValid(book.mSkill))
            {
                // A book to read stays with its owner
                result.mOutcome = Outcome::Read;
                if (counts)
                    mProgress.modifyMiscStat(statBooksRead, 1);
                return result;
            }
            // Two books read at the same time add up: the skill is read, raised and written by one at a time
            const std::lock_guard reading(mBookMutex);
            const double skill = mValues.base(actor, book.mSkill);
            // The same as for an aid item: the call that takes the book is the one that reads it
            if (mState.removeItem(actor, item, 1) < 1)
                return result;
            double points = bookPoints;
            if (perks != nullptr)
                points = perks->apply(EntryPoint::bookSkillPoints, actor, points);
            const double gained = std::max(points, 0.0);
            mValues.setBase(actor, book.mSkill, skill + gained);
            result.mOutcome = Outcome::Read;
            result.mGained = gained;
            if (counts)
                mProgress.modifyMiscStat(statBooksRead, 1);
        }
        return result;
    }

    void ItemUse::update(double seconds)
    {
        if (seconds <= 0)
            return;
        struct Step
        {
            FormId mActor;
            int mActorValue;
            double mAmount;
            bool mDetrimental;
        };
        std::vector<Step> steps;
        // A change that held while an effect lasted, which is taken back now
        struct Expiry
        {
            FormId mActor;
            int mActorValue;
            double mMagnitude;
        };
        std::vector<Expiry> expired;
        PerkEngine* perks = nullptr;
        FormId player = 0;
        {
            const std::lock_guard lock(mMutex);
            perks = mPerks;
            player = mPlayer;
            for (Active& active : mActive)
            {
                const double elapsed = std::min(seconds, active.mRemaining);
                if (!active.mTemporary)
                    steps.push_back({ active.mActor, active.mActorValue, std::abs(active.mMagnitude) * elapsed,
                        active.mMagnitude < 0 });
                active.mRemaining -= seconds;
                if (active.mRemaining <= 0 && active.mTemporary)
                    expired.push_back({ active.mActor, active.mActorValue, active.mMagnitude });
            }
            mActive.erase(
                std::remove_if(mActive.begin(), mActive.end(), [](const Active& a) { return a.mRemaining <= 0; }),
                mActive.end());
        }
        // A death that comes of it is handled with the lock free: the host reacts to it
        for (const Expiry& expiry : expired)
        {
            if (expiry.mActorValue == ActorValue::health && expiry.mMagnitude > 0)
            {
                // A character that lived on the health a boost gave dies, or falls, when the boost ends
                const auto transitions = mState.holdTransitions();
                mValues.modify(expiry.mActor, expiry.mActorValue, -expiry.mMagnitude);
                dieIfSpent(expiry.mActor);
            }
            else
                mValues.modify(expiry.mActor, expiry.mActorValue, -expiry.mMagnitude);
        }
        for (const Step& step : steps)
            instant(step.mActor, step.mActorValue, step.mAmount, step.mDetrimental);

        // The perks that recover health: the executables work the entry point out from none, in points each second
        // (the recovery that the characters have of themselves is not modelled)
        if (perks != nullptr && player != 0 && !mState.isDead(player))
        {
            const double rate = perks->apply(EntryPoint::recoveredHealth, player, 0.0);
            if (rate > 0)
                mValues.restore(player, ActorValue::health, rate * seconds);
        }
    }

    std::vector<ItemUse::Active> ItemUse::active(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        std::vector<Active> result;
        for (const Active& active : mActive)
            if (active.mActor == actor)
                result.push_back(active);
        return result;
    }

    std::vector<FormId> ItemUse::addictions(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> result;
        for (const auto& [holder, item] : mAddictions)
            if (holder == actor)
                result.push_back(item);
        return result;
    }

    void ItemUse::cure(FormId actor, FormId item)
    {
        const std::lock_guard lock(mMutex);
        mAddictions.erase(
            std::remove(mAddictions.begin(), mAddictions.end(), std::make_pair(actor, item)), mAddictions.end());
    }
}
