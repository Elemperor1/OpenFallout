#include "aicombat.hpp"

#include <components/detournavigator/navigatorutils.hpp>
#include <components/esm3/aisequence.hpp>
#include <components/misc/coordinateconverter.hpp>
#include <components/misc/mathutil.hpp>
#include <components/misc/pathgridutils.hpp>
#include <components/misc/rng.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwphysics/raycasting.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "actorutil.hpp"
#include "aicombataction.hpp"
#include "character.hpp"
#include "combat.hpp"
#include "creaturestats.hpp"
#include "movement.hpp"
#include "pathgrid.hpp"
#include "steering.hpp"
#include "weapontype.hpp"

namespace
{

    // chooses an attack depending on probability to avoid uniformity
    std::string_view chooseBestAttack(const ESM::Weapon* weapon);

    bool hitAttemptMatchesTarget(const OFWorld::Ptr& actor, const OFWorld::Ptr& target)
    {
        ESM::RefNum hitNum = actor.getClass().getCreatureStats(actor).getHitAttemptActor();
        return hitNum.isSet() && target.getCellRef().getRefNum() == hitNum;
    }
}

namespace OFMechanics
{
    AiCombat::AiCombat(const OFWorld::Ptr& actor)
    {
        mTargetActor = actor.getCellRef().getRefNum();
    }

    AiCombat::AiCombat(const ESM::AiSequence::AiCombat* combat)
    {
        mTargetActor = combat->mTargetActor;
    }

    OFWorld::Ptr AiCombat::getTarget() const
    {
        const OFWorld::Ptr target = AiPackage::getTarget();
        if (!target.isEmpty() && !target.getClass().isActor())
            return {};
        return target;
    }

    void AiCombat::init() {}

    /*
     * Current AiCombat movement states (as of 0.29.0), ignoring the details of the
     * attack states such as CombatMove, Strike and ReadyToAttack:
     *
     *    +----(within strike range)----->attack--(beyond strike range)-->follow
     *    |                                 | ^                            | |
     *    |                                 | |                            | |
     *  pursue<---(beyond follow range)-----+ +----(within strike range)---+ |
     *    ^                                                                  |
     *    |                                                                  |
     *    +-------------------------(beyond follow range)--------------------+
     *
     *
     * Below diagram is high level only, the code detail is a little different
     * (but including those detail will just complicate the diagram w/o adding much)
     *
     *    +----------(same)-------------->attack---------(same)---------->follow
     *    |                                 |^^                            |||
     *    |                                 |||                            |||
     *    |       +--(same)-----------------+|+----------(same)------------+||
     *    |       |                          |                              ||
     *    |       |                          | (in range)                   ||
     *    |   <---+         (too far)        |                              ||
     *  pursue<-------------------------[door open]<-----+                  ||
     *    ^^^                                            |                  ||
     *    |||                                            |                  ||
     *    ||+----------evade-----+                       |                  ||
     *    ||                     |    [closed door]      |                  ||
     *    |+----> maybe stuck, check --------------> back up, check door    ||
     *    |         ^   |   ^                          |   ^                ||
     *    |         |   |   |                          |   |                ||
     *    |         |   +---+                          +---+                ||
     *    |         +-------------------------------------------------------+|
     *    |                                                                  |
     *    +---------------------------(same)---------------------------------+
     *
     * FIXME:
     *
     * The new scheme is way too complicated, should really be implemented as a
     * proper state machine.
     *
     * TODO:
     *
     * Use the observer pattern to coordinate attacks, provide intelligence on
     * whether the target was hit, etc.
     */

