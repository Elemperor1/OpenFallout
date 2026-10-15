#include "combatstate.hpp"

#include <algorithm>

namespace FalloutScript
{
    namespace
    {
        /// How long a character counts as attacked after a blow
        constexpr float attackedSeconds = 2.f;
        constexpr std::size_t mostChanges = 1024;
    }

    void CombatState::clear()
    {
        const std::lock_guard lock(mMutex);
        mTargets.clear();
        mAlert.clear();
        mUnconscious.clear();
        mRestrained.clear();
        mEssential.clear();
        mRelations.clear();
        mAttacked.clear();
        mLastHits.clear();
        mHitEvents.clear();
        mChanges.clear();
    }

    void CombatState::noteChange(FormId actor, FormId target, FormId was)
    {
        if (mChanges.size() < mostChanges)
            mChanges.push_back({ actor, target, was });
    }

    void CombatState::start(FormId actor, FormId target)
    {
        if (actor == 0 || target == 0 || actor == target)
            return;
        const std::lock_guard lock(mMutex);
        FormId& current = mTargets[actor];
        if (current == target)
            return;
        const FormId was = current;
        current = target;
        noteChange(actor, target, was);
    }

    void CombatState::stop(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mTargets.find(actor);
        if (found == mTargets.end())
            return;
        const FormId was = found->second;
        mTargets.erase(found);
        noteChange(actor, 0, was);
    }

    void CombatState::withdraw(FormId actor)
    {
        stop(actor);
        for (const FormId pursuer : attackersOf(actor))
            stop(pursuer);
    }

    FormId CombatState::target(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mTargets.find(actor);
        return found == mTargets.end() ? 0 : found->second;
    }

    bool CombatState::isFought(FormId target) const
    {
        const std::lock_guard lock(mMutex);
        return std::any_of(
            mTargets.begin(), mTargets.end(), [target](const auto& entry) { return entry.second == target; });
    }

    std::vector<FormId> CombatState::attackersOf(FormId target) const
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> result;
        for (const auto& [actor, fought] : mTargets)
            if (fought == target)
                result.push_back(actor);
        return result;
    }

    void CombatState::setAlert(FormId actor, bool alert)
    {
        const std::lock_guard lock(mMutex);
        if (alert)
            mAlert.insert(actor);
        else
            mAlert.erase(actor);
    }

    bool CombatState::alert(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mAlert.count(actor) != 0;
    }

    void CombatState::setUnconscious(FormId actor, bool unconscious)
    {
        const std::lock_guard lock(mMutex);
        if (unconscious)
            mUnconscious.insert(actor);
        else
            mUnconscious.erase(actor);
    }

    bool CombatState::unconscious(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mUnconscious.count(actor) != 0;
    }

    void CombatState::setRestrained(FormId actor, bool restrained)
    {
        const std::lock_guard lock(mMutex);
        if (restrained)
            mRestrained.insert(actor);
        else
            mRestrained.erase(actor);
    }

    bool CombatState::restrained(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mRestrained.count(actor) != 0;
    }

    void CombatState::setEssential(FormId actor, bool essential)
    {
        const std::lock_guard lock(mMutex);
        mEssential[actor] = essential;
    }

    std::optional<bool> CombatState::essential(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEssential.find(actor);
        if (found == mEssential.end())
            return std::nullopt;
        return found->second;
    }

    void CombatState::setRelation(FormId first, FormId second, Relation relation)
    {
        if (first == 0 || second == 0)
            return;
        const std::lock_guard lock(mMutex);
        mRelations[{ first, second }] = relation;
    }

    std::optional<CombatState::Relation> CombatState::relation(FormId first, FormId second) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mRelations.find({ first, second });
        if (found == mRelations.end())
            return std::nullopt;
        return found->second;
    }

    void CombatState::noteHit(FormId victim, const Hit& hit)
    {
        const std::lock_guard lock(mMutex);
        mAttacked[victim] = attackedSeconds;
        mLastHits[victim] = hit;
        if (mHitEvents.size() < mostChanges)
            mHitEvents.push_back({ victim, hit });
    }

    bool CombatState::attacked(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mAttacked.count(actor) != 0;
    }

    bool CombatState::lastHit(FormId actor, Hit& hit) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mLastHits.find(actor);
        if (found == mLastHits.end())
            return false;
        hit = found->second;
        return true;
    }

    void CombatState::advance(float seconds)
    {
        const std::lock_guard lock(mMutex);
        for (auto it = mAttacked.begin(); it != mAttacked.end();)
        {
            it->second -= seconds;
            it = it->second <= 0 ? mAttacked.erase(it) : std::next(it);
        }
    }

    std::vector<CombatState::HitEvent> CombatState::takeHits()
    {
        const std::lock_guard lock(mMutex);
        std::vector<HitEvent> hits;
        hits.swap(mHitEvents);
        return hits;
    }

    std::vector<CombatState::Change> CombatState::takeChanges()
    {
        const std::lock_guard lock(mMutex);
        std::vector<Change> changes;
        changes.swap(mChanges);
        return changes;
    }
}
