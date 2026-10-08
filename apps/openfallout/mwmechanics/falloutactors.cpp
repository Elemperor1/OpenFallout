#include "falloutactors.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/detournavigator/areatype.hpp>
#include <components/detournavigator/flags.hpp>
#include <components/detournavigator/navigatorutils.hpp>
#include <components/detournavigator/status.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/packageschedule.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4creature.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwrender/falloutactoranimation.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/timestamp.hpp"
#include "../mwworld/worldmodel.hpp"

#include "falloutpackages.hpp"

namespace OFMechanics
{
    namespace
    {
        // How often the packages of an actor are looked at again, in seconds: the time of the game goes by slowly, and
        // an actor that changes its mind twice in a second looks lost
        constexpr float reviewInterval = 1.f;
        // A frame that took longer than this is a pause of the game or a load, not time in which people walk
        constexpr float longestStep = 0.25f;
        // A character that the walking animation does not give a speed for walks at this speed, units a second
        constexpr float defaultWalkSpeed = 120.f;
        constexpr float slowestWalkSpeed = 40.f;
        constexpr float fastestWalkSpeed = 400.f;
        // How fast a character turns toward where it walks, radians a second
        constexpr float turnSpeed = 6.f;
        // Within this distance (across the ground) of a point of its path an actor goes on to the next
        constexpr float waypointDistance = 20.f;
        // Two places that are further apart in height than this are not on the same floor, however near they are on the
        // map: a character below a bed in a loft has not reached it
        constexpr float floorReach = 128.f;
        // A path is followed from the navigator's points on the surface; the feet are put on the ground that a ray
        // finds from this far above them to this far below them
        constexpr float groundProbeUp = 48.f;
        constexpr float groundProbeDown = 160.f;
        // An actor waits for the player to move out of the way when it would come this close
        constexpr float yieldDistance = 70.f;
        // ... unless the player is this far above or below it: a player upstairs is not in the way
        constexpr float yieldHeight = 64.f;
        // An actor that has not come this fraction of the distance that it should have for this long is stuck
        constexpr float progressInterval = 1.5f;
        constexpr float progressFraction = 0.3f;
        constexpr float stuckTime = 4.5f;
        // How long it waits before it asks for a path again, after the navigator had none (its mesh is made as the
        // player goes about), and before it tries again to go where it could not
        constexpr float pathRetryTime = 1.f;
        constexpr float giveUpTime = 10.f;
        // The navigator is asked for paths that end at the nearest place within this distance of the target
        constexpr float endTolerance = 96.f;
        // A follower asks for a new path when the one it follows has gone this far from where its path ends
        constexpr float repathDistance = 150.f;
        // A follower that runs does so at this speed when its animation does not say, units a second
        constexpr float defaultRunSpeed = 300.f;
        constexpr float fastestRunSpeed = 800.f;

        enum class Walk
        {
            Moving, // it moved, or waits for the player to get out of its way
            Waiting, // there is no path yet; ask again soon
            Arrived,
            Failed, // there is no way, or it did not get on
        };

        float distanceAcross(const osg::Vec3f& a, const osg::Vec3f& b)
        {
            return (osg::Vec2f(a.x(), a.y()) - osg::Vec2f(b.x(), b.y())).length();
        }

        bool onSameFloor(const osg::Vec3f& a, const osg::Vec3f& b)
        {
            return std::abs(a.z() - b.z()) <= floorReach;
        }

        /// Whether `a` is within `reach` of `b` on the map and on the same floor
        bool isNear(const osg::Vec3f& a, const osg::Vec3f& b, float reach)
        {
            return onSameFloor(a, b) && distanceAcross(a, b) <= reach;
        }
    }

    struct FalloutActors::Mind
    {
        OFWorld::Ptr mPtr;
        std::vector<const ESM4::AIPackage*> mPackages;
        ESM4::LevelledRandom mRandom;

        /// Whom it follows: the player, or a reference
        struct Following
        {
            bool mPlayer = false;
            ESM::RefNum mRef;
            float mDistance = 0.f;
        };

        const ESM4::AIPackage* mPackage = nullptr; // the one that it follows now
        FalloutGoal mGoal;
        Following mFollowing;
        bool mCommanded = false; // a script told it to follow, which the packages do not change
        bool mHasFacing = false;
        float mFacing = 0.f; // which way it faces when it gets there

