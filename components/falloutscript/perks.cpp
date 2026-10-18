#include "perks.hpp"

#include <algorithm>

#include "actorvalues.hpp"
#include "playerprogress.hpp"

namespace FalloutScript
{
    PerkEngine::PerkEngine(PerkRecords& records, PlayerProgress& progress, ActorValues& values)
        : mRecords(records)
        , mProgress(progress)
        , mValues(values)
    {
    }

    void PerkEngine::setConditions(ConditionCheck check)
    {
        const std::lock_guard lock(mMutex);
        mConditions = std::move(check);
    }

    void PerkEngine::setQuestSetter(std::function<void(FormId, int)> setter)
    {
        const std::lock_guard lock(mMutex);
        mQuestSetter = std::move(setter);
    }

    bool PerkEngine::info(FormId perk, PerkInfo& result)
    {
        {
            const std::lock_guard lock(mMutex);
            const auto known = mKnown.find(perk);
            if (known != mKnown.end())
            {
                result = known->second;
                return true;
            }
            if (mUnknown.count(perk) != 0)
                return false;
        }
        PerkInfo read;
        const bool found = mRecords.perk(perk, read);
        const std::lock_guard lock(mMutex);
        if (!found)
        {
            mUnknown[perk] = true;
            return false;
        }
        result = mKnown.emplace(perk, std::move(read)).first->second;
        return true;
    }

    namespace
    {
        /// What an argument of an entry point after the owner is
        enum class Argument
        {
            None,
            Weapon,
            Other,
        };

        struct Tabs
        {
            Argument mFirst = Argument::None;
            Argument mSecond = Argument::None;
        };

        /// The arguments of the entry points after the owner, from the tables of the executables (the tab 1 and the
        /// tab 2 of the conditions of an entry)
        Tabs tabsOf(int entryPoint)
        {
            switch (entryPoint)
            {
                // damage, critical chance and damage, to hit, the reward of a kill, the threshold of the attacker, and
                // the critical chance against the owner (the owner is hit, the other is the attacker)
                case 0:
                case 1:
                case 2:
                case 8:
                case 35:
                case 36:
                case 58:
                    return { Argument::Weapon, Argument::Other };
                // limb damage and the threshold of the defender: the owner is hit, then the attacker and its weapon
                case 6:
                case 56:
                    return { Argument::Other, Argument::Weapon };
                // a weapon (or an item) and nothing else
                case 3:
                case 4:
                case 17:
                case 34:
                case 37:
                case 38:
                case 39:
                case 40:
                case 43:
                case 52:
                case 53:
                case 57:
                case 59:
                case 72:
                    return { Argument::Weapon, Argument::None };
                // a leveled list on death and an activation: the other character
                case 21:
                case 27:
                    return { Argument::Other, Argument::None };
                default:
                    return {};
            }
        }
    }

    ConditionContext PerkEngine::contextFor(
        const PerkConditionGroup& group, int entryPoint, FormId owner, const Context& context) const
    {
        ConditionContext result;
        result.mSubject = owner;
        result.mTarget = context.mOther;
        if (!group.mHasRunOn)
            return result;
        const Tabs tabs = tabsOf(entryPoint);
        Argument argument = Argument::None;
        if (group.mRunOn == 1)
            argument = tabs.mFirst;
        else if (group.mRunOn == 2)
            argument = tabs.mSecond;
        switch (argument)
        {
            case Argument::Other:
                result.mSubject = context.mOther;
                result.mTarget = owner;
                break;
            case Argument::Weapon:
                if (context.mWeapon != 0)
                    result.mSubject = context.mWeapon;
                break;
            case Argument::None:
                break;
        }
        return result;
    }

    bool PerkEngine::holds(const PerkEntry& entry, FormId owner, const Context& context) const
    {
        if (!mConditions)
            return entry.mGroups.empty();
        for (const PerkConditionGroup& group : entry.mGroups)
            if (!group.mConditions.empty()
                && !mConditions(group.mConditions, contextFor(group, entry.mEntryPoint, owner, context)))
                return false;
        return true;
    }

    double PerkEngine::apply(int entryPoint, FormId owner, double value)
    {
        return apply(entryPoint, owner, value, Context());
    }

    double PerkEngine::apply(int entryPoint, FormId owner, double value, const Context& context)
    {
        if (owner == 0)
            return value;
        const std::vector<std::pair<FormId, int>> perks = mProgress.perksOf(owner);
        if (perks.empty())
            return value;

        // The entries that are live for the entry point, in the order they were put in the list of the games
        std::vector<PerkEntry> live;
        for (const auto& [perk, rank] : perks)
        {
            PerkInfo perkInfo;
            if (!info(perk, perkInfo))
                continue;
            for (PerkEntry& entry : perkInfo.mEntries)
            {
                if (entry.mType != PerkEntry::EntryPoint || entry.mEntryPoint != entryPoint)
                    continue;
                // Each rank has the entries of its own
                if (entry.mRank != std::min(rank, perkInfo.mRanks) - 1)
                    continue;
                live.push_back(std::move(entry));
            }
        }
        // The games put each entry before the ones of the same priority and rank that are already in the list, so the
        // last one in goes first; a higher priority comes before a lower one, a lower rank before a higher one
        std::reverse(live.begin(), live.end());
        std::stable_sort(live.begin(), live.end(), [](const PerkEntry& a, const PerkEntry& b) {
            return a.mPriority != b.mPriority ? a.mPriority > b.mPriority : a.mRank < b.mRank;
        });
        for (const PerkEntry& entry : live)
        {
            if (!holds(entry, owner, context))
                continue;
            switch (entry.mFunction)
            {
                case 1:
                    value = entry.mValue;
                    break;
                case 2:
                    value += entry.mValue;
                    break;
                case 3:
                    value *= entry.mValue;
                    break;
                default:
                    break;
            }
        }
        return value;
    }

