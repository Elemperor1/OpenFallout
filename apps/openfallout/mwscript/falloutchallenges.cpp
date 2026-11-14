#include "falloutscripts.hpp"

#include <cmath>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadchal.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadweap.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// The skill of a blow without a weapon (the actor value of unarmed in New Vegas, the default of 0x89DC54)
        constexpr int unarmedSkill = 45;

        /// The kind of the weapon plus one, 1 for none (0x446390 reads the animation type of the record, 0x89DC1E)
        int weaponKind(const OFWorld::ESMStore& store, FalloutScript::FormId weapon)
        {
            if (weapon == 0)
                return 1;
            const ESM4::Weapon* record = store.get<ESM4::Weapon>().search(refId(weapon));
            return record != nullptr && record->mHasGameData ? static_cast<int>(record->mGame.mAnimationType) + 1 : 1;
        }

        /// What a kill tells of the kind of the victim: the type of a creature plus one, 0 for a person (0x89DBFD)
        int creatureKind(const OFWorld::ESMStore& store, FalloutScript::FormId base)
        {
            const ESM4::Creature* record = base != 0 ? store.get<ESM4::Creature>().search(refId(base)) : nullptr;
            return record != nullptr ? static_cast<int>(record->mCreatureType) + 1 : 0;
        }

        /// The group of the body that a blow struck, which a challenge of hits and of damage tells by (the table of
        /// FalloutNV.exe at 0x1196DAC, indexed by the part of the body): 1 head, 2 torso, 3 left arm, 4 right arm, 5 legs,
        /// 6 the weapon held
        int bodyGroup(int limb)
        {
            static constexpr int groups[] = { 2, 1, 1, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 1, 6 };
            return limb >= 0 && limb < static_cast<int>(sizeof(groups) / sizeof(groups[0])) ? groups[limb] : 0;
        }
    }

    void ChallengeRecordSource::forEachChallenge(const std::function<void(const FalloutScript::ChallengeRecord&)>& function)
    {
        for (const ESM4::Challenge& challenge : mStore.get<ESM4::Challenge>())
        {
            FalloutScript::ChallengeRecord record;
            record.mId = numberOf(challenge.mId);
            if (record.mId == 0)
                continue;
            record.mName = challenge.mFullName;
            record.mDescription = challenge.mDescription;
            record.mIcon = challenge.mIcon;
            record.mType = static_cast<int>(challenge.mData.mType);
            record.mThreshold = static_cast<int>(challenge.mData.mThreshold);
            record.mFlags = static_cast<int>(challenge.mData.mFlags);
            record.mInterval = static_cast<int>(challenge.mData.mInterval);
            // The values of the type: two words and a third of which the executable reads the low word
            const auto& bytes = challenge.mData.mTypeData;
            record.mFirst = bytes[0] | (bytes[1] << 8);
            record.mSecond = bytes[2] | (bytes[3] << 8);
            record.mThird = bytes[4] | (bytes[5] << 8);
            record.mSnam = numberOf(challenge.mSnam);
            record.mXnam = numberOf(challenge.mXnam);
            record.mScript = numberOf(challenge.mScript);
            function(record);
        }
    }

    bool ChallengeRecordSource::inList(FalloutScript::FormId list, FalloutScript::FormId form)
    {
        return list != 0 && form != 0 && mLists.contains(list, form);
    }

    void FalloutScripts::setUpChallenges()
    {
        // The records are read when a game starts or loads (reset); what the player does is counted from then on
        mChallenges.setScriptRunner([this](FalloutScript::FormId script) {
            Log(Debug::Info) << "Challenge: its script " << FalloutScript::hex(script) << " runs for the player";
            mObjects.runEffectOnce(mPlayer, script);
        });
        mChallenges.setCompletedCounter([this](int stat, int amount) { mProgress.modifyMiscStat(stat, amount); });
        mChallenges.setNotice([this](const FalloutScript::ChallengeNotice& notice) {
            // The text and the picture of the challenge, and the sound of a level up when it is completed (the
            // function at 0x5F5950 plays the interface sound 0x15 after the message)
            Log(Debug::Info) << "Challenge: " << FalloutScript::hex(notice.mChallenge)
                             << (notice.mCompleted ? " is completed (a notice is shown)"
                                                   : " is at " + std::to_string(mChallenges.progress(notice.mChallenge))
                                     + " (a notice is shown)");
            mHudMessages.push(notice.mText, notice.mIcon, FalloutScript::Hud::messageSeconds, 0,
                notice.mCompleted ? "UILevelUp" : std::string());
        });
        // The counters of the Pip-Boy are what the challenges of type 11 count
        mProgress.setMiscStatListener([this](int stat, int amount) {
            FalloutScript::ChallengeEvent event;
            event.mType = FalloutScript::ChallengeType::miscStat;
            event.mAmount = amount;
            event.mFirst = stat;
            mChallenges.event(event);
        });
    }

    void FalloutScripts::noteChallengeAcquired(FalloutScript::FormId item, int count)
    {
        if (item == 0 || count <= 0)
            return;
        // The player got the items (0x574A80): a challenge of one item and one of a list of items both count them
        FalloutScript::ChallengeEvent event;
        event.mAmount = count;
        event.mFormA = item;
        event.mType = FalloutScript::ChallengeType::acquireItemFromList;
        mChallenges.event(event);
        event.mType = FalloutScript::ChallengeType::acquireItem;
        mChallenges.event(event);
    }

    void FalloutScripts::noteChallengeUsed(FalloutScript::FormId item)
    {
        if (item == 0)
            return;
        // An item the player put to use (0x88C830, 0x95E900)
        FalloutScript::ChallengeEvent event;
        event.mFormA = item;
        event.mType = FalloutScript::ChallengeType::useItem;
        mChallenges.event(event);
        event.mType = FalloutScript::ChallengeType::useItemFromList;
        mChallenges.event(event);
    }

    void FalloutScripts::noteChallengeMarker(FalloutScript::FormId marker)
    {
        // The player found a place (0x779070): the place is both forms of the event
        FalloutScript::ChallengeEvent event;
        event.mType = FalloutScript::ChallengeType::discoverMarker;
        event.mFormA = marker;
        event.mFormB = marker;
        mChallenges.event(event);
    }

    void FalloutScripts::noteChallengeCrafted(FalloutScript::FormId recipe)
    {
        FalloutScript::ChallengeEvent event;
        event.mType = FalloutScript::ChallengeType::craft;
        event.mFormA = recipe;
        mChallenges.event(event);
    }

    void FalloutScripts::noteChallengeHit(FalloutScript::FormId victim, const FalloutScript::CombatState::Hit& hit)
    {
        if (hit.mAttacker != mPlayer || mPlayer == 0 || victim == 0 || victim == mPlayer)
            return;
        // What the executable passes (0x899CB0): the form the victim is made from, the weapon, the group of the part that
        // was struck, a kind of hit (1 for a blow that lands) and the kind of weapon plus one
        FalloutScript::FormId base = 0;
        baseFormOf(victim, base);
        const int kind = weaponKind(mStore, hit.mWeapon);
        FalloutScript::ChallengeEvent event;
        event.mType = FalloutScript::ChallengeType::hit;
        event.mFormA = base;
        event.mFormB = hit.mWeapon;
        event.mFirst = bodyGroup(hit.mLimb);
        event.mSecond = 1;
        event.mThird = kind;
        mChallenges.event(event);
        const int damage = static_cast<int>(std::lround(hit.mDamage));
        if (damage > 0)
        {
            event.mType = FalloutScript::ChallengeType::damage;
            event.mAmount = damage;
            event.mSecond = 0;
            mChallenges.event(event);
        }
    }

    void FalloutScripts::noteChallengeKill(
        FalloutScript::FormId victim, FalloutScript::FormId killer, FalloutScript::FormId weapon)
    {
        if (victim == 0 || victim == mPlayer || mPlayer == 0 || killer != mPlayer)
            return;
        FalloutScript::FormId base = 0;
        baseFormOf(victim, base);
        // The weapon is the one the killing blow was struck with, told with the death: the last round can leave the hands
        // before the death is told (a death that no blow caused has none)
        const int kind = weaponKind(mStore, weapon);
        // The skill of the weapon (unarmed without one)
        int skill = unarmedSkill;
        if (const ESM4::Weapon* record = weapon != 0 ? mStore.get<ESM4::Weapon>().search(refId(weapon)) : nullptr)
            if (record->mHasGameData)
                skill = static_cast<int>(record->mGame.mSkill);
        FalloutScript::ChallengeEvent event;
        event.mType = FalloutScript::ChallengeType::killSpecific;
        event.mFormA = base;
        event.mFormB = weapon;
        event.mSecond = skill;
        event.mThird = kind;
        mChallenges.event(event);
        event.mType = FalloutScript::ChallengeType::killFromList;
        mChallenges.event(event);
        // Any kill: no form, the kind of creature plus one (0 for a person) and the kind of weapon
        event.mType = FalloutScript::ChallengeType::killAny;
        event.mFormA = 0;
        event.mSecond = creatureKind(mStore, base);
        mChallenges.event(event);
    }
}