    bool AiCombat::execute(
        const OFWorld::Ptr& actor, CharacterController& characterController, AiState& state, float duration)
    {
        // Get or create temporary storage
        AiCombatStorage& storage = state.get<AiCombatStorage>();

        const OFWorld::Class& actorClass = actor.getClass();
        if (!actorClass.isActor())
            return true;

        OFMechanics::CreatureStats& actorStats = actorClass.getCreatureStats(actor);

        // No combat for dead creatures
        if (actorStats.isDead())
            return true;

        const OFWorld::Ptr target = getTarget(); // The target to follow

        // Stop if the target doesn't exist
        if (target.isEmpty() || !target.getCellRef().getCount() || !target.getRefData().isEnabled()
            || target.getClass().getCreatureStats(target).isDead() || !target.getRefData().getBaseNode())
            return true;

        if (actor == target) // This should never happen.
            return true;

        // No actions for totally static creatures
        if (!actorClass.isMobile(actor))
        {
            storage.mFleeState = AiCombatStorage::FleeState_Idle;
            return false;
        }

        if (actorStats.isParalyzed() || actorStats.getKnockedDown())
            return false;

        if (!storage.isFleeing())
        {
            const ESM::Weapon* weapon = nullptr;
            bool isRangedCombat = false;

            if (storage.mCurrentAction.get()) // need to wait to init action with its attack range
            {
                // Update every frame. UpdateLOS uses a timer, so the LOS check does not happen every frame.
                updateLOS(actor, target, duration, storage);
                const float targetReachedTolerance
                    = storage.mLOS && !storage.mUseCustomDestination ? storage.mAttackRange : 0.0f;
                const osg::Vec3f destination = storage.mUseCustomDestination
                    ? storage.mCustomDestination
                    : target.getRefData().getPosition().asVec3();
                const bool isTargetReached = pathTo(actor, destination, duration,
                    characterController.getSupportedMovementDirections(), targetReachedTolerance);
                if (isTargetReached)
                    storage.mReadyToAttack = true;

                weapon = storage.mCurrentAction->getWeapon();
                storage.mCurrentAction->getCombatRange(isRangedCombat);

                if (!isRangedCombat && storage.mShouldApproach && storage.mReadyToAttack)
                {
                    if (getDistanceToBounds(actor, target) > 64.f)
                    {
                        storage.stopCombatMove();
                        storage.mMovement.mPosition[1] = 1.f;
                    }
                    else
                    {
                        storage.mShouldApproach = false;
                        storage.mMovement.mPosition[1] = 0.f;
                    }
                }
            }

            storage.updateCombatMove(duration);
            storage.mRotateMove = false;
            if (storage.mReadyToAttack)
            {
                OFBase::World* world = OFBase::Environment::get().getWorld();
                const osg::Vec3f actorPos(actor.getRefData().getPosition().asVec3());
                const osg::Vec3f targetPos(target.getRefData().getPosition().asVec3());
                const osg::Vec3f targetRelativePos = world->aimToTarget(actor, target, isRangedCombat);
                storage.mMovement.mRotation[0] = getXAngleToDir(targetRelativePos);
                // using targetRelativePos results in spastic movements since the head is animated
                storage.mMovement.mRotation[2] = getZAngleToDir(targetPos - actorPos);
                updateActorsMovement(actor, duration, storage);
                if (storage.mRotateMove)
                    return false;
            }

            storage.updateAttack(actor, characterController, weapon, isRangedCombat, duration);
        }
        else
        {
            updateFleeing(actor, target, duration, characterController.getSupportedMovementDirections(), storage);
        }
        storage.mActionCooldown -= duration;

        if (storage.mReaction.update(duration) == Misc::TimerStatus::Waiting)
            return false;

        return attack(actor, target, storage, characterController);
    }