    void PerkEngine::refresh(FormId owner)
    {
        if (owner == 0)
            return;
        // The check of what is applied and the applying of what is missing are one step: of two refreshes at the
        // same time, the second one finds the work of the first one done and applies nothing again
        std::unique_lock refreshing(mRefreshMutex);
        const std::vector<std::pair<FormId, int>> perks = mProgress.perksOf(owner);

        // What was applied for a perk that is gone, or for a rank that is no longer the rank, is taken back
        std::vector<std::pair<FormId, FormId>> stale;
        {
            const std::lock_guard lock(mMutex);
            for (const auto& [key, applied] : mApplied)
            {
                if (key.first != owner)
                    continue;
                const auto has = std::find_if(perks.begin(), perks.end(), [&key, &applied](const auto& perk) {
                    return perk.first == key.second && perk.second == applied.mRank;
                });
                if (has == perks.end())
                    stale.push_back(key);
            }
        }
        for (const auto& key : stale)
        {
            Applied applied;
            {
                const std::lock_guard lock(mMutex);
                applied = std::move(mApplied[key]);
                mApplied.erase(key);
            }
            for (const AbilityEffect& effect : applied.mEffects)
                mValues.modify(owner, effect.mActorValue, -effect.mMagnitude);
        }

        // The quest stages that a perk sets are set when the refresh is done, with nothing held: setting a stage may
        // run a script, which may give another perk and refresh again
        std::vector<std::pair<FormId, int>> pendingStages;
        for (const auto& [perk, rank] : perks)
        {
            {
                const std::lock_guard lock(mMutex);
                const auto found = mApplied.find({ owner, perk });
                if (found != mApplied.end() && found->second.mRank == rank)
                    continue;
            }
            PerkInfo perkInfo;
            if (!info(perk, perkInfo))
                continue;
            const int rankIndex = std::min(rank, perkInfo.mRanks) - 1;
            Applied applied;
            applied.mRank = rank;
            std::vector<std::pair<FormId, int>> stages;
            for (const PerkEntry& entry : perkInfo.mEntries)
            {
                if (entry.mRank != rankIndex)
                    continue;
                if (entry.mType == PerkEntry::Ability)
                {
                    std::vector<AbilityEffect> effects;
                    if (mRecords.abilityEffects(entry.mAbility, effects))
                        applied.mEffects.insert(applied.mEffects.end(), effects.begin(), effects.end());
                }
                else if (entry.mType == PerkEntry::QuestStage && entry.mQuest != 0)
                    stages.emplace_back(entry.mQuest, entry.mStage);
            }
            for (const AbilityEffect& effect : applied.mEffects)
                mValues.modify(owner, effect.mActorValue, effect.mMagnitude);
            {
                const std::lock_guard lock(mMutex);
                mApplied[{ owner, perk }] = std::move(applied);
            }
            pendingStages.insert(pendingStages.end(), stages.begin(), stages.end());
        }

        std::function<void(FormId, int)> setter;
        {
            const std::lock_guard lock(mMutex);
            setter = mQuestSetter;
        }
        refreshing.unlock();
        if (setter)
            for (const auto& [quest, stage] : pendingStages)
                setter(quest, stage);
    }

    std::vector<PerkEngine::Offer> PerkEngine::levelUpList(FormId owner, int level)
    {
        struct Candidate
        {
            FormId mPerk;
            bool mAvailable;
            int mLevel;
            std::string mName;
        };
        ConditionCheck check;
        {
            const std::lock_guard lock(mMutex);
            check = mConditions;
        }
        std::vector<Candidate> candidates;
        for (const FormId perk : mRecords.perks())
        {
            PerkInfo perkInfo;
            if (!info(perk, perkInfo))
                continue;
            if (!perkInfo.mPlayable || perkInfo.mTrait || perkInfo.mMinimumLevel <= 0)
                continue;
            if (mProgress.perkRank(owner, perk) >= perkInfo.mRanks)
                continue;
            ConditionContext context;
            context.mSubject = owner;
            // The test of the sex is made for every perk, the others only decide whether the perk is available
            if (!perkInfo.mRequirements.empty() && perkInfo.mRequirements.back().functionIndex == ESM4::FUN_GetIsSex
                && (!check || !check({ perkInfo.mRequirements.back() }, context)))
                continue;
            bool available = perkInfo.mMinimumLevel <= level;
            if (available && !perkInfo.mRequirements.empty())
                available = check && check(perkInfo.mRequirements, context);
            candidates.push_back({ perk, available, perkInfo.mMinimumLevel, perkInfo.mName });
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.mAvailable != b.mAvailable)
                return a.mAvailable;
            return a.mLevel != b.mLevel ? a.mLevel < b.mLevel : a.mName < b.mName;
        });
        std::vector<Offer> result;
        for (const Candidate& candidate : candidates)
            result.push_back({ candidate.mPerk, candidate.mAvailable });
        return result;
    }

    std::vector<FormId> PerkEngine::offered(FormId owner, int level)
    {
        std::vector<FormId> result;
        for (const Offer& offer : levelUpList(owner, level))
            if (offer.mAvailable)
                result.push_back(offer.mPerk);
        return result;
    }

    void PerkEngine::clear()
    {
        const std::lock_guard lock(mMutex);
        mApplied.clear();
    }
}
