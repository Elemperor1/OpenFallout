#include "dismember.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"
#include "interpreter.hpp"

namespace FalloutScript
{
    namespace
    {
        const BodyPart* byType(const std::vector<BodyPart>& parts, int type)
        {
            for (const BodyPart& part : parts)
                if (part.mType == type)
                    return &part;
            return nullptr;
        }

        const BodyPart* byCondition(const std::vector<BodyPart>& parts, int condition)
        {
            // The parts are looked at in the order of their types, as the executable's loop over the 15 does
            for (int type = 0; type < DismemberLimb::count; ++type)
                if (const BodyPart* part = byType(parts, type); part != nullptr && part->mCondition == condition)
                    return part;
            return nullptr;
        }

        bool explodable(const BodyPart& part)
        {
            return (part.mFlags & DismemberLimb::explodable) != 0;
        }

        bool severable(const BodyPart& part)
        {
            return (part.mFlags & DismemberLimb::severable) != 0;
        }

        bool absolute(const BodyPart& part)
        {
            return (part.mFlags & DismemberLimb::absoluteChance) != 0;
        }

        /// A part whose burst the data makes certain: it can burst, its chance is absolute and 100
        bool certainBurst(const BodyPart& part)
        {
            return explodable(part) && absolute(part) && part.mExplodeChance >= 100;
        }

        /// Whether the blow bursts the part for sure: the blow bursts what it hits, or the data say so
        bool forced(const BodyPart& part, const DismemberHit& hit)
        {
            return explodable(part) && (hit.mExplodes || certainBurst(part));
        }
    }

    DismemberSettings DismemberSettings::fromGameSettings(
        const std::function<std::optional<double>(std::string_view)>& get)
    {
        DismemberSettings settings;
        const auto read = [&get](std::string_view name, int& value) {
            if (const std::optional<double> found = get(name))
                value = static_cast<int>(*found);
        };
        read("iCombatExplodePartChance", settings.mExplodePartChance);
        read("iCombatDismemberPartChance", settings.mDismemberPartChance);
        read("iMessTargetedLimbExplodeBonus", settings.mMessTargetedExplodeBonus);
        read("iMessCrippledLimbExplodeBonus", settings.mMessCrippledExplodeBonus);
        read("iMessIntactLimbExplodeBonus", settings.mMessIntactExplodeBonus);
        read("iMessIntactLimbDismemberChance", settings.mMessIntactDismemberChance);
        read("iMessTorsoExplodeChance", settings.mMessTorsoExplodeChance);
        return settings;
    }

    namespace DismemberRule
    {
        Roll roll(const DismemberSettings& settings, int random, int explodableChance, int dismemberBonus,
            int explodeBonus, bool absoluteChance, int onHit)
        {
            Roll result;
            // The limit of a burst is the part's chance plus the bonus when the chance is absolute, and else the share
            // of the part's chance that the setting and the bonus allow, cut to a whole number
            const int explodeLimit = absoluteChance
                ? explodableChance + explodeBonus
                : static_cast<int>((settings.mExplodePartChance + explodeBonus) * explodableChance * 0.01);
            const int dismemberLimit = dismemberBonus + settings.mDismemberPartChance;
            if (random < explodeLimit)
                result.mExplode = true;
            else if (random < dismemberLimit)
                result.mDismember = true;
            if (result.mExplode || result.mDismember)
            {
                if (onHit == 1)
                {
                    result.mExplode = false;
                    result.mDismember = true;
                }
                else if (onHit == 2)
                {
                    result.mExplode = true;
                    result.mDismember = false;
                }
                else if (onHit == 3)
                {
                    result.mExplode = false;
                    result.mDismember = false;
                }
            }
            return result;
        }