    bool AiCombat::attack(const OFWorld::Ptr& actor, const OFWorld::Ptr& target, AiCombatStorage& storage,
        CharacterController& characterController)
    {
        const OFWorld::CellStore*& currentCell = storage.mCell;
        bool cellChange = currentCell && (actor.getCell() != currentCell);
        if (!currentCell || cellChange)
        {
            currentCell = actor.getCell();
        }

        const OFWorld::Class& actorClass = actor.getClass();
        OFMechanics::CreatureStats& stats = actorClass.getCreatureStats(actor);

        bool forceFlee = false;
        if (!canFight(actor, target))
        {
            storage.stopAttack();
            stats.setAttackingOrSpell(false);
            storage.mActionCooldown = 0.f;
            // Continue combat if target is player or player follower/escorter and an attack has been attempted
            const auto& playerFollowersAndEscorters
                = OFBase::Environment::get().getMechanicsManager()->getActorsSidingWith(OFMechanics::getPlayer());
            bool targetSidesWithPlayer
                = (std::find(playerFollowersAndEscorters.begin(), playerFollowersAndEscorters.end(), target)
                    != playerFollowersAndEscorters.end());
            if ((target == OFMechanics::getPlayer() || targetSidesWithPlayer)
                && (hitAttemptMatchesTarget(actor, target) || hitAttemptMatchesTarget(target, actor)))
                forceFlee = true;
            else // Otherwise end combat
                return true;
        }

        stats.setMovementFlag(CreatureStats::Flag_Run, true);
        float& actionCooldown = storage.mActionCooldown;
        std::unique_ptr<Action>& currentAction = storage.mCurrentAction;

        if (!forceFlee)
        {
            if (actionCooldown > 0)
                return false;

            if (characterController.readyToPrepareAttack())
            {
                currentAction = prepareNextAction(actor, target);
                actionCooldown = currentAction->getActionCooldown();
            }
        }
        else
        {
            currentAction = std::make_unique<ActionFlee>();
            actionCooldown = currentAction->getActionCooldown();
        }

        if (!currentAction)
            return false;

        if (storage.isFleeing() != currentAction->isFleeing())
        {
            if (currentAction->isFleeing())
            {
                storage.startFleeing();
                OFBase::Environment::get().getDialogueManager()->say(actor, ESM::RefId::stringRefId("flee"));
                return false;
            }
            else
                storage.stopFleeing();
        }

        bool isRangedCombat = false;
        float& rangeAttack = storage.mAttackRange;
        rangeAttack = currentAction->getCombatRange(isRangedCombat);

        ESM::Position pos = actor.getRefData().getPosition();
        const osg::Vec3f vActorPos(pos.asVec3());
        const osg::Vec3f vTargetPos(target.getRefData().getPosition().asVec3());

        float distToTarget = getDistanceToBounds(actor, target);

        storage.mReadyToAttack = (currentAction->isAttackingOrSpell() && distToTarget <= rangeAttack && storage.mLOS);

        if (storage.mReadyToAttack)
        {
            storage.startCombatMove(isRangedCombat, distToTarget, rangeAttack, actor, target);
        }
        else
        {
            storage.mShouldApproach = true;
        }

        // If actor uses custom destination it has to try to rebuild path because environment can change
        // (door is opened between actor and target) or target position has changed and current custom destination
        // is not good enough to attack target.
        if (storage.mCurrentAction->isAttackingOrSpell()
            && ((!storage.mReadyToAttack && !mPathFinder.isPathConstructed())
                || (storage.mUseCustomDestination && (storage.mCustomDestination - vTargetPos).length() > rangeAttack)))
        {
            const OFBase::World* world = OFBase::Environment::get().getWorld();
            // Try to build path to the target.
            const auto agentBounds = world->getPathfindingAgentBounds(actor);
            const DetourNavigator::Flags navigatorFlags = getNavigatorFlags(actor);
            const DetourNavigator::AreaCosts areaCosts = getAreaCosts(actor, navigatorFlags);
            const ESM::Pathgrid* pathgrid = world->getStore().get<ESM::Pathgrid>().search(*actor.getCell()->getCell());
            const auto& pathGridGraph = getPathGridGraph(pathgrid);
            mPathFinder.buildPath(actor, vActorPos, vTargetPos, pathGridGraph, agentBounds, navigatorFlags, areaCosts,
                storage.mAttackRange, PathType::Full);

            if (!mPathFinder.isPathConstructed())
            {
                // If there is no path, try to find a point on a line from the actor position to target projected
                // on navmesh to attack the target from there.
                const auto navigator = world->getNavigator();
                const auto hit
                    = DetourNavigator::raycast(*navigator, agentBounds, vActorPos, vTargetPos, navigatorFlags);

                if (hit.has_value() && (*hit - vTargetPos).length() <= rangeAttack)
                {
                    // If the point is close enough, try to find a path to that point.
                    mPathFinder.buildPath(actor, vActorPos, *hit, pathGridGraph, agentBounds, navigatorFlags, areaCosts,
                        storage.mAttackRange, PathType::Full);
                    if (mPathFinder.isPathConstructed())
                    {
                        // If path to that point is found use it as custom destination.
                        storage.mCustomDestination = *hit;
                        storage.mUseCustomDestination = true;
                    }
                }

                if (!mPathFinder.isPathConstructed())
                {
                    storage.mUseCustomDestination = false;
                    storage.stopAttack();
                    stats.setAttackingOrSpell(false);
                    currentAction = std::make_unique<ActionFlee>();
                    actionCooldown = currentAction->getActionCooldown();
                    storage.startFleeing();
                    OFBase::Environment::get().getDialogueManager()->say(actor, ESM::RefId::stringRefId("flee"));
                }
            }
            else
            {
                storage.mUseCustomDestination = false;
            }
        }

        return false;
    }

