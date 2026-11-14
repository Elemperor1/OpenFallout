#include "falloutactors.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
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
#include <components/falloutscript/actorstatus.hpp>
#include <components/falloutscript/aimemory.hpp>
#include <components/falloutscript/worldrules.hpp>
#include <components/falloutscript/fightai.hpp>
#include <components/misc/constants.hpp>
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
        /// The game time as hours since the start (days times 24 plus the hour of the day)
        double gameHours(const OFWorld::TimeStamp& stamp)
        {
            return stamp.getDay() * 24.0 + stamp.getHour();
        }
    }

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
        // The walk in a straight line where the navigator has no path looks at the way from this far above the floor
        constexpr float straightKneeHeight = 35.f;
        constexpr float straightChestHeight = 100.f;
        /// How near to a door a character that goes through it has arrived
        constexpr float doorArrival = 96.f;
        // A follower asks for a new path when the one it follows has gone this far from where its path ends
        constexpr float repathDistance = 150.f;
        // A follower that runs does so at this speed when its animation does not say, units a second
        constexpr float defaultRunSpeed = 300.f;
        constexpr float fastestRunSpeed = 800.f;
        // How often a character at peace looks for the player to go for, in seconds
        constexpr float sightInterval = 0.5f;
        // Characters see each other from this far above their feet
        constexpr float eyeHeight = 110.f;
        // A character that goes for its target stops at this share of the distance it attacks from, so that it does not
        // start again at once when the target steps back
        constexpr float engageShare = 0.9f;
        // A character that goes for its target asks for a new path when the target has gone this far from the end of
        // the path
        constexpr float fightRepathDistance = 60.f;
        // A character that runs away goes this far at a time, in the direction away from the one it runs from
        constexpr float fleeStep = 600.f;
        // A shot or a throw is shown with the weapon that made it: when it used the weapon up and the character took
        // another, the next weapon is drawn this many seconds later
        constexpr float shotShownFor = 0.8f;
        /// Real seconds a character in range of its target may go without firing before it says why
        constexpr float holdFireReportAfter = 5.f;
        /// A character in a fight that has not shot and has not gone this far in holdFireReportAfter seconds says where
        /// it stands and why (round 10O: a raider stood idle 113 and 402 units from the player and no line said why)
        constexpr float idleReportMove = 30.f;
        /// How near to the place it was told to shoot from a character has to be to shoot
        constexpr float scriptedPlaceReach = 64.f;
        // A character that a blow throws back is stopped by what it would run into within this distance of its middle,
        // at knee height
        constexpr float thrownBodyRadius = 24.f;
        // ... steps up onto ground that is this much higher than it stands, and steps off ground that falls away by
        // more than this (and falls)
        constexpr float thrownStepUp = 36.f;
        constexpr float thrownStepDown = 40.f;
        // A fall looks for the floor this far below, and a character that has fallen further than this with none below
        // it is left where it is
        constexpr float fallProbeDown = 8000.f;
        constexpr float fallGiveUp = 20000.f;

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
        float mSight = 0.f; // seconds until it looks for the player to go for
        bool mFighting = false; // it was in a fight the last time it was looked at
        FalloutScript::FormId mDrawn = 0; // the weapon whose animations it was last given in a fight
        float mShowAfter = 0.f; // seconds until the weapon it holds is drawn, after a shot that used the last one up
        float mWatch = 0.f; // seconds until it looks for the player again, for the sneak meter
        float mHeld = 0.f; // seconds it has stood in range of its target without firing (it says why now and then)
        float mIdle = 0.f; // seconds a character in a fight has neither shot nor moved far (it says where it stands)
        osg::Vec3f mIdleAt; // where it stood when that began
        bool mOrdered = false; // a script told it to shoot (UseWeapon) the last time it was looked at
        bool mPlaceAbandoned = false; // it could not get to the place of the order: it shoots from where it stands
        unsigned mOrderSerial = 0; // which order of the script that is (a new order has another number)
        bool mSeenAlive = false; // it was alive when it was looked at
        bool mFell = false; // it is dead and its body has fallen
        float mReview = 0.f; // seconds until the packages are looked at again
        float mWait = 0.f; // seconds to stand still before it does anything else
        bool mWalking = false; // it goes to mDestination
        bool mFleeing = false; // mDestination is a place to run to from the target of its fight, not the target
        bool mPartial = false; // the path it follows stops short of mDestination: the navigator could not make the rest
        std::string mPathStatus; // why the last path asked for was not one (said when a package cannot be walked)
        bool mAtGoal = false; // a goal that stays is reached
        bool mSpoke = false; // a Dialogue package: it has said what it came to say, and is done when the lines are over
        /// A package that is done and has nothing after it is over for the actor, and its Change script has been told
        bool mChangeTold = false;
        /// SetPackDuration: how long the package it follows may last, in hundredths of a game hour (0 for no limit), and
        /// the game hour (days times 24 plus the hour) at which it began
        double mDurationHundredths = 0;
        double mStartedHours = 0;
        /// The package it follows is done because its duration ran out and not because it did what it came to do: a character
        /// in a piece of furniture does not stay there for it
        bool mExpired = false;
        /// The package was kept in a saved game: its goal is made when the packages are looked at first, without the
        /// character beginning it again
        bool mNeedsGoal = false;
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

        // What the scripts were last told of its package (GetCurrentAIProcedure, IsWaiting, GetPackageTarget): told when
        // it changes, for the reference mReported
        FalloutScript::FormId mReported = 0;
        int mToldType = -2;
        int mToldStage = 0;
        std::uint32_t mToldFlags = 0;
        FalloutScript::FormId mToldTarget = 0;

        // Whether it gets on
        osg::Vec3f mCheckpoint;
        float mCheckpointTime = 0.f;
        float mStuck = 0.f;
        bool mYielded = false;
        FalloutYield mYield; // how long it has waited for the player to move out of its way, and whether it goes on
        float mPathWaiting = 0.f; // how long the navigator has had no mesh to make a path on
        bool mPathWaitingReported = false;

        // A blow threw it back: it goes along the ground by mKnock (units a second), which falls to nothing over
        // mKnockTotal seconds, and falls when the ground ends
        bool mThrown = false;
        osg::Vec3f mKnock;
        float mKnockLeft = 0.f;
        float mKnockTotal = 0.f;
        bool mAirborne = false;
        float mFallSpeed = 0.f; // units a second, down is negative
        float mFallFrom = 0.f; // the height where the fall began
        osg::Vec3f mDrift; // how fast it goes along the ground while it is in the air

        Mind(const OFWorld::Ptr& ptr, std::vector<const ESM4::AIPackage*> packages, std::uint64_t seed)
            : mPtr(ptr)
            , mPackages(std::move(packages))
            , mRandom(seed)
        {
            // Actors that load together do not all look at their packages in the same frame
            mReview = reviewInterval * static_cast<float>(mRandom.below(1000)) / 1000.f;
            mSight = sightInterval * static_cast<float>((seed >> 20) % 1000) / 1000.f;
        }

        osg::Vec3f position() const { return mPtr.getRefData().getPosition().asVec3(); }
    };

    namespace
    {
        /// The number a saved game knows a record or reference by, 0 for one that has none
        FalloutScript::FormId savedNumber(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        /// What a saved game keeps of the mind of a character
        FalloutScript::AiState stateOf(const FalloutActors::Mind& mind)
        {
            FalloutScript::AiState state;
            for (const ESM4::AIPackage* package : mind.mDone)
                if (const FalloutScript::FormId id = savedNumber(package->mId); id != 0)
                    state.mDone.push_back(id);
            std::sort(state.mDone.begin(), state.mDone.end());
            if (mind.mPackage != nullptr)
                state.mPackage = savedNumber(mind.mPackage->mId);
            state.mChangeTold = mind.mChangeTold;
            // The duration that a script gave the package belongs to the package that is followed
            if (mind.mPackage != nullptr && mind.mDurationHundredths > 0)
            {
                state.mDurationHundredths = mind.mDurationHundredths;
                state.mStartedHours = mind.mStartedHours;
            }
            // So does why it is done: a package that ran out does not keep a character in its furniture
            state.mExpired = mind.mPackage != nullptr && mind.mExpired;
            if (mind.mCommanded)
            {
                state.mFollows = true;
                state.mFollowsPlayer = mind.mFollowing.mPlayer;
                state.mFollowTarget = savedNumber(mind.mFollowing.mRef);
                state.mFollowDistance = mind.mFollowing.mDistance;
                // The one it follows has no number a save can name: it follows nobody after a load
                if (!state.mFollowsPlayer && state.mFollowTarget == 0)
                    state.mFollows = false;
            }
            return state;
        }

        /// Gives the saved game what the mind of the character holds now
        void keepState(const FalloutActors::Mind& mind)
        {
            if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                scripts->rememberAi(mind.mPtr, stateOf(mind));
        }

        /// Takes back what a saved game kept of the character, or what it held before its cell was unloaded: the
        /// packages that were done are still done, and the one that it followed is followed without being begun again
        void restoreState(FalloutActors::Mind& mind)
        {
            OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            if (scripts == nullptr)
                return;
            const std::optional<FalloutScript::AiState> saved = scripts->savedAi(mind.mPtr);
            if (!saved)
                return;
            const auto find = [&mind](FalloutScript::FormId id) -> const ESM4::AIPackage* {
                for (const auto* list : { &mind.mScripted, &mind.mPackages })
                    for (const ESM4::AIPackage* package : *list)
                        if (savedNumber(package->mId) == id)
                            return package;
                return nullptr;
            };
            for (const FalloutScript::FormId id : saved->mDone)
                if (const ESM4::AIPackage* package = find(id))
                    mind.mDone.insert(package);
            if (saved->mPackage != 0)
                if (const ESM4::AIPackage* package = find(saved->mPackage))
                {
                    mind.mPackage = package;
                    mind.mNeedsGoal = true;
                    mind.mChangeTold = saved->mChangeTold;
                    // The package goes on with the time it had (a cell that was unloaded, a game that was loaded)
                    mind.mDurationHundredths = saved->mDurationHundredths;
                    mind.mStartedHours = saved->mStartedHours;
                    mind.mExpired = saved->mExpired;
                }
        }

        /// Tells the scripts which package the actor follows and at which stage of it (the type of the package, its flags
        /// and the reference it names as its target), when that is not what they were told last. The stage is chosen,
        /// not read: the first while it goes to the place of the package, the last of the executable's list for that type
        /// of package while it is there, and past the list (the end of the package) when it is done.
        void publishMind(OFScript::FalloutScripts* scripts, FalloutActors::Mind& mind, bool dead)
        {
            if (scripts == nullptr)
                return;
            int type = -1;
            std::uint32_t flags = 0;
            FalloutScript::FormId target = 0;
            int stage = 0;
            if (!dead)
            {
                // The package that a script gave it to shoot with is a package of the type Use Weapon (16)
                if (mind.mOrdered)
                    type = 16;
                else if (mind.mPackage != nullptr)
                {
                    type = mind.mPackage->mData.type;
                    flags = mind.mPackage->mData.flags;
                    // A reference is the target when its kind is 0; an object kind or an object id names no one
                    if (mind.mPackage->mTarget.type == 0)
                        target = savedNumber(ESM::FormId::fromUint32(mind.mPackage->mTarget.target));
                }
                if (type >= 0)
                {
                    const int stages = FalloutScript::procedureStageCount(type);
                    if (mind.mPackage != nullptr && !mind.mOrdered && mind.mDone.count(mind.mPackage) != 0)
                        stage = stages;
                    else if (stages > 1 && mind.mAtGoal && !mind.mWalking)
                        stage = stages - 1;
                }
            }
            if (mind.mReported != 0 && type == mind.mToldType && stage == mind.mToldStage && flags == mind.mToldFlags
                && target == mind.mToldTarget)
                return;
            const FalloutScript::FormId reported
                = scripts->noteActivity(mind.mPtr, [=](FalloutScript::ActorActivity& activity) {
                      activity.mPackageType = type;
                      activity.mStage = stage;
                      activity.mPackageFlags = flags;
                      activity.mPackageTarget = target;
                  });
            mind.mReported = reported;
            mind.mToldType = type;
            mind.mToldStage = stage;
            mind.mToldFlags = flags;
            mind.mToldTarget = target;
        }

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
            /// Every actor that has a mind, for the pieces of furniture that others are on their way to
            const std::map<const OFWorld::LiveCellRefBase*, std::unique_ptr<FalloutActors::Mind>>* mMinds = nullptr;
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

        /// Whether a character needs a mind though it has no package: it goes for the player when it sees it, or
        /// fights already (a blow or a script started a fight with one that never starts one), or it is dead, and the
        /// mind is what makes its body fall
        bool needsMind(const OFWorld::Ptr& ptr)
        {
            OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
            return scripts != nullptr && ptr != OFBase::Environment::get().getWorld()->getPlayerPtr()
                && (scripts->isAggressive(ptr) || scripts->fighting(ptr) || scripts->isDead(ptr)
                    || scripts->hasScriptedAttack(ptr));
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
                case ESM4::PackageBehaviour::Use:
                    return "goes and uses furniture";
                case ESM4::PackageBehaviour::Escort:
                    return "escorts";
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
            mind.mFleeing = false;
            mind.mWaypoints.clear();
            mind.mStuck = 0.f;
            mind.mYielded = false;
            mind.mYield = FalloutYield();
            mind.mPathWaiting = 0.f;
            mind.mPathWaitingReported = false;
        }

        /// The body of an actor that died falls, and what walks is no longer stopped by it
        void fall(Frame& frame, FalloutActors::Mind& mind)
        {
            auto* animation = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr));
            // (round 14A: the bodies of two characters that were shot dead stood upright with their weapons out)
            Log(Debug::Info) << mind.mPtr.getCellRef().getRefId() << " dies: "
                             << (animation != nullptr ? "its body falls" : "it has no body of Fallout to fall")
                             << (mind.mSeenAlive ? "" : ", at once (it was dead when it came into view)");
            if (animation != nullptr)
                animation->die(!mind.mSeenAlive);
            frame.mWorld.setObjectCollision(mind.mPtr, false);
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

    namespace
    {
        /// The height of the floor under a place, looked for from `up` above it to `down` below it
        std::optional<float> floorBelow(OFBase::World& world, const osg::Vec3f& place, float up, float down)
        {
            const osg::Vec3f from(place.x(), place.y(), place.z() + up);
            const float distance = world.getDistToNearestRayHit(from, osg::Vec3f(0.f, 0.f, -1.f), up + down);
            if (distance >= up + down)
                return std::nullopt;
            return from.z() - distance;
        }

        /// A character that a blow threw back moves on by the force of it, and one that comes to the end of the ground
        /// falls and is hurt by the fall (the executables' rule for a fall, as the player's). True while it is thrown or
        /// in the air: it does nothing else then.
        bool thrownStep(const Frame& frame, FalloutActors::Mind& mind, float duration)
        {
            OFBase::World& world = frame.mWorld;
            const osg::Vec3f position = mind.position();
            osg::Vec3f push;
            if (mind.mAirborne)
                push = mind.mDrift * duration;
            else if (mind.mKnockLeft > 0.f)
            {
                // The force goes down to nothing over its time
                const float share = mind.mKnockTotal > 0.f ? mind.mKnockLeft / mind.mKnockTotal : 0.f;
                push = mind.mKnock * (share * duration);
                mind.mKnockLeft = std::max(0.f, mind.mKnockLeft - duration);
            }
            push.z() = 0.f;

            // It is stopped by what is in its way
            const float length = push.length();
            if (length > 1e-3f)
            {
                const osg::Vec3f from(position.x(), position.y(), position.z() + straightKneeHeight);
                const float reach = length + thrownBodyRadius;
                if (world.getDistToNearestRayHit(from, push / length, reach) < reach)
                {
                    push = osg::Vec3f();
                    mind.mKnockLeft = 0.f;
                    mind.mDrift = osg::Vec3f();
                }
            }

            osg::Vec3f place = position + push;
            bool landed = false;
            if (mind.mAirborne)
            {
                mind.mFallSpeed -= Constants::GravityConst * Constants::UnitsPerMeter * duration;
                const float height = position.z() + mind.mFallSpeed * duration;
                const std::optional<float> floor = floorBelow(world, osg::Vec3f(place.x(), place.y(), position.z()),
                    groundProbeUp, fallProbeDown);
                if (floor && height <= *floor)
                {
                    place.z() = *floor;
                    landed = true;
                }
                else if (!floor && mind.mFallFrom - height > fallGiveUp)
                {
                    // No floor at all below it: it is left where it is
                    mind.mAirborne = false;
                    mind.mThrown = false;
                    return false;
                }
                else
                    place.z() = height;
            }
            else
            {
                std::optional<float> floor = floorBelow(world, place, groundProbeUp, groundProbeDown);
                // None within the usual reach: the ground may end in a drop, which it looks for further down
                if (!floor)
                    floor = floorBelow(world, place, groundProbeUp, fallProbeDown);
                if (!floor || *floor > position.z() + thrownStepUp)
                {
                    // Ground it cannot step onto, or none: the push is over
                    place = position;
                    mind.mKnockLeft = 0.f;
                }
                else if (position.z() - *floor > thrownStepDown)
                {
                    // The ground ends: it goes on in the air with the speed it had
                    mind.mAirborne = true;
                    mind.mFallSpeed = 0.f;
                    mind.mFallFrom = position.z();
                    mind.mDrift = length > 1e-3f && duration > 0.f ? push / duration : osg::Vec3f();
                    place.z() = position.z();
                    Log(Debug::Info) << mind.mPtr.getCellRef().getRefId() << " goes off the edge: the ground is "
                                     << static_cast<int>(position.z() - *floor) << " units below";
                }
                else
                    place.z() = *floor;
            }

            if ((place - position).length2() > 1e-6f && isLoaded(frame, mind.mPtr, place))
                world.moveObject(mind.mPtr, place);

            if (landed)
            {
                const float heightLost = mind.mFallFrom - place.z();
                mind.mAirborne = false;
                mind.mFallSpeed = 0.f;
                mind.mDrift = osg::Vec3f();
                mind.mKnockLeft = 0.f;
                if (frame.mScripts != nullptr)
                    frame.mScripts->fell(mind.mPtr, heightLost);
            }
            if (mind.mAirborne || mind.mKnockLeft > 0.f)
                return true;

            // It is on its feet again: the packages and the fight take it up from where it is
            mind.mThrown = false;
            stopWalking(mind);
            mind.mAtGoal = false;
            mind.mWait = 0.f;
            mind.mReview = 0.f;
            return false;
        }

        /// The same, and the body is told whether it is in the air (it plays the animation of a jump, and the landing)
        bool thrown(const Frame& frame, FalloutActors::Mind& mind, float duration)
        {
            const bool result = thrownStep(frame, mind, duration);
            if (auto* animation = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr)))
                animation->setAirborne(mind.mAirborne);
            return result;
        }
    }

    void FalloutActors::getObjectsInRange(
        const osg::Vec3f& position, float radius, std::vector<OFWorld::Ptr>& objects) const
    {
        const float squared = radius * radius;
        for (const auto& entry : mMinds)
        {
            const OFWorld::Ptr& ptr = entry.second->mPtr;
            if (ptr.isEmpty() || !ptr.isInCell())
                continue;
            if ((ptr.getRefData().getPosition().asVec3() - position).length2() <= squared)
                objects.push_back(ptr);
        }
        for (const auto& entry : mUnminded)
        {
            const OFWorld::Ptr& ptr = entry.second;
            if (ptr.isEmpty() || !ptr.isInCell())
                continue;
            if ((ptr.getRefData().getPosition().asVec3() - position).length2() <= squared)
                objects.push_back(ptr);
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
        // A command from a script outlives the cell: the character takes it up again when the cell is loaded, and
        // when the game is loaded, from what the saved game kept of it
        const ESM::RefNum refNum = ptr.getCellRef().getRefNum();
        if (mCommands.count(refNum) == 0)
            if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                if (const std::optional<FalloutScript::AiState> saved = scripts->savedAi(ptr); saved && saved->mFollows)
                {
                    Command command;
                    command.mPlayer = saved->mFollowsPlayer;
                    command.mTarget = ESM::FormId::fromUint32(saved->mFollowTarget);
                    command.mDistance = saved->mFollowDistance;
                    mCommands[refNum] = command;
                }
        const auto command = mCommands.find(refNum);
        if (command == mCommands.end() && packagesOf(ptr).empty() && scriptPackagesOf(ptr).empty() && !needsMind(ptr))
        {
            // No mind, but it is there: whoever looks for those that see a thing finds it
            mUnminded[ptr.mRef] = ptr;
            return;
        }
        Mind& mind = makeMind(ptr);
        if (command != mCommands.end())
            obey(mind, command->second);
    }

    FalloutActors::Mind& FalloutActors::makeMind(const OFWorld::Ptr& ptr)
    {
        const ESM::RefNum refNum = ptr.getCellRef().getRefNum();
        const std::uint64_t seed = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(refNum.mContentFile)) << 32)
            ^ refNum.mIndex ^ 0x9E3779B97F4A7C15ull;
        mUnminded.erase(ptr.mRef);
        Mind& mind = *mMinds.emplace(ptr.mRef, std::make_unique<Mind>(ptr, packagesOf(ptr), seed)).first->second;
        mind.mScripted = scriptPackagesOf(ptr);
        restoreState(mind);
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
            // A character with no packages of its own has no mind until a script gives it one, or it has to fight
            if (scriptPackagesOf(ptr).empty() && !needsMind(ptr))
                return;
            makeMind(ptr).mReview = 0.f;
            return;
        }
        found->second->mScripted = scriptPackagesOf(ptr);
        found->second->mReview = 0.f;
    }

    void FalloutActors::knock(const OFWorld::Ptr& ptr, const osg::Vec3f& velocity, float seconds)
    {
        if (!handles(ptr) || seconds <= 0.f || velocity.length2() < 1e-6f)
            return;
        const auto found = mMinds.find(ptr.mRef);
        Mind& mind = found == mMinds.end() ? makeMind(ptr) : *found->second;
        mind.mThrown = true;
        osg::Vec3f push = velocity;
        push.z() = 0.f;
        if (mind.mAirborne)
        {
            // Already in the air, where it goes by what it had: the blow adds to that (a throw's force is spent over its time,
            // and the share of it that is the average speed is what the body takes on)
            mind.mDrift += push * 0.5f;
            mind.mKnockLeft = 0.f;
        }
        else
        {
            mind.mKnock = push;
            mind.mKnockTotal = seconds;
            mind.mKnockLeft = seconds;
        }
        stopWalking(mind);
        Log(Debug::Info) << ptr.getCellRef().getRefId() << " is thrown back by a blow: " << static_cast<int>(velocity.length())
                         << " units a second for " << seconds << " s";
    }

    void FalloutActors::setPackageDuration(const OFWorld::Ptr& ptr, double hundredths)
    {
        const auto found = mMinds.find(ptr.mRef);
        if (found == mMinds.end() || found->second->mPackage == nullptr)
            return;
        found->second->mDurationHundredths = hundredths;
        // (the package begins again from now, as the duration counts from when the call is made; one that had run out
        // runs again, and runs out again at the end of the new duration)
        found->second->mStartedHours = gameHours(OFBase::Environment::get().getWorld()->getTimeStamp());
        if (found->second->mExpired)
        {
            // (it is not done any more: a Dialogue package says its line again, and runs out again with the new duration,
            // or with none when the duration is 0)
            found->second->mExpired = false;
            found->second->mDone.erase(found->second->mPackage);
        }
        keepState(*found->second);
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
        keepState(mind);
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
        {
            // A character whose cell is not loaded is told to stop in the saved game
            if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                if (std::optional<FalloutScript::AiState> saved = scripts->savedAi(ptr); saved && saved->mFollows)
                {
                    saved->mFollows = false;
                    scripts->rememberAi(ptr, *saved);
                }
            return;
        }
        Mind& mind = *found->second;
        mind.mCommanded = false;
        mind.mFollowing = Mind::Following();
        mind.mPackage = nullptr;
        mind.mGoal = FalloutGoal();
        mind.mReview = 0.f;
        stopWalking(mind);
        keepState(mind);
    }

    void FalloutActors::remove(const OFWorld::Ptr& ptr)
    {
        mUnminded.erase(ptr.mRef);
        const auto found = mMinds.find(ptr.mRef);
        if (found == mMinds.end())
            return;
        releaseAgent(*found->second);
        // The scripts are not told any more of what the actor does
        if (found->second->mReported != 0)
            if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                scripts->forgetMind(found->second->mReported);
        mMinds.erase(found);
    }

    void FalloutActors::updatePtr(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr)
    {
        if (mUnminded.erase(old.mRef) > 0)
            mUnminded[ptr.mRef] = ptr;
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
        // The actors without a mind of an unloaded cell are forgotten too, or the scans would go through every
        // character that was ever near
        for (auto it = mUnminded.begin(); it != mUnminded.end();)
        {
            if (it->second.getCell() == cell)
                it = mUnminded.erase(it);
            else
                ++it;
        }
        for (auto it = mMinds.begin(); it != mMinds.end();)
        {
            if (it->second->mPtr.getCell() == cell)
            {
                releaseAgent(*it->second);
                if (it->second->mReported != 0)
                    if (OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts())
                        scripts->forgetMind(it->second->mReported);
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
        mUnminded.clear();
        mCommands.clear();
    }

    namespace
    {
        /// How many characters other than `mind` are on their way to use the furniture
        int claimsOn(const Frame& frame, const FalloutActors::Mind& mind, const ESM::RefNum& furniture)
        {
            int claims = 0;
            if (frame.mMinds != nullptr)
                for (const auto& [key, other] : *frame.mMinds)
                    if (other.get() != &mind && other->mPackage != nullptr && !other->mAtGoal && !other->mExpired
                        && other->mGoal.mBehaviour == ESM4::PackageBehaviour::Use && other->mGoal.mTarget == furniture)
                        ++claims;
            return claims;
        }

        /// Tells once that a package is left out, as the packages that run scripts are the ones a quest waits for
        bool leftOut(const FalloutActors::Mind& mind, const ESM4::AIPackage& package, const std::string& why)
        {
            if (mind.mSkipTold.insert(&package).second)
                Log(hasEvents(package) ? Debug::Info : Debug::Verbose)
                    << "Package " << package.mEditorId << " of " << mind.mPtr.getCellRef().getRefId() << ": left out, "
                    << why;
            return false;
        }

        /// The furniture a Find or Use Item At package takes, and where the character goes to use it: the one the
        /// package names, or the nearest in the cell that has room and is within the radius of the place of the
        /// package. A piece that a character is on its way to has one place less. False when there is none (the
        /// package is not on then), or the package names something that is not furniture.
        bool findFurniture(const Frame& frame, const FalloutActors::Mind& mind, const ESM4::AIPackage& package,
            FalloutGoal& goal, bool& hasFacing, float& facing)
        {
            if (frame.mScripts == nullptr)
                return false;
            const int kind = package.mTarget.type;
            if (kind != 0 && kind != 1 && kind != 2)
                return leftOut(mind, package, "it names nothing to take");
            if (kind == 2 && package.mTarget.target != static_cast<std::uint32_t>(falloutFurnitureObjectType))
                return leftOut(mind, package,
                    "it takes an object of type " + std::to_string(package.mTarget.target)
                        + ", of which only furniture (11) is done");

            // Where it looks
            osg::Vec3f center = mind.mPtr.getCellRef().getPosition().asVec3();
            if (package.mLocation.type == ESM4::Location_NearCurrent)
                center = mind.position();
            else if (package.mLocation.type == ESM4::Location_NearReference)
            {
                const OFWorld::Ptr place = OFBase::Environment::get().getWorldModel()->getPtr(
                    ESM::RefNum(ESM::FormId::fromUint32(package.mLocation.location)));
                if (place.isEmpty() || !place.getRefData().isEnabled() || !sharesSpace(mind.mPtr, place))
                    return false;
                center = place.getRefData().getPosition().asVec3();
            }
            const float radius
                = package.mLocation.radius > 0 ? static_cast<float>(package.mLocation.radius) : falloutDefaultSearch;

            std::vector<OFWorld::Ptr> furniture;
            if (kind == 0)
            {
                const OFWorld::Ptr named = OFBase::Environment::get().getWorldModel()->getPtr(
                    ESM::RefNum(ESM::FormId::fromUint32(package.mTarget.target)));
                if (named.isEmpty() || !named.getRefData().isEnabled() || !sharesSpace(mind.mPtr, named))
                    return false;
                if (named.getType() != ESM4::Furniture::sRecordId)
                    return leftOut(mind, package, "it uses something that is not furniture");
                furniture.push_back(named);
            }
            else
            {
                OFWorld::CellStore* cell = mind.mPtr.getCell();
                if (cell == nullptr)
                    return false;
                const ESM::FormId wanted = ESM::FormId::fromUint32(package.mTarget.target);
                cell->forEachType<ESM4::Furniture>([&](const OFWorld::Ptr& ptr) {
                    if (!ptr.getRefData().isEnabled() || ptr.getCellRef().getCount() <= 0)
                        return true;
                    if (kind == 1 && ptr.get<ESM4::Furniture>()->mBase->mId != wanted)
                        return true;
                    // A piece out of reach, or on another floor, is no candidate
                    if (!isNear(ptr.getRefData().getPosition().asVec3(), center, radius))
                        return true;
                    furniture.push_back(ptr);
                    return true;
                });
            }

            std::vector<FalloutCandidate> candidates;
            candidates.reserve(furniture.size());
            for (const OFWorld::Ptr& piece : furniture)
            {
                FalloutCandidate candidate;
                candidate.mOrigin = piece.getRefData().getPosition().asVec3();
                candidate.mPlace = candidate.mOrigin;
                float heading = 0.f;
                frame.mScripts->furnitureSeat(mind.mPtr, piece, candidate.mPlace, heading);
                candidate.mFree = frame.mScripts->freeFurniturePlaces(piece)
                    + (frame.mScripts->usesFurniture(mind.mPtr, piece) ? 1 : 0)
                    - claimsOn(frame, mind, piece.getCellRef().getRefNum());
                candidates.push_back(candidate);
            }

            int chosen = -1;
            if (mind.mPackage == &package)
            {
                // The one it is on its way to, or sits in, stays its choice for as long as it can have it
                for (std::size_t i = 0; i < furniture.size(); ++i)
                    if (candidates[i].mFree > 0 && furniture[i].getCellRef().getRefNum() == mind.mGoal.mTarget)
                        chosen = static_cast<int>(i);
            }
            // The package names the piece: it goes to it whatever the radius
            if (chosen < 0)
                chosen = falloutChooseCandidate(candidates, center, kind == 0 ? 0.f : radius, mind.position());
            if (chosen < 0)
                return false;

            goal.mCenter = candidates[chosen].mPlace;
            goal.mTarget = furniture[chosen].getCellRef().getRefNum();
            hasFacing = false;
            facing = 0.f;
            return true;
        }

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

            if (goal.mBehaviour == ESM4::PackageBehaviour::Use)
                return findFurniture(frame, mind, package, goal, hasFacing, facing);

            if (goal.mBehaviour == ESM4::PackageBehaviour::Escort && package.mTarget.type == 0)
            {
                // The one it escorts has to be around, or the package is not on. A package that names no one to
                // escort (an object of a kind) goes to its place alone.
                const ESM::RefNum target = ESM::FormId::fromUint32(package.mTarget.target);
                following.mPlayer
                    = falloutIsPlayerReference(target, falloutFirstPlugin(frame.mWorld.getContentFiles()));
                following.mRef = target;
                following.mDistance = falloutEscortWait(package.mTarget.distance);
                osg::Vec3f position;
                if (!followPosition(frame, mind.mPtr, following, position))
                {
                    if (mind.mSkipTold.insert(&package).second)
                        Log(hasEvents(package) ? Debug::Info : Debug::Verbose)
                            << "Package " << package.mEditorId << " of " << mind.mPtr.getCellRef().getRefId()
                            << ": left out, the one it escorts (" << target << ") is not around";
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
                    if (!reference.isEmpty() && !reference.getRefData().isEnabled())
                        return false;
                    // Only where it can go from where it is: a place in another space (a building and the world outside
                    // it) is gone to through the door that leads there, as the characters of the games leave a building
                    // by its door (the place may be in a cell that is not loaded). Only a package that goes to the
                    // place and stays there does so: the others (going about the place, using furniture there) go on
                    // as they did, which is to be left out.
                    if (reference.isEmpty() || !sharesSpace(mind.mPtr, reference))
                    {
                        osg::Vec3f door;
                        if (goal.mBehaviour != ESM4::PackageBehaviour::Stay || frame.mScripts == nullptr
                            || !frame.mScripts->doorTowards(mind.mPtr, target.toUint32(), door))
                            return false;
                        goal.mCenter = door;
                        goal.mThrough = target;
                        goal.mRadius = doorArrival;
                        return true;
                    }
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

        /// Whether the actor can walk the straight line to the place on the floor that the world has under it (see
        /// falloutStraightWalkable): used where the navigator has no path, because its mesh has holes and islands
        /// (round 10P: a vault of Fallout 3) or the target stands off it (the pistol raider in round 10O)
        bool walksStraight(OFBase::World& world, const osg::Vec3f& from, const osg::Vec3f& to)
        {
            const auto floor = [&world](float x, float y, float last) -> std::optional<float> {
                const osg::Vec3f above(x, y, last + groundProbeUp);
                const float reach = groundProbeUp + groundProbeDown;
                const float distance = world.getDistToNearestRayHit(above, osg::Vec3f(0.f, 0.f, -1.f), reach);
                if (distance >= reach)
                    return std::nullopt;
                return above.z() - distance;
            };
            // The knees and the chest of the walker
            const auto blocked = [&world](const osg::Vec3f& a, const osg::Vec3f& b) {
                return falloutSegmentBlocked(
                    a, b,
                    [&world](const osg::Vec3f& start, const osg::Vec3f& direction, float longest) {
                        return world.getDistToNearestRayHit(start, direction, longest);
                    },
                    { straightKneeHeight, straightChestHeight });
            };
            return falloutStraightWalkable(from, to, floor, blocked);
        }

        /// The actor goes to the place in a straight line, which the world lets it walk
        Walk straightWalk(FalloutActors::Mind& mind, const osg::Vec3f& destination, const char* why)
        {
            static int said = 0;
            if (said < 40)
            {
                ++said;
                Log(Debug::Info) << mind.mPtr.getCellRef().getRefId() << " walks straight to " << destination.x() << ","
                                 << destination.y() << "," << destination.z() << " (" << why
                                 << "): the floor is there and nothing is in the way";
            }
            mind.mWaypoints.clear();
            mind.mWaypoints.push_back(destination);
            mind.mPartial = false;
            mind.mPathStatus.clear();
            return Walk::Moving;
        }

        /// Asks the navigator for the path from where the actor is to the place. Success when it has waypoints.
        Walk buildPath(const Frame& frame, FalloutActors::Mind& mind, const osg::Vec3f& destination)
        {
            mind.mWaypoints.clear();
            mind.mPartial = false;
            mind.mPathStatus.clear();
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
                    {
                        // The mesh ends where the nearest of its places to the target is: when the target stands off it
                        // (up to the tolerance), the character goes on in a straight line from there
                        const osg::Vec3f last = mind.mWaypoints.back();
                        if (distanceAcross(last, destination) > waypointDistance
                            && walksStraight(frame.mWorld, last, destination))
                            mind.mWaypoints.push_back(destination);
                        return Walk::Moving;
                    }
                    mind.mPathStatus = "the path has no waypoints";
                    return Walk::Failed;
                case DetourNavigator::Status::PartialPath:
                    // The place is on another island of the mesh, or in a part of it that is not made yet: the path
                    // goes as far as the navigator can, and a path that goes nowhere from here is no path
                    mind.mPartial = true;
                    if (mind.mWaypoints.empty() || (start - mind.mWaypoints.back()).length() < waypointDistance)
                    {
                        mind.mWaypoints.clear();
                        if (walksStraight(frame.mWorld, start, destination))
                            return straightWalk(mind, destination, "the path goes nowhere from here");
                        mind.mPathStatus
                            = "the path goes nowhere from here (another island of the mesh, or a part of "
                              "it that is not made)";
                        return Walk::Failed;
                    }
                    return Walk::Moving;
                case DetourNavigator::Status::NavMeshNotFound:
                    mind.mWaypoints.clear();
                    mind.mPathStatus = "the navigator has no mesh for the cell yet";
                    return Walk::Waiting;
                default:
                    mind.mPathStatus = DetourNavigator::getMessage(status);
                    if (walksStraight(frame.mWorld, start, destination))
                        return straightWalk(mind, destination, DetourNavigator::getMessage(status));
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
            // A script may force it to run (SetForceRun), or to sneak, which is slower than a run (the speed is then
            // scaled in moveSpeedMultiplier)
            if (frame.mScripts != nullptr)
                run = (run || frame.mScripts->actorForcesRun(mind.mPtr)) && !frame.mScripts->actorForcesSneak(mind.mPtr);
            float speed = run && animation->canRun() ? runSpeed(*animation) : walkSpeed(*animation);
            // A crippled leg slows the walk (fMoveOneCrippledLegSpeedMult, fMoveTwoCrippledLegsSpeedMult)
            if (frame.mScripts != nullptr)
                speed *= static_cast<float>(frame.mScripts->moveSpeedMultiplier(mind.mPtr));
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
            // SetNoAvoidance: nothing steps aside for the player
            const bool avoids = frame.mScripts == nullptr || !frame.mScripts->actorsAvoidanceOff();
            if (avoids && falloutYields(mind.mYield, nearPlayer, inTheWay, duration, yieldPatience))
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
            // SetPackDuration: a package that has run for its duration is done (FalloutNV.exe 0x8F4020), as one that has
            // got to its goal is. A package that is done already (a character that got to the furniture of a Find or Use
            // Item At package) still runs out, which lets the character leave the furniture; it is not told as done twice.
            if (mind.mPackage != nullptr && !mind.mExpired && !mind.mOrdered
                && FalloutScript::World::packageTimeIsUp(
                    mind.mDurationHundredths, gameHours(frame.mWorld.getTimeStamp()), mind.mStartedHours))
            {
                const bool wasDone = !mind.mDone.insert(mind.mPackage).second;
                mind.mDurationHundredths = 0;
                mind.mExpired = true;
                keepState(mind);
                if (!wasDone)
                    raise(frame, mind.mPtr, *mind.mPackage, OFScript::FalloutScripts::PackageEvent::Done);
            }
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
            const bool seated = current.mPackage != nullptr && !mind.mExpired && frame.mScripts != nullptr
                && frame.mScripts->usesFurniture(mind.mPtr);
            const Choice& chosen = seated     ? current
                : fresh.mPackage != nullptr   ? fresh
                : current.mPackage != nullptr ? current
                                              : done;

            if (chosen.mPackage == mind.mPackage)
            {
                // The package that a saved game kept goes on from where the character is, without a new beginning
                if (mind.mNeedsGoal && chosen.mPackage != nullptr)
                {
                    mind.mGoal = chosen.mGoal;
                    mind.mFollowing = chosen.mFollowing;
                    mind.mHasFacing = chosen.mHasFacing;
                    mind.mFacing = chosen.mFacing;
                }
                mind.mNeedsGoal = false;
                // A place that the package names by a reference goes with it when something moves the reference, and
                // a package that looks for furniture follows its choice
                if (chosen.mPackage != nullptr
                    && (chosen.mPackage->mLocation.type == ESM4::Location_NearReference
                        || chosen.mGoal.mBehaviour == ESM4::PackageBehaviour::Use))
                {
                    mind.mHasFacing = chosen.mHasFacing;
                    mind.mFacing = chosen.mFacing;
                    if (!(chosen.mGoal.mTarget == mind.mGoal.mTarget) || !(chosen.mGoal.mThrough == mind.mGoal.mThrough)
                        || (chosen.mGoal.mCenter - mind.mGoal.mCenter).length() > waypointDistance)
                    {
                        mind.mGoal.mTarget = chosen.mGoal.mTarget;
                        mind.mGoal.mThrough = chosen.mGoal.mThrough;
                        mind.mGoal.mCenter = chosen.mGoal.mCenter;
                        mind.mGoal.mRadius = chosen.mGoal.mRadius;
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
                    keepState(mind);
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
            // (a duration that a script set belongs to the package that was followed)
            mind.mDurationHundredths = 0;
            mind.mExpired = false;
            mind.mStartedHours = gameHours(frame.mWorld.getTimeStamp());
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
            mind.mNeedsGoal = false;
            stopWalking(mind);
            keepState(mind);
            // The packages that run scripts are the ones a quest waits for: they are told even if the log is not
            Log(chosen.mPackage != nullptr && hasEvents(*chosen.mPackage) ? Debug::Info : Debug::Verbose)
                << "Package of " << mind.mPtr.getCellRef().getRefId() << " at " << frame.mClock.mHour
                << " o'clock: " << (chosen.mPackage != nullptr ? chosen.mPackage->mEditorId : std::string("none"))
                << " ("
                << behaviourName(chosen.mPackage != nullptr ? ESM4::packageBehaviour(chosen.mPackage->mData.type)
                                                            : ESM4::PackageBehaviour::None)
                << ")";
        }

        /// A character has got to the place of its Travel, Find, Use Item At or Escort package: the package is done,
        /// once. One that was done already (its cell was unloaded and loaded again, so it stands where it sat) takes
        /// the furniture again, and nobody is told.
        void finishTravel(const Frame& frame, FalloutActors::Mind& mind)
        {
            const ESM4::AIPackage* package = mind.mPackage;
            if (package == nullptr)
                return;
            const bool done = mind.mDone.count(package) != 0;
            // A character that has got to the door that leads to its place goes through it: it is at the place, in the
            // cell of it, the next frame
            if (!mind.mGoal.mThrough.isZeroOrUnset() && !done && frame.mScripts != nullptr)
                frame.mScripts->goThrough(mind.mPtr, mind.mGoal.mThrough.toUint32());
            const bool uses = mind.mGoal.mBehaviour == ESM4::PackageBehaviour::Use;
            // Eating and sleeping are done on furniture: they are never done as such, the character stays on it
            const bool beds = package->mData.type == ESM4::Package_Eat || package->mData.type == ESM4::Package_Sleep;
            if (package->mData.type != ESM4::Package_Travel && !uses && !beds
                && mind.mGoal.mBehaviour != ESM4::PackageBehaviour::Escort)
                return;
            // A character that has got to a piece of furniture uses it, and the script of the package is told after
            // that, as the games tell it when the character is in place
            if (uses && frame.mScripts != nullptr)
            {
                const OFWorld::Ptr place = OFBase::Environment::get().getWorldModel()->getPtr(mind.mGoal.mTarget);
                if (place.isEmpty() || !frame.mScripts->useFurniture(mind.mPtr, place, true))
                {
                    // Someone else got there first: it looks for another piece, soon
                    mind.mAtGoal = false;
                    mind.mGoal.mTarget = ESM::RefNum();
                    mind.mWait = pathRetryTime;
                    mind.mReview = 0.f;
                    return;
                }
            }
            else if (package->mLocation.type == ESM4::Location_NearReference && frame.mScripts != nullptr)
            {
                const OFWorld::Ptr place = OFBase::Environment::get().getWorldModel()->getPtr(
                    ESM::RefNum(ESM::FormId::fromUint32(package->mLocation.location)));
                if (!place.isEmpty() && place.getType() == ESM4::Furniture::sRecordId
                    && !frame.mScripts->usesFurniture(mind.mPtr, place)
                    && !frame.mScripts->useFurniture(mind.mPtr, place, true))
                {
                    // Every place of it is taken: the package is not done, and the character tries again later
                    mind.mAtGoal = false;
                    mind.mWait = giveUpTime;
                    return;
                }
            }
            if (beds || done)
                return;
            mind.mDone.insert(package);
            keepState(mind);
            raise(frame, mind.mPtr, *package, OFScript::FalloutScripts::PackageEvent::Done);
            // The packages are looked at again at once: the next one is followed
            mind.mReview = 0.f;
        }
    }

    namespace
    {
        /// Whether a package that cannot be walked is done by putting the character at its place: those that a quest
        /// waits for the character to be at
        bool jumpsToPlace(const ESM4::AIPackage& package)
        {
            switch (ESM4::packageBehaviour(package.mData.type))
            {
                case ESM4::PackageBehaviour::Use:
                case ESM4::PackageBehaviour::Escort:
                    return true;
                case ESM4::PackageBehaviour::Talk:
                    return package.mLocation.type == ESM4::Location_NearReference;
                default:
                    return package.mData.type == ESM4::Package_Travel;
            }
        }

        /// A character that has a package that tells scripts what happens to it cannot get to its place: that is
        /// logged, and a Travel package that it has failed twice is done by putting the character at the place, since a
        /// quest waits for it (the way may be one that the navigator does not know, or the character cannot walk)
        void failToGetThere(const Frame& frame, FalloutActors::Mind& mind)
        {
            const ESM4::AIPackage& package = *mind.mPackage;
            ++mind.mFailures;
            // The way to a door is the one that is not known: the character goes through it as well
            const bool jump = mind.mFailures >= failuresBeforeJump
                && (jumpsToPlace(package) || !mind.mGoal.mThrough.isZeroOrUnset());
            Log(Debug::Info) << "Package " << package.mEditorId << " of " << mind.mPtr.getCellRef().getRefId()
                             << ": no way to get there ("
                             << (walker(frame.mWorld, mind.mPtr) == nullptr ? "it cannot walk"
                                                                            : "the navigator has no path")
                             << (mind.mPathStatus.empty() ? "" : ": " + mind.mPathStatus) << "; from "
                             << static_cast<int>(mind.position().x()) << "," << static_cast<int>(mind.position().y())
                             << "," << static_cast<int>(mind.position().z()) << " to "
                             << static_cast<int>(mind.mGoal.mCenter.x()) << ","
                             << static_cast<int>(mind.mGoal.mCenter.y()) << ","
                             << static_cast<int>(mind.mGoal.mCenter.z()) << ")" << (jump ? ", put there" : "");
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
                    // A character that cannot walk to a piece of furniture is put on it whatever the package tells
                    // scripts: they read whether it sits
                    if (mind.mPackage != nullptr
                        && (hasEvents(*mind.mPackage) || mind.mGoal.mBehaviour == ESM4::PackageBehaviour::Use
                            || !mind.mGoal.mThrough.isZeroOrUnset()))
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
                keepState(mind);
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

        /// Whether nothing of the world is between the eyes of one character and those of the other
        bool canSee(OFBase::World& world, const osg::Vec3f& from, const osg::Vec3f& to)
        {
            const osg::Vec3f eyes(from.x(), from.y(), from.z() + eyeHeight);
            const osg::Vec3f goal(to.x(), to.y(), to.z() + eyeHeight);
            const float apart = (goal - eyes).length();
            return apart < 1.f || world.getDistToNearestRayHit(eyes, goal - eyes, apart) >= apart - 1.f;
        }

        /// The animations and the model of the weapon the character holds are the ones it is drawn with
        void showWeapon(const Frame& frame, FalloutActors::Mind& mind, OFScript::FalloutScripts& scripts)
        {
            const OFScript::FalloutScripts::HeldWeapon held = scripts.heldWeapon(mind.mPtr);
            if (mind.mDrawn == held.mWeapon && mind.mDrawn != 0)
                return;
            auto* animation = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr));
            if (animation == nullptr)
                return;
            // Bare hands are held ready once, the form 0 being no weapon: the fight is the mark. A weapon that was
            // drawn (a gun that is used up) is put away by the hands that take its place.
            if (held.mWeapon == 0 && mind.mDrawn == 0 && animation->isWeaponDrawn())
                return;
            mind.mDrawn = held.mWeapon;
            animation->drawWeapon(held.mModel, held.mAnimationType, held.mWeapon);
        }

        /// A character that fights goes for its target, attacks it from where its weapon reaches, or runs away from it.
        /// A character at peace that goes for the player on sight starts a fight. True when the fight takes the
        /// character's frame, so that the packages wait.
        bool fight(const Frame& frame, FalloutActors::Mind& mind, float duration)
        {
            OFScript::FalloutScripts& scripts = *frame.mScripts;
            const OFWorld::Ptr player = frame.mWorld.getPlayerPtr();
            if (mind.mPtr == player)
                return false;
            // What the character makes of the player is kept for the sneak meter
            if ((mind.mWatch -= duration) <= 0.f)
            {
                mind.mWatch = static_cast<float>(scripts.detectionInterval());
                scripts.watchPlayer(mind.mPtr);
            }
            // The packages go on where they were left once the fight is over: the character goes back to its place
            const auto over = [&mind, &frame] {
                if (mind.mDrawn != 0 || mind.mFighting)
                    if (auto* animation
                        = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr)))
                        animation->holsterWeapon();
                mind.mDrawn = 0;
                mind.mShowAfter = 0.f;
                mind.mFighting = false;
                stopWalking(mind);
                mind.mAtGoal = false;
                mind.mWait = 0.f;
                mind.mReview = 0.f;
            };

            // A script told it to shoot (UseWeapon): that takes the place of a fight and of the packages
            OFScript::FalloutScripts::ScriptedAttack order = scripts.scriptedAttack(mind.mPtr);
            // A target that is in another interior, or in a cell that is not around, is where the shooter can not go
            // to and its shots can not land: the order is over, as a fight is
            if (order.mActive && !sharesSpace(mind.mPtr, order.mTarget))
            {
                scripts.endScriptedAttack(mind.mPtr, "its target is in another space");
                order = OFScript::FalloutScripts::ScriptedAttack();
            }
            if (order.mActive)
            {
                // A second order replaces the first one, and gets the walk to its place from the start
                if (!mind.mOrdered || mind.mOrderSerial != order.mSerial)
                {
                    mind.mOrdered = true;
                    mind.mOrderSerial = order.mSerial;
                    mind.mPlaceAbandoned = false;
                    mind.mFighting = true;
                    stopWalking(mind);
                    mind.mAtGoal = false;
                    showWeapon(frame, mind, scripts);
                }
                // It shoots from the place it was told to, which it goes to first
                if (!mind.mPlaceAbandoned && !order.mPlace.isEmpty()
                    && !isNear(mind.position(), order.mPlace.getRefData().getPosition().asVec3(), scriptedPlaceReach))
                {
                    const osg::Vec3f place = order.mPlace.getRefData().getPosition().asVec3();
                    if (!mind.mWalking || distanceAcross(place, mind.mDestination) > fightRepathDistance)
                    {
                        mind.mWaypoints.clear();
                        mind.mDestination = place;
                        mind.mFleeing = false;
                    }
                    switch (walkTo(frame, mind, mind.mDestination, duration, scriptedPlaceReach, true))
                    {
                        case Walk::Moving:
                        case Walk::Waiting:
                            return true;
                        default:
                            // It is there, or it cannot get there: it shoots from where it stands, and does not try
                            // the walk again for this order (a failed walk waits a second before the next try)
                            stopWalking(mind);
                            mind.mPlaceAbandoned = true;
                            break;
                    }
                }
                // Far from the player it does nothing, and keeps the order (fAIUseWeaponDistance: 5000 in the
                // executable, 15000 in both master files; FalloutNV.exe 0x8f7776)
                if (distanceAcross(mind.position(), frame.mPlayer)
                    > scripts.gameSetting("fAIUseWeaponDistance").value_or(5000.0))
                    return true;
                // It turns toward the place the order names when there is one, and the shot is still at the target
                const osg::Vec3f victim = order.mTarget.getRefData().getPosition().asVec3();
                const osg::Vec3f aim = order.mAim.isEmpty() ? victim : order.mAim.getRefData().getPosition().asVec3();
                faceToward(
                    frame, mind, std::atan2(aim.x() - mind.position().x(), aim.y() - mind.position().y()), duration);
                // With HoldFire it does not shoot while another character stands in the way, nearer than the target
                // and within 30 degrees of the line to it (FalloutNV.exe 0x8f8966; what the list of the executable
                // holds is not known, the characters of the cell stand for it). The line and the distance are those
                // of the victim, because the shot lands on the victim whatever place the character turns toward
                if (order.mOrder.mHoldFire && mind.mPtr.getCell() != nullptr)
                {
                    // Characters that have no mind stand in the way as well: they are all the characters of the cell
                    std::vector<FalloutScript::FightAI::FloorPoint> others;
                    mind.mPtr.getCell()->forEach([&](const OFWorld::Ptr& other) {
                        if ((other.getType() == ESM4::Npc::sRecordId || other.getType() == ESM4::Creature::sRecordId)
                            && other != mind.mPtr && other != order.mTarget && other.getRefData().isEnabled()
                            && !scripts.isDead(other))
                        {
                            const osg::Vec3f at = other.getRefData().getPosition().asVec3();
                            others.push_back({ at.x(), at.y() });
                        }
                        return true;
                    });
                    if (FalloutScript::FightAI::lineOfFireBlocked(
                            { mind.position().x(), mind.position().y() }, { victim.x(), victim.y() }, others))
                    {
                        if ((mind.mHeld += duration) >= holdFireReportAfter)
                        {
                            mind.mHeld = 0.f;
                            Log(Debug::Info) << mind.mPtr.getCellRef().getRefId()
                                             << " holds fire: another character is in the way of its shot";
                        }
                        return true;
                    }
                }
                if (scripts.scriptedShot(mind.mPtr, order.mTarget).mFired)
                {
                    mind.mHeld = 0.f;
                    if (auto* animation
                        = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr)))
                        animation->attack();
                }
                return true;
            }
            if (mind.mOrdered)
            {
                // The attacks are made, or the target is gone: the character goes back to what it was doing
                mind.mOrdered = false;
                over();
            }

            OFScript::FalloutScripts::FightStance stance = scripts.fightStance(mind.mPtr);
            if (stance.mTarget.isEmpty())
            {
                if (mind.mFighting)
                    over();
                // A character that is at peace looks for the player now and then
                mind.mSight -= duration;
                if (mind.mSight > 0.f)
                    return false;
                mind.mSight = static_cast<float>(scripts.detectionInterval());
                float radius = 0.f;
                if (!sharesSpace(mind.mPtr, player) || !scripts.attacksPlayerOnSight(mind.mPtr, radius))
                    return false;
                if (radius > 0.f)
                {
                    // A character with a radius in its AI data goes for the player once it is within it and in sight
                    if (distanceByFloor(mind.position(), frame.mPlayer) > radius
                        || !canSee(frame.mWorld, mind.position(), frame.mPlayer))
                        return false;
                }
                else if (scripts.sightOfPlayer(mind.mPtr) != FalloutScript::Detection::Sighting::Seen)
                    return false;
                Log(Debug::Info) << mind.mPtr.getCellRef().getRefId() << " sees the player and goes for it";
                scripts.fightPlayer(mind.mPtr);
                stance = scripts.fightStance(mind.mPtr);
                if (stance.mTarget.isEmpty())
                    return false;
            }
            if (!sharesSpace(mind.mPtr, stance.mTarget))
            {
                // The target is somewhere it can not go to
                Log(Debug::Info) << mind.mPtr.getCellRef().getRefId()
                                 << " ends its fight: its target is in another space";
                scripts.endFight(mind.mPtr);
                over();
                return false;
            }
            if (!mind.mFighting)
            {
                mind.mFighting = true;
                stopWalking(mind);
                scripts.prepareFight(mind.mPtr, true);
                showWeapon(frame, mind, scripts);
            }

            const osg::Vec3f target = stance.mTarget.getRefData().getPosition().asVec3();
            const float away = distanceByFloor(mind.position(), target);
            const bool visible = canSee(frame.mWorld, mind.position(), target);
            const float arrival = visible ? static_cast<float>(stance.mEngage) * engageShare : 0.f;
            const float heading = std::atan2(target.x() - mind.position().x(), target.y() - mind.position().y());
            const FalloutScript::FightAI::Move move
                = FalloutScript::FightAI::decide(away, stance.mEngage, visible, stance.mFlee);
            std::optional<Walk> walked;
            bool shotMade = false;
            switch (move)
            {
                case FalloutScript::FightAI::Move::Approach:
                {
                    if (!mind.mWalking || mind.mFleeing
                        || distanceAcross(target, mind.mDestination) > fightRepathDistance)
                    {
                        mind.mWaypoints.clear();
                        mind.mDestination = target;
                        mind.mFleeing = false;
                    }
                    // A character that cannot get to its target, or see it from where the path ends, stands there
                    // with its weapon drawn (the pistol raider of Fallout 3 in rounds 10L and 10O): the line at the end
                    // of the fight says so
                    walked = walkTo(frame, mind, mind.mDestination, duration, arrival, true);
                    if (walked == Walk::Arrived || walked == Walk::Failed)
                        stopWalking(mind);
                    break;
                }
                case FalloutScript::FightAI::Move::Retreat:
                {
                    // A character that was going for the target and loses its nerve turns away at once, not when the
                    // walk to the target is over
                    if (!mind.mWalking || !mind.mFleeing)
                    {
                        mind.mWaypoints.clear();
                        mind.mFleeing = true;
                        osg::Vec3f direction = mind.position() - target;
                        direction.z() = 0.f;
                        if (direction.length2() < 1.f)
                            direction = osg::Vec3f(-std::sin(heading), -std::cos(heading), 0.f);
                        direction.normalize();
                        mind.mDestination = mind.position() + direction * fleeStep;
                    }
                    switch (walkTo(frame, mind, mind.mDestination, duration, 0.f, true))
                    {
                        case Walk::Arrived:
                        case Walk::Failed:
                            stopWalking(mind);
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case FalloutScript::FightAI::Move::Hold:
                    if (mind.mWalking)
                        stopWalking(mind);
                    if (stance.mFlee)
                    {
                        // It is far enough away: the fight is over for it
                        Log(Debug::Info) << mind.mPtr.getCellRef().getRefId()
                                         << " ends its fight: it lost its nerve and is far enough away ("
                                         << static_cast<int>(away) << " units)";
                        scripts.endFight(mind.mPtr);
                        over();
                        return false;
                    }
                    faceToward(frame, mind, heading, duration);
                    if (mind.mShowAfter > 0.f)
                    {
                        mind.mShowAfter -= duration;
                        if (mind.mShowAfter <= 0.f)
                            showWeapon(frame, mind, scripts);
                    }
                    const OFScript::FalloutScripts::FireResult shot = scripts.attack(mind.mPtr, stance.mTarget);
                    if (!shot.mFired)
                    {
                        // The weapon may have changed without a shot (it was empty, the character took another)
                        if (mind.mShowAfter <= 0.f)
                            showWeapon(frame, mind, scripts);
                        // A character in range that does not fire for a while says why (round 10L: the pistol raider of
                        // Fallout 3 stood at 1,250 units for 30 s and the log had no line about it)
                        if ((mind.mHeld += duration) >= holdFireReportAfter)
                        {
                            mind.mHeld = 0.f;
                            Log(Debug::Info)
                                << mind.mPtr.getCellRef().getRefId() << " holds fire at " << static_cast<int>(away)
                                << " units (engage " << static_cast<int>(stance.mEngage) << ", "
                                << (visible ? "sees" : "does not see")
                                << " the target): " << scripts.attackDiagnosis(mind.mPtr);
                        }
                        break;
                    }
                    mind.mHeld = 0.f;
                    shotMade = true;
                    // The weapon that fired is the one that is shown with the shot, which has used it up when it was
                    // the last round or the last grenade and the character took another: that one is drawn after the
                    // shot
                    if (mind.mShowAfter > 0.f)
                    {
                        mind.mShowAfter = 0.f;
                        showWeapon(frame, mind, scripts);
                    }
                    if (auto* animation
                        = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr)))
                        animation->attack();
                    if (scripts.heldWeapon(mind.mPtr).mWeapon != mind.mDrawn)
                        mind.mShowAfter = shotShownFor;
                    break;
            }
            // A character that has neither shot nor gone anywhere for a while says what it is doing and why
            if (shotMade || distanceAcross(mind.position(), mind.mIdleAt) > idleReportMove)
            {
                mind.mIdle = 0.f;
                mind.mIdleAt = mind.position();
            }
            else if ((mind.mIdle += duration) >= holdFireReportAfter)
            {
                mind.mIdle = 0.f;
                const char* moveName = move == FalloutScript::FightAI::Move::Approach ? "going for the target"
                    : move == FalloutScript::FightAI::Move::Retreat                   ? "running from the target"
                                                                                      : "in range of the target";
                const char* walkName = !walked ? "not walking"
                    : *walked == Walk::Arrived ? "arrived"
                    : *walked == Walk::Failed  ? "no way"
                    : *walked == Walk::Waiting
                    ? (mind.mPartial ? "waiting, the path stops short" : "waiting for a path")
                    : "moving";
                Log(Debug::Info) << scripts.referenceName(mind.mPtr) << " has stood still in a fight for "
                                 << static_cast<int>(holdFireReportAfter) << " s, " << moveName << ", "
                                 << static_cast<int>(away) << " units away (engage " << static_cast<int>(stance.mEngage)
                                 << ", " << (visible ? "sees" : "does not see") << " it; " << walkName
                                 << "): " << scripts.attackDiagnosis(mind.mPtr);
            }
            return true;
        }
    }

    namespace
    {
        /// The character that the player talks with stands still and turns to the player, as the games have it face
        /// the one it talks with (the camera of the conversation looks at its face); one in a piece of furniture stays
        /// in it. True for that character, whose turn it is then.
        bool facesSpeaker(const Frame& frame, FalloutActors::Mind& mind, const OFWorld::Ptr& speaker, float duration)
        {
            if (speaker.isEmpty() || !(mind.mPtr == speaker) || frame.mScripts == nullptr
                || !sharesSpace(mind.mPtr, frame.mWorld.getPlayerPtr()) || frame.mScripts->usesFurniture(mind.mPtr))
                return false;
            stopWalking(mind);
            faceToward(frame, mind,
                std::atan2(frame.mPlayer.x() - mind.position().x(), frame.mPlayer.y() - mind.position().y()), duration);
            return true;
        }
    }

    void FalloutActors::faceSpeaker(float duration)
    {
        // A conversation pauses the world, and the minds are not given their turn then: the one it is with turns here
        OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
        if (mMinds.empty() || scripts == nullptr)
            return;
        const OFWorld::Ptr speaker = scripts->conversationSpeaker();
        if (speaker.isEmpty())
            return;
        const auto found = mMinds.find(speaker.mRef);
        if (found == mMinds.end())
            return;
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return;
        const OFWorld::TimeStamp now = world.getTimeStamp();
        Frame frame{ world, falloutClock(now.getHour(), now.getDay()), player.getRefData().getPosition().asVec3(),
            Settings::navigator().mEnable, scripts, nullptr, &mMinds };
        facesSpeaker(frame, *found->second, speaker, std::min(duration, longestStep));
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
            Settings::navigator().mEnable, OFBase::Environment::get().getFalloutScripts(), &events, &mMinds };
        duration = std::min(duration, longestStep);
        const float range = static_cast<float>(Settings::game().mActorsProcessingRange);

        // The character that the player talks with
        const OFWorld::Ptr speaker = frame.mScripts != nullptr ? frame.mScripts->conversationSpeaker() : OFWorld::Ptr();

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

            // It is dead (a blow or a script killed it): it does nothing more, and its body falls, at once for one
            // that was dead when it came into view
            if (frame.mScripts != nullptr && frame.mScripts->isDead(mind.mPtr))
            {
                stopWalking(mind);
                mind.mFighting = false;
                publishMind(frame.mScripts, mind, true);
                if (!mind.mFell)
                {
                    mind.mFell = true;
                    fall(frame, mind);
                }
                // A blow that killed it threw it back, or it died in the air: the body goes on to where the throw takes it
                // and falls to the ground (a body that is dead takes no damage from the fall)
                if (mind.mThrown)
                    thrown(frame, mind, duration);
                continue;
            }
            mind.mSeenAlive = true;

            // A blow threw it back, or it is falling: it does nothing else until it stands
            if (mind.mThrown)
            {
                if (thrown(frame, mind, duration))
                    continue;
                // It died of the fall that ended it: the next frame lets its body fall, and nothing of the fight or the
                // packages is done for a corpse
                if (frame.mScripts != nullptr && frame.mScripts->isDead(mind.mPtr))
                    continue;
            }

            // Scripts told it to do nothing (SetActorsAI 0): it follows no package, fights no one and turns to no one (the
            // executable does not process an actor whose flag is off); it stands where it is
            if (frame.mScripts != nullptr && frame.mScripts->actorAiOff(mind.mPtr))
            {
                if (mind.mWalking)
                    stopWalking(mind);
                mind.mFighting = false;
                publishMind(frame.mScripts, mind, false);
                continue;
            }
            publishMind(frame.mScripts, mind, false);

            if (facesSpeaker(frame, mind, speaker, duration))
                continue;

            // A character in a piece of furniture holds the pose of its entry
            if (frame.mScripts != nullptr)
                if (auto* animation
                    = dynamic_cast<OFRender::FalloutActorAnimation*>(frame.mWorld.getAnimation(mind.mPtr)))
                    animation->setPose(frame.mScripts->furnitureKindOf(mind.mPtr));

            // A fight comes before the packages
            if (frame.mScripts != nullptr && fight(frame, mind, duration))
                continue;

            mind.mReview -= duration;
            if (mind.mReview <= 0.f)
            {
                mind.mReview = reviewInterval;
                if (!mind.mCommanded)
                    review(frame, mind);
            }
            mind.mWait = std::max(0.f, mind.mWait - duration);

            // A package that ran out (SetPackDuration) is over: when no other package is on, the review keeps it as the one
            // that is followed, but what it asks (a walk, a follow, a roam) is not carried on: the character stays where
            // it is until a package comes on
            if (mind.mPackage != nullptr && mind.mExpired)
            {
                if (mind.mWalking)
                    stopWalking(mind);
                continue;
            }

            switch (mind.mGoal.mBehaviour)
            {
                case ESM4::PackageBehaviour::None:
                    break;
                case ESM4::PackageBehaviour::Stay:
                {
                    // Something else (a script) moved it from where it stood, up or down as well: it goes back
                    if (mind.mAtGoal && (mind.position() - mind.mRestPlace).length() > waypointDistance
                        && !(frame.mScripts != nullptr && frame.mScripts->usesFurniture(mind.mPtr)))
                        mind.mAtGoal = false;
                    if (!mind.mAtGoal && mind.mWait <= 0.f && goToPlace(frame, mind, duration))
                        finishTravel(frame, mind);
                    if (mind.mAtGoal && mind.mHasFacing
                        && !(frame.mScripts != nullptr && frame.mScripts->usesFurniture(mind.mPtr)))
                        faceToward(frame, mind, mind.mFacing, duration);
                    break;
                }
                case ESM4::PackageBehaviour::Use:
                {
                    // Something else moved it from the furniture it was led to: it goes back
                    if (mind.mAtGoal && (mind.position() - mind.mRestPlace).length() > waypointDistance
                        && !(frame.mScripts != nullptr && frame.mScripts->usesFurniture(mind.mPtr)))
                        mind.mAtGoal = false;
                    if (!mind.mAtGoal && mind.mWait <= 0.f && goToPlace(frame, mind, duration))
                        finishTravel(frame, mind);
                    break;
                }
                case ESM4::PackageBehaviour::Escort:
                {
                    // It leads the way: it waits for the one it escorts when that one is far behind
                    const bool hasTarget = mind.mFollowing.mPlayer || mind.mFollowing.mRef.isSet();
                    if (mind.mAtGoal && (mind.position() - mind.mRestPlace).length() > waypointDistance)
                        mind.mAtGoal = false;
                    if (hasTarget && !mind.mAtGoal)
                    {
                        osg::Vec3f target;
                        if (!followPosition(frame, mind.mPtr, mind.mFollowing, target)
                            || !falloutEscortMoves(
                                distanceByFloor(mind.position(), target), mind.mFollowing.mDistance, mind.mWalking))
                        {
                            if (mind.mWalking)
                                stopWalking(mind);
                            break;
                        }
                    }
                    if (!mind.mAtGoal && mind.mWait <= 0.f && goToPlace(frame, mind, duration))
                        finishTravel(frame, mind);
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
        {
            // What a script of a package asks of the actor (GetCurrentAIProcedure) is the state the frame ended with: the
            // package, the stage and the end of it that the frame brought, not what was told before it
            for (const PendingEvent& event : events)
                if (!event.mActor.isEmpty())
                    if (const auto found = mMinds.find(event.mActor.mRef); found != mMinds.end())
                        publishMind(frame.mScripts, *found->second, frame.mScripts->isDead(event.mActor));
            for (const PendingEvent& event : events)
                if (!event.mActor.isEmpty())
                    frame.mScripts->packageEvent(event.mActor, *event.mPackage, event.mEvent);
        }
    }
}
