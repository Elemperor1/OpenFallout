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

    void ItemUse::setBookRules(double skillCap, double points)
    {
        const std::lock_guard lock(mMutex);
        mSkillCap = skillCap;
        mBookPoints = points;
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
            // A level goes up when it is hurt and down when it is restored, and stays between none and the most
            const double now = mValues.current(actor, actorValue);
            mValues.setBase(actor, actorValue, std::max(now + (detrimental ? amount : -amount), 0.0));
        }
        else if (detrimental && actorValue == ActorValue::health)
        {
            // No blow comes between the damage to the health and the death or fall that follows
            const auto transitions = mState.holdTransitions();
            mValues.damage(actor, actorValue, amount);
            // A character with no health left is dead, unless it is one that falls (an essential one)
            if (mValues.current(actor, actorValue) <= 0 && !mState.falls(actor))
                mState.setDead(actor, true);
        }
        else if (detrimental)
            mValues.damage(actor, actorValue, amount);
        else
            mValues.restore(actor, actorValue, amount);
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
            active.mMagnitude = effect.mDetrimental ? -effect.mMagnitude : effect.mMagnitude;
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
        double skillCap = 100;
        double bookPoints = 1;
        FormId player = 0;
        {
            const std::lock_guard lock(mMutex);
            check = mConditions;
            perks = mPerks;
            skillCap = mSkillCap;
            bookPoints = mBookPoints;
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
            // How long the effects last may be changed by a perk (Chemist doubles the time of a chem)
            const double durationScale = perks != nullptr ? perks->apply(EntryPoint::chemDuration, actor, 1.0) : 1.0;
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
                applyEffect(actor, effect, durationScale);
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
            const double skill = mValues.base(actor, book.mSkill);
            if (skill >= skillCap)
            {
                result.mOutcome = Outcome::NothingToLearn;
                return result;
            }
            // The same as for an aid item: the call that takes the book is the one that reads it
            if (mState.removeItem(actor, item, 1) < 1)
                return result;
            double points = bookPoints;
            if (perks != nullptr)
                points = perks->apply(EntryPoint::bookSkillPoints, actor, points);
            const double gained = std::min(points, skillCap - skill);
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
        {
            const std::lock_guard lock(mMutex);
            for (Active& active : mActive)
            {
                const double elapsed = std::min(seconds, active.mRemaining);
                if (!active.mTemporary)
                    steps.push_back({ active.mActor, active.mActorValue, std::abs(active.mMagnitude) * elapsed,
                        active.mMagnitude < 0 });
                active.mRemaining -= seconds;
                if (active.mRemaining <= 0 && active.mTemporary)
                    mValues.modify(active.mActor, active.mActorValue, -active.mMagnitude);
            }
            mActive.erase(
                std::remove_if(mActive.begin(), mActive.end(), [](const Active& a) { return a.mRemaining <= 0; }),
                mActive.end());
        }
        // A death that comes of it is handled with the lock free: the host reacts to it
        for (const Step& step : steps)
            instant(step.mActor, step.mActorValue, step.mAmount, step.mDetrimental);
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