    void OFMechanics::AiCombat::updateLOS(
        const OFWorld::Ptr& actor, const OFWorld::Ptr& target, float duration, OFMechanics::AiCombatStorage& storage)
    {
        const float losUpdateDuration = 0.5f;
        if (storage.mUpdateLOSTimer <= 0.f)
        {
            storage.mLOS = OFBase::Environment::get().getWorld()->getLOS(actor, target);
            storage.mUpdateLOSTimer = losUpdateDuration;
        }
        else
            storage.mUpdateLOSTimer -= duration;
    }

    void OFMechanics::AiCombat::updateFleeing(const OFWorld::Ptr& actor, const OFWorld::Ptr& target, float duration,
        OFWorld::MovementDirectionFlags supportedMovementDirections, AiCombatStorage& storage)
    {
        const float blindRunDuration = 1.0f;

        updateLOS(actor, target, duration, storage);

        AiCombatStorage::FleeState& state = storage.mFleeState;
        switch (state)
        {
            case AiCombatStorage::FleeState_None:
                return;

            case AiCombatStorage::FleeState_Idle:
            {
                float triggerDist = getMaxAttackDistance(target);
                const OFWorld::Cell* cellVariant = storage.mCell->getCell();
                if (storage.mLOS && (triggerDist >= 1000 || getDistanceMinusHalfExtents(actor, target) <= triggerDist))
                {
                    const ESM::Pathgrid* pathgrid
                        = OFBase::Environment::get().getESMStore()->get<ESM::Pathgrid>().search(*cellVariant);

                    bool runFallback = true;

                    if (pathgrid != nullptr && !pathgrid->mPoints.empty()
                        && !actor.getClass().isPureWaterCreature(actor))
                    {
                        ESM::Pathgrid::PointList points;
                        const Misc::CoordinateConverter coords
                            = Misc::makeCoordinateConverter(*storage.mCell->getCell());

                        osg::Vec3f localPos = actor.getRefData().getPosition().asVec3();
                        coords.toLocal(localPos);

                        const std::size_t closestPointIndex = Misc::getClosestPoint(*pathgrid, localPos);
                        for (std::size_t i = 0; i < pathgrid->mPoints.size(); i++)
                        {
                            if (i != closestPointIndex
                                && getPathGridGraph(pathgrid).isPointConnected(closestPointIndex, i))
                            {
                                points.push_back(pathgrid->mPoints[i]);
                            }
                        }

                        if (!points.empty())
                        {
                            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
                            ESM::Pathgrid::Point dest = points[Misc::Rng::rollDice(points.size(), prng)];
                            coords.toWorld(dest);

                            state = AiCombatStorage::FleeState_RunToDestination;
                            storage.mFleeDest = ESM::Pathgrid::Point(dest.mX, dest.mY, dest.mZ);

                            runFallback = false;
                        }
                    }

                    if (runFallback)
                    {
                        state = AiCombatStorage::FleeState_RunBlindly;
                        storage.mFleeBlindRunTimer = 0.0f;
                    }
                }
            }
            break;

            case AiCombatStorage::FleeState_RunBlindly:
            {
                // timer to prevent twitchy movement that can be observed in vanilla MW
                if (storage.mFleeBlindRunTimer < blindRunDuration)
                {
                    storage.mFleeBlindRunTimer += duration;

                    storage.mMovement.mRotation[0] = -actor.getRefData().getPosition().rot[0];
                    storage.mMovement.mRotation[2] = osg::PIf
                        + getZAngleToDir(
                            target.getRefData().getPosition().asVec3() - actor.getRefData().getPosition().asVec3());
                    storage.mMovement.mPosition[1] = 1;
                    updateActorsMovement(actor, duration, storage);
                }
                else
                    state = AiCombatStorage::FleeState_Idle;
            }
            break;

            case AiCombatStorage::FleeState_RunToDestination:
            {
                static const float fFleeDistance = OFBase::Environment::get()
                                                       .getESMStore()
                                                       ->get<ESM::GameSetting>()
                                                       .find("fFleeDistance")
                                                       ->mValue.getFloat();

                float dist
                    = (actor.getRefData().getPosition().asVec3() - target.getRefData().getPosition().asVec3()).length();
                if ((dist > fFleeDistance && !storage.mLOS)
                    || pathTo(
                        actor, Misc::Convert::makeOsgVec3f(storage.mFleeDest), duration, supportedMovementDirections))
                {
                    state = AiCombatStorage::FleeState_Idle;
                }
            }
            break;
        };
    }

