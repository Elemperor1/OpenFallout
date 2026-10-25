#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnote.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/hud.hpp>
#include <components/falloutscript/messagetext.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"

namespace OFScript
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        /// How many degrees across the window of the compass shows. The games draw a strip of 345 units, and what the
        /// executable does to put a marker on it is not read: this is a stand-in.
        constexpr double compassField = 120.0;

        /// A character or creature of Fallout (their classes are not those of Morrowind's actors)
        bool isCharacterOrCreature(const OFWorld::Ptr& ptr)
        {
            return !ptr.isEmpty()
                && (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId);
        }

    }

    bool FalloutScripts::sharesSpace(const OFWorld::CellStore* a, const OFWorld::CellStore* b)
    {
        return a == b
            || (a != nullptr && b != nullptr && a->isExterior() && b->isExterior()
                && a->getCell()->getWorldSpace() == b->getCell()->getWorldSpace());
    }

    void FalloutScripts::watchPlayer(const OFWorld::Ptr& watcher)
    {
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        const FalloutScript::FormId self = scriptReferenceOf(watcher);
        if (self == 0 || self == mPlayer || !watcher.isInCell() || !player.isInCell())
            return;
        // The meter is for a player that sneaks
        if (!OFBase::Environment::get().getMechanicsManager()->isSneaking(player))
        {
            mWatchers.forget(self);
            return;
        }
        if (mActors.isDead(self) || mCombat.unconscious(self) || !sharesSpace(watcher.getCell(), player.getCell()))
        {
            mWatchers.forget(self);
            return;
        }
        // Near enough to tell: the distance a sneaking player can be seen from, and more outside
        double range = gameSetting("fSneakMaxDistance", 1500);
        if (watcher.getCell()->isExterior())
            range *= gameSetting("fSneakExteriorDistanceMult", 2);
        const osg::Vec3f from = watcher.getRefData().getPosition().asVec3();
        const osg::Vec3f to = player.getRefData().getPosition().asVec3();
        if ((to - from).length() > range)
        {
            mWatchers.forget(self);
            return;
        }
        const bool hostile = mCombat.target(self) == mPlayer;
        mWatchers.note(self, sightOfPlayer(watcher), hostile, mHudTime);
    }

    FalloutScripts::HudStatus FalloutScripts::hudStatus()
    {
        namespace AV = FalloutScript::ActorValue;
        HudStatus status;
        status.mRads = mActorValues.current(mPlayer, 54);
        status.mRadsMax = gameSetting("fRadiationMax", 1000);
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        status.mSneaking = !player.isEmpty() && OFBase::Environment::get().getMechanicsManager()->isSneaking(player);
        status.mSneak = mWatchers.status(mHudTime);
        const char* setting = "sSneakHidden";
        switch (status.mSneak)
        {
            case FalloutScript::Hud::Sneak::Hidden:
                break;
            case FalloutScript::Hud::Sneak::Caution:
                setting = "sSneakCaution";
                break;
            case FalloutScript::Hud::Sneak::Detected:
                setting = "sSneakDetected";
                break;
            case FalloutScript::Hud::Sneak::Danger:
                setting = "sSneakDanger";
                break;
        }
        status.mSneakText = gameSettingText(setting);
        // A part of the body is crippled when the player has it and nothing is left of its condition
        std::set<std::string> crippled;
        for (const FalloutScript::BodyPart& part : mLimbs.partsOf(mPlayer))
        {
            if (part.mCondition < 0 || mLimbs.condition(mPlayer, part.mType) > 0)
                continue;
            if (const auto icon = FalloutScript::Hud::limbIcon(part.mType))
                crippled.insert(*icon);
        }
        status.mCrippled.assign(crippled.begin(), crippled.end());
        status.mWeaponDrawn = mEquipment.weaponDrawn(mPlayer);
        return status;
    }

    bool FalloutScripts::hudTarget(const OFWorld::Ptr& target, HudTarget& result)
    {
        namespace AV = FalloutScript::ActorValue;
        if (target.isEmpty() || !target.isInCell() || !isCharacterOrCreature(target))
            return false;
        const FalloutScript::FormId id = scriptReferenceOf(target);
        if (id == 0 || id == mPlayer)
            return false;
        result.mName = std::string(target.getClass().getName(target));
        result.mHealth = std::max(0.0, mActorValues.current(id, AV::health));
        result.mMaxHealth = std::max(1.0, mActorValues.permanent(id, AV::health));
        result.mDead = mActors.isDead(id);
        result.mHostile = !result.mDead && mCombat.target(id) == mPlayer;
        return true;
    }

    std::optional<std::string> FalloutScripts::noteText(FalloutScript::FormId note)
    {
        const ESM4::Note* record = mStore.get<ESM4::Note>().search(ESM::FormId::fromUint32(note));
        if (record == nullptr || record->mText.empty())
            return std::nullopt;
        return FalloutScript::formatMessage(
            record->mText, {}, [this](std::string_view name) { return messageToken(name); });
    }

    const std::vector<FalloutScripts::PlacedMarker>& FalloutScripts::placedMarkers()
    {
        if (!mPlacedMarkers)
        {
            std::vector<PlacedMarker> markers;
            mStore.get<ESM4::Reference>().forEachPlaced([&](const ESM4::Reference& reference) {
                if (!reference.mIsMapMarker)
                    return;
                PlacedMarker marker;
                marker.mId = reference.mId.toUint32();
                marker.mPosition = osg::Vec3f(reference.mPos.pos[0], reference.mPos.pos[1], reference.mPos.pos[2]);
                marker.mName = reference.mFullName;
                markers.push_back(std::move(marker));
            });
            for (PlacedMarker& marker : markers)
                marker.mWorldspace = worldspaceOf(marker.mId);
            std::sort(markers.begin(), markers.end(),
                [](const PlacedMarker& a, const PlacedMarker& b) { return a.mId < b.mId; });
            mPlacedMarkers = std::move(markers);
        }
        return *mPlacedMarkers;
    }

    std::vector<FalloutScripts::HudMarker> FalloutScripts::hudCompass(double& heading)
    {
        std::vector<HudMarker> result;
        heading = 0;
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return result;
        const osg::Vec3f here = player.getRefData().getPosition().asVec3();
        heading = std::fmod(std::fmod(player.getRefData().getPosition().rot[2] * 180.0 / pi, 360.0) + 360.0, 360.0);
        const auto place = [&](const osg::Vec3f& there, HudMarker marker) {
            const osg::Vec3f away = there - here;
            const auto share = FalloutScript::Hud::compassShare(
                heading, FalloutScript::Hud::bearing(away.x(), away.y()), compassField);
            if (!share)
                return;
            marker.mShare = *share;
            marker.mDistance = std::sqrt(away.x() * away.x() + away.y() * away.y());
            result.push_back(std::move(marker));
        };

        // Where the objectives that the player has been given send it: a target the compass shows while its conditions
        // hold, in the space the player is in
        struct Candidate
        {
            const ESM4::QuestTarget* mTarget;
            std::string mName;
        };
        std::vector<Candidate> candidates;
        mQuests.forEachState([&candidates](FalloutScript::FormId, const FalloutScript::QuestState& state) {
            if (!state.mRunning || state.mCompleted || state.mFailed || state.mRecord == nullptr)
                return;
            for (const ESM4::QuestObjective& objective : state.mRecord->mObjectives)
            {
                const auto found = state.mObjectives.find(objective.mIndex);
                if (found == state.mObjectives.end() || !found->second.mDisplayed || found->second.mCompleted)
                    continue;
                for (const ESM4::QuestTarget& target : objective.mTargets)
                    candidates.push_back({ &target, state.mRecord->mQuestName });
            }
        });
        // The cell of an exterior has an id as well: whether the player is in the open world is what its cell says
        Place playerPlace;
        if (!whereIs(mPlayer, playerPlace))
            return result;
        const OFWorld::CellStore* playerCell = cellAt(playerPlace);
        if (playerCell == nullptr)
            return result;
        const bool outside = playerCell->isExterior();
        const FalloutScript::FormId playerSpace = worldspaceOf(mPlayer);
        std::set<FalloutScript::FormId> shown;
        FalloutScript::ConditionContext context;
        context.mSubject = mPlayer;
        for (const Candidate& candidate : candidates)
        {
            const FalloutScript::FormId target = candidate.mTarget->mTarget.toUint32();
            if (target == 0 || shown.count(target) != 0
                || !mConditions.holds(candidate.mTarget->mTargetConditions, context))
                continue;
            Place there;
            if (!whereIs(target, there))
                continue;
            // The same cell inside, the same worldspace outside
            if (!sharesSpace(cellAt(there), playerCell))
                continue;
            shown.insert(target);
            HudMarker marker;
            marker.mKind = HudMarker::Quest;
            marker.mName = candidate.mName;
            place(there.mPosition, std::move(marker));
        }

        // The places the player has found: the map markers that are shown, in this worldspace, within the distance the
        // games show them from
        if (outside)
        {
            const double most = gameSetting("iMapMarkerVisibleDistance", 20000);
            for (const PlacedMarker& placed : placedMarkers())
            {
                if (placed.mWorldspace != playerSpace || mMapMarkers.state(placed.mId) == 0)
                    continue;
                if ((placed.mPosition - here).length() > most)
                    continue;
                HudMarker marker;
                marker.mKind = HudMarker::Landmark;
                marker.mName = placed.mName;
                place(placed.mPosition, std::move(marker));
            }
        }

        // The characters that fight the player
        const std::size_t ticks = static_cast<std::size_t>(std::max(0.0, gameSetting("iHUDMaxCompassNPCTicks", 10)));
        std::vector<HudMarker> enemies;
        for (const FalloutScript::FormId attacker : mCombat.attackersOf(mPlayer))
        {
            if (mActors.isDead(attacker))
                continue;
            const OFWorld::Ptr enemy = findReference(attacker);
            if (enemy.isEmpty() || !enemy.isInCell() || !sharesSpace(enemy.getCell(), player.getCell()))
                continue;
            const std::size_t before = result.size();
            HudMarker marker;
            marker.mKind = HudMarker::Enemy;
            place(enemy.getRefData().getPosition().asVec3(), std::move(marker));
            if (result.size() > before)
            {
                enemies.push_back(std::move(result.back()));
                result.pop_back();
            }
        }
        std::sort(enemies.begin(), enemies.end(),
            [](const HudMarker& a, const HudMarker& b) { return a.mDistance < b.mDistance; });
        if (enemies.size() > ticks)
            enemies.resize(ticks);
        for (HudMarker& enemy : enemies)
            result.push_back(std::move(enemy));
        return result;
    }
}