        Plan plan(const DismemberSettings& settings, const std::vector<BodyPart>& parts, const DismemberHit& hit,
            const std::function<double(int)>& condition, const std::function<int()>& random)
        {
            constexpr std::size_t most = 7;
            Plan result;
            if (!hit.mBloodyMess)
            {
                if (hit.mFromExplosion)
                {
                    // The parts that the explosion reached, as the host tells them
                    for (const int limb : hit.mExplosionLimbs)
                    {
                        if (result.mEntries.size() >= most)
                            break;
                        if (const BodyPart* part = byType(parts, limb))
                            result.mEntries.push_back({ limb, part->mCondition, 0, 0 });
                    }
                    return result;
                }
                // A part that was hit and has no condition left is tried (a blow at the torso only when the data make
                // its burst certain)
                const BodyPart* part = hit.mLimb >= 0 ? byType(parts, hit.mLimb) : nullptr;
                if (part != nullptr && (certainBurst(*part) || hit.mLimb != 0) && part->mCondition != -1
                    && condition(part->mCondition) <= 0)
                    result.mEntries.push_back({ hit.mLimb, part->mCondition, 0, 0 });
                return result;
            }

            // Bloody Mess
            int hitCondition = -1;
            const BodyPart* hitPart = hit.mLimb >= 0 ? byType(parts, hit.mLimb) : nullptr;
            int torsoLimb = -1;
            if (hitPart != nullptr)
            {
                const bool certain = forced(*hitPart, hit);
                if (hitPart->mCondition == DismemberLimb::torsoCondition)
                {
                    // A blow at the torso bursts the body when the data make it certain, or by the chance of the
                    // setting for a part that can burst
                    if (certain || (explodable(*hitPart) && random() < settings.mMessTorsoExplodeChance))
                    {
                        torsoLimb = hitPart->mType;
                        result.mTorsoExplodes = true;
                    }
                }
                else
                {
                    hitCondition = hitPart->mCondition;
                    result.mEntries.push_back({ hit.mLimb, hitCondition, 100,
                        certain ? 100 : settings.mMessTargetedExplodeBonus });
                }
            }
            for (int i = 0; i < 7 && result.mEntries.size() < most; ++i)
            {
                const int av = DismemberLimb::firstCondition + i;
                if (av == hitCondition || av == DismemberLimb::torsoCondition)
                    continue;
                const BodyPart* part = byCondition(parts, av);
                if (part == nullptr)
                    continue;
                const bool certain = forced(*part, hit);
                if (condition(av) <= 0)
                    result.mEntries.push_back(
                        { part->mType, av, 100, certain ? 100 : settings.mMessCrippledExplodeBonus });
                else if (certain || result.mTorsoExplodes || random() <= settings.mMessIntactDismemberChance)
                    result.mEntries.push_back(
                        { part->mType, av, 100, certain ? 100 : settings.mMessIntactExplodeBonus });
            }
            if (result.mTorsoExplodes && torsoLimb != -1)
                result.mEntries.push_back({ torsoLimb, DismemberLimb::torsoCondition, 100, 100 });
            return result;
        }

        std::optional<Cut> decide(const DismemberSettings& settings, const std::vector<BodyPart>& parts,
            const DismemberHit& hit, const DismemberEntry& entry, bool torsoExplodes,
            const std::function<bool(int)>& isGone, const std::function<int()>& random)
        {
            if (entry.mCondition < DismemberLimb::firstCondition || entry.mCondition > DismemberLimb::lastCondition)
                return std::nullopt;
            if (isGone(entry.mLimb))
                return std::nullopt;
            const BodyPart* part = byType(parts, entry.mLimb);
            const bool explodes = hit.mExplodes || (torsoExplodes && entry.mCondition == DismemberLimb::torsoCondition);
            Roll result;
            result.mExplode = part != nullptr && certainBurst(*part);
            if (explodes)
                result.mExplode = true;
            else if (hit.mDismembers)
                result.mDismember = true;
            if (!result.mExplode && !result.mDismember)
            {
                // Another limb than the one hit never takes the weapon's setting
                const bool otherLimb = hit.mLimb != entry.mLimb;
                const int chance = part != nullptr && explodable(*part) ? part->mExplodeChance : 0;
                result = roll(settings, random(), chance, entry.mDismemberBonus, entry.mExplodeBonus,
                    part != nullptr && absolute(*part), otherLimb ? 0 : hit.mOnHit);
            }
            if (!result.mExplode && !result.mDismember)
                return std::nullopt;

            // The part that goes is the first of the body with that condition
            const BodyPart* going = byCondition(parts, entry.mCondition);
            if (going == nullptr)
                return std::nullopt;
            if (result.mExplode && !explodable(*going))
                result.mExplode = false;
            if (!severable(*going) && !explodable(*going))
                return std::nullopt;
            if (isGone(going->mType))
                return std::nullopt;
            return Cut{ going->mType, result.mExplode };
        }