        bool mAgent = false; // the navigator has been told of an actor of its size, so that it makes a mesh for it
        float mReview = 0.f; // seconds until the packages are looked at again
        float mWait = 0.f; // seconds to stand still before it does anything else
        bool mWalking = false; // it goes to mDestination
        bool mPartial = false; // the path it follows stops short of mDestination: the navigator could not make the rest
        bool mAtGoal = false; // a goal that stays is reached
        osg::Vec3f mRestPlace; // where it stood when it reached it
        osg::Vec3f mDestination;
        std::deque<osg::Vec3f> mWaypoints;
        float mPathRetry = 0.f;

        // Whether it gets on
        osg::Vec3f mCheckpoint;
        float mCheckpointTime = 0.f;
        float mStuck = 0.f;
        bool mYielded = false;

        Mind(const OFWorld::Ptr& ptr, std::vector<const ESM4::AIPackage*> packages, std::uint64_t seed)
            : mPtr(ptr)
            , mPackages(std::move(packages))
            , mRandom(seed)
        {
            // Actors that load together do not all look at their packages in the same frame
            mReview = reviewInterval * static_cast<float>(mRandom.below(1000)) / 1000.f;
        }

        osg::Vec3f position() const { return mPtr.getRefData().getPosition().asVec3(); }
    };

    namespace
    {
        // The things of the game that all the actors use in a frame
        struct Frame
        {
            OFBase::World& mWorld;
            ESM4::PackageClock mClock;
            osg::Vec3f mPlayer;
            bool mNavigator = false;
        };

        const std::vector<const ESM4::AIPackage*>& packagesOf(const OFWorld::Ptr& ptr)
        {
            if (ptr.getType() == ESM4::Creature::sRecordId)
                return OFClass::ESM4Creature::getPackages(ptr);
            return OFClass::ESM4Npc::getPackages(ptr);
        }

        /// Whether the place is in a cell of the game that is loaded, so that an actor that goes there stays in the
        /// scene
        bool isLoaded(const Frame& frame, const OFWorld::Ptr& ptr, const osg::Vec3f& position)
        {
            const OFWorld::CellStore* cell = ptr.getCell();
            if (cell == nullptr)
                return false;
            if (!cell->isExterior())
                return true;
            const ESM::ExteriorCellLocation index
                = ESM::positionToExteriorCellLocation(position.x(), position.y(), cell->getCell()->getWorldSpace());
            for (const OFWorld::CellStore* active : frame.mWorld.getActiveCells())
                if (active->isExterior() && active->getCell()->getExteriorCellLocation() == index)
                    return true;
            return false;
        }

        /// The animation of the actor that tells how it walks, null when it has none or cannot walk
        const OFRender::FalloutActorAnimation* walker(OFBase::World& world, const OFWorld::Ptr& ptr)
        {
            const auto* animation = dynamic_cast<const OFRender::FalloutActorAnimation*>(world.getAnimation(ptr));
            return animation != nullptr && animation->canWalk() ? animation : nullptr;
        }

        float walkSpeed(const OFRender::FalloutActorAnimation& animation)
        {
            const float velocity = animation.getWalkVelocity();
            return velocity > 0.f ? std::clamp(velocity, slowestWalkSpeed, fastestWalkSpeed) : defaultWalkSpeed;
        }

        float runSpeed(const OFRender::FalloutActorAnimation& animation)
        {
            const float velocity = animation.getRunVelocity();
            return velocity > 0.f ? std::clamp(velocity, slowestWalkSpeed, fastestRunSpeed) : defaultRunSpeed;
        }

        /// Whether the other reference is somewhere the actor can walk to from where it is: in the same cell, or in an
        /// exterior cell of the same worldspace (an interior is a cell of its own)
        bool sharesSpace(const OFWorld::Ptr& actor, const OFWorld::Ptr& other)
        {
            if (other.isEmpty() || !other.isInCell() || !actor.isInCell())
                return false;
            const OFWorld::CellStore* cell = actor.getCell();
            const OFWorld::CellStore* otherCell = other.getCell();
            return otherCell == cell
                || (otherCell->isExterior() && cell->isExterior()
                    && otherCell->getCell()->getWorldSpace() == cell->getCell()->getWorldSpace());
        }

