#include "falloutactors.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <set>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/detournavigator/areatype.hpp>
#include <components/detournavigator/flags.hpp>
#include <components/detournavigator/navigatorutils.hpp>
#include <components/detournavigator/status.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/packageschedule.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4creature.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwrender/falloutactoranimation.hpp"
#include "../mwscript/falloutscripts.hpp"
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
        // ... for this many seconds: a player that stands in the way (it is not moved by anyone) is walked past
        constexpr float yieldPatience = 4.f;
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
        // A package that scripts wait for and has had no path for this long is logged once (the navigator makes its
        // mesh as the player goes about, and a character in a cell that it does not make one for waits for ever)
        constexpr float pathWaitingReport = 10.f;
        // A character that has not got to the place of a Travel package that scripts wait for after this many tries
        // is put there
        constexpr int failuresBeforeJump = 2;
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

        /// How far `a` is from `b` on the map; on another floor, too far for any distance to be near
        float distanceByFloor(const osg::Vec3f& a, const osg::Vec3f& b)
        {
            return onSameFloor(a, b) ? distanceAcross(a, b) : std::numeric_limits<float>::max();
        }
    }

    struct FalloutActors::Mind
    {
        OFWorld::Ptr mPtr;
        std::vector<const ESM4::AIPackage*> mPackages;
        /// The packages that scripts gave it, the last first, which come before its own whatever the time and the
        /// conditions say
        std::vector<const ESM4::AIPackage*> mScripted;
        ESM4::LevelledRandom mRandom;

        /// Whom it follows: the player, or a reference
        struct Following
        {
            bool mPlayer = false;
            ESM::RefNum mRef;
            float mDistance = 0.f;
        };

        const ESM4::AIPackage* mPackage = nullptr; // the one that it follows now
        /// The packages it has done (a Travel package whose place it got to) and has not begun again since. They come
        /// after the packages that are not done, and it stays with the one it follows if that is the only one left.
        std::set<const ESM4::AIPackage*> mDone;
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
        bool mSpoke = false; // a Dialogue package: it has said what it came to say, and is done when the lines are over
        /// A package that is done and has nothing after it is over for the actor, and its Change script has been told
        bool mChangeTold = false;
        int mWaitingFor = 0; // why a Dialogue package stands still (a waitFor constant), once told in the log
        /// The Dialogue packages that are on and that are left out because the one to talk to is not around, once told
        /// in the log
        mutable std::set<const ESM4::AIPackage*> mSkipTold;
        /// How often the character went from one package to another, for the log: a pair that comes back is told
        std::map<std::pair<const ESM4::AIPackage*, const ESM4::AIPackage*>, int> mSwitches;
        int mFailures = 0; // how many times in a row it has had no way to get to the place of the package
        osg::Vec3f mRestPlace; // where it stood when it reached it
        osg::Vec3f mDestination;
        std::deque<osg::Vec3f> mWaypoints;
        float mPathRetry = 0.f;

        // Whether it gets on
        osg::Vec3f mCheckpoint;
        float mCheckpointTime = 0.f;
        float mStuck = 0.f;
        bool mYielded = false;
        FalloutYield mYield; // how long it has waited for the player to move out of its way, and whether it goes on
        float mPathWaiting = 0.f; // how long the navigator has had no mesh to make a path on
        bool mPathWaitingReported = false;

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
        struct PendingEvent
        {
            OFWorld::Ptr mActor;
            const ESM4::AIPackage* mPackage = nullptr;
            OFScript::FalloutScripts::PackageEvent mEvent = OFScript::FalloutScripts::PackageEvent::Start;
        };

        // The things of the game that all the actors use in a frame
        struct Frame
        {
            OFBase::World& mWorld;
            ESM4::PackageClock mClock;
            osg::Vec3f mPlayer;
            bool mNavigator = false;
            /// What decides the conditions of the packages: null when there are no scripts
            OFScript::FalloutScripts* mScripts = nullptr;
            /// What the packages of the actors do that scripts are told of, which they are after all the actors have
            /// had their turn: a script that runs in the middle of it can give an actor a package or move it
            std::vector<PendingEvent>* mEvents = nullptr;
        };

        const std::vector<const ESM4::AIPackage*>& packagesOf(const OFWorld::Ptr& ptr)
        {
            if (ptr.getType() == ESM4::Creature::sRecordId)
                return OFClass::ESM4Creature::getPackages(ptr);
            return OFClass::ESM4Npc::getPackages(ptr);
        }

        /// The packages that scripts gave the actor, the last first, as records
        std::vector<const ESM4::AIPackage*> scriptPackagesOf(const OFWorld::Ptr& ptr)
        {
            OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            return scripts != nullptr ? scripts->scriptPackages(ptr) : std::vector<const ESM4::AIPackage*>();
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
                case ESM4::PackageBehaviour::Talk:
                    return "goes and talks";
                default:
                    return "does nothing";
            }
        }

        /// Whether the package runs scripts when it begins, is done or is changed
        bool hasEvents(const ESM4::AIPackage& package)
        {
            return package.mBegin.mPresent || package.mEnd.mPresent || package.mChange.mPresent;
        }

        void raise(const Frame& frame, const OFWorld::Ptr& actor, const ESM4::AIPackage& package,
            OFScript::FalloutScripts::PackageEvent event)
        {
            if (frame.mEvents != nullptr)
                frame.mEvents->push_back({ actor, &package, event });
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
            mind.mYield = FalloutYield();
            mind.mPathWaiting = 0.f;
            mind.mPathWaitingReported = false;
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
        if (!handles(ptr))
            return;
        // A command from a script outlives the cell: the character takes it up again when the cell is loaded
        const auto command = mCommands.find(ptr.getCellRef().getRefNum());
        if (command == mCommands.end() && packagesOf(ptr).empty() && scriptPackagesOf(ptr).empty())
            return;
        Mind& mind = makeMind(ptr);
        if (command != mCommands.end())
            obey(mind, command->second);
    }

    FalloutActors::Mind& FalloutActors::makeMind(const OFWorld::Ptr& ptr)
    {
        const ESM::RefNum refNum = ptr.getCellRef().getRefNum();
        const std::uint64_t seed = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(refNum.mContentFile)) << 32)
            ^ refNum.mIndex ^ 0x9E3779B97F4A7C15ull;
        Mind& mind = *mMinds.emplace(ptr.mRef, std::make_unique<Mind>(ptr, packagesOf(ptr), seed)).first->second;
        mind.mScripted = scriptPackagesOf(ptr);
        registerAgent(mind);
        return mind;
    }

    void FalloutActors::evaluate(const OFWorld::Ptr& ptr)
    {
        if (!handles(ptr))
            return;
        const auto found = mMinds.find(ptr.mRef);
        if (found == mMinds.end())
        {
            // A character with no packages of its own has no mind until a script gives it one
            if (scriptPackagesOf(ptr).empty())
                return;
            makeMind(ptr).mReview = 0.f;
            return;
        }
        found->second->mScripted = scriptPackagesOf(ptr);
        found->second->mReview = 0.f;
    }

    ESM::FormId FalloutActors::currentPackage(const OFWorld::Ptr& ptr) const
    {
        const auto found = mMinds.find(ptr.mRef);
        if (found == mMinds.end() || found->second->mPackage == nullptr)
            return ESM::FormId();
        return found->second->mPackage->mId;
    }

    void FalloutActors::follow(const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, float distance)
    {
        if (!handles(ptr) || target.isEmpty() || target == ptr)
            return;
        const auto found = mMinds.find(ptr.mRef);
        Mind& mind = found != mMinds.end() ? *found->second : makeMind(ptr);
        // A character in a piece of furniture gets up when it is told to follow, which no package change tells
        if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
            scripts->leaveFurniture(ptr);
        Command command;
        command.mPlayer = target == OFBase::Environment::get().getWorld()->getPlayerPtr();
        command.mTarget = target.getCellRef().getRefNum();
        command.mDistance = falloutScriptFollowDistance(distance);
        obey(mind, command);
        if (const ESM::RefNum refNum = ptr.getCellRef().getRefNum(); refNum.isSet())
            mCommands[refNum] = command;
    }

    void FalloutActors::obey(Mind& mind, const Command& command)
    {
        mind.mCommanded = true;
        mind.mPackage = nullptr;
        mind.mGoal = FalloutGoal();
        mind.mGoal.mBehaviour = ESM4::PackageBehaviour::Follow;
        mind.mHasFacing = false;
        mind.mFollowing.mPlayer = command.mPlayer;
        mind.mFollowing.mRef = command.mTarget;
        mind.mFollowing.mDistance = command.mDistance;
        mind.mWait = 0.f;
        stopWalking(mind);
    }

    void FalloutActors::stopFollowing(const OFWorld::Ptr& ptr)
    {
        mCommands.erase(ptr.getCellRef().getRefNum());
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

    void FalloutActors::clear()
    {
        // The cells are unloaded before this, so there are no minds left to give their agents back
        mMinds.clear();
        mCommands.clear();
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

            if (goal.mBehaviour == ESM4::PackageBehaviour::Talk)
            {
                // Only a reference can be talked to; it has to be around, or the package is not on
                if (package.mTarget.type != 0)
                    return false;
                const ESM::RefNum target = ESM::FormId::fromUint32(package.mTarget.target);
                following.mPlayer
                    = falloutIsPlayerReference(target, falloutFirstPlugin(frame.mWorld.getContentFiles()));
                following.mRef = target;
                following.mDistance = falloutTalkDistance(package.mTarget.distance);
                osg::Vec3f position;
                if (!followPosition(frame, mind.mPtr, following, position))
                {
                    // A package that is on and is left out says so, once: a quest can wait for it
                    if (mind.mSkipTold.insert(&package).second)
                        Log(hasEvents(package) ? Debug::Info : Debug::Verbose)
                            << "Package " << package.mEditorId << " of " << mind.mPtr.getCellRef().getRefId()
                            << ": left out, the one it talks to ("
                            << ESM::RefId(ESM::FormId::fromUint32(package.mTarget.target)) << ") is not around";
                    return false;
                }
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
                    // A character that goes to a piece of furniture goes to the place of its marker where it sits down,
                    // which is beside the furniture, not to the middle of it
                    if (frame.mScripts != nullptr && reference.getType() == ESM4::Furniture::sRecordId)
                    {
                        float heading = 0.f;
                        frame.mScripts->furnitureSeat(mind.mPtr, reference, goal.mCenter, heading);
                    }
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

        /// One frame of walking to the place. The actor is there within `arrival` units of the place (0: at it) and on
        /// its floor, and may run.
        Walk walkTo(const Frame& frame, FalloutActors::Mind& mind, const osg::Vec3f& destination, float duration,
            float arrival = 0.f, bool run = false)
        {
            // A character that is there already has arrived, whether it could walk or not
            if (arrival > 0.f && isNear(mind.position(), destination, arrival))
            {
                mind.mWaypoints.clear();
                return Walk::Arrived;
            }

            const OFRender::FalloutActorAnimation* animation = walker(frame.mWorld, mind.mPtr);
            if (animation == nullptr)
                return Walk::Failed;

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

            // The player is in the way: wait where it is, for a while
            const float toPlayer = distanceAcross(place, frame.mPlayer);
            const bool wasPassing = mind.mYield.mPassing;
            const bool nearPlayer = std::abs(position.z() - frame.mPlayer.z()) < yieldHeight
                && distanceAcross(position, frame.mPlayer) < yieldDistance;
            const bool inTheWay = std::abs(place.z() - frame.mPlayer.z()) < yieldHeight && toPlayer < yieldDistance
                && toPlayer < distanceAcross(position, frame.mPlayer);
            if (falloutYields(mind.mYield, nearPlayer, inTheWay, duration, yieldPatience))
            {
                mind.mYielded = true;
                return Walk::Moving;
            }
            if (mind.mYield.mPassing && !wasPassing)
                Log(Debug::Info) << mind.mPtr.getCellRef().getRefId() << " waited " << yieldPatience
                                 << " s for the player to move out of its way and goes on";
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

        /// What the actor does for a package that is on
        struct Choice
        {
            const ESM4::AIPackage* mPackage = nullptr;
            FalloutGoal mGoal;
            FalloutActors::Mind::Following mFollowing;
            bool mHasFacing = false;
            float mFacing = 0.f;
        };

        /// Looks at the packages and starts to follow the first that is on, if it is not the one that is followed. A
        /// package that is done comes after the others that are on, and the actor goes on with another package when
        /// there is one; when there is none it stays with the one it follows if that is done, else it goes back to the
        /// first that is done (a character does the Travel package of the day again once the night is over).
        void review(const Frame& frame, FalloutActors::Mind& mind)
        {
            Choice fresh; // the first that is on and not done
            Choice current; // the one that is followed, when it is done and still on
            Choice done; // the first of the others that are done and on
            // Why the package that is followed is not the one to follow, for the log
            std::string followedIs = "on";
            const auto consider = [&](const ESM4::AIPackage* package, bool scripted) {
                if (package == nullptr)
                    return;
                const bool followed = package == mind.mPackage;
                if (fresh.mPackage != nullptr)
                {
                    if (followed)
                        followedIs = "behind " + fresh.mPackage->mEditorId;
                    return;
                }
                if (scripted)
                {
                    // What a script gave it is on whatever the time and the conditions are
                    if (ESM4::packageBehaviour(package->mData.type) == ESM4::PackageBehaviour::None)
                    {
                        if (followed)
                            followedIs = "of a kind that does nothing";
                        return;
                    }
                }
                else
                {
                    ESM4::PackageSkip skip = ESM4::packageSkip(*package, frame.mClock);
                    // The time and the kind are right: it is on if its conditions hold
                    if (skip == ESM4::PackageSkip::Conditions && frame.mScripts != nullptr
                        && frame.mScripts->packageConditionsHold(*package, mind.mPtr))
                        skip = ESM4::PackageSkip::None;
                    if (skip != ESM4::PackageSkip::None)
                    {
                        if (followed)
                            followedIs = skip == ESM4::PackageSkip::Schedule ? "not for this time"
                                : skip == ESM4::PackageSkip::Conditions      ? "not on, its conditions do not hold"
                                                                             : "of a kind that does nothing";
                        return;
                    }
                }
                Choice choice;
                choice.mPackage = package;
                if (!makeGoal(
                        frame, mind, *package, choice.mGoal, choice.mHasFacing, choice.mFacing, choice.mFollowing))
                {
                    if (followed)
                        followedIs = "without a place or a target to go to";
                    return;
                }
                if (mind.mDone.count(package) == 0)
                    fresh = std::move(choice);
                else if (package == mind.mPackage)
                    current = std::move(choice);
                else if (done.mPackage == nullptr)
                    done = std::move(choice);
            };
            for (const ESM4::AIPackage* package : mind.mScripted)
                consider(package, true);
            for (const ESM4::AIPackage* package : mind.mPackages)
                consider(package, false);
            // An actor sitting in the furniture that its package led it to stays there while that package is on and
            // no package that comes before it is: the games take it from the chair when a package asks, not because
            // another one has become possible
            const bool seated
                = current.mPackage != nullptr && frame.mScripts != nullptr && frame.mScripts->usesFurniture(mind.mPtr);
            const Choice& chosen = seated     ? current
                : fresh.mPackage != nullptr   ? fresh
                : current.mPackage != nullptr ? current
                                              : done;

            if (chosen.mPackage == mind.mPackage)
            {
                // A place that the package names by a reference goes with it when something moves the reference
                if (chosen.mPackage != nullptr && chosen.mPackage->mLocation.type == ESM4::Location_NearReference)
                {
                    mind.mHasFacing = chosen.mHasFacing;
                    mind.mFacing = chosen.mFacing;
                    if ((chosen.mGoal.mCenter - mind.mGoal.mCenter).length() > waypointDistance)
                    {
                        mind.mGoal.mCenter = chosen.mGoal.mCenter;
                        mind.mAtGoal = false;
                        mind.mPathRetry = 0.f;
                        stopWalking(mind);
                    }
                }
                // A package that is done and that nothing follows (the character stays where it did it) is over: the
                // games take it from the character, and its Change script is told, once. A character in a piece of
                // furniture is still at what the package led it to.
                if (chosen.mPackage != nullptr && !seated && !mind.mChangeTold
                    && mind.mDone.count(chosen.mPackage) != 0)
                {
                    mind.mChangeTold = true;
                    raise(frame, mind.mPtr, *chosen.mPackage, OFScript::FalloutScripts::PackageEvent::Change);
                }
                return;
            }

            // A character that goes back and forth between the same two packages is told once, and again later
            if (mind.mPackage != nullptr && chosen.mPackage != nullptr)
            {
                const int times = ++mind.mSwitches[{ mind.mPackage, chosen.mPackage }];
                if (times == 2 || times == 20)
                    Log(Debug::Info) << "Package of " << mind.mPtr.getCellRef().getRefId() << ": goes from "
                                     << mind.mPackage->mEditorId << " to " << chosen.mPackage->mEditorId << " again ("
                                     << times << " times; " << mind.mPackage->mEditorId << " is " << followedIs
                                     << (mind.mDone.count(mind.mPackage) != 0 ? ", done" : ", not done") << "; "
                                     << chosen.mPackage->mEditorId
                                     << (mind.mDone.count(chosen.mPackage) != 0 ? " was done" : " is not done") << ")";
            }

            // The package it followed is changed, done or not, unless it was told so already, and the one it follows
            // begins
            if (mind.mPackage != nullptr && !mind.mChangeTold)
                raise(frame, mind.mPtr, *mind.mPackage, OFScript::FalloutScripts::PackageEvent::Change);
            // A character in a piece of furniture gets up when it is given another package (the games ask it to leave)
            if (frame.mScripts != nullptr)
                frame.mScripts->leaveFurniture(mind.mPtr);
            if (chosen.mPackage != nullptr)
            {
                mind.mDone.erase(chosen.mPackage);
                raise(frame, mind.mPtr, *chosen.mPackage, OFScript::FalloutScripts::PackageEvent::Start);
            }

            mind.mPackage = chosen.mPackage;
            mind.mGoal = chosen.mPackage != nullptr ? chosen.mGoal : FalloutGoal();
            mind.mFollowing = chosen.mPackage != nullptr ? chosen.mFollowing : FalloutActors::Mind::Following();
            mind.mHasFacing = chosen.mPackage != nullptr && chosen.mHasFacing;
            mind.mFacing = chosen.mFacing;
            mind.mAtGoal = false;
            mind.mSpoke = false;
            mind.mChangeTold = false;
            mind.mWaitingFor = 0;
            mind.mFailures = 0;
            mind.mWait = 0.f;
            mind.mPathRetry = 0.f;
            stopWalking(mind);
            // The packages that run scripts are the ones a quest waits for: they are told even if the log is not
            Log(chosen.mPackage != nullptr && hasEvents(*chosen.mPackage) ? Debug::Info : Debug::Verbose)
                << "Package of " << mind.mPtr.getCellRef().getRefId() << " at " << frame.mClock.mHour
                << " o'clock: " << (chosen.mPackage != nullptr ? chosen.mPackage->mEditorId : std::string("none"))
                << " ("
                << behaviourName(chosen.mPackage != nullptr ? ESM4::packageBehaviour(chosen.mPackage->mData.type)
                                                            : ESM4::PackageBehaviour::None)
                << ")";
        }

        /// A character has got to the place of its Travel package: the package is done, once
        void finishTravel(const Frame& frame, FalloutActors::Mind& mind)
        {
            const ESM4::AIPackage* package = mind.mPackage;
            if (package == nullptr || package->mData.type != ESM4::Package_Travel || mind.mDone.count(package) != 0)
                return;
            // A character that has got to a piece of furniture uses it, and the script of the package is told after
            // that, as the games tell it when the character is in place
            if (package->mLocation.type == ESM4::Location_NearReference && frame.mScripts != nullptr)
            {
                const OFWorld::Ptr place = OFBase::Environment::get().getWorldModel()->getPtr(
                    ESM::RefNum(ESM::FormId::fromUint32(package->mLocation.location)));
                if (!place.isEmpty() && place.getType() == ESM4::Furniture::sRecordId
                    && !frame.mScripts->useFurniture(mind.mPtr, place, true))
                {
                    // Every place of it is taken: the package is not done, and the character tries again later
                    mind.mAtGoal = false;
                    mind.mWait = giveUpTime;
                    return;
                }
            }
            mind.mDone.insert(package);
            raise(frame, mind.mPtr, *package, OFScript::FalloutScripts::PackageEvent::Done);
            // The packages are looked at again at once: the next one is followed
            mind.mReview = 0.f;
        }
    }

    namespace
    {
        /// A character that has a package that tells scripts what happens to it cannot get to its place: that is
        /// logged, and a Travel package that it has failed twice is done by putting the character at the place, since a
        /// quest waits for it (the way may be one that the navigator does not know, or the character cannot walk)
        void failToGetThere(const Frame& frame, FalloutActors::Mind& mind)
        {
            const ESM4::AIPackage& package = *mind.mPackage;
            ++mind.mFailures;
            const bool jump = package.mData.type == ESM4::Package_Travel && mind.mFailures >= failuresBeforeJump;
            Log(Debug::Info) << "Package " << package.mEditorId << " of " << mind.mPtr.getCellRef().getRefId()
                             << ": no way to get there ("
                             << (walker(frame.mWorld, mind.mPtr) == nullptr ? "it cannot walk"
                                                                            : "the navigator has no path")
                             << ")" << (jump ? ", put there" : "");
            if (!jump)
                return;
            osg::Vec3f place = mind.mGoal.mCenter;
            place.z() = groundHeight(frame.mWorld, place);
            frame.mWorld.moveObject(mind.mPtr, place);
            mind.mAtGoal = true;
            mind.mRestPlace = mind.position();
            mind.mWait = 0.f;
            finishTravel(frame, mind);
        }

        /// One frame of going to the place of the package; true when the character has got there
        bool goToPlace(const Frame& frame, FalloutActors::Mind& mind, float duration)
        {
            switch (walkTo(frame, mind, mind.mGoal.mCenter, duration, mind.mGoal.mRadius))
            {
                case Walk::Arrived:
                    mind.mAtGoal = true;
                    mind.mRestPlace = mind.position();
                    mind.mFailures = 0;
                    stopWalking(mind);
                    return true;
                case Walk::Failed:
                    // It cannot get there; it tries again after a while
                    stopWalking(mind);
                    mind.mWait = giveUpTime;
                    if (mind.mPackage != nullptr && hasEvents(*mind.mPackage))
                        failToGetThere(frame, mind);
                    else
                        ++mind.mFailures;
                    return false;
                case Walk::Waiting:
                    mind.mPathWaiting += duration;
                    if (mind.mPathWaiting >= pathWaitingReport && !mind.mPathWaitingReported && mind.mPackage != nullptr
                        && hasEvents(*mind.mPackage))
                    {
                        mind.mPathWaitingReported = true;
                        Log(Debug::Info) << "Package " << mind.mPackage->mEditorId << " of "
                                         << mind.mPtr.getCellRef().getRefId() << ": no path for " << pathWaitingReport
                                         << " s (the navigator has no mesh here yet)";
                    }
                    return false;
                default:
                    mind.mPathWaiting = 0.f;
                    return false;
            }
        }

        constexpr int waitNothing = 0;
        constexpr int waitForTarget = 1; // the one it talks to is not around
        constexpr int waitForPlayer = 2; // the player is busy with a menu, a box or a conversation

        /// Tells why a Dialogue package stands still, once for each reason, as the packages that run scripts are the
        /// ones a quest waits for
        void waitFor(FalloutActors::Mind& mind, int reason, const std::string& detail = std::string())
        {
            if (mind.mWaitingFor == reason || mind.mPackage == nullptr)
                return;
            mind.mWaitingFor = reason;
            if (reason == waitNothing)
                return;
            Log(hasEvents(*mind.mPackage) ? Debug::Info : Debug::Verbose)
                << "Package " << mind.mPackage->mEditorId << " of " << mind.mPtr.getCellRef().getRefId() << ": waits, "
                << (reason == waitForTarget ? "the one it talks to is not around"
                                            : "the player is busy (" + detail + ")");
        }

        /// One frame of a Dialogue package. The character goes to the place of the package when it names one, then to
        /// its target, as near as the package says, and talks: a Say To package says a response of its topic, a
        /// Conversation package begins the conversation. The package is done when it has said what it came to say.
        void talk(const Frame& frame, FalloutActors::Mind& mind, float duration)
        {
            const ESM4::AIPackage* package = mind.mPackage;
            if (package == nullptr || mind.mDone.count(package) != 0 || mind.mWait > 0.f)
                return;

            if (!mind.mAtGoal)
            {
                // Only a place that a reference names is gone to; otherwise the character is where it talks from. One
                // that cannot get to the place talks from where it is, as a Travel package is done by putting the
                // character there: a quest waits for it
                if (package->mLocation.type != ESM4::Location_NearReference)
                    mind.mAtGoal = true;
                else if (!goToPlace(frame, mind, duration))
                {
                    if (mind.mFailures < failuresBeforeJump)
                        return;
                    mind.mAtGoal = true;
                    mind.mFailures = 0;
                    mind.mWait = 0.f;
                }
            }

            if (mind.mSpoke)
            {
                // It is done with the last line that it says
                if (frame.mScripts != nullptr && frame.mScripts->isSaying(mind.mPtr))
                    return;
                mind.mSpoke = false;
                mind.mDone.insert(package);
                raise(frame, mind.mPtr, *package, OFScript::FalloutScripts::PackageEvent::Done);
                mind.mReview = 0.f;
                return;
            }

            osg::Vec3f target;
            if (!followPosition(frame, mind.mPtr, mind.mFollowing, target))
            {
                // The one it talks to is not around: it waits where it is
                stopWalking(mind);
                waitFor(mind, waitForTarget);
                return;
            }
            const float distance = mind.mFollowing.mDistance;
            const float away = distanceByFloor(mind.position(), target);
            // The games walk a Say To package up to the player; any other talk starts where the character is once it
            // has been to its place. One that cannot get to the player says what it came to say from where it is.
            const bool approach = package->mDialogue.mType == ESM4::AIPackage::DialogueData::SayTo
                && mind.mFollowing.mPlayer && mind.mFailures < failuresBeforeJump;
            if (approach && falloutFollowMoves(away, distance, mind.mWalking))
            {
                // The one it talks to has gone on: the path is made again
                if (!mind.mWalking || distanceAcross(target, mind.mDestination) > repathDistance)
                {
                    mind.mDestination = target;
                    mind.mWaypoints.clear();
                }
                switch (walkTo(frame, mind, mind.mDestination, duration, distance, falloutFollowRuns(away, distance)))
                {
                    case Walk::Arrived:
                        stopWalking(mind);
                        mind.mFailures = 0;
                        break;
                    case Walk::Failed:
                        stopWalking(mind);
                        mind.mWait = pathRetryTime;
                        if (hasEvents(*package))
                            failToGetThere(frame, mind);
                        else
                            ++mind.mFailures;
                        break;
                    default:
                        break;
                }
                return;
            }

            if (mind.mWalking)
                stopWalking(mind);
            faceToward(
                frame, mind, std::atan2(target.x() - mind.position().x(), target.y() - mind.position().y()), duration);

            OFWorld::Ptr listener;
            if (!mind.mFollowing.mPlayer)
                listener = OFBase::Environment::get().getWorldModel()->getPtr(mind.mFollowing.mRef);
            std::string busy;
            if (frame.mScripts != nullptr && !frame.mScripts->speakPackage(mind.mPtr, *package, listener, &busy))
            {
                // The player is busy: it tries again in a moment
                mind.mWait = pathRetryTime;
                waitFor(mind, waitForPlayer, busy);
                return;
            }
            mind.mWaitingFor = waitNothing;
            mind.mSpoke = true;
            Log(Debug::Info) << "Package " << package->mEditorId << " of " << mind.mPtr.getCellRef().getRefId()
                             << ": it talks to " << (mind.mFollowing.mPlayer ? "the player" : "a character");
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
        std::vector<PendingEvent> events;
        Frame frame{ world, falloutClock(now.getHour(), now.getDay()), player.getRefData().getPosition().asVec3(),
            Settings::navigator().mEnable, OFBase::Environment::get().getFalloutScripts(), &events };
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

            // A script killed it, and nothing else in the game does yet: it does nothing more
            if (frame.mScripts != nullptr && frame.mScripts->isDead(mind.mPtr))
            {
                stopWalking(mind);
                continue;
            }

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
                    if (!mind.mAtGoal && mind.mWait <= 0.f && goToPlace(frame, mind, duration))
                        finishTravel(frame, mind);
                    if (mind.mAtGoal && mind.mHasFacing)
                        faceToward(frame, mind, mind.mFacing, duration);
                    break;
                }
                case ESM4::PackageBehaviour::Talk:
                    talk(frame, mind, duration);
                    break;
                case ESM4::PackageBehaviour::Follow:
                {
                    osg::Vec3f target;
                    if (!followPosition(frame, mind.mPtr, mind.mFollowing, target))
                    {
                        // The one it follows is not around: it waits where it is
                        stopWalking(mind);
                        break;
                    }
                    const float away = distanceByFloor(mind.position(), target);
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

        // The scripts of the packages run now that every actor has had its turn: what they do (give an actor a package,
        // move one) is for the next frame
        if (frame.mScripts != nullptr)
            for (const PendingEvent& event : events)
                if (!event.mActor.isEmpty())
                    frame.mScripts->packageEvent(event.mActor, *event.mPackage, event.mEvent);
    }
}