    void AiCombat::updateActorsMovement(const OFWorld::Ptr& actor, float duration, AiCombatStorage& storage)
    {
        // apply combat movement
        float deltaAngle = storage.mMovement.mRotation[2] - actor.getRefData().getPosition().rot[2];
        osg::Vec2f movement = Misc::rotateVec2f(
            osg::Vec2f(storage.mMovement.mPosition[0], storage.mMovement.mPosition[1]), -deltaAngle);

        OFMechanics::Movement& actorMovementSettings = actor.getClass().getMovementSettings(actor);
        actorMovementSettings.mPosition[0] = movement.x();
        actorMovementSettings.mPosition[1] = movement.y();
        actorMovementSettings.mPosition[2] = storage.mMovement.mPosition[2];

        rotateActorOnAxis(actor, 2, actorMovementSettings, storage);
        rotateActorOnAxis(actor, 0, actorMovementSettings, storage);
    }

    void AiCombat::rotateActorOnAxis(
        const OFWorld::Ptr& actor, int axis, OFMechanics::Movement& actorMovementSettings, AiCombatStorage& storage)
    {
        actorMovementSettings.mRotation[axis] = 0;
        bool isRangedCombat = false;
        storage.mCurrentAction->getCombatRange(isRangedCombat);
        float eps = isRangedCombat ? osg::DegreesToRadians(0.5f) : osg::DegreesToRadians(3.f);
        float targetAngleRadians = storage.mMovement.mRotation[axis];
        storage.mRotateMove = !smoothTurn(actor, targetAngleRadians, axis, eps);
    }

    void AiCombat::writeState(ESM::AiSequence::AiSequence& sequence) const
    {
        auto combat = std::make_unique<ESM::AiSequence::AiCombat>();
        combat->mTargetActor = mTargetActor;

        ESM::AiSequence::AiPackageContainer package;
        package.mType = ESM::AiSequence::Ai_Combat;
        package.mPackage = std::move(combat);
        sequence.mPackages.push_back(std::move(package));
    }

    AiCombatStorage::AiCombatStorage()
        : mAttackCooldown(0.0f)
        , mReaction(OFBase::Environment::get().getWorld()->getPrng())
        , mTimerCombatMove(0.0f)
        , mReadyToAttack(false)
        , mAttack(false)
        , mAttackRange(0.0f)
        , mCombatMove(false)
        , mRotateMove(false)
        , mCell(nullptr)
        , mCurrentAction()
        , mActionCooldown(0.0f)
        , mStrength()
        , mForceNoShortcut(false)
        , mShortcutFailPos()
        , mMovement()
        , mFleeState(FleeState_None)
        , mLOS(false)
        , mUpdateLOSTimer(0.0f)
        , mFleeBlindRunTimer(0.0f)
        , mUseCustomDestination(false)
        , mCustomDestination()
    {
    }

