#include "limbs.hpp"

#include <algorithm>

#include "actorvalues.hpp"

namespace FalloutScript
{
    Limbs::Limbs(LimbRecords& records, ActorValues& values)
        : mRecords(records)
        , mValues(values)
    {
    }

    const std::vector<BodyPart>* Limbs::cached(FormId actor)
    {
        const auto found = mParts.find(actor);
        if (found != mParts.end())
            return &found->second;
        std::vector<BodyPart> parts;
        if (!mRecords.bodyParts(actor, parts))
            return nullptr;
        return &mParts.emplace(actor, std::move(parts)).first->second;
    }

    std::vector<BodyPart> Limbs::partsOf(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const std::vector<BodyPart>* parts = cached(actor);
        return parts != nullptr ? *parts : std::vector<BodyPart>();
    }

    double Limbs::damageMultiplier(FormId actor, int part)
    {
        const std::lock_guard lock(mMutex);
        const std::vector<BodyPart>* parts = cached(actor);
        if (parts == nullptr || part < 0 || part >= static_cast<int>(parts->size()))
            return 1.0;
        return (*parts)[part].mDamageMult;
    }

    double Limbs::condition(FormId actor, int part)
    {
        const std::lock_guard lock(mMutex);
        const std::vector<BodyPart>* parts = cached(actor);
        if (parts == nullptr || part < 0 || part >= static_cast<int>(parts->size()) || (*parts)[part].mCondition < 0)
            return 100.0;
        return mValues.current(actor, (*parts)[part].mCondition);
    }

    Limbs::Hurt Limbs::hurt(FormId actor, int part, double damage)
    {
        Hurt result;
        const std::lock_guard lock(mMutex);
        const std::vector<BodyPart>* parts = cached(actor);
        if (parts == nullptr || part < 0 || part >= static_cast<int>(parts->size()) || damage <= 0)
            return result;
        const BodyPart& hit = (*parts)[part];
        if (hit.mCondition < 0)
            return result;
        const double before = mValues.current(actor, hit.mCondition);
        const double health = std::max(mValues.permanent(actor, ActorValue::health), 1.0);
        const double share = std::max(health * hit.mHealthPercent / 100.0, 1.0);
        mValues.damage(actor, hit.mCondition, 100.0 * damage / share);
        result.mCondition = mValues.current(actor, hit.mCondition);
        result.mCrippled = before > 0 && result.mCondition <= 0;
        return result;
    }

    void Limbs::clear()
    {
        const std::lock_guard lock(mMutex);
        mParts.clear();
    }
}
