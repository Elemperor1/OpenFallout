#include "falloutactoranimation.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <vector>

#include <osg/ComputeBoundsVisitor>
#include <osg/Matrix>
#include <osg/MatrixTransform>
#include <osg/Transform>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/riggeometryosgaextension.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwmechanics/character.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/refdata.hpp"

#include "blendmask.hpp"
#include "falloutface.hpp"

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
        /// Says a thing about a skeleton folder or a file once, whatever number of characters share it: the characters
        /// of a folder all have the same problem, and a log line for each of them hides the others
        bool firstTimeFor(const std::string& key)
        {
            static std::mutex mutex;
            static std::set<std::string> told;
            const std::lock_guard lock(mutex);
            return told.insert(key).second;
        }

        /// Finds out whether a part has meshes that are skinned to the bones of a skeleton
        class FindSkinnedMesh : public osg::NodeVisitor
        {
        public:
            FindSkinnedMesh()
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
            {
            }

            void apply(osg::Drawable& drawable) override
            {
                if (dynamic_cast<SceneUtil::RigGeometry*>(&drawable) != nullptr
                    || dynamic_cast<SceneUtil::RigGeometryHolder*>(&drawable) != nullptr)
                    mSkinned = true;
            }

            bool mSkinned = false;
        };

        /// Says in numbers where a part that is not skinned has been put: the box of its meshes in the frame of the
        /// node that holds it, that node's place and turn in the character, and the box in the frame of the character
        /// (feet at 0, up is z). Empty for a part that is skinned, which needs no placing.
        std::string describePlacement(osg::Node& part, const osg::Node* characterRoot, const std::string& bone)
        {
            FindSkinnedMesh skin;
            part.accept(skin);
            if (skin.mSkinned)
                return {};

            osg::ComputeBoundsVisitor bounds;
            part.accept(bounds);
            const osg::BoundingBox box = bounds.getBoundingBox();

            osg::Matrix parentToCharacter;
            const osg::NodePathList paths = part.getParentalNodePaths(const_cast<osg::Node*>(characterRoot));
            if (!paths.empty() && paths.front().size() > 1)
            {
                osg::NodePath path = paths.front();
                path.pop_back();
                parentToCharacter = osg::computeLocalToWorld(path);
            }

            const auto vec = [](std::ostringstream& out, const osg::Vec3d& v) {
                out << "(" << v.x() << ", " << v.y() << ", " << v.z() << ")";
            };
            std::ostringstream out;
            out << std::fixed << std::setprecision(2);
            out << (bone.empty() ? std::string("not skinned and on no bone") : "on the bone " + bone);
            if (!box.valid())
            {
                out << ", no mesh";
                return out.str();
            }
            out << ", box in the frame of what holds it ";
            vec(out, box._min);
            out << " to ";
            vec(out, box._max);

            const osg::Quat turn = parentToCharacter.getRotate();
            out << ", that frame in the character: at ";
            vec(out, parentToCharacter.getTrans());
            out << " with x towards ";
            vec(out, turn * osg::Vec3d(1.0, 0.0, 0.0));
            out << ", y towards ";
            vec(out, turn * osg::Vec3d(0.0, 1.0, 0.0));
            out << ", z towards ";
            vec(out, turn * osg::Vec3d(0.0, 0.0, 1.0));

            osg::BoundingBoxd inCharacter;
            for (int corner = 0; corner < 8; ++corner)
                inCharacter.expandBy(osg::Vec3d(box.corner(corner)) * parentToCharacter);
            out << ", box in the character ";
            vec(out, inCharacter._min);
            out << " to ";
            vec(out, inCharacter._max);
            return out.str();
        }

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
        watchPose(duration);
    }

    void FalloutActorAnimation::watchPose(float duration)
    {
        // A part of the body that no animation moves is left in the pose of the skeleton file, which for the people of
        // the games is the T. The idle plays on below everything else (nothing disables it), so a part with nothing
        // on it means the idle was lost, and it is started again.
        if (isDown() || mDefaultIdleGroup.empty())
            return;
        // And an idle that has nothing for a part of the body leaves it in that pose whatever plays: said once for
        // each kind of idle and skeleton, since starting it again changes nothing. Only for the people: the bone
        // groups are those of their skeleton, and the animals and the machines do not have arms to leave out.
        if (!mCoverageChecked && isPlaying(mDefaultIdleGroup))
        {
            mCoverageChecked = true;
            std::string missing;
            if (mPtr.getType() == ESM4::Npc::sRecordId)
                for (int group = 0; group < Num_BoneGroups; ++group)
                    if (!hasControllersFor(mDefaultIdleGroup, static_cast<BoneGroup>(group)))
                        missing += (missing.empty() ? "" : ", ") + std::to_string(group);
            if (!missing.empty()
                && firstTimeFor("coverage\n" + mDefaultIdleGroup + "\n" + mSkeletonPath + "\n" + missing))
                Log(Debug::Warning) << "T-pose watch: the idle " << mDefaultIdleGroup << " of " << mSkeletonPath
                                    << " has no animation for the bone groups " << missing
                                    << "; they stay in the pose of the skeleton file (seen on "
                                    << mPtr.getCellRef().getRefId() << ")";
        }
        bool bare = false;
        for (int group = 0; group < Num_BoneGroups; ++group)
            bare = bare || getActiveGroup(static_cast<BoneGroup>(group)).empty();
        if (!bare)
        {
            mBareTime = 0.f;
            return;
        }
        mBareTime += duration;
        constexpr float graceTime = 0.5f;
        if (mBareTime < graceTime)
            return;
        mBareTime = 0.f;
        if (!mBareReported)
        {
            mBareReported = true;
            Log(Debug::Warning) << "T-pose watch: " << mPtr.getCellRef().getRefId() << " (" << mSkeletonPath
                                << ") has a part of the body with no animation; its idle " << mDefaultIdleGroup
                                << " starts again";
        }
        // The group may have been removed from the states by something else that played at its priority
        disable(mDefaultIdleGroup);
        play(mDefaultIdleGroup, OFRender::AnimPriority(OFMechanics::Priority_Default), OFRender::BlendMask_All, false,
            1.f, "start", "stop", 0.f, ~0u, false);
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
            if (firstTimeFor("nothing\n" + file + "\n" + skeleton))
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
        {
            if (firstTimeFor("nofolder\n" + skeleton))
                Log(Debug::Info) << "Animations: the skeleton \"" << skeleton << "\" of "
                                 << mPtr.getCellRef().getRefId() << " has no folder; its character is not animated";
            return;
        }

        mSkeletonPath = skeleton;
        const std::vector<std::string>& files = animationFilesOf(*mResourceSystem->getVFS(), folder);

        bool playing = false;
        const std::string idle = chooseFalloutIdle(folder, files);
        if (idle.empty())
        {
            if (firstTimeFor("noidle\n" + folder))
                Log(Debug::Info) << "Animations: no idle animation among the " << files.size() << " files in " << folder
                                 << " (" << mPtr.getCellRef().getRefId()
                                 << " and the others of it stand in the pose of " << skeleton << ")";
        }
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
                mDefaultIdleGroup = group;
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
        {
            if (firstTimeFor("noanimation\n" + skeleton))
                Log(Debug::Info) << "Animations: nothing plays on " << skeleton << " (" << mPtr.getCellRef().getRefId()
                                 << " and the others of it stand in its pose): idle " << (idle.empty() ? "none" : idle)
                                 << ", walk " << locomotion.mWalk << ", run " << locomotion.mRun;
            return;
        }

        // The idle and sitting animations of a person key the pelvis on "Bip01 NonAccum" and leave "Bip01" out, which
        // the walking ones key at height 0: the skeleton file has "Bip01" at the standing height of the pelvis, so a
        // person that has not walked yet had that height twice and stood 61 to 67 units above the floor (found on the
        // Mac, round 11J)
        if (mPtr.getType() == ESM4::Npc::sRecordId)
        {
            SceneUtil::FindByNameVisitor find("Bip01");
            mObjectRoot->accept(find);
            if (osg::MatrixTransform* pelvisRoot = dynamic_cast<osg::MatrixTransform*>(find.mFoundNode))
                lowerToTheFloor(*pelvisRoot);
        }

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

    osg::ref_ptr<osg::Node> FalloutActorAnimation::insertPart(std::string_view model, bool forHead)
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

        // A part that is not skinned (the hair, eyes and mouth, a hat) is put on a bone of the skeleton by its file,
        // that is the head for all of these: at the root of the character it would stay at the feet
        std::string attachTo = attachmentName(*part);
        osg::Quat partRoot = attachmentRotation(*part);
        if (attachTo.empty() && forHead)
        {
            // Most hats name no bone. Their vertices are around the origin of the head (7 to 17 units above it for a
            // hat), where a few are in the frame of the skeleton, at head height: those are right where they are
            FindSkinnedMesh skin;
            part->accept(skin);
            osg::ComputeBoundsVisitor bounds;
            part->accept(bounds);
            constexpr double skeletonHeightFrom = 60.0;
            if (!skin.mSkinned && bounds.getBoundingBox().valid()
                && bounds.getBoundingBox().center().z() < skeletonHeightFrom)
            {
                attachTo = "Bip01 Head";
                partRoot = osg::Quat();
            }
        }
        if (!attachTo.empty())
        {
            // The part is out of the scene while the bone is looked for, so that the bone is one of the skeleton of
            // the character and never a node of the part itself (which the part would then be put under)
            // (getParents() of a node that is not const hands out a copy of the list, so it is taken once: the two ends
            // of a range must come from the same list)
            const osg::Node::ParentList parentList = part->getParents();
            const std::vector<osg::ref_ptr<osg::Group>> parents(parentList.begin(), parentList.end());
            for (const osg::ref_ptr<osg::Group>& parent : parents)
                parent->removeChild(part);
            SceneUtil::FindByNameVisitor find(attachTo);
            mObjectRoot->accept(find);
            osg::Group* bone = find.mFoundNode;
            if (bone != nullptr)
            {
                // The vertices of these parts are in the frame of the head model, not in that of the bone, which the
                // bone is turned from by 90 degrees: the turn that gets them there is put between the two
                osg::Quat boneRest;
                const osg::NodePathList paths = bone->getParentalNodePaths(mObjectRoot.get());
                if (!paths.empty())
                    boneRest = osg::computeLocalToWorld(paths.front()).getRotate();
                const osg::Quat frame = attachmentFrame(boneRest, partRoot);
                if (std::abs(frame.w()) < 0.99999)
                {
                    osg::ref_ptr<osg::MatrixTransform> turn = new osg::MatrixTransform(osg::Matrix::rotate(frame));
                    turn->setName("PartFrame");
                    turn->addChild(part);
                    bone->addChild(turn);
                }
                else
                    bone->addChild(part);
            }
            else
            {
                for (const osg::ref_ptr<osg::Group>& parent : parents)
                    parent->addChild(part);
                static std::mutex sReportedMutex;
                static std::set<std::string> sReported;
                const std::lock_guard lock(sReportedMutex);
                if (sReported.insert(std::string(model) + "\n" + attachTo).second)
                    Log(Debug::Warning) << "The part " << model << " asks to be on the bone " << attachTo
                                        << ", which the skeleton of " << mPtr.getCellRef().getRefId()
                                        << " does not have; it stays where the character is";
            }
        }

        // Where the parts that are not skinned ended up, once for each model: what to read when one is in the wrong
        // place
        if (firstTimeFor("placed\n" + std::string(model)))
        {
            const std::string where = describePlacement(*part, mObjectRoot, attachTo);
            if (!where.empty())
                Log(Debug::Info) << "Placed " << model << ": " << where;
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
        // A file that is the idle it stands in, or one of the gaits, is already what it plays: a one-shot of the same
        // group would end and take the group with it, and nothing starts it again
        if (group == mDefaultIdleGroup || group == mWalkGroup || group == mRunGroup || group == mPoseGroup)
            return true;
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