    void AiCombatStorage::startCombatMove(bool isDistantCombat, float distToTarget, float rangeAttack,
        const OFWorld::Ptr& actor, const OFWorld::Ptr& target)
    {
        auto& prng = OFBase::Environment::get().getWorld()->getPrng();

        // get the range of the target's weapon
        OFWorld::Ptr targetWeapon = OFWorld::Ptr();
        const OFWorld::Class& targetClass = target.getClass();

        if (targetClass.hasInventoryStore(target))
        {
            ESM::RefId weaponTypeId;
            OFWorld::ContainerStoreIterator weaponSlot = OFMechanics::getActiveWeapon(target, &weaponTypeId);
            if (isWeaponType(weaponTypeId))
                targetWeapon = *weaponSlot;
        }

        bool targetUsesRanged = false;
        float rangeAttackOfTarget = ActionWeapon(targetWeapon).getCombatRange(targetUsesRanged);

        if (mMovement.mPosition[0])
        {
            mTimerCombatMove = 0.1f + 0.1f * Misc::Rng::rollClosedProbability(prng);
            mCombatMove = true;
        }
        // dodge movements (for NPCs and bipedal creatures)
        // Note: do not use for ranged combat yet since in couple with back up behaviour can move actor out of cliff
        else if (actor.getClass().isBipedal(actor) && !isDistantCombat)
        {
            float moveDuration = 0;
            double angleToTarget
                = Misc::normalizeAngle(mMovement.mRotation[2] - actor.getRefData().getPosition().rot[2]);
            // Apply a big side step if enemy tries to get around and come from behind.
            // Otherwise apply a random side step (kind of dodging) with some probability
            // if actor is within range of target's weapon.
            if (std::abs(angleToTarget) > osg::PI / 4)
                moveDuration = 0.2f;
            else if (distToTarget <= rangeAttackOfTarget && Misc::Rng::rollClosedProbability(prng) < 0.25)
                moveDuration = 0.1f + 0.1f * Misc::Rng::rollClosedProbability(prng);
            if (moveDuration > 0)
            {
                mMovement.mPosition[0] = Misc::Rng::rollProbability(prng) < 0.5 ? 1.0f : -1.0f; // to the left/right
                mTimerCombatMove = moveDuration;
                mCombatMove = true;
            }
        }

        mMovement.mPosition[1] = 0;
        if (isDistantCombat)
        {
            // Backing up behaviour
            // Actor backs up slightly further away than opponent's weapon range
            // (in vanilla - only as far as opponent's weapon range),
            // or not at all if opponent is using a ranged weapon

            if (targetUsesRanged
                || distToTarget > rangeAttackOfTarget * 1.5) // Don't back up if the target is wielding ranged weapon
                return;

            // actor should not back up into water
            if (OFBase::Environment::get().getWorld()->isUnderwater(OFWorld::ConstPtr(actor), 0.5f))
                return;

            int mask
                = OFPhysics::CollisionType_World | OFPhysics::CollisionType_HeightMap | OFPhysics::CollisionType_Door;

            // Actor can not back up if there is no free space behind
            // Currently we take the 35% of actor's height from the ground as vector height.
            // This approach allows us to detect small obstacles (e.g. crates) and curved walls.
            osg::Vec3f halfExtents = OFBase::Environment::get().getWorld()->getHalfExtents(actor);
            osg::Vec3f pos = actor.getRefData().getPosition().asVec3();
            osg::Vec3f source = pos + osg::Vec3f(0, 0, 0.75f * halfExtents.z());
            osg::Vec3f fallbackDirection = actor.getRefData().getBaseNode()->getAttitude() * osg::Vec3f(0, -1, 0);
            osg::Vec3f destination = source + fallbackDirection * (halfExtents.y() + 16);

            const auto* rayCasting = OFBase::Environment::get().getWorld()->getRayCasting();
            bool isObstacleDetected = rayCasting->castRay(source, destination, mask).mHit;
            if (isObstacleDetected)
                return;

            // Check if there is nothing behind - probably actor is near cliff.
            // A current approach: cast ray 1.5-yard ray down in 1.5 yard behind actor from 35% of actor's height.
            // If we did not hit anything, there is a cliff behind actor.
            source = pos + osg::Vec3f(0, 0, 0.75f * halfExtents.z()) + fallbackDirection * (halfExtents.y() + 96);
            destination = source - osg::Vec3f(0, 0, 0.75f * halfExtents.z() + 96);
            bool isCliffDetected = !rayCasting->castRay(source, destination, mask).mHit;
            if (isCliffDetected)
                return;

            mMovement.mPosition[1] = -1;
        }
    }

    void AiCombatStorage::updateCombatMove(float duration)
    {
        if (mCombatMove)
        {
            mTimerCombatMove -= duration;
            if (mTimerCombatMove <= 0)
            {
                stopCombatMove();
            }
        }
    }

