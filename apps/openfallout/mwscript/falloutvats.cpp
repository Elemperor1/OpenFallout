#include "falloutscripts.hpp"

#include <algorithm>
#include <random>

#include <components/esm4/creaturemodel.hpp>
#include <components/esm4/loadbptd.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
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

        /// The local numbers, in the first master, of the body part data of the player and of the other characters
        constexpr FalloutScript::FormId playerBodyParts = 0x1C;
        constexpr FalloutScript::FormId defaultBodyParts = 0x1D;

        /// The actor values of the condition of the arms
        constexpr int leftArmCondition = 27;
        constexpr int rightArmCondition = 28;
    }

    bool FalloutScripts::bodyParts(FalloutScript::FormId actor, std::vector<FalloutScript::BodyPart>& parts)
    {
        namespace AV = FalloutScript::ActorValue;
        parts.clear();
        ESM::FormId base;
        if (!baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        // The games (read from both executables) give the player the body part data "PlayerBodyPartData" and every
        // other character "DefaultBodyPartData", two records of the first master with the local numbers 0x1C and 0x1D;
        // a creature names its own (PNAM) or has the default, and a record that has a template takes it from the one
        // that gives its model
        ESM::FormId data;
        if (mStore.get<ESM4::Npc>().search(base) != nullptr)
            data = ESM::FormId::fromUint32(masterForm(actor == mPlayer ? playerBodyParts : defaultBodyParts));
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
            const OFClass::StoreCreatureSource source(mStore);
            const ESM4::Creature* owner = ESM4::creatureTemplateOwner(
                ESM4::creatureTemplateChain(source, *creature, OFClass::ESM4Impl::sDefaultLevel, seed),
                ESM4::Creature::Template_UseModel);
            if (owner == nullptr)
                owner = creature;
            data = ESM::FormId::fromUint32(masterForm(defaultBodyParts));
            if (!owner->mBodyParts.empty() && !owner->mBodyParts.front().isZeroOrUnset())
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
            found.mType = part.mData.partType;
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
            // number of points, and the chance of a shot is worked out in another way
            mVatsRules->mNewVegas = mGame == FalloutScript::Game::NewVegas;
            if (mVatsRules->mNewVegas)
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
        // V.A.T.S. reads the smaller spread of the record (DNAM at 16, "Min Spread"), not the one after it
        result.mSpread = game.mMinSpread;
        result.mRangeFixed = (game.mFlags2 & ESM4::Weapon::GameData::RangeFixed) != 0;
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
        // New Vegas lets the perks of the player change what an attack costs
        if (vatsRules().mNewVegas)
        {
            FalloutScript::PerkEngine::Context attacking;
            attacking.mWeapon = mEquipment.weapon(mPlayer);
            state.mCost = std::max(
                mPerks.apply(FalloutScript::EntryPoint::actionPointCost, mPlayer, state.mCost, attacking), 0.0);
        }
        const WeaponStats stats = weaponStats(mPlayer);
        state.mRange = FalloutScript::Vats::reach(vatsRules(), vatsWeapon(mPlayer));
        if (stats.mWeapon != 0)
            state.mWeaponName = itemName(stats.mWeapon);
        state.mAvailable = !mActors.isDead(mPlayer) && !mCombat.unconscious(mPlayer);
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
        const FalloutScript::FormId held = mEquipment.weapon(mPlayer);
        double skill = 100;
        const ESM4::Weapon* record = held != 0 ? mStore.get<ESM4::Weapon>().search(refId(held)) : nullptr;
        if (record != nullptr && record->mHasGameData && record->mGame.mSkill >= AV::firstSkill
            && record->mGame.mSkill <= AV::lastSkill)
            skill = mActorValues.current(mPlayer, static_cast<int>(record->mGame.mSkill));
        else if (record == nullptr)
            skill = mActorValues.current(mPlayer, AV::lastSkill);
        // The skills are read as 0 to 100 whatever they have grown to
        skill = std::clamp(skill, 0.0, 100.0);

        FalloutScript::Vats::Shot shot;
        shot.distance = apart;
        shot.skill = skill;
        // The size of the target: twice the largest half of its box
        if (const OFBase::World* world = OFBase::Environment::get().getWorld())
        {
            const osg::Vec3f half = world->getHalfExtents(target);
            shot.bound = 2.0 * std::max({ half.x(), half.y(), half.z() });
        }
        // A target that has chameleon, or is hidden by the invisibility of the player, is hard to aim at (the stealth
        // multiplier); New Vegas lets the perks of the player see through it
        shot.hidden = mActorValues.permanent(victim, AV::chameleon) > 0
            || mActorValues.permanent(mPlayer, AV::invisibility) > 0;
        if (shot.hidden && vatsRules().mNewVegas
            && mPerks.apply(FalloutScript::EntryPoint::improvedDetection, mPlayer, 0.0) > 0)
            shot.hidden = false;
        // The shooter's own aim adds to the spread of the weapon, in degrees, after the perks have had their say
        const bool leftCrippled = mActorValues.current(mPlayer, leftArmCondition) <= 0;
        const bool rightCrippled = mActorValues.current(mPlayer, rightArmCondition) <= 0;
        shot.arms = !leftCrippled && !rightCrippled;
        double factor = FalloutScript::Vats::spreadFactor(
            vatsRules(), weapon, (leftCrippled ? 1 : 0) + (rightCrippled ? 1 : 0), 0, 0, skill);
        FalloutScript::PerkEngine::Context aiming;
        aiming.mWeapon = held;
        factor = mPerks.apply(FalloutScript::EntryPoint::calculateGunSpread, mPlayer, factor, aiming);
        shot.extraSpread = FalloutScript::Vats::spreadDegrees(vatsRules(), factor);

        // The perks of the player change the chance of a hit (entry point 8, with the weapon and the target)
        FalloutScript::PerkEngine::Context hitting;
        hitting.mOther = victim;
        hitting.mWeapon = held;
        const std::function<double(double)> toHit = [this, hitting](double chance) {
            return mPerks.apply(FalloutScript::EntryPoint::hitChance, mPlayer, chance, hitting);
        };

        const std::vector<FalloutScript::BodyPart> parts = mLimbs.partsOf(victim);
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (parts[i].mTarget.empty())
                continue;
            VatsPart part;
            part.mIndex = static_cast<int>(i);
            part.mType = parts[i].mType;
            part.mName = parts[i].mName;
            part.mTarget = parts[i].mTarget;
            shot.partToHit = parts[i].mToHit;
            part.mChance
                = FalloutScript::Vats::adjustedChance(FalloutScript::Vats::hitChance(vatsRules(), weapon, shot), toHit);
            part.mCondition = mLimbs.condition(victim, parts[i].mType);
            result.push_back(std::move(part));
        }
        return result;
    }

    FalloutScripts::VatsAttackResult FalloutScripts::vatsAttack(const OFWorld::Ptr& target, int part)
    {
        VatsAttackResult result;
        // The dead and the unconscious strike no blows (strike() refuses them), so nothing is spent on their attacks
        if (mPlayer == 0 || mActors.isDead(mPlayer) || mCombat.unconscious(mPlayer))
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
        // A target that fell unconscious from an earlier attack of the queue is not struck again
        if (victim == 0 || mActors.isDead(victim) || mCombat.unconscious(victim))
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
        // The blow tells the part by its type
        const FireResult fired = fire(mPlayer, target, aimed->mType, aimed->mChance / 100.0, true);
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