        /// Where the actor is sent to follow, false when the one it follows is not around
        bool followPosition(const Frame& frame, const OFWorld::Ptr& actor,
            const FalloutActors::Mind::Following& following, osg::Vec3f& position)
        {
            if (following.mPlayer)
            {
                if (!sharesSpace(actor, frame.mWorld.getPlayerPtr()))
                    return false;
                position = frame.mPlayer;
                return true;
            }
            const OFWorld::Ptr target = OFBase::Environment::get().getWorldModel()->getPtr(following.mRef);
            if (!sharesSpace(actor, target) || !target.getRefData().isEnabled())
                return false;
            position = target.getRefData().getPosition().asVec3();
            return isLoaded(frame, actor, position);
        }

        /// Where the feet should be: on the ground that a ray finds under the place, else at the height given
        float groundHeight(OFBase::World& world, const osg::Vec3f& place)
        {
            const osg::Vec3f from(place.x(), place.y(), place.z() + groundProbeUp);
            const float maxDistance = groundProbeUp + groundProbeDown;
            const float distance = world.getDistToNearestRayHit(from, osg::Vec3f(0.f, 0.f, -1.f), maxDistance);
            return distance < maxDistance ? from.z() - distance : place.z();
        }

        const char* behaviourName(ESM4::PackageBehaviour behaviour)
        {
            switch (behaviour)
            {
                case ESM4::PackageBehaviour::Stay:
                    return "stays";
                case ESM4::PackageBehaviour::Roam:
                    return "goes about";
                default:
                    return "does nothing";
            }
        }

    }

    namespace
    {
        /// The navigator makes a navigation mesh for each size of actor that it has been told of. The bodies of these
        /// actors are not physics actors (which the scene tells the navigator about), and an interior has the mesh for
        /// the size of its player only, so the actors tell it themselves. It is the default size for all of them, as
        /// it is for every actor in an exterior: the game has no mesh for each size of creature.
        void registerAgent(FalloutActors::Mind& mind)
        {
            OFBase::World& world = *OFBase::Environment::get().getWorld();
            DetourNavigator::Navigator* navigator = world.getNavigator();
            if (navigator != nullptr && navigator->addAgent(world.getPathfindingAgentBounds(mind.mPtr)))
                mind.mAgent = true;
        }

        void stopWalking(FalloutActors::Mind& mind)
        {
            mind.mWalking = false;
            mind.mWaypoints.clear();
            mind.mStuck = 0.f;
            mind.mYielded = false;
        }

        void releaseAgent(FalloutActors::Mind& mind)
        {
            if (!mind.mAgent)
                return;
            mind.mAgent = false;
            OFBase::World& world = *OFBase::Environment::get().getWorld();
            if (DetourNavigator::Navigator* navigator = world.getNavigator())
                navigator->removeAgent(world.getPathfindingAgentBounds(mind.mPtr));
        }
    }

    FalloutActors::FalloutActors() = default;
    FalloutActors::~FalloutActors() = default;

    bool FalloutActors::handles(const OFWorld::Ptr& ptr)
    {
        // Only the records of Fallout 3 and New Vegas: the packages of the other games are laid out differently
        if (ptr.getType() == ESM4::Npc::sRecordId)
            return ptr.get<ESM4::Npc>()->mBase->mIsFONV;
        if (ptr.getType() == ESM4::Creature::sRecordId)
            return ptr.get<ESM4::Creature>()->mBase->mIsFONV;
        return false;
    }

    void FalloutActors::add(const OFWorld::Ptr& ptr)
    {
        remove(ptr);
        if (!handles(ptr) || packagesOf(ptr).empty())
            return;
        makeMind(ptr);
    }