    void AiCombatStorage::stopCombatMove()
    {
        mTimerCombatMove = 0;
        mMovement.mPosition[0] = 0;
        mCombatMove = false;
    }

    void AiCombatStorage::updateAttack(const OFWorld::Ptr& actor, CharacterController& characterController,
        const ESM::Weapon* weapon, bool distantCombat, float duration)
    {
        const OFWorld::Class& actorClass = actor.getClass();
        OFMechanics::CreatureStats& actorStats = actorClass.getCreatureStats(actor);

        if (mAttack)
        {
            float attackStrength = characterController.calculateWindUp();
            if (characterController.readyToPrepareAttack() || attackStrength >= mStrength || attackStrength == -1.f)
                mAttack = false;
        }
        else if (mReadyToAttack && characterController.readyToStartAttack())
        {
            if (mAttackCooldown <= 0)
            {
                mAttack = true; // attack starts just now

                if (!distantCombat)
                    characterController.setAIAttackType(chooseBestAttack(weapon));

                auto& prng = OFBase::Environment::get().getWorld()->getPrng();
                mStrength = Misc::Rng::rollClosedProbability(prng);

                const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();

                bool canShout = true;
                ESM::RefId spellId = mCurrentAction->getSpell();
                if (!spellId.empty())
                {
                    const ESM::Spell* spell = store.get<ESM::Spell>().find(spellId);
                    if (spell->mEffects.mList.empty() || spell->mEffects.mList[0].mData.mRange != ESM::RT_Target)
                        canShout = false;
                }

                if (canShout)
                {
                    // Say a provoking combat phrase
                    const int iVoiceAttackOdds
                        = store.get<ESM::GameSetting>().find("iVoiceAttackOdds")->mValue.getInteger();
                    if (Misc::Rng::roll0to99(prng) < iVoiceAttackOdds)
                    {
                        OFBase::Environment::get().getDialogueManager()->say(actor, ESM::RefId::stringRefId("attack"));
                    }
                }

                if (mCurrentAction->getActionCooldown() > 0.f)
                {
                    mAttackCooldown = mCurrentAction->getActionCooldown();
                }
                else
                {
                    std::string_view delayGmst = actor.getClass().isNpc() ? "fCombatDelayNPC" : "fCombatDelayCreature";
                    const float baseDelay = store.get<ESM::GameSetting>().find(delayGmst)->mValue.getFloat();
                    mAttackCooldown = std::min(baseDelay + 0.01f * Misc::Rng::roll0to99(prng), baseDelay + 0.9f);
                }
            }
            else
                mAttackCooldown -= duration;
        }

        actorStats.setAttackingOrSpell(mAttack);
    }

    void AiCombatStorage::stopAttack()
    {
        mMovement.mPosition[0] = 0;
        mMovement.mPosition[1] = 0;
        mMovement.mPosition[2] = 0;
        mReadyToAttack = false;
        mAttack = false;
    }

    void AiCombatStorage::startFleeing()
    {
        stopFleeing();
        mFleeState = FleeState_Idle;
    }

    void AiCombatStorage::stopFleeing()
    {
        mMovement.mPosition[0] = 0;
        mMovement.mPosition[1] = 0;
        mMovement.mPosition[2] = 0;
        mFleeState = FleeState_None;
        mFleeDest = ESM::Pathgrid::Point(0, 0, 0);
    }

    bool AiCombatStorage::isFleeing() const
    {
        return mFleeState != FleeState_None;
    }
}

namespace
{

    std::string_view chooseBestAttack(const ESM::Weapon* weapon)
    {
        if (weapon != nullptr)
        {
            // the more damage attackType deals the more probability it has
            int slash = (weapon->mData.mSlash[0] + weapon->mData.mSlash[1]) / 2;
            int chop = (weapon->mData.mChop[0] + weapon->mData.mChop[1]) / 2;
            int thrust = (weapon->mData.mThrust[0] + weapon->mData.mThrust[1]) / 2;

            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
            float roll = Misc::Rng::rollClosedProbability(prng) * (slash + chop + thrust);
            if (roll <= slash)
                return "slash";
            else if (roll <= (slash + thrust))
                return "thrust";
            else
                return "chop";
        }
        return OFMechanics::CharacterController::getRandomAttackType();
    }

}
