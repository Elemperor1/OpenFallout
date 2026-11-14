#include "placedexplosives.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript::Explosives
{
    namespace
    {
        /// The reference number of the player (the cause that an event names when the player set a mine off)
        constexpr std::uint32_t playerReference = 0x14;
    }

    Rules rules(const Lookup& setting)
    {
        Rules result;
        const auto read = [&setting](std::string_view name, double& value) {
            if (setting)
                if (const std::optional<double> found = setting(name))
                    value = *found;
        };
        read("fMineExteriorRadiusMult", result.mExteriorRadiusMult);
        read("fMinesDelayMin", result.mDelayMin);
        read("fMineAgeMax", result.mMineAgeMax);
        read("fGrenadeAgeMax", result.mGrenadeAgeMax);
        double shooter = result.mShooterCanTrigger ? 1.0 : 0.0;
        read("iProjectileMineShooterCanTrigger", shooter);
        result.mShooterCanTrigger = shooter > 0;
        double experience = result.mDisarmExperience;
        read("iMineDisarmExperience", experience);
        result.mDisarmExperience = static_cast<int>(experience);
        return result;
    }

    bool isMine(const Kind& kind)
    {
        return kind.mDetonates || (kind.mAltTrigger && kind.mProximity > 0.0);
    }

    double ageLimit(const Rules& rules, const Kind& kind)
    {
        // A mine (the alt trigger with a proximity, or a detonating record) lives for fMineAgeMax, which is 0 (for ever)
        // unless the files say; any other laid projectile for fGrenadeAgeMax
        if ((kind.mAltTrigger && kind.mProximity > 0.0) || kind.mDetonates)
            return rules.mMineAgeMax;
        return rules.mGrenadeAgeMax;
    }

    bool triggers(const Rules& rules, std::uint32_t character, const Layer& layer)
    {
        // The one who laid it is spared unless the setting lets the shooter trigger it
        if (!rules.mShooterCanTrigger && layer.mActor != 0 && character == layer.mActor)
            return false;
        // A character of the faction that owns the mine is spared
        if (layer.mFactionMember)
            return false;
        // With a layer, only a character that the layer takes for neutral or an enemy sets it off
        if (layer.mReaction)
            return *layer.mReaction == Neutral || *layer.mReaction == Enemy;
        return true;
    }

    double triggerDelay(const Rules& rules, const Kind& kind, bool player, double explosivesSkill)
    {
        if (!player)
            return rules.mDelayMin;
        return rules.mDelayMin + std::floor(explosivesSkill) * kind.mFuseTimer / 100.0;
    }

    std::uint32_t Laid::plant(const Placed& placed)
    {
        Placed copy = placed;
        copy.mId = mNext++;
        copy.mAge = 0;
        copy.mState = 0;
        // A mine waits to be set off; a charge with no proximity counts its record's timer down from the start
        copy.mTimer = copy.mKind.mProximity > 0.0 ? waiting : copy.mKind.mFuseTimer;
        mPlaced.push_back(copy);
        return copy.mId;
    }

    int Laid::disableAllMines()
    {
        int count = 0;
        for (Placed& laid : mPlaced)
            if (isMine(laid.mKind) && laid.mKind.mCanBeDisabled && (laid.mState & stateDisabled) == 0)
            {
                laid.mState |= stateDisabled;
                ++count;
            }
        return count;
    }

    int Laid::detonate(std::uint32_t owner)
    {
        int count = 0;
        for (Placed& laid : mPlaced)
            if (laid.mOwner == owner && laid.mKind.mDetonates)
            {
                laid.mState |= stateDetonate;
                ++count;
            }
        return count;
    }

    Laid::Disarm Laid::disable(const Rules& rules, std::uint32_t id, bool silent, bool byPlayer, bool wouldTrigger)
    {
        Disarm result;
        for (Placed& laid : mPlaced)
        {
            if (laid.mId != id)
                continue;
            if (!laid.mKind.mCanBeDisabled || (laid.mState & stateDisabled) != 0)
                return result;
            laid.mState |= stateDisabled;
            result.mDisabled = true;
            // The experience: a disarm that is not silent, by the player, of a mine that had not been set off or one that
            // the disarming character would have set off
            if (!silent && byPlayer && (laid.mTimer == waiting || wouldTrigger) && rules.mDisarmExperience > 0)
                result.mExperience = rules.mDisarmExperience;
            return result;
        }
        return result;
    }

    std::vector<Event> Laid::update(const Rules& rules, double seconds, const Surroundings& around)
    {
        std::vector<Event> events;
        std::vector<Placed> kept;
        kept.reserve(mPlaced.size());
        for (Placed& laid : mPlaced)
        {
            laid.mAge += seconds;
            // Its age is up (the limit is 0 for none)
            const double limit = ageLimit(rules, laid.mKind);
            if (limit != 0.0 && laid.mAge >= limit)
            {
                events.push_back({ Event::Kind::Expire, laid.mId, 0, laid });
                continue;
            }
            bool explode = false;
            std::uint32_t cause = 0;
            // Only a projectile with an explosion that has not been disabled can go off
            if (laid.mKind.mHasExplosion && (laid.mState & stateDisabled) == 0)
            {
                if (laid.mKind.mAltTrigger)
                {
                    bool counting = laid.mTimer != waiting || laid.mKind.mProximity == 0.0;
                    if (!counting)
                    {
                        const double radius
                            = laid.mKind.mProximity * (laid.mExterior ? rules.mExteriorRadiusMult : 1.0);
                        bool setOff = false;
                        bool byPlayer = false;
                        std::uint32_t who = 0;
                        if (around.character)
                            if (const std::optional<std::uint32_t> found = around.character(laid, radius))
                            {
                                setOff = true;
                                who = *found;
                            }
                        // The player is looked at last, and once the mine has let it pass it lets it pass for good
                        if (!setOff && (laid.mState & statePlayerSpared) == 0 && around.player
                            && around.player(laid, radius))
                        {
                            const double chance = around.playerChance ? around.playerChance(laid) : 100.0;
                            const int roll = around.roll ? around.roll() : 0;
                            if (roll < static_cast<int>(std::trunc(chance)))
                            {
                                setOff = true;
                                byPlayer = true;
                                who = playerReference;
                            }
                            else
                                laid.mState |= statePlayerSpared;
                        }
                        if (setOff)
                        {
                            laid.mTimer = triggerDelay(
                                rules, laid.mKind, byPlayer, around.skill ? around.skill() : 0.0);
                            laid.mCause = who;
                            counting = true;
                        }
                    }
                    if (counting)
                    {
                        laid.mTimer -= seconds;
                        if (laid.mTimer <= 0.0)
                        {
                            explode = true;
                            cause = laid.mCause;
                        }
                    }
                }
                else if ((laid.mState & stateDetonate) != 0)
                    explode = true;
            }
            if (explode)
            {
                events.push_back({ Event::Kind::Explode, laid.mId, cause, laid });
                continue;
            }
            kept.push_back(laid);
        }
        mPlaced = std::move(kept);
        return events;
    }

    const Placed* Laid::find(std::uint32_t id) const
    {
        const auto found = std::find_if(mPlaced.begin(), mPlaced.end(), [id](const Placed& p) { return p.mId == id; });
        return found == mPlaced.end() ? nullptr : &*found;
    }

    void Laid::clear()
    {
        mPlaced.clear();
        mNext = 1;
    }

    void Laid::restore(std::vector<Placed> placed, std::uint32_t next)
    {
        mPlaced = std::move(placed);
        mNext = next;
    }

    void writeExplosives(const Laid& laid, SaveWriter& out)
    {
        out.u32(laid.next());
        out.count(laid.all().size());
        for (const Placed& p : laid.all())
        {
            out.u32(p.mId);
            out.form(p.mOwner);
            out.form(p.mProjectile);
            out.boolean(p.mKind.mAltTrigger);
            out.boolean(p.mKind.mDetonates);
            out.boolean(p.mKind.mCanBeDisabled);
            out.boolean(p.mKind.mHasExplosion);
            out.f64(p.mKind.mProximity);
            out.f64(p.mKind.mFuseTimer);
            for (const float coordinate : p.mPosition)
                out.f32(coordinate);
            out.form(p.mCell);
            out.boolean(p.mExterior);
            out.f64(p.mAge);
            out.f64(p.mTimer);
            out.u32(p.mState);
            out.u32(p.mCause);
        }
    }

    void readExplosives(Laid& laid, SaveReader& in, std::uint32_t version)
    {
        if (version != explosivesSectionVersion)
            throw SaveError("explosives: unknown version");
        const std::uint32_t next = in.u32();
        // each entry takes at least 64 bytes
        const std::size_t count = in.count(64);
        std::vector<Placed> placed;
        placed.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            Placed p;
            p.mId = in.u32();
            const std::optional<FormId> owner = in.form();
            const std::optional<FormId> projectile = in.form();
            p.mKind.mAltTrigger = in.boolean();
            p.mKind.mDetonates = in.boolean();
            p.mKind.mCanBeDisabled = in.boolean();
            p.mKind.mHasExplosion = in.boolean();
            p.mKind.mProximity = in.f64();
            p.mKind.mFuseTimer = in.f64();
            for (float& coordinate : p.mPosition)
                coordinate = in.f32();
            const std::optional<FormId> cell = in.form();
            p.mExterior = in.boolean();
            p.mAge = in.f64();
            p.mTimer = in.f64();
            p.mState = in.u32();
            p.mCause = in.u32();
            // A projectile whose record is no longer loaded is dropped; the cell it lay in is only a guide
            if (!projectile)
                continue;
            p.mOwner = owner.value_or(0);
            p.mProjectile = *projectile;
            p.mCell = cell.value_or(0);
            placed.push_back(p);
        }
        laid.restore(std::move(placed), next);
    }
}