        std::vector<Cut> cuts(const DismemberSettings& settings, const std::vector<BodyPart>& parts,
            const DismemberHit& hit, const std::function<double(int)>& condition,
            const std::function<bool(int)>& isGone, const std::function<int()>& random)
        {
            std::vector<Cut> result;
            const Plan chosen = plan(settings, parts, hit, condition, random);
            // What is torn off in this blow is gone for the parts after it
            const auto goneNow = [&](int limb) {
                if (isGone(limb))
                    return true;
                return std::any_of(result.begin(), result.end(), [limb](const Cut& cut) { return cut.mLimb == limb; });
            };
            for (const DismemberEntry& entry : chosen.mEntries)
                if (const std::optional<Cut> cut
                    = decide(settings, parts, hit, entry, chosen.mTorsoExplodes, goneNow, random))
                    result.push_back(*cut);
            return result;
        }

        int causeOf(bool fromExplosion, bool fromProjectile, bool fromWeapon)
        {
            if (fromExplosion)
                return DeathCause::explosion;
            if (fromProjectile)
                return DeathCause::projectile;
            if (fromWeapon)
                return DeathCause::weapon;
            return DeathCause::other;
        }

        double wholeHours(double daysPassed, double hourOfDay)
        {
            // The world's days are whole (its hour is kept apart); a number that is not one counts as none
            if (!std::isfinite(daysPassed) || !std::isfinite(hourOfDay))
                return 0.0;
            return std::floor(std::floor(daysPassed) * 24.0 + hourOfDay);
        }
    }

    bool Dismemberment::isGone(FormId actor, int limb)
    {
        if (limb < 0 || limb >= DismemberLimb::count)
            return false;
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found != mEntries.end() && (found->second.mMask & (1u << limb)) != 0;
    }

    std::uint16_t Dismemberment::mask(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found == mEntries.end() ? 0 : found->second.mMask;
    }

    std::uint16_t Dismemberment::exploded(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found == mEntries.end() ? 0 : found->second.mExploded;
    }

    int Dismemberment::cause(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found == mEntries.end() ? DeathCause::none : found->second.mCause;
    }

    FormId Dismemberment::killer(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found == mEntries.end() ? 0 : found->second.mKiller;
    }

    void Dismemberment::markGone(FormId actor, int limb, bool exploded)
    {
        if (actor == 0 || limb < 0 || limb >= DismemberLimb::count)
            return;
        const std::lock_guard lock(mMutex);
        Entry& entry = mEntries[actor];
        entry.mMask = static_cast<std::uint16_t>(entry.mMask | (1u << limb));
        if (exploded)
            entry.mExploded = static_cast<std::uint16_t>(entry.mExploded | (1u << limb));
    }

