#include "falloutactoranimation.hpp"

#include <algorithm>
#include <map>
#include <memory>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwmechanics/character.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/refdata.hpp"

#include "blendmask.hpp"

namespace OFRender
{
    /// The time of the animations of a actor moves with the frames of the scene: a actor of Fallout has no
    /// mechanics yet (that is the work of the next slices) to say how long a frame was.
    class FalloutActorAnimation::AdvanceCallback : public SceneUtil::NodeCallback<AdvanceCallback>
    {
    public:
        explicit AdvanceCallback(FalloutActorAnimation& animation)
            : mAnimation(&animation)
        {
        }

        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            const double now = nv->getFrameStamp()->getSimulationTime();
            if (mLastTime >= 0.0)
                mAnimation->advance(static_cast<float>(std::max(0.0, now - mLastTime)));
            mLastTime = now;
            traverse(node, nv);
        }

    private:
        FalloutActorAnimation* mAnimation;
        double mLastTime = -1.0;
    };

    /// The body falls over a short time, slowly at first and faster as it goes, as a body does
    class FalloutActorAnimation::FallCallback : public SceneUtil::NodeCallback<FallCallback, osg::MatrixTransform*>
    {
    public:
        FallCallback(const osg::Vec3f& axis, float lift, bool fallen)
            : mAxis(axis)
            , mLift(lift)
            , mFallen(fallen)
        {
        }

        void operator()(osg::MatrixTransform* node, osg::NodeVisitor* nv)
        {
            if (!mDone)
            {
                constexpr double fallSeconds = 0.7;
                const double now = nv->getFrameStamp()->getSimulationTime();
                if (mStart < 0.0)
                    mStart = now;
                const float progress = mFallen ? 1.f : static_cast<float>(std::min(1.0, (now - mStart) / fallSeconds));
                const float eased = progress * progress;
                // It turns about the point on the ground where it stood, and is lifted by the thickness of its body so
                // that it does not lie in the ground
                node->setMatrix(
                    osg::Matrix::rotate(eased * osg::PI_2, mAxis) * osg::Matrix::translate(0.f, 0.f, eased * mLift));
                mDone = progress >= 1.f;
            }
            traverse(node, nv);
        }

    private:
        osg::Vec3f mAxis;
        float mLift;
        bool mFallen;
        bool mDone = false;
        double mStart = -1.0;
    };

    namespace
    {
        /// The animation files (.kf, lower case paths) under a folder, listed once for all the characters that share it
        const std::vector<std::string>& animationFilesOf(const VFS::Manager& vfs, const std::string& folder)
        {
            static std::map<std::string, std::vector<std::string>> cache;
            const auto found = cache.find(folder);
            if (found != cache.end())
                return found->second;
            constexpr VFS::Path::ExtensionView kf("kf");
            std::vector<std::string> files;
            for (const VFS::Path::Normalized& name : vfs.getRecursiveDirectoryIterator(folder))
                if (name.extension() == kf)
                    files.push_back(name.value());
            return cache.emplace(folder, std::move(files)).first->second;
        }
    }

    FalloutActorAnimation::FalloutActorAnimation(
        const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : Animation(ptr, std::move(parentNode), resourceSystem)
    {
    }

    FalloutActorAnimation::~FalloutActorAnimation()
    {
        if (mAdvanceCallback != nullptr)
            mInsert->removeUpdateCallback(mAdvanceCallback);
    }

    void FalloutActorAnimation::die(bool fallen)
    {
        if (mFall != nullptr || mObjectRoot == nullptr || mObjectRoot->getNumParents() == 0)
            return;
        // What it did stops where it is
        if (mAdvanceCallback != nullptr)
        {
            mInsert->removeUpdateCallback(mAdvanceCallback);
            mAdvanceCallback = nullptr;
        }

        // A person falls on its back, and a creature (a four-legged one, a robot) on its side. The body is as thick
        // as a fifth of its size across, which keeps it out of the ground.
        const bool person = mPtr.getType() == ESM4::Npc::sRecordId;
        const float lift = std::clamp(mObjectRoot->getBound().radius() * 0.2f, 8.f, 40.f);
        osg::ref_ptr<osg::MatrixTransform> fall = new osg::MatrixTransform;
        fall->setName("Fall");
        osg::Group* parent = mObjectRoot->getParent(0);
        parent->replaceChild(mObjectRoot, fall);
        fall->addChild(mObjectRoot);
        fall->addUpdateCallback(new FallCallback(person ? osg::Vec3f(1, 0, 0) : osg::Vec3f(0, 1, 0), lift, fallen));
        mFall = fall;
    }

    void FalloutActorAnimation::advance(float duration)
    {
        updateGait(duration);
        runAnimation(duration);
    }

    std::string FalloutActorAnimation::addAnimation(const std::string& file, const std::string& skeleton)
    {
        if (file.empty())
            return {};
        addAnimSource(file, skeleton);
        // The loader of animation files names the group of a file after it
        std::string group(VFS::Path::Normalized(file).stem());
        if (!hasAnimation(group))
        {
            Log(Debug::Warning) << "The animation " << file << " has nothing to play on " << skeleton;
            return {};
        }
        return group;
    }

    void FalloutActorAnimation::startAnimations(bool female)
    {
        if (mObjectRoot == nullptr)
            return;

        const std::string skeleton = mPtr.getClass().getCorrectedModel(mPtr);
        const std::string folder = falloutAnimationFolder(skeleton);
        if (folder.empty())
            return;

        mSkeletonPath = skeleton;
        const std::vector<std::string>& files = animationFilesOf(*mResourceSystem->getVFS(), folder);

        bool playing = false;
        const std::string idle = chooseFalloutIdle(folder, files);
        if (idle.empty())
            Log(Debug::Verbose) << "No idle animation among the " << files.size() << " files in " << folder;
        else
        {
            Log(Debug::Verbose) << "Idle animation of " << mPtr.getCellRef().getRefId() << ": " << idle;
            const std::string group = addAnimation(idle, skeleton);
            if (!group.empty())
            {
                // A sequence that loops has the keys "loop start" and "loop stop" (the loader makes them), so it plays
                // on without end; one that clamps has none, and stays on its last frame
                play(group, OFRender::AnimPriority(OFMechanics::Priority_Default), OFRender::BlendMask_All, false, 1.f,
                    "start", "stop", 0.f, ~0u, false);
                playing = true;
            }
        }

        // The animations that play over the idle one while the actor is moved
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(folder, files, female);
        mWalkGroup = addAnimation(locomotion.mWalk, skeleton);
        mRunGroup = addAnimation(locomotion.mRun, skeleton);
        mGaits.mHasWalk = !mWalkGroup.empty();
        mGaits.mHasRun = !mRunGroup.empty();
        if (mGaits.mHasWalk)
            mGaits.mWalkVelocity = getVelocity(mWalkGroup);
        if (mGaits.mHasRun)
            mGaits.mRunVelocity = getVelocity(mRunGroup);
        if (mGaits.mHasWalk || mGaits.mHasRun)
        {
            Log(Debug::Verbose) << "Walking and running animations of " << mPtr.getCellRef().getRefId() << ": "
                                << (mGaits.mHasWalk ? locomotion.mWalk : "none") << " (" << mGaits.mWalkVelocity
                                << " units a second), " << (mGaits.mHasRun ? locomotion.mRun : "none") << " ("
                                << mGaits.mRunVelocity << ")";
            playing = true;
        }

        if (!playing)
            return;
        mAdvanceCallback = new AdvanceCallback(*this);
        mInsert->addUpdateCallback(mAdvanceCallback);
    }

    void FalloutActorAnimation::updateGait(float duration)
    {
        if (!mGaits.mHasWalk && !mGaits.mHasRun)
            return;

        // The actor has no mechanics to say how fast it moves, so the speed is that at which whatever moves it
        // (a script, the physics) changes its place from one frame to the next, across the ground. A jump from one
        // place to another is a teleport and not a movement.
        constexpr float teleportSpeed = 3000.f;
        constexpr float smoothingTime = 0.2f;
        const osg::Vec3f position = mPtr.getRefData().getPosition().asVec3();
        if (mHasLastPosition && duration > 0.f)
        {
            const osg::Vec2f step(position.x() - mLastPosition.x(), position.y() - mLastPosition.y());
            const float speed = step.length() / duration;
            if (speed > teleportSpeed)
                mSpeed = 0.f;
            else
                mSpeed += (speed - mSpeed) * std::min(1.f, duration / smoothingTime);
        }
        mLastPosition = position;
        mHasLastPosition = true;

        setGait(chooseFalloutGait(mSpeed, mGait, mGaits));

        if (mGait == FalloutGait::Walk)
            adjustSpeedMult(mWalkGroup, falloutAnimationSpeed(mSpeed, mGaits.mWalkVelocity));
        else if (mGait == FalloutGait::Run)
            adjustSpeedMult(mRunGroup, falloutAnimationSpeed(mSpeed, mGaits.mRunVelocity));
    }

    void FalloutActorAnimation::setGait(FalloutGait gait)
    {
        if (gait == mGait)
            return;

        // The idle animation plays on below the others, so the actor goes back to it when they stop
        if (mGait == FalloutGait::Walk)
            disable(mWalkGroup);
        else if (mGait == FalloutGait::Run)
            disable(mRunGroup);

        const std::string& group = gait == FalloutGait::Walk ? mWalkGroup : mRunGroup;
        if (gait != FalloutGait::Idle)
            play(group, OFRender::AnimPriority(OFMechanics::Priority_Movement), OFRender::BlendMask_All, false, 1.f,
                "start", "stop", 0.f, ~0u, false);
        mGait = gait;
    }

    osg::ref_ptr<osg::Node> FalloutActorAnimation::insertPart(std::string_view model)
    {
        if (model.empty())
            return nullptr;
        osg::ref_ptr<osg::Node> part = mResourceSystem->getSceneManager()->getInstance(
            Misc::ResourceHelpers::correctMeshPath(VFS::Path::Normalized(model)), mObjectRoot.get());

        // A mesh that is skinned binds to the bones of the nearest skeleton above it, and the file of a skinned part
        // has the bones that it names for itself, so the loader makes it a skeleton: the mesh would follow the bones
        // of the part, that no animation moves, and not those of the character. The part is an ordinary group here,
        // so that its meshes follow the skeleton of the character (and its copies of the bones are never looked up, the
        // first bone of a name is the one that is).
        if (SceneUtil::Skeleton* skeleton = dynamic_cast<SceneUtil::Skeleton*>(part.get()))
        {
            osg::ref_ptr<osg::Group> group = new osg::Group;
            group->setName(skeleton->getName());
            group->setStateSet(skeleton->getStateSet());
            group->setUserDataContainer(skeleton->getUserDataContainer());
            for (unsigned int i = 0; i < skeleton->getNumChildren(); ++i)
                group->addChild(skeleton->getChild(i));
            mObjectRoot->replaceChild(skeleton, group);
            part = group;
        }
        return part;
    }

    std::string FalloutActorAnimation::playWeaponMove(FalloutWeaponMove move, int priority, bool loop)
    {
        if (mObjectRoot == nullptr || mSkeletonPath.empty())
            return {};
        const std::string folder = falloutAnimationFolder(mSkeletonPath);
        const std::string file = chooseFalloutWeaponAnimation(
            folder, mWeaponType, move, animationFilesOf(*mResourceSystem->getVFS(), folder), mAttacks);
        if (file.empty())
            return {};
        std::string group;
        if (const auto known = mWeaponGroups.find(file); known != mWeaponGroups.end())
            group = known->second;
        else
        {
            group = addAnimation(file, mSkeletonPath);
            mWeaponGroups.emplace(file, group);
        }
        if (group.empty())
            return {};
        // One that was playing and has not ended starts again
        if (isPlaying(group))
            disable(group);
        play(group, OFRender::AnimPriority(priority), OFRender::BlendMask_UpperBody, !loop, 1.f, "start", "stop", 0.f,
            loop ? ~0u : 0u, false);
        return group;
    }

    void FalloutActorAnimation::drawWeapon(std::string_view model, int animationType)
    {
        if (mObjectRoot == nullptr)
            return;
        holsterWeapon();
        mWeaponType = animationType;
        mDrawn = true;

        if (!model.empty())
        {
            // The weapon sits on the hand: the node the games name for it, else the hand itself
            osg::Group* hand = nullptr;
            for (const char* name : { "Weapon", "Bip01 R Hand" })
            {
                SceneUtil::FindByNameVisitor find(name);
                mObjectRoot->accept(find);
                if (find.mFoundNode != nullptr)
                {
                    hand = find.mFoundNode;
                    break;
                }
            }
            if (hand == nullptr)
                Log(Debug::Warning) << "The weapon " << model << " has no hand to be in on "
                                    << mPtr.getCellRef().getRefId();
            else
            {
                mWeaponNode = mResourceSystem->getSceneManager()->getInstance(
                    Misc::ResourceHelpers::correctMeshPath(VFS::Path::Normalized(model)), hand);
                mWeaponParent = hand;
                Log(Debug::Verbose) << "Weapon " << model << " is in the hand " << hand->getName() << " of "
                                    << mPtr.getCellRef().getRefId();
            }
        }

        // A weapon is taken out and then held ready; bare hands are only held ready
        if (animationType >= 0)
            playWeaponMove(FalloutWeaponMove::Equip, OFMechanics::Priority_Weapon, false);
        mAimGroup = playWeaponMove(FalloutWeaponMove::Aim, OFMechanics::Priority_Hit, true);
    }

    void FalloutActorAnimation::holsterWeapon()
    {
        if (!mDrawn)
            return;
        mDrawn = false;
        if (!mAimGroup.empty())
        {
            disable(mAimGroup);
            mAimGroup.clear();
        }
        if (mWeaponNode != nullptr && mWeaponParent != nullptr)
            mWeaponParent->removeChild(mWeaponNode);
        mWeaponNode = nullptr;
        mWeaponParent = nullptr;
    }

    void FalloutActorAnimation::attack()
    {
        if (!mDrawn)
            return;
        playWeaponMove(FalloutWeaponMove::Attack, OFMechanics::Priority_Weapon, false);
        ++mAttacks;
    }

    void FalloutActorAnimation::setPose(int entryKind)
    {
        if (entryKind < 0)
            entryKind = 0;
        if (entryKind == mPoseKind)
            return;
        // It gets up: the idle animation plays on below, and the gait is the one it was in
        if (!mPoseGroup.empty())
        {
            disable(mPoseGroup);
            mPoseGroup.clear();
        }
        mPoseKind = entryKind;
        if (entryKind == 0 || mObjectRoot == nullptr || mSkeletonPath.empty() || isDown())
            return;

        const std::string folder = falloutAnimationFolder(mSkeletonPath);
        const std::string file
            = chooseFalloutPose(folder, entryKind, animationFilesOf(*mResourceSystem->getVFS(), folder));
        if (file.empty())
        {
            Log(Debug::Verbose) << mPtr.getCellRef().getRefId() << " has no pose for the furniture entry " << entryKind;
            return;
        }
        std::string group;
        if (const auto known = mPoseGroups.find(file); known != mPoseGroups.end())
            group = known->second;
        else
        {
            group = addAnimation(file, mSkeletonPath);
            mPoseGroups.emplace(file, group);
        }
        if (group.empty())
            return;
        play(group, OFRender::AnimPriority(OFMechanics::Priority_Movement), OFRender::BlendMask_All, false, 1.f,
            "start", "stop", 0.f, ~0u, false);
        mPoseGroup = group;
        Log(Debug::Verbose) << mPtr.getCellRef().getRefId() << " holds the pose " << file;
    }

    bool FalloutActorAnimation::playIdle(const std::string& file)
    {
        if (mObjectRoot == nullptr || mSkeletonPath.empty() || isDown() || file.empty())
            return false;
        std::string group;
        if (const auto known = mIdleGroups.find(file); known != mIdleGroups.end())
            group = known->second;
        else
        {
            group = addAnimation(file, mSkeletonPath);
            mIdleGroups.emplace(file, group);
        }
        if (group.empty())
            return false;
        // One that was playing and has not ended starts again
        if (isPlaying(group))
            disable(group);
        play(group, OFRender::AnimPriority(OFMechanics::Priority_Scripted), OFRender::BlendMask_All, true, 1.f, "start",
            "stop", 0.f, 0u, false);
        return true;
    }

    void FalloutActorAnimation::flinch()
    {
        if (isDown())
            return;
        playWeaponMove(FalloutWeaponMove::Recoil, OFMechanics::Priority_Block, false);
    }
}
