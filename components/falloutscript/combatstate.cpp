#include "combatstate.hpp"

#include <algorithm>

namespace FalloutScript
{
    namespace
    {
        /// How long a character that is in no fight counts as attacked after a blow
        constexpr float attackedSeconds = 2.f;
        constexpr std::size_t mostChanges = 1024;
    }

    void CombatState::clear()
    {
        const std::lock_guard lock(mMutex);
        mTargets.clear();
        mAlert.clear();
        mUnconscious.clear();
        mWakeTimers.clear();
        mWoken.clear();
        mRestrained.clear();
        mEssential.clear();
        mRelations.clear();
        mAttacked.clear();
        mHitTimes.clear();
        mClock = 0;
        mLastHits.clear();
        mHitEvents.clear();
        mChanges.clear();
    }

    void CombatState::noteChange(FormId actor, FormId target, FormId was)
    {
        if (mChanges.size() < mostChanges)
            mChanges.push_back({ actor, target, was });
    }

    bool CombatState::inFight(FormId actor) const
    {
        return mTargets.count(actor) != 0 || std::any_of(mTargets.begin(), mTargets.end(), [actor](const auto& entry) {
            return entry.second == actor;
        });
    }

    void CombatState::settleAttacked(FormId actor)
    {
        if (!inFight(actor))
            mAttacked.erase(actor);
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
        // The one it fought is out of its last fight if nobody else fights it
        if (was != 0)
            settleAttacked(was);
    }

    bool CombatState::startUnlessOut(FormId actor, FormId target)
    {
        if (actor == 0 || target == 0 || actor == target)
            return false;
        const std::lock_guard lock(mMutex);
        if (mUnconscious.count(actor) != 0)
            return false;
        FormId& current = mTargets[actor];
        if (current != target)
        {
            const FormId was = current;
            current = target;
            noteChange(actor, target, was);
            if (was != 0)
                settleAttacked(was);
        }
        return true;
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
        settleAttacked(actor);
        settleAttacked(was);
    }

    void CombatState::withdraw(FormId actor)
    {
        // In one go: a fight that starts against the character while it is withdrawn from the others would be left
        const std::lock_guard lock(mMutex);
        std::vector<FormId> left;
        for (auto it = mTargets.begin(); it != mTargets.end();)
        {
            if (it->first == actor || it->second == actor)
            {
                noteChange(it->first, 0, it->second);
                left.push_back(it->first);
                left.push_back(it->second);
                it = mTargets.erase(it);
            }
            else
                ++it;
        }
        // Those who left their last fight are not attacked any more
        for (const FormId gone : left)
            settleAttacked(gone);
        settleAttacked(actor);
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

    void CombatState::setUnconscious(FormId actor, bool unconscious, float wakeAfter)
    {
        const std::lock_guard lock(mMutex);
        if (unconscious)
        {
            mUnconscious.insert(actor);
            if (wakeAfter > 0)
                mWakeTimers[actor] = wakeAfter;
        }
        else
        {
            mUnconscious.erase(actor);
            mWakeTimers.erase(actor);
        }
    }

    std::vector<FormId> CombatState::takeWoken()
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> woken;
        woken.swap(mWoken);
        return woken;
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
        if (hit.mAttacker != 0)
        {
            std::vector<double>& times = mHitTimes[victim][hit.mAttacker];
            std::erase_if(times, [this](double time) { return mClock - time >= mHitLife; });
            if (times.empty() || mClock - times.back() >= mHitSpacing)
                times.push_back(mClock);
        }
        mLastHits[victim] = hit;
        if (mHitEvents.size() < mostChanges)
            mHitEvents.push_back({ victim, hit });
    }

    bool CombatState::attacked(FormId actor) const
    {
        const std::lock_guard lock(mMutex);
        return mAttacked.count(actor) != 0;
    }

    std::vector<std::pair<FormId, int>> CombatState::hitsTaken(FormId victim) const
    {
        const std::lock_guard lock(mMutex);
        std::vector<std::pair<FormId, int>> result;
        const auto found = mHitTimes.find(victim);
        if (found == mHitTimes.end())
            return result;
        for (const auto& [attacker, times] : found->second)
            if (const auto count
                = std::count_if(times.begin(), times.end(), [this](double time) { return mClock - time < mHitLife; });
                count > 0)
                result.emplace_back(attacker, static_cast<int>(count));
        return result;
    }

    void CombatState::setFriendHitRules(double lifeSeconds, double spacingSeconds)
    {
        const std::lock_guard lock(mMutex);
        mHitLife = std::max(lifeSeconds, 0.0);
        mHitSpacing = std::max(spacingSeconds, 0.0);
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
        mClock += seconds;
        // Blows that expired are forgotten
        for (auto victim = mHitTimes.begin(); victim != mHitTimes.end();)
        {
            for (auto attacker = victim->second.begin(); attacker != victim->second.end();)
            {
                std::erase_if(attacker->second, [this](double time) { return mClock - time >= mHitLife; });
                attacker = attacker->second.empty() ? victim->second.erase(attacker) : std::next(attacker);
            }
            victim = victim->second.empty() ? mHitTimes.erase(victim) : std::next(victim);
        }
        for (auto it = mAttacked.begin(); it != mAttacked.end();)
        {
            // In a fight, it stays attacked until the fight is over
            if (inFight(it->first))
            {
                ++it;
                continue;
            }
            it->second -= seconds;
            it = it->second <= 0 ? mAttacked.erase(it) : std::next(it);
        }
        for (auto it = mWakeTimers.begin(); it != mWakeTimers.end();)
        {
            it->second -= seconds;
            if (it->second <= 0)
            {
                mUnconscious.erase(it->first);
                mWoken.push_back(it->first);
                it = mWakeTimers.erase(it);
            }
            else
                ++it;
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