    FalloutActors::Mind& FalloutActors::makeMind(const OFWorld::Ptr& ptr)
    {
        const ESM::RefNum refNum = ptr.getCellRef().getRefNum();
        const std::uint64_t seed = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(refNum.mContentFile)) << 32)
            ^ refNum.mIndex ^ 0x9E3779B97F4A7C15ull;
        Mind& mind = *mMinds.emplace(ptr.mRef, std::make_unique<Mind>(ptr, packagesOf(ptr), seed)).first->second;
        registerAgent(mind);
        return mind;
    }

    void FalloutActors::follow(const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, float distance)
    {
        if (!handles(ptr) || target.isEmpty() || target == ptr)
            return;
        const auto found = mMinds.find(ptr.mRef);
        Mind& mind = found != mMinds.end() ? *found->second : makeMind(ptr);
        mind.mCommanded = true;
        mind.mPackage = nullptr;
        mind.mGoal = FalloutGoal();
        mind.mGoal.mBehaviour = ESM4::PackageBehaviour::Follow;
        mind.mHasFacing = false;
        mind.mFollowing.mPlayer = target == OFBase::Environment::get().getWorld()->getPlayerPtr();
        mind.mFollowing.mRef = target.getCellRef().getRefNum();
        mind.mFollowing.mDistance = falloutFollowDistance(static_cast<std::int32_t>(distance));
        mind.mWait = 0.f;
        stopWalking(mind);
    }

    void FalloutActors::stopFollowing(const OFWorld::Ptr& ptr)
    {
        const auto found = mMinds.find(ptr.mRef);
        if (found == mMinds.end() || !found->second->mCommanded)
            return;
        Mind& mind = *found->second;
        mind.mCommanded = false;
        mind.mFollowing = Mind::Following();
        mind.mPackage = nullptr;
        mind.mGoal = FalloutGoal();
        mind.mReview = 0.f;
        stopWalking(mind);
    }

    void FalloutActors::remove(const OFWorld::Ptr& ptr)
    {
        const auto found = mMinds.find(ptr.mRef);
        if (found == mMinds.end())
            return;
        releaseAgent(*found->second);
        mMinds.erase(found);
    }

    void FalloutActors::updatePtr(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr)
    {
        const auto found = mMinds.find(old.mRef);
        if (found == mMinds.end())
            return;
        std::unique_ptr<Mind> mind = std::move(found->second);
        mMinds.erase(found);
        mind->mPtr = ptr;
        mMinds.emplace(ptr.mRef, std::move(mind));
    }

    void FalloutActors::drop(const OFWorld::CellStore* cell)
    {
        for (auto it = mMinds.begin(); it != mMinds.end();)
        {
            if (it->second->mPtr.getCell() == cell)
            {
                releaseAgent(*it->second);
                it = mMinds.erase(it);
            }
            else
                ++it;
        }
    }

    namespace
    {
        /// Where the package sends the actor: false when it cannot be done here (the reference it names is not in a
        /// cell that is loaded, or is not where the actor can go)
        bool makeGoal(const Frame& frame, const FalloutActors::Mind& mind, const ESM4::AIPackage& package,
            FalloutGoal& goal, bool& hasFacing, float& facing, FalloutActors::Mind::Following& following)
        {
            goal = FalloutGoal();
            hasFacing = false;
            following = FalloutActors::Mind::Following();
            goal.mBehaviour = ESM4::packageBehaviour(package.mData.type);
            goal.mRadius = falloutGoalRadius(package.mLocation.radius, goal.mBehaviour);

            if (goal.mBehaviour == ESM4::PackageBehaviour::Follow)
            {
                // Only a reference can be followed: an object of a kind or a linked reference can not be told yet
                if (package.mTarget.type != 0)
                    return false;
                const ESM::RefNum target = ESM::FormId::fromUint32(package.mTarget.target);
                following.mPlayer
                    = falloutIsPlayerReference(target, falloutFirstPlugin(frame.mWorld.getContentFiles()));
                following.mRef = target;
                following.mDistance = falloutFollowDistance(package.mTarget.distance);
                osg::Vec3f position;
                return followPosition(frame, mind.mPtr, following, position);
            }

            // Where it was put by the file, which is where most packages are about
            goal.mCenter = mind.mPtr.getCellRef().getPosition().asVec3();
            switch (package.mLocation.type)
            {
                case ESM4::Location_NearReference:
                {
                    const ESM::RefNum target = ESM::FormId::fromUint32(package.mLocation.location);
                    const OFWorld::Ptr reference = OFBase::Environment::get().getWorldModel()->getPtr(target);
                    if (reference.isEmpty() || !reference.getRefData().isEnabled())
                        return false;
                    // Only where it can go from where it is
                    if (!sharesSpace(mind.mPtr, reference))
                        return false;
                    goal.mCenter = reference.getRefData().getPosition().asVec3();
                    if (!isLoaded(frame, mind.mPtr, goal.mCenter))
                        return false;
                    hasFacing = goal.mBehaviour == ESM4::PackageBehaviour::Stay;
                    facing = reference.getRefData().getPosition().rot[2];
                    break;
                }
                case ESM4::Location_NearCurrent:
                    goal.mCenter = mind.position();
                    break;
                default:
                    break;
            }
            return true;
        }

        /// Asks the navigator for the path from where the actor is to the place. Success when it has waypoints.
        Walk buildPath(const Frame& frame, FalloutActors::Mind& mind, const osg::Vec3f& destination)
        {
            mind.mWaypoints.clear();
            mind.mPartial = false;
            const osg::Vec3f start = mind.position();
            if (!frame.mNavigator)
            {
                // Without a navigator the actor goes straight, as far as the ground allows
                mind.mWaypoints.push_back(destination);
                return Walk::Moving;
            }

            const DetourNavigator::Status status = DetourNavigator::findPath(*frame.mWorld.getNavigator(),
                frame.mWorld.getPathfindingAgentBounds(mind.mPtr), start, destination, DetourNavigator::Flag_walk,
                DetourNavigator::AreaCosts(), endTolerance, {}, std::back_inserter(mind.mWaypoints));
            switch (status)
            {
                case DetourNavigator::Status::Success:
                    if (!mind.mWaypoints.empty())
                        return Walk::Moving;
                    return Walk::Failed;
                case DetourNavigator::Status::PartialPath:
                    // The place is on another island of the mesh, or in a part of it that is not made yet: the path
                    // goes as far as the navigator can, and a path that goes nowhere from here is no path
                    mind.mPartial = true;
                    if (mind.mWaypoints.empty() || (start - mind.mWaypoints.back()).length() < waypointDistance)
                    {
                        mind.mWaypoints.clear();
                        return Walk::Failed;
                    }
                    return Walk::Moving;
                case DetourNavigator::Status::NavMeshNotFound:
                    mind.mWaypoints.clear();
                    return Walk::Waiting;
                default:
                    Log(Debug::Verbose) << "No path for " << mind.mPtr.getCellRef().getRefId() << " from " << start.x()
                                        << "," << start.y() << " to " << destination.x() << "," << destination.y()
                                        << ": " << DetourNavigator::getMessage(status);
                    mind.mWaypoints.clear();
                    return Walk::Failed;
            }
        }

        void faceToward(const Frame& frame, FalloutActors::Mind& mind, float target, float duration)
        {
            const ESM::Position& position = mind.mPtr.getRefData().getPosition();
            const float turned = falloutTurnToward(position.rot[2], target, turnSpeed * duration);
            if (std::abs(turned - position.rot[2]) > 1e-4f)
                frame.mWorld.rotateObject(
                    mind.mPtr, osg::Vec3f(position.rot[0], position.rot[1], turned), OFBase::RotationFlag_none);
        }

        /// One frame of walking to the place. The actor is there within `arrival` units of the place (0: at it), and may
        /// run.
        Walk walkTo(const Frame& frame, FalloutActors::Mind& mind, const osg::Vec3f& destination, float duration,
            float arrival = 0.f, bool run = false)
        {
            const OFRender::FalloutActorAnimation* animation = walker(frame.mWorld, mind.mPtr);
            if (animation == nullptr)
                return Walk::Failed;

            if (arrival > 0.f && isNear(mind.position(), destination, arrival))
            {
                mind.mWaypoints.clear();
                return Walk::Arrived;
            }

            if (!mind.mWalking)
            {
                mind.mWalking = true;
                mind.mWaypoints.clear();
                mind.mStuck = 0.f;
                mind.mCheckpoint = mind.position();
                mind.mCheckpointTime = 0.f;
            }
            if (mind.mWaypoints.empty())
            {
                if (mind.mPathRetry > 0.f)
                {
                    mind.mPathRetry -= duration;
                    return Walk::Waiting;
                }
                const Walk built = buildPath(frame, mind, destination);
                if (built != Walk::Moving)
                {
                    mind.mPathRetry = pathRetryTime;
                    return built;
                }
            }

            const osg::Vec3f position = mind.position();
            while (mind.mWaypoints.size() > 1 && isNear(position, mind.mWaypoints.front(), waypointDistance))
                mind.mWaypoints.pop_front();
            if (isNear(position, mind.mWaypoints.front(), waypointDistance / 2.f))
            {
                mind.mWaypoints.clear();
                // The end of a path that stopped short is not the place: another path is asked for from here, which
                // fails when it gets no nearer
                if (mind.mPartial)
                {
                    mind.mPathRetry = 0.f;
                    return Walk::Waiting;
                }
                return Walk::Arrived;
            }

            const osg::Vec3f next = mind.mWaypoints.front();
            const float speed = run && animation->canRun() ? runSpeed(*animation) : walkSpeed(*animation);
            osg::Vec3f place = falloutStepToward(position, next, speed * duration);
            place.z() = groundHeight(frame.mWorld, place);

            faceToward(frame, mind, std::atan2(next.x() - position.x(), next.y() - position.y()), duration);

            // The player is in the way: wait where it is
            const float toPlayer = distanceAcross(place, frame.mPlayer);
            if (std::abs(place.z() - frame.mPlayer.z()) < yieldHeight && toPlayer < yieldDistance
                && toPlayer < distanceAcross(position, frame.mPlayer))
            {
                mind.mYielded = true;
                return Walk::Moving;
            }
            if (!isLoaded(frame, mind.mPtr, place))
                return Walk::Failed;

            frame.mWorld.moveObject(mind.mPtr, place);

            // Does it get on?
            mind.mCheckpointTime += duration;
            if (mind.mCheckpointTime >= progressInterval)
            {
                const float moved = distanceAcross(mind.position(), mind.mCheckpoint);
                if (mind.mYielded || moved >= speed * mind.mCheckpointTime * progressFraction)
                    mind.mStuck = 0.f;
                else
                    mind.mStuck += mind.mCheckpointTime;
                mind.mCheckpoint = mind.position();
                mind.mCheckpointTime = 0.f;
                mind.mYielded = false;
            }
            return mind.mStuck >= stuckTime ? Walk::Failed : Walk::Moving;
        }

        /// Looks at the packages and starts to follow the first that is on, if it is not the one that is followed
        void review(const Frame& frame, FalloutActors::Mind& mind)
        {
            const ESM4::AIPackage* chosen = nullptr;
            FalloutGoal goal;
            FalloutActors::Mind::Following following;
            bool hasFacing = false;
            float facing = 0.f;
            for (const ESM4::AIPackage* package : mind.mPackages)
            {
                if (package == nullptr || ESM4::packageSkip(*package, frame.mClock) != ESM4::PackageSkip::None)
                    continue;
                if (!makeGoal(frame, mind, *package, goal, hasFacing, facing, following))
                    continue;
                chosen = package;
                break;
            }
            if (chosen == mind.mPackage)
            {
                // A place that the package names by a reference goes with it when something moves the reference
                if (chosen != nullptr && chosen->mLocation.type == ESM4::Location_NearReference)
                {
                    mind.mHasFacing = hasFacing;
                    mind.mFacing = facing;
                    if ((goal.mCenter - mind.mGoal.mCenter).length() > waypointDistance)
                    {
                        mind.mGoal.mCenter = goal.mCenter;
                        mind.mAtGoal = false;
                        mind.mPathRetry = 0.f;
                        stopWalking(mind);
                    }
                }
                return;
            }

            mind.mPackage = chosen;
            mind.mGoal = chosen != nullptr ? goal : FalloutGoal();
            mind.mFollowing = chosen != nullptr ? following : FalloutActors::Mind::Following();
            mind.mHasFacing = chosen != nullptr && hasFacing;
            mind.mFacing = facing;
            mind.mAtGoal = false;
            mind.mWait = 0.f;
            mind.mPathRetry = 0.f;
            stopWalking(mind);
            Log(Debug::Verbose) << "Package of " << mind.mPtr.getCellRef().getRefId() << " at " << frame.mClock.mHour
                                << " o'clock: " << (chosen != nullptr ? chosen->mEditorId : std::string("none")) << " ("
                                << behaviourName(chosen != nullptr ? ESM4::packageBehaviour(chosen->mData.type)
                                                                   : ESM4::PackageBehaviour::None)
                                << ")";
        }
    }

    void FalloutActors::update(float duration)
    {
        if (mMinds.empty())
            return;

        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return;

        const OFWorld::TimeStamp now = world.getTimeStamp();
        Frame frame{ world, falloutClock(now.getHour(), now.getDay()), player.getRefData().getPosition().asVec3(),
            Settings::navigator().mEnable };
        duration = std::min(duration, longestStep);
        const float range = static_cast<float>(Settings::game().mActorsProcessingRange);

        // An actor that moves to another cell is another reference, and it is added to the map as it is moved
        std::vector<const OFWorld::LiveCellRefBase*> keys;
        keys.reserve(mMinds.size());
        for (const auto& [key, mind] : mMinds)
            keys.push_back(key);

        for (const OFWorld::LiveCellRefBase* key : keys)
        {
            const auto found = mMinds.find(key);
            if (found == mMinds.end())
                continue;
            Mind& mind = *found->second;
            if (distanceAcross(mind.position(), frame.mPlayer) > range)
                continue;

            mind.mReview -= duration;
            if (mind.mReview <= 0.f)
            {
                mind.mReview = reviewInterval;
                if (!mind.mCommanded)
                    review(frame, mind);
            }
            mind.mWait = std::max(0.f, mind.mWait - duration);

            switch (mind.mGoal.mBehaviour)
            {
                case ESM4::PackageBehaviour::None:
                    break;
                case ESM4::PackageBehaviour::Stay:
                {
                    // Something else (a script) moved it from where it stood, up or down as well: it goes back
                    if (mind.mAtGoal && (mind.position() - mind.mRestPlace).length() > waypointDistance)
                        mind.mAtGoal = false;
                    if (!mind.mAtGoal && mind.mWait <= 0.f)
                    {
                        switch (walkTo(frame, mind, mind.mGoal.mCenter, duration, mind.mGoal.mRadius))
                        {
                            case Walk::Arrived:
                                mind.mAtGoal = true;
                                mind.mRestPlace = mind.position();
                                stopWalking(mind);
                                break;
                            case Walk::Failed:
                                // It cannot get there; it tries again after a while
                                stopWalking(mind);
                                mind.mWait = giveUpTime;
                                break;
                            default:
                                break;
                        }
                    }
                    if (mind.mAtGoal && mind.mHasFacing)
                        faceToward(frame, mind, mind.mFacing, duration);
                    break;
                }
                case ESM4::PackageBehaviour::Follow:
                {
                    osg::Vec3f target;
                    if (!followPosition(frame, mind.mPtr, mind.mFollowing, target))
                    {
                        // The one it follows is not around: it waits where it is
                        stopWalking(mind);
                        break;
                    }
                    const float away = distanceAcross(mind.position(), target);
                    if (!falloutFollowMoves(away, mind.mFollowing.mDistance, mind.mWalking))
                    {
                        if (mind.mWalking)
                            stopWalking(mind);
                        faceToward(frame, mind,
                            std::atan2(target.x() - mind.position().x(), target.y() - mind.position().y()), duration);
                        break;
                    }
                    if (mind.mWait > 0.f)
                        break;
                    // The one it follows has gone on: the path is made again
                    if (!mind.mWalking || distanceAcross(target, mind.mDestination) > repathDistance)
                    {
                        mind.mDestination = target;
                        mind.mWaypoints.clear();
                    }
                    switch (walkTo(frame, mind, mind.mDestination, duration, mind.mFollowing.mDistance,
                        falloutFollowRuns(away, mind.mFollowing.mDistance)))
                    {
                        case Walk::Arrived:
                            stopWalking(mind);
                            break;
                        case Walk::Failed:
                            stopWalking(mind);
                            mind.mWait = pathRetryTime;
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case ESM4::PackageBehaviour::Roam:
                {
                    if (!mind.mWalking)
                    {
                        if (mind.mWait > 0.f)
                            break;
                        mind.mDestination = falloutRoamPoint(mind.mGoal, mind.mRandom);
                        // On its own floor the point is at the height of the character, as the ground is not at the
                        // height of the place; the place of another floor keeps its height, so that the path goes there
                        if (onSameFloor(mind.position(), mind.mDestination))
                            mind.mDestination.z() = mind.position().z();
                    }
                    switch (walkTo(frame, mind, mind.mDestination, duration))
                    {
                        case Walk::Arrived:
                            stopWalking(mind);
                            mind.mWait = falloutRoamPause(mind.mRandom);
                            break;
                        case Walk::Failed:
                            stopWalking(mind);
                            mind.mWait = pathRetryTime + falloutRoamPause(mind.mRandom) / 3.f;
                            break;
                        default:
                            break;
                    }
                    break;
                }
            }
        }
    }
}
