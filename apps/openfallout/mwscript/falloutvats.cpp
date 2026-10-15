#include "falloutscripts.hpp"

#include <algorithm>
#include <random>

#include <components/esm4/creaturemodel.hpp>
#include <components/esm4/loadbptd.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwclass/esm4base.hpp"
#include "../mwclass/esm4sources.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        bool isCharacterOrCreature(const OFWorld::Ptr& ptr)
        {
            return !ptr.isEmpty()
                && (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId);
        }

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }
    }

    bool FalloutScripts::bodyParts(FalloutScript::FormId actor, std::vector<FalloutScript::BodyPart>& parts)
    {
        namespace AV = FalloutScript::ActorValue;
        parts.clear();
        ESM::FormId base;
        if (!baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        // The body part data is part of the race of a character, and of the model of a creature, which a record that
        // has a template takes from it
        ESM::FormId data;
        if (mStore.get<ESM4::Npc>().search(base) != nullptr)
        {
            FalloutScript::ActorTraits traits;
            if (!traitsOf(actor, traits))
                return false;
            if (const ESM4::Race* race = mStore.get<ESM4::Race>().search(refId(traits.mRace)))
                data = race->mBodyPartData;
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
            const OFClass::StoreCreatureSource source(mStore);
            const ESM4::Creature* owner = ESM4::creatureTemplateOwner(
                ESM4::creatureTemplateChain(source, *creature, OFClass::ESM4Impl::sDefaultLevel, seed),
                ESM4::Creature::Template_UseModel);
            if (owner == nullptr)
                owner = creature;
            if (!owner->mBodyParts.empty())
                data = owner->mBodyParts.front();
        }
        if (data.isZeroOrUnset())
            return true;
        const ESM4::BodyPartData* record = mStore.get<ESM4::BodyPartData>().search(ESM::RefId::formIdRefId(data));
        if (record == nullptr)
            return true;
        for (const ESM4::BodyPartData::BodyPart& part : record->mBodyParts)
        {
            FalloutScript::BodyPart found;
            found.mName = part.mPartName;
            found.mTarget = part.mVATSTarget;
            const int condition = part.mData.actorValue;
            found.mCondition = condition >= AV::firstCondition && condition <= AV::lastCondition ? condition : -1;
            found.mDamageMult = part.mData.damageMult;
            found.mToHit = part.mData.toHitChance;
            found.mHealthPercent = part.mData.healthPercent;
            parts.push_back(std::move(found));
        }
        return true;
    }

    namespace
    {
        /// The share of the most action points that comes back in a second in New Vegas
        constexpr double newVegasRestoreShare = 0.06;
    }

    const FalloutScript::Vats::Rules& FalloutScripts::vatsRules()
    {
        const std::lock_guard lock(mDerivedSettingsMutex);
        if (!mVatsRules)
        {
            const std::map<std::string, double>& settings = gameSettingValues();
            mVatsRules = FalloutScript::Vats::rules([&settings](std::string_view name) -> std::optional<double> {
                const auto found = settings.find(Misc::StringUtils::lowerCase(name));
                return found != settings.end() ? std::optional<double>(found->second) : std::nullopt;
            });
            // New Vegas gives back a share of the most a second (read from its executable) where Fallout 3 gives back a
            // number of points
            if (mGame == FalloutScript::Game::NewVegas)
                mVatsRules->mRestoreShare = newVegasRestoreShare;
        }
        return *mVatsRules;
    }

    FalloutScript::Vats::Weapon FalloutScripts::vatsWeapon(FalloutScript::FormId attacker)
    {
        FalloutScript::Vats::Weapon result;
        const FalloutScript::FormId weapon = attacker != 0 ? mEquipment.weapon(attacker) : 0;
        const ESM4::Weapon* record = weapon != 0 ? mStore.get<ESM4::Weapon>().search(refId(weapon)) : nullptr;
        if (record == nullptr || !record->mHasGameData)
            return result;
        const ESM4::Weapon::GameData& game = record->mGame;
        result.mAnimationType = game.mAnimationType;
        result.mBaseChance = game.mBaseVatsChance;
        result.mSpread = game.mSpread;
        result.mRange = FalloutScript::Vats::isMelee(game.mAnimationType) ? 0.0 : weaponStats(attacker).mRange;
        if ((game.mFlags2 & ESM4::Weapon::GameData::OverrideActionPoints) != 0)
            result.mActionPoints = game.mOverrideActionPoints;
        return result;
    }

    FalloutScripts::VatsState FalloutScripts::vatsState()
    {
        namespace AV = FalloutScript::ActorValue;
        VatsState state;
        state.mOpen = mVatsOpen;
        if (mPlayer == 0)
            return state;
        state.mActionPoints = mActorValues.current(mPlayer, AV::actionPoints);
        state.mMaxActionPoints = mActorValues.permanent(mPlayer, AV::actionPoints);
        state.mCost = FalloutScript::Vats::attackCost(vatsRules(), vatsWeapon(mPlayer));
        const WeaponStats stats = weaponStats(mPlayer);
        state.mRange = FalloutScript::Vats::isMelee(vatsWeapon(mPlayer).mAnimationType) ? vatsRules().mMeleeMaxDistance
                                                                                        : stats.mRange;
        if (stats.mWeapon != 0)
            state.mWeaponName = itemName(stats.mWeapon);
        state.mAvailable = !mActors.isDead(mPlayer);
        return state;
    }

    FalloutScripts::VatsTarget FalloutScripts::vatsTarget(const OFWorld::Ptr& target)
    {
        namespace AV = FalloutScript::ActorValue;
        VatsTarget result;
        if (target.isEmpty() || !isCharacterOrCreature(target))
            return result;
        result.mName = std::string(target.getClass().getName(target));
        const FalloutScript::FormId victim = scriptReferenceOf(target);
        if (victim == 0)
            return result;
        distance(mPlayer, victim, result.mDistance);
        result.mHealth = mActorValues.current(victim, AV::health);
        result.mMaxHealth = mActorValues.permanent(victim, AV::health);
        return result;
    }

    bool FalloutScripts::vatsCanTarget(const OFWorld::Ptr& target)
    {
        if (mPlayer == 0 || target.isEmpty() || !isCharacterOrCreature(target))
            return false;
        const FalloutScript::FormId victim = scriptReferenceOf(target);
        if (victim == 0 || victim == mPlayer || mActors.isDead(victim) || mCombat.unconscious(victim))
            return false;
        double apart = 0;
        return distance(mPlayer, victim, apart) && apart <= vatsState().mRange;
    }

    std::vector<FalloutScripts::VatsPart> FalloutScripts::vatsParts(const OFWorld::Ptr& target)
    {
        std::vector<VatsPart> result;
        if (mPlayer == 0 || target.isEmpty() || !isCharacterOrCreature(target))
            return result;
        const FalloutScript::FormId victim = scriptReferenceOf(target);
        if (victim == 0)
            return result;
        double apart = 0;
        if (!distance(mPlayer, victim, apart))
            return result;
        namespace AV = FalloutScript::ActorValue;
        const FalloutScript::Vats::Weapon weapon = vatsWeapon(mPlayer);
        double skill = 100;
        const ESM4::Weapon* record = mEquipment.weapon(mPlayer) != 0
            ? mStore.get<ESM4::Weapon>().search(refId(mEquipment.weapon(mPlayer)))
            : nullptr;
        if (record != nullptr && record->mHasGameData && record->mGame.mSkill >= AV::firstSkill
            && record->mGame.mSkill <= AV::lastSkill)
            skill = mActorValues.current(mPlayer, static_cast<int>(record->mGame.mSkill));
        else if (record == nullptr)
            skill = mActorValues.current(mPlayer, AV::lastSkill);
        // A target that does not notice the player is hit with the stealth multiplier of the settings
        const bool hidden = !detects(victim, mPlayer);
        const std::vector<FalloutScript::BodyPart> parts = mLimbs.partsOf(victim);
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (parts[i].mTarget.empty())
                continue;
            VatsPart part;
            part.mIndex = static_cast<int>(i);
            part.mName = parts[i].mName;
            part.mTarget = parts[i].mTarget;
            part.mChance = FalloutScript::Vats::hitChance(vatsRules(), weapon, parts[i].mToHit, apart, skill, hidden);
            part.mCondition = mLimbs.condition(victim, static_cast<int>(i));
            result.push_back(std::move(part));
        }
        return result;
    }

    FalloutScripts::VatsAttackResult FalloutScripts::vatsAttack(const OFWorld::Ptr& target, int part)
    {
        VatsAttackResult result;
        if (mPlayer == 0 || mActors.isDead(mPlayer))
        {
            result.mRefused = "dead";
            return result;
        }
        if (target.isEmpty() || !isCharacterOrCreature(target))
        {
            result.mRefused = "none";
            return result;
        }
        const FalloutScript::FormId victim = scriptReferenceOf(target);
        if (victim == 0 || mActors.isDead(victim))
        {
            result.mRefused = "dead";
            return result;
        }
        const std::vector<VatsPart> parts = vatsParts(target);
        const auto aimed
            = std::find_if(parts.begin(), parts.end(), [part](const VatsPart& p) { return p.mIndex == part; });
        if (aimed == parts.end())
        {
            result.mRefused = "none";
            return result;
        }
        result.mChance = aimed->mChance;
        const VatsState state = vatsState();
        result.mCost = state.mCost;
        double apart = 0;
        if (!distance(mPlayer, victim, apart) || apart > state.mRange || aimed->mChance <= 0)
        {
            result.mRefused = "range";
            return result;
        }
        if (state.mActionPoints < state.mCost)
        {
            result.mRefused = "points";
            return result;
        }
        const WeaponStats stats = weaponStats(mPlayer);
        if (stats.mAmmoUse > 0 && stats.mRounds < stats.mAmmoUse)
        {
            result.mRefused = "ammo";
            return result;
        }
        namespace AV = FalloutScript::ActorValue;
        mActorValues.damage(mPlayer, AV::actionPoints, state.mCost);
        const FireResult fired = fire(mPlayer, target, part, aimed->mChance / 100.0, true);
        result.mHit = fired.mHit;
        result.mStrike = fired.mStrike;
        if (!fired.mFired)
            result.mRefused = fired.mOutOfAmmo ? "ammo" : "points";
        return result;
    }

    void FalloutScripts::setVatsOpen(bool open)
    {
        mVatsOpen = open;
    }
}