    void Dismemberment::recordDeath(FormId actor, int cause, FormId killer)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        Entry& entry = mEntries[actor];
        entry.mCause = cause;
        entry.mKiller = killer;
    }

    int Dismemberment::criticalStage(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found == mEntries.end() ? CriticalStage::none : found->second.mCritical;
    }

    void Dismemberment::setCriticalStage(FormId actor, int stage)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        if (stage == CriticalStage::none)
        {
            if (const auto found = mEntries.find(actor); found != mEntries.end())
                found->second.mCritical = 0;
            return;
        }
        mEntries[actor].mCritical = stage;
    }

    double Dismemberment::deathHour(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mEntries.find(actor);
        return found == mEntries.end() ? 0.0 : found->second.mDeathHour;
    }

    void Dismemberment::setDeathHour(FormId actor, double hour)
    {
        if (actor == 0)
            return;
        const std::lock_guard lock(mMutex);
        if (hour == 0)
        {
            if (const auto found = mEntries.find(actor); found != mEntries.end())
                found->second.mDeathHour = 0;
            return;
        }
        mEntries[actor].mDeathHour = hour;
    }

    void Dismemberment::restore(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        mEntries.erase(actor);
    }

    void Dismemberment::newFrame()
    {
        const std::lock_guard lock(mMutex);
        mThisFrame = 0;
    }

    bool Dismemberment::takeSlot(int perFrame)
    {
        const std::lock_guard lock(mMutex);
        if (mThisFrame >= perFrame)
            return false;
        ++mThisFrame;
        return true;
    }

    void Dismemberment::clear()
    {
        const std::lock_guard lock(mMutex);
        mEntries.clear();
        mThisFrame = 0;
    }

    void Dismemberment::forget(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        mEntries.erase(actor);
    }

    std::vector<FormId> Dismemberment::actors()
    {
        const std::lock_guard lock(mMutex);
        std::vector<FormId> result;
        for (const auto& [actor, entry] : mEntries)
            result.push_back(actor);
        return result;
    }

    template <>
    struct SaveAccess<Dismemberment>
    {
        static void write(Dismemberment& dismemberment, SaveWriter& out)
        {
            const std::lock_guard lock(dismemberment.mMutex);
            out.count(dismemberment.mEntries.size());
            for (const auto& [actor, entry] : dismemberment.mEntries)
            {
                out.form(actor);
                out.u16(entry.mMask);
                out.u16(entry.mExploded);
                out.i32(entry.mCause);
                out.form(entry.mKiller);
                out.i32(entry.mCritical);
                out.f64(entry.mDeathHour);
            }
        }

        static void read(Dismemberment& dismemberment, SaveReader& in, std::uint32_t)
        {
            std::map<FormId, Dismemberment::Entry> entries;
            const std::size_t count = in.count(28);
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::optional<FormId> actor = in.form();
                Dismemberment::Entry entry;
                entry.mMask = in.u16();
                entry.mExploded = in.u16();
                entry.mCause = in.i32();
                const std::optional<FormId> killer = in.form();
                entry.mKiller = killer.value_or(0);
                entry.mCritical = in.i32();
                entry.mDeathHour = in.f64();
                if (actor)
                    entries[*actor] = entry;
            }
            const std::lock_guard lock(dismemberment.mMutex);
            dismemberment.mEntries = std::move(entries);
        }
    };

    void writeDismemberment(Dismemberment& dismemberment, SaveWriter& out)
    {
        SaveAccess<Dismemberment>::write(dismemberment, out);
    }

    void readDismemberment(Dismemberment& dismemberment, SaveReader& in, std::uint32_t version)
    {
        SaveAccess<Dismemberment>::read(dismemberment, in, version);
    }

    void addDismemberCommands(Interpreter& interpreter, Dismemberment& dismemberment, DismemberRecords& records)
    {
        // The bit of the limb in the mask that the executable keeps for a character (an actor only: the other things
        // answer 0)
        interpreter.setHandler("IsLimbGone", [&dismemberment, &records](CallContext& call) {
            if (call.mReference == 0 || !records.dismemberIsActor(call.mReference))
                return 0.0;
            return dismemberment.isGone(call.mReference, intArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetCauseofDeath", [&dismemberment](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(dismemberment.cause(call.mReference)) : -1.0;
        });
        // The executable keeps the stage in the actor itself (the other things have none: SetCriticalStage does nothing
        // for them and IsInCriticalStage answers 0)
        interpreter.setHandler("SetCriticalStage", [&dismemberment, &records](CallContext& call) {
            const int stage = intArgument(call, 0);
            if (call.mReference != 0 && records.dismemberIsActor(call.mReference) && stage >= CriticalStage::none
                && stage <= CriticalStage::disintegrateEnd)
            {
                dismemberment.setCriticalStage(call.mReference, stage);
                records.criticalStageSet(call.mReference, stage);
            }
            return 0.0;
        });
        interpreter.setHandler("IsInCriticalStage", [&dismemberment, &records](CallContext& call) {
            if (call.mReference == 0 || !records.dismemberIsActor(call.mReference))
                return 0.0;
            return dismemberment.criticalStage(call.mReference) == intArgument(call, 0) ? 1.0 : 0.0;
        });
    }
}
