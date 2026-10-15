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

    const BodyPart* Limbs::byType(const std::vector<BodyPart>* parts, int type)
    {
        if (parts == nullptr || type < 0)
            return nullptr;
        const auto found
            = std::find_if(parts->begin(), parts->end(), [type](const BodyPart& part) { return part.mType == type; });
        return found != parts->end() ? &*found : nullptr;
    }

    double Limbs::damageMultiplier(FormId actor, int type)
    {
        const std::lock_guard lock(mMutex);
        const BodyPart* part = byType(cached(actor), type);
        return part != nullptr ? part->mDamageMult : 1.0;
    }

    double Limbs::condition(FormId actor, int type)
    {
        const std::lock_guard lock(mMutex);
        const BodyPart* part = byType(cached(actor), type);
        if (part == nullptr || part->mCondition < 0)
            return 100.0;
        return mValues.current(actor, part->mCondition);
    }

    double Limbs::limbDamage(FormId actor, int type, double damage)
    {
        const std::lock_guard lock(mMutex);
        const BodyPart* part = byType(cached(actor), type);
        if (part == nullptr)
            return 0.0;
        const double health = mValues.permanent(actor, ActorValue::health);
        if (health <= 0)
            return damage;
        const double share = health * part->mHealthPercent * 0.01;
        return share <= 0 ? 0.0 : 100.0 * damage / share;
    }

    Limbs::Hurt Limbs::hurt(FormId actor, int type, double limbDamage)
    {
        Hurt result;
        const std::lock_guard lock(mMutex);
        const BodyPart* part = byType(cached(actor), type);
        if (part == nullptr || part->mCondition < 0 || limbDamage <= 0)
            return result;
        const double before = mValues.current(actor, part->mCondition);
        mValues.damage(actor, part->mCondition, limbDamage);
        result.mCondition = mValues.current(actor, part->mCondition);
        result.mCrippled = before > 0 && result.mCondition <= 0;
        return result;
    }

    void Limbs::clear()
    {
        const std::lock_guard lock(mMutex);
        mParts.clear();
    }
}
