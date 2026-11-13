#include "falloutscripts.hpp"

#include "../mwsound/falloutfootsteps.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <set>

#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnote.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/derivedstats.hpp>
#include <components/falloutscript/falldamage.hpp>
#include <components/falloutscript/hud.hpp>
#include <components/falloutscript/messagetext.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwinput/triggerfeel.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"

namespace OFScript
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        /// How many degrees of bearing the icons of the compass stretch over: the executable draws a marker when its
        /// bearing times 1.45 is within 70 of the front, and puts it across 274 texels of the strip (Mac round 12AN B8)
        constexpr double compassField = 2.0 * 70.0 / 1.45;

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

    double FalloutScripts::mostBreath()
    {
        namespace AV = FalloutScript::ActorValue;
        // The breath lasts by the heal rate of the player (the endurance gives it), not by the endurance itself
        return FalloutScript::DerivedStats::swimBreath(derivedSettings(), mActorValues.current(mPlayer, AV::healRate));
    }

    void FalloutScripts::updateBreath(float frametime)
    {
        namespace AV = FalloutScript::ActorValue;
        namespace Derived = FalloutScript::DerivedStats;
        if (mPlayer == 0 || frametime < 0)
            return;
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (world == nullptr)
            return;
        const OFWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return;
        // The head is under the water when the point at 0.875 of the height is
        const bool headUnder = world->isUnderwater(player, static_cast<float>(Derived::swimSubmersion));
        mHeadUnder = headUnder;
        const double most = mostBreath();
        // The first frame, a loaded game and a player that is back from the dead start with the breath full
        double left = mBreathLeft;
        if (left < 0 || mActors.isDead(mPlayer))
            left = most;
        // Nothing the port has gives a character the breath of water (the executables ask for a magic effect of it and
        // for the actor's CanBreatheWater)
        const double health = mActorValues.current(mPlayer, AV::health);
        const Derived::SwimBreathStep step
            = Derived::swimBreathStep(derivedSettings(), left, most, headUnder, false, health, frametime);
        mBreathLeft = step.mRemaining;
        if ((step.mDamage <= 0 && !step.mFatal) || mActors.isDead(mPlayer) || world->getGodModeState())
            return;
        // Out of breath the player takes a fifth of its health each second, with the sound of drowning and the flash of
        // a blow (one that is let finish before the next begins: the damage comes every frame)
        if (const FalloutScript::FormId drowning = soundByEditorId("NPCHumanDrowning");
            drowning != 0 && !isSoundPlaying(drowning, mPlayer))
            playSound(drowning, mPlayer);
        if (OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager())
            windows->activateHitOverlay(false);
        painRumble();
        // No blow comes between the damage and the death of a player that has none left of its health
        const std::lock_guard strikeLock(mStrikeMutex);
        const auto transitions = mActors.holdTransitions();
        mActorValues.damage(mPlayer, AV::health, step.mDamage);
        // (a player with less than a point of health is out as well: see SwimBreathStep::mFatal)
        if (step.mFatal || mActorValues.current(mPlayer, AV::health) <= 0)
            mActors.kill(mPlayer);
    }

    void FalloutScripts::playGroundStep(const OFWorld::Ptr& ptr, FalloutScript::Footsteps::Slot slot)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (world == nullptr || ptr.isEmpty())
            return;
        OFSound::FootstepRequest request;
        request.mActor = ptr;
        request.mSlot = slot;
        request.mPlayerInFirstPerson = world->isFirstPerson();
        OFSound::playFootstep(request);
    }

    void FalloutScripts::painRumble()
    {
        if (OFBase::InputManager* input = OFBase::Environment::get().getInputManager())
            input->rumble(static_cast<float>(gameSetting("fRumblePainStrength", 0.5)),
                static_cast<float>(gameSetting("fRumblePainTime", 0.2)));
    }

    void FalloutScripts::updateTriggerFeel()
    {
        OFBase::InputManager* input = OFBase::Environment::get().getInputManager();
        if (input == nullptr)
            return;
        OFInput::TriggerFeel::PlayerState player;
        std::optional<OFInput::TriggerFeel::Weapon>& weapon = player.mWeapon;
        if (mPlayer != 0 && !mActors.isDead(mPlayer))
        {
            player.mWeaponDrawn = mEquipment.weaponDrawn(mPlayer);
            const FalloutScript::FormId held = mEquipment.weapon(mPlayer);
            const ESM4::Weapon* record = held != 0
                ? mStore.get<ESM4::Weapon>().search(ESM::RefId::formIdRefId(ESM::FormId::fromUint32(held)))
                : nullptr;
            if (record != nullptr && record->mHasGameData)
            {
                weapon.emplace();
                weapon->mAnimationType = record->mGame.mAnimationType;
                weapon->mAutomatic = (record->mGame.mFlags1 & ESM4::Weapon::GameData::Automatic) != 0;
                weapon->mRumbleLeft = record->mGame.mRumbleLeftMotor;
                weapon->mRumbleRight = record->mGame.mRumbleRightMotor;
                weapon->mRumbleSeconds = record->mGame.mRumbleDuration;
            }
            else
                weapon.emplace(); // the hands
        }
        player.mMenuOpen = menuOpen(0);
        input->setTriggerState(player);
    }

    void FalloutScripts::jumped(const OFWorld::Ptr& ptr)
    {
        if (mPlayer != 0 && !ptr.isEmpty() && scriptReferenceOf(ptr) == mPlayer)
            playGroundStep(ptr, FalloutScript::Footsteps::Slot::Jump);
    }

    bool FalloutScripts::landed(const OFWorld::Ptr& ptr, float heightLost)
    {
        namespace AV = FalloutScript::ActorValue;
        namespace Fall = FalloutScript::FallDamage;
        // Only the player lands here: the characters and creatures of the world are moved by the Fallout actor update,
        // which keeps them on the ground and has no fall to measure (the executables hurt them by the same formula,
        // minus the exemptions of the reads, so this is where it would go if they ever fall)
        if (mPlayer == 0 || ptr.isEmpty() || scriptReferenceOf(ptr) != mPlayer)
            return false;
        // The player comes down on the ground with the sound that the ground has for it, whether it hurts or not
        playGroundStep(ptr, FalloutScript::Footsteps::Slot::Land);
        const Fall::Settings settings = Fall::Settings::fromGameSettings(
            [this](std::string_view name) -> std::optional<double> { return gameSetting(name); });
        const double damage = Fall::damage(settings, heightLost);
        if (damage <= 0)
            return true;
        // A landing that hurts has a sound of its own, heavy above the threshold; the god mode does not silence it
        if (const FalloutScript::FormId landing
            = soundByEditorId(Fall::isHardLanding(settings, damage) ? "FSTLandHardHeavy" : "FSTLandHardLight");
            landing != 0)
            playSound(landing, mPlayer);
        if (OFBase::Environment::get().getWorld()->getGodModeState() || mActors.isDead(mPlayer))
            return true;
        if (OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager())
            windows->activateHitOverlay();
        painRumble();
        // The damage of a fall has no attacker and no armor: it is taken off the health as it is, after what the
        // difficulty of the game makes of the damage to the player
        const double toPlayer = Fall::damageToPlayerMultiplier(gameDifficulty());
        int leftRoll = 0;
        int rightRoll = 0;
        {
            const std::lock_guard lock(mCombatRandomMutex);
            std::uniform_int_distribution<int> percent(0, 99);
            leftRoll = percent(mCombatRandom);
            rightRoll = percent(mCombatRandom);
        }
        const std::lock_guard strikeLock(mStrikeMutex);
        const auto transitions = mActors.holdTransitions();
        mActorValues.damage(mPlayer, AV::health, damage * toPlayer);
        // Each leg is hurt on its own chance by half of the damage before the difficulty, and with that gets crippled
        // (the condition of the part is the actor value, 0x1D for the left leg and 0x1E for the right)
        constexpr int leftLegCondition = 29;
        constexpr int rightLegCondition = 30;
        if (Fall::legHurt(settings, leftRoll))
            mActorValues.damage(mPlayer, leftLegCondition, damage * settings.mLegMultiplier);
        if (Fall::legHurt(settings, rightRoll))
            mActorValues.damage(mPlayer, rightLegCondition, damage * settings.mLegMultiplier);
        if (mActorValues.current(mPlayer, AV::health) <= 0)
            mActors.kill(mPlayer);
        return true;
    }

    FalloutScripts::HudStatus FalloutScripts::hudStatus()
    {
        namespace AV = FalloutScript::ActorValue;
        HudStatus status;
        status.mRads = mActorValues.current(mPlayer, 54);
        status.mRadsMax = gameSetting("fRadiationMax", 1000);
        status.mControlsOff = mControlsOff.load();
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        status.mSneaking = !player.isEmpty() && OFBase::Environment::get().getMechanicsManager()->isSneaking(player);
        if (status.mSneaking != mSneakingLogged)
        {
            mSneakingLogged = status.mSneaking;
            log(status.mSneaking ? "Player sneaks" : "Player stops sneaking");
        }
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
        const double most = mostBreath();
        const double left = mBreathLeft;
        status.mHeadUnder = mHeadUnder;
        status.mBreath = left < 0 || !(most > 0) ? 1.0 : std::clamp(left / most, 0.0, 1.0);
        status.mBreathTicks = FalloutScript::DerivedStats::airMeterVisibleTicks(left < 0 ? most : left, most);
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
                marker.mType = reference.mMapMarker & 0xFF;
                markers.push_back(std::move(marker));
            });
            for (PlacedMarker& marker : markers)
                marker.mWorldspace = worldspaceOf(marker.mId);
            std::map<FalloutScript::FormId, std::size_t> perSpace;
            for (const PlacedMarker& marker : markers)
                if (marker.mWorldspace != 0)
                    ++perSpace[marker.mWorldspace];
            std::size_t most = 0;
            for (const auto& [space, count] : perSpace)
                if (count > most)
                {
                    most = count;
                    mMainSpace = space;
                }
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

    int FalloutScripts::gameDifficulty() const
    {
        // Kept as the number plus one: a switch that is 0 was never set
        const int kept = mSwitches.get("GameDifficulty");
        return kept > 0 ? std::clamp(kept - 1, 0, 4) : FalloutScript::FallDamage::normalDifficulty;
    }

    void FalloutScripts::setGameDifficulty(int difficulty)
    {
        mSwitches.set("GameDifficulty", std::clamp(difficulty, 0, 4) + 1);
    }
}
