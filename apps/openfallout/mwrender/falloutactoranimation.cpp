#include "falloutactoranimation.hpp"

#include <algorithm>

#include <components/debug/debuglog.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/skeleton.hpp>
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

        constexpr VFS::Path::ExtensionView kf("kf");
        std::vector<std::string> files;
        for (const VFS::Path::Normalized& name : mResourceSystem->getVFS()->getRecursiveDirectoryIterator(folder))
            if (name.extension() == kf)
                files.push_back(name.value());

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
}
