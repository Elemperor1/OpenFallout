#include "falloutactoranimation.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include <osg/ComputeBoundsVisitor>
#include <osg/Matrix>
#include <osg/Quat>
#include <osg/MatrixTransform>
#include <osg/Transform>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadipct.hpp>
#include <components/esm4/loadipds.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadproj.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/falloutscript/footsteps.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/controller.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/riggeometryosgaextension.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/character.hpp"
#include "../mwscript/falloutscripts.hpp"
#include "../mwsound/falloutfootsteps.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/livecellref.hpp"
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

    namespace
    {
        // Keeps the node "Bip01" of a person on the floor. The skeleton file has it at the standing height of the
        // pelvis (67.77) and the animations of the games put that height on its child "Bip01 NonAccum", so the node is
        // lowered after everything that could have put it back (a controller of the skeleton file itself) on every
        // frame. A node that an animation keys at another height is left as that animation has it.
        class LowerRootCallback : public SceneUtil::NodeCallback<LowerRootCallback, osg::MatrixTransform*>
        {
        public:
            explicit LowerRootCallback(float restHeight)
                : mRestHeight(restHeight)
            {
            }

            void operator()(osg::MatrixTransform* node, osg::NodeVisitor* nv)
            {
                if (std::abs(node->getMatrix().getTrans().z() - mRestHeight) < 0.01f)
                    lowerToTheFloor(*node);
                traverse(node, nv);
            }

        private:
            float mRestHeight;
        };
    }

    /// Hands the text keys of the animations that name a sound to the actor
    class FalloutActorAnimation::SoundKeys : public Animation::TextKeyListener
    {
    public:
        explicit SoundKeys(FalloutActorAnimation& animation)
            : mAnimation(animation)
        {
        }

        void handleTextKey(std::string_view groupname, SceneUtil::TextKeyMap::ConstIterator key,
            const SceneUtil::TextKeyMap& map) override
        {
            mAnimation.playTextKeySound(key->second);
        }

    private:
        FalloutActorAnimation& mAnimation;
    };

    namespace
    {
        std::atomic<bool> sFirstPersonArmsShown{ false };
    }

    void FalloutActorAnimation::setFirstPersonArmsShown(bool shown)
    {
        sFirstPersonArmsShown = shown;
    }

    FalloutActorAnimation::FalloutActorAnimation(
        const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : Animation(ptr, std::move(parentNode), resourceSystem)
        , mSoundKeys(std::make_unique<SoundKeys>(*this))
    {
        setTextKeyListener(mSoundKeys.get());
    }

    FalloutActorAnimation::~FalloutActorAnimation()
    {
        setTextKeyListener(nullptr);
        if (mAdvanceCallback != nullptr)
            mInsert->removeUpdateCallback(mAdvanceCallback);
    }

    void FalloutActorAnimation::playTextKeySound(std::string_view text)
    {
        namespace Steps = FalloutScript::Footsteps;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        if (sound == nullptr || !sound->isEnabled())
            return;
        // The body of the player stands in for the player, and the arms are the player seen through their own eyes
        const bool playerBody = !mFirstPerson && !mMotionSource.isEmpty() && mMotionSource == OFMechanics::getPlayer();
        if (text.starts_with("sound: "))
        {
            // Of the player, the arms make the sounds that animations name while they show, and the body when they do
            // not (the body goes on moving when it is not drawn)
            if ((playerBody || mFirstPerson) && mFirstPerson != sFirstPersonArmsShown)
                return;
            std::string_view name = text.substr(7);
            name = name.substr(0, name.find(' '));
            const OFWorld::Ptr actor = playerBody || mFirstPerson ? OFMechanics::getPlayer() : mPtr;
            sound->playFalloutSound(name, actor, 1.f, OFSound::Type::Sfx, OFSound::PlayMode::Normal);
        }
        else if (text.starts_with("enum: "))
        {
            const std::optional<int> index = Steps::enumKeyIndex(text.substr(6));
            if (!index)
                return;
            // Taking a weapon out makes the sound of the weapon, in the arms of the player as in the body of anyone
            if (*index == static_cast<int>(Steps::EnumKey::Equip))
            {
                playWeaponHandled(true);
                return;
            }
            if (mFirstPerson)
                return;
            if (mPtr.getType() == ESM4::Creature::sRecordId)
                playCreatureSound(*index);
            else if (*index <= static_cast<int>(Steps::EnumKey::BackRight))
                playFootstep(*index);
            else if (*index >= static_cast<int>(Steps::EnumKey::FoleySneak)
                && *index <= static_cast<int>(Steps::EnumKey::ArmorRun))
                playWornSound(*index);
        }
    }

    void FalloutActorAnimation::playWeaponHandled(bool equip)
    {
        if (mWeaponForm == 0 || !OFBase::Environment::get().hasFalloutScripts())
            return;
        // Of the player the body and the arms both run the move: the one that is shown makes the sound
        const bool playerBody = !mMotionSource.isEmpty() && mMotionSource == OFMechanics::getPlayer();
        if ((playerBody || mFirstPerson) && mFirstPerson != sFirstPersonArmsShown)
            return;
        OFBase::Environment::get().getFalloutScripts()->weaponHandled(
            mFirstPerson ? OFMechanics::getPlayer() : (playerBody ? mMotionSource : mPtr), mWeaponForm, equip);
    }

    void FalloutActorAnimation::playWornSound(int type)
    {
        namespace Steps = FalloutScript::Footsteps;
        if (mFall != nullptr)
            return;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        if (sound == nullptr || store == nullptr)
            return;
        const bool playerBody = !mMotionSource.isEmpty() && mMotionSource == OFMechanics::getPlayer();
        const OFWorld::Ptr actor = playerBody ? mMotionSource : mPtr;
        static const Steps::Settings settings;
        if (!Steps::isHeard(
                settings, (sound->getListenerPosition() - actor.getRefData().getPosition().asVec3()).length()))
            return;
        thread_local std::minstd_rand random{ 0xf01e };
        for (const ESM4::Armor* worn : OFClass::ESM4Npc::getEquippedArmor(mPtr))
        {
            // A piece with no sounds of its own has those of the audio template it names
            const ESM4::Armor* source = worn;
            if (source->mSounds.empty() && !source->mTemplate.isZeroOrUnset())
                source = store->get<ESM4::Armor>().search(ESM::RefId::formIdRefId(source->mTemplate));
            if (source == nullptr)
                continue;
            std::vector<const ESM4::Armor::WornSound*> matching;
            std::vector<int> chances;
            for (const ESM4::Armor::WornSound& candidate : source->mSounds)
                if (candidate.mType == static_cast<std::uint32_t>(type))
                {
                    matching.push_back(&candidate);
                    chances.push_back(candidate.mChance);
                }
            if (matching.empty())
                continue;
            // One sound is made for the event: a piece whose chance does not come up leaves it to the next
            const int pick = Steps::pickByChance(chances, std::uniform_real_distribution<float>(0.f, 1.f)(random));
            if (pick < 0)
                continue;
            // (a sound that has no file to play leaves the event to the next piece too)
            if (sound->playFalloutSound(matching[static_cast<std::size_t>(pick)]->mSound, actor,
                    Steps::stepVolume(settings, playerBody && OFBase::Environment::get().getWorld()->isFirstPerson(),
                        std::uniform_real_distribution<float>(0.f, 1.f)(random)),
                    OFSound::Type::Foot, OFSound::PlayMode::Overlap)
                != nullptr)
                return;
        }
    }

    void FalloutActorAnimation::playCreatureSound(int type)
    {
        namespace Steps = FalloutScript::Footsteps;
        if (mFall != nullptr || type > static_cast<int>(Steps::EnumKey::Death))
            return;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        const OFWorld::LiveCellRef<ESM4::Creature>* ref = mPtr.get<ESM4::Creature>();
        if (sound == nullptr || store == nullptr || ref == nullptr)
            return;
        // The sounds of the creature, or of the one that it takes its sounds from (CSCR)
        const ESM4::Creature::CreatureSoundGroup* group = nullptr;
        const ESM4::Creature* record = ref->mBase;
        for (int depth = 0; depth < 4 && record != nullptr && group == nullptr; ++depth)
        {
            for (const ESM4::Creature::CreatureSoundGroup& candidate : record->mSoundGroups)
                if (candidate.mType == static_cast<std::uint32_t>(type) && !candidate.mSounds.empty())
                {
                    group = &candidate;
                    break;
                }
            if (group != nullptr || record->mSoundBase.isZeroOrUnset())
                break;
            record = store->get<ESM4::Creature>().search(ESM::RefId::formIdRefId(record->mSoundBase));
        }
        if (group == nullptr)
            return;

        thread_local std::minstd_rand random{ 0xc7ea };
        std::vector<int> chances;
        for (const ESM4::Creature::CreatureSound& candidate : group->mSounds)
            chances.push_back(candidate.mChance);
        const int pick = Steps::pickByChance(chances, std::uniform_real_distribution<float>(0.f, 1.f)(random));
        if (pick < 0)
            return;
        const bool foot = type <= static_cast<int>(Steps::EnumKey::BackRight);
        // A step is heard no farther than the INI says
        if (foot)
        {
            static const Steps::Settings settings;
            if (!Steps::isHeard(settings,
                    (sound->getListenerPosition() - mPtr.getRefData().getPosition().asVec3()).length()))
                return;
        }
        sound->playFalloutSound(group->mSounds[static_cast<std::size_t>(pick)].mSound, mPtr, 1.f,
            foot ? OFSound::Type::Foot : OFSound::Type::Sfx,
            foot ? OFSound::PlayMode::Overlap : OFSound::PlayMode::Normal);
    }

    void FalloutActorAnimation::playFootstep(int foot)
    {
        if (mFall != nullptr)
            return;
        const bool playerBody = !mMotionSource.isEmpty() && mMotionSource == OFMechanics::getPlayer();
        OFSound::FootstepRequest request;
        request.mActor = playerBody ? mMotionSource : mPtr;
        request.mFoot = static_cast<FalloutScript::Footsteps::Foot>(foot);
        request.mSneaking = mSneaking;
        request.mRunning = mGait == FalloutGait::Run;
        request.mPlayerInFirstPerson = playerBody && OFBase::Environment::get().getWorld()->isFirstPerson();
        request.mArmor = mArmorSounds;
        OFSound::playFootstep(request);
    }

    void FalloutActorAnimation::die(bool fallen)
    {
        if (mFall != nullptr || mObjectRoot == nullptr || mObjectRoot->getNumParents() == 0)
        {
            if (mFall == nullptr)
                Log(Debug::Info) << mPtr.getCellRef().getRefId() << " cannot fall: "
                                 << (mObjectRoot == nullptr ? "its body is not made" : "its body is in no scene");
            return;
        }
        // What it did stops where it is
        if (mAdvanceCallback != nullptr)
        {
            mInsert->removeUpdateCallback(mAdvanceCallback);
            mAdvanceCallback = nullptr;
        }
        // ... and is over: the groups it was playing (a walk, an attack, the reel of the blow that killed it) are taken out
        // of the list of what it plays, which would show a dead body that goes on with its attack (the pose stays: nothing
        // is updating the bones any more)
        std::vector<std::string> playing;
        for (const auto& state : mStates)
            playing.push_back(state.first);
        for (const std::string& group : playing)
            disable(group);
        mMoveGroup.clear();

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
        logPose(duration);
        logWeapon(duration);
        updateEffects(duration);
    }

    void FalloutActorAnimation::logWeapon(float duration)
    {
        if (!mDrawn || mWeaponLooks >= 2)
            return;
        mWeaponTime += duration;
        if (mWeaponTime < (mWeaponLooks == 0 ? 1.f : 5.f))
            return;
        ++mWeaponLooks;
        sayWeapon(mWeaponLooks == 1 ? "a second after it was taken out" : "five seconds after it was taken out");
    }

    void FalloutActorAnimation::sayWeapon(const char* when)
    {
        // Only for the first few weapons of a run: the lines are for reading what the hand holds and where it points
        static std::atomic<int> sSaid{ 0 };
        if (!mDrawn || mObjectRoot == nullptr || sSaid.fetch_add(1) >= 24)
            return;
        std::ostringstream out;
        out << std::fixed << std::setprecision(1);
        out << "Weapon of " << mPtr.getCellRef().getRefId() << ", " << when << ": ";
        if (mWeaponNode == nullptr || mWeaponParent == nullptr)
        {
            out << "none on the hand (type " << mWeaponType << ")";
            Log(Debug::Info) << out.str();
            return;
        }
        out << "on " << mWeaponParent->getName() << ", parents " << mWeaponNode->getNumParents() << ", node mask "
            << mWeaponNode->getNodeMask() << ", gait " << static_cast<int>(mGait) << ", aim group "
            << (mAimGroup.empty() ? "none" : mAimGroup) << (!mAimGroup.empty() && isPlaying(mAimGroup) ? " (plays)" : " (not playing)");
        const osg::NodePathList paths = mWeaponNode->getParentalNodePaths(mObjectRoot.get());
        if (!paths.empty())
        {
            // The weapon's own axes in the frame of the character (it faces +Y, up is +Z): an end of the barrel is
            // along one of them
            const osg::Matrix toCharacter = osg::computeLocalToWorld(paths.front());
            const osg::Vec3d x = osg::Matrix::transform3x3(osg::Vec3d(1, 0, 0), toCharacter);
            const osg::Vec3d y = osg::Matrix::transform3x3(osg::Vec3d(0, 1, 0), toCharacter);
            const osg::Vec3d z = osg::Matrix::transform3x3(osg::Vec3d(0, 0, 1), toCharacter);
            const osg::Vec3d at = toCharacter.getTrans();
            out << "; at (" << at.x() << ", " << at.y() << ", " << at.z() << ") in the character, its axes X ("
                << x.x() << ", " << x.y() << ", " << x.z() << "), Y (" << y.x() << ", " << y.y() << ", " << y.z()
                << "), Z (" << z.x() << ", " << z.y() << ", " << z.z() << ")";
        }
        const osg::BoundingSphere bound = mWeaponNode->getBound();
        out << "; bound radius " << bound.radius();
        Log(Debug::Info) << out.str();
    }

    void FalloutActorAnimation::logPose(float duration)
    {
        // Where the bones of a person stand a second into the idle, for the first few of them: what to read when one
        // stands above the floor or sinks into it. The character has its feet at height 0.
        if (mPoseLogged || mObjectRoot == nullptr || mPtr.getType() != ESM4::Npc::sRecordId)
            return;
        mPoseTime += duration;
        if (mPoseTime < 1.f)
            return;
        mPoseLogged = true;
        static std::atomic<int> sLogged{ 0 };
        if (sLogged.fetch_add(1) >= 6)
            return;
        std::ostringstream out;
        out << std::fixed << std::setprecision(2);
        bool first = true;
        for (const char* name :
            { "Bip01", "Bip01 NonAccum", "Bip01 Pelvis", "Bip01 Head", "Bip01 L Foot", "Bip01 R Foot" })
        {
            SceneUtil::FindByNameVisitor find(name);
            mObjectRoot->accept(find);
            if (find.mFoundNode == nullptr)
                continue;
            const osg::NodePathList paths = find.mFoundNode->getParentalNodePaths(mObjectRoot.get());
            if (paths.empty())
                continue;
            out << (first ? "" : ", ") << name << " z " << osg::computeLocalToWorld(paths.front()).getTrans().z();
            first = false;
        }
        Log(Debug::Info) << "Pose of " << mPtr.getCellRef().getRefId()
                         << " after a second (heights in the character, feet at 0): " << out.str();
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

        const std::string skeleton = mSkeletonOverride.empty()
            ? std::string(mPtr.getClass().getCorrectedModel(mPtr).value())
            : mSkeletonOverride;
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
        mWalkFile = locomotion.mWalk;
        mRunFile = locomotion.mRun;
        mMoveFolder = folder;
        mFemale = female;
        if (!mWalkGroup.empty())
            mMoveGroups.emplace(mWalkFile, mWalkGroup);
        if (!mRunGroup.empty())
            mMoveGroups.emplace(mRunFile, mRunGroup);
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
        if (mPtr.getType() == ESM4::Npc::sRecordId && !mFirstPerson)
        {
            SceneUtil::FindByNameVisitor find("Bip01");
            mObjectRoot->accept(find);
            if (osg::MatrixTransform* pelvisRoot = dynamic_cast<osg::MatrixTransform*>(find.mFoundNode))
            {
                const float restHeight = pelvisRoot->getMatrix().getTrans().z();
                lowerToTheFloor(*pelvisRoot);
                pelvisRoot->addUpdateCallback(new LowerRootCallback(restHeight));
            }
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
        mFrameTime = duration;
        const OFWorld::Ptr& source = mMotionSource.isEmpty() ? mPtr : mMotionSource;
        const ESM::Position& placed = source.getRefData().getPosition();
        const osg::Vec3f position = placed.asVec3();
        if (mHasLastPosition && duration > 0.f)
        {
            const osg::Vec2f step(position.x() - mLastPosition.x(), position.y() - mLastPosition.y());
            const float speed = step.length() / duration;
            const float blend = std::min(1.f, duration / smoothingTime);
            if (speed > teleportSpeed)
            {
                mSpeed = 0.f;
                mForwardSpeed = 0.f;
                mSideSpeed = 0.f;
            }
            else
            {
                mSpeed += (speed - mSpeed) * blend;
                // Which way that goes in relation to where the actor faces (the games name the movements ahead, back,
                // left and right): the yaw turns clockwise from +Y
                const float yaw = placed.rot[2];
                const float ahead = (step.x() * std::sin(yaw) + step.y() * std::cos(yaw)) / duration;
                const float across = (step.x() * std::cos(yaw) - step.y() * std::sin(yaw)) / duration;
                mForwardSpeed += (ahead - mForwardSpeed) * blend;
                mSideSpeed += (across - mSideSpeed) * blend;
            }
        }
        mLastPosition = position;
        mHasLastPosition = true;

        // How fast it turns where it stands: the change of its yaw from one frame to the next (a jump of the yaw, a
        // teleport or a script that faces it, is not a turn that is walked)
        {
            constexpr float turnSmoothing = 0.1f;
            constexpr float instantTurnRate = 12.f;
            const float yaw = placed.rot[2];
            if (mHasLastYaw && duration > 0.f)
            {
                float change = yaw - mLastYaw;
                const float pi = static_cast<float>(osg::PI);
                while (change > pi)
                    change -= 2.f * pi;
                while (change < -pi)
                    change += 2.f * pi;
                const float rate = change / duration;
                if (std::abs(rate) > instantTurnRate)
                    mTurnRate = 0.f;
                else
                    mTurnRate += (rate - mTurnRate) * std::min(1.f, duration / turnSmoothing);
            }
            mLastYaw = yaw;
            mHasLastYaw = true;
        }

        // A character in a piece of furniture does not walk: its place changes by a few units when it is put on its
        // seat, which would play the walk over the pose of its seat
        setGait(mPoseKind != 0 ? FalloutGait::Idle : chooseFalloutGait(mSpeed, mGait, mGaits));
        // A character of the world faces where it goes (it turns as it walks, so for a while the way it goes is to its
        // side or behind it: round 14F saw walking backward and sideways); only the player's body strafes and steps back
        mDirection = mMotionSource.isEmpty() ? FalloutDirection::Forward
                                              : chooseFalloutDirection(mForwardSpeed, mSideSpeed, mDirection);
        updateMovement();
    }

    void FalloutActorAnimation::setGait(FalloutGait gait)
    {
        if (gait == mGait)
            return;
        mGait = gait;
        // A weapon that was in sight and is not when the gait changes is what round 14E saw
        if (mDrawn && mWeaponLooks >= 1)
            sayWeapon("the gait changed");
    }

    void FalloutActorAnimation::updateMovement()
    {
        // What the actor does: stands (the idle plays on its own, or the sneaking idle over it), walks or runs
        const OFWorld::Ptr& source = mMotionSource.isEmpty() ? mPtr : mMotionSource;
        // Only the player's body is asked: the characters of the world have no stats of the engine's creatures to
        // say whether they sneak (the stance of a Fallout character is its own, and nothing sets it for them yet)
        bool sneak = false;
        if (mPoseKind == 0 && !mMotionSource.isEmpty() && mMotionSource.getClass().isActor())
        {
            try
            {
                sneak = OFBase::Environment::get().getMechanicsManager()->isSneaking(source);
            }
            catch (const std::exception& error)
            {
                // A class with no stats (round 14F: "class does not have creature stats" ended the engine at the load of
                // the first cell) does not sneak
                static bool said = false;
                if (!std::exchange(said, true))
                    Log(Debug::Warning) << "Cannot tell whether " << mPtr.getCellRef().getRefId() << " sneaks: " << error.what();
            }
        }
        const std::string_view prefix = mDrawn ? falloutWeaponPrefix(mWeaponType) : std::string_view();

        // In the air, touching the ground again, and swimming: the characters of the world are told whether they are in
        // the air; the body of the player asks the world, and is in the air when it has been off the ground for a
        // moment (a step down a stair is not a jump)
        constexpr float airborneAfter = 0.15f;
        constexpr float landingTime = 0.4f;
        bool airborne = mAirborneTold;
        bool swimming = false;
        if (mPoseKind == 0 && !mFirstPerson && !isDown() && !mMotionSource.isEmpty()
            && mMotionSource == OFMechanics::getPlayer())
        {
            OFBase::World* world = OFBase::Environment::get().getWorld();
            swimming = world->isSwimming(source);
            mAirTime = world->isOnGround(source) || swimming ? 0.f : mAirTime + mFrameTime;
            airborne = mAirTime > airborneAfter;
        }
        if (mPoseKind != 0 || mFirstPerson || isDown())
        {
            airborne = false;
            mWasAirborne = false;
            mLandLeft = 0.f;
        }
        else if (airborne)
        {
            mWasAirborne = true;
            mLandLeft = 0.f;
        }
        else if (mWasAirborne)
        {
            mWasAirborne = false;
            mLandLeft = landingTime;
        }
        else if (mLandLeft > 0.f)
            mLandLeft -= mFrameTime;
        int special = 0;
        if (airborne)
            special = 1;
        else if (mLandLeft > 0.f)
            special = 2;
        // Standing and turning where it stands (in the water too): the turn animation plays for as long as it turns,
        // and a moment more, so that a short turn is not a flicker
        constexpr float turnFrom = 0.5f;
        constexpr float turnHold = 0.35f;
        if (special == 0 && mPoseKind == 0 && mGait == FalloutGait::Idle && !isDown() && !mFirstPerson)
        {
            if (std::abs(mTurnRate) > turnFrom)
            {
                mTurning = mTurnRate > 0.f ? 5 : 4;
                mTurnLeft = turnHold;
            }
            else if (mTurnLeft > 0.f)
                mTurnLeft -= mFrameTime;
            else
                mTurning = 0;
        }
        else
        {
            mTurning = 0;
            mTurnLeft = 0.f;
        }
        if (special == 0)
        {
            if (mTurning != 0)
                special = mTurning;
            else if (swimming)
                special = 3;
        }

        // The file is chosen again only when what it is chosen for changes: the files of the folder are looked through
        MoveKey key{ mGait, mGait == FalloutGait::Idle ? FalloutDirection::Forward : mDirection, sneak,
            std::string(prefix), true };
        key.mSpecial = special;
        // A character with a crippled leg limps when it moves
        if (mGait != FalloutGait::Idle && mPoseKind == 0 && OFBase::Environment::get().hasFalloutScripts())
            key.mHurt = OFBase::Environment::get().getFalloutScripts()->crippledLegs(source) > 0;
        std::string wanted = mMoveFile;
        if (!(key == mMoveKey))
        {
            mMoveKey = key;
            wanted.clear();
            const std::vector<std::string>& files = animationFilesOf(*mResourceSystem->getVFS(), mMoveFolder);
            if (special != 0)
            {
                // (a skeleton that has none of the files goes on with its gaits)
                const FalloutAction action = special == 5 ? FalloutAction::TurnRight
                    : special == 4                       ? FalloutAction::TurnLeft
                    : special == 3                       ? FalloutAction::Swim
                    : special == 1                       ? FalloutAction::JumpLoop
                                                         : FalloutAction::JumpLand;
                wanted = chooseFalloutAction(mMoveFolder, prefix, sneak, action, mDirection, mGait == FalloutGait::Idle,
                    mGait == FalloutGait::Run, mFemale, files, swimming);
            }
            if (wanted.empty())
            {
                if (mGait == FalloutGait::Idle)
                {
                    if (sneak)
                        wanted = chooseFalloutSneakIdle(mMoveFolder, prefix, mFemale, files);
                }
                else
                {
                    wanted = chooseFalloutMove(
                        mMoveFolder, prefix, sneak, mDirection, mGait == FalloutGait::Run, mFemale, files, key.mHurt);
                    // A creature has the forward ones only, found by other names
                    if (wanted.empty())
                        wanted = mGait == FalloutGait::Run && mGaits.mHasRun ? mRunFile : mWalkFile;
                    if (wanted.empty())
                        wanted = mGait == FalloutGait::Run ? mWalkFile : mRunFile;
                }
            }
        }
        if (sneak != mSneaking)
        {
            mSneaking = sneak;
            Log(Debug::Info) << mPtr.getCellRef().getRefId() << (sneak ? " sneaks" : " stops sneaking");
        }

        if (wanted != mMoveFile)
        {
            // The idle animation plays on below, so the actor goes back to it when the movement stops
            if (!mMoveGroup.empty())
            {
                disable(mMoveGroup);
                mMoveGroup.clear();
            }
            mMoveFile = wanted;
            if (!wanted.empty())
            {
                std::string group;
                if (const auto known = mMoveGroups.find(wanted); known != mMoveGroups.end())
                    group = known->second;
                else
                {
                    group = addAnimation(wanted, mSkeletonPath);
                    mMoveGroups.emplace(wanted, group);
                }
                if (!group.empty())
                {
                    play(group, OFRender::AnimPriority(OFMechanics::Priority_Movement), OFRender::BlendMask_All, false,
                        1.f, "start", "stop", 0.f, ~0u, false);
                    mMoveGroup = group;
                }
                // (the jumps, landings, swims and turns are said apart from the gaits, whose changes use the first twelve up)
                if (mMovesSaid < 12 || (special != 0 && mSpecialsSaid < 12))
                {
                    if (mMovesSaid < 12)
                        ++mMovesSaid;
                    else
                        ++mSpecialsSaid;
                    Log(Debug::Info) << "Movement of " << mPtr.getCellRef().getRefId() << ": " << wanted << " (gait "
                                     << static_cast<int>(mGait) << ", direction " << static_cast<int>(mDirection)
                                     << ", sneaks " << sneak << ", limps " << key.mHurt << ", special " << special
                                     << ", weapon prefix \"" << prefix << "\", "
                                     << (group.empty() ? "no group" : "playing") << ")";
                }
            }
        }

        // The feet keep up with the ground: the animation plays as much faster or slower as the actor is
        if (!mMoveGroup.empty() && mGait != FalloutGait::Idle && special == 0)
            adjustSpeedMult(mMoveGroup, falloutAnimationSpeed(mSpeed, getVelocity(mMoveGroup)));
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

    void FalloutActorAnimation::drawWeapon(std::string_view model, int animationType, std::uint32_t weapon)
    {
        if (mObjectRoot == nullptr)
            return;
        holsterWeapon();
        mWeaponType = animationType;
        mWeaponForm = weapon;
        mDrawn = true;
        // The sound that goes on while the weapon is held out (of the player the body and the arms both hold it: the one
        // that is shown asks, the other does not)
        if (weapon != 0 && OFBase::Environment::get().hasFalloutScripts())
        {
            const bool playerBody = !mMotionSource.isEmpty() && mMotionSource == OFMechanics::getPlayer();
            const bool arms = mFirstPerson;
            if (!((playerBody || arms) && mFirstPerson != sFirstPersonArmsShown))
                OFBase::Environment::get().getFalloutScripts()->weaponHeld(
                    arms ? OFMechanics::getPlayer() : (playerBody ? mMotionSource : mPtr), weapon, true);
        }
        mWeaponTime = 0.f;
        mWeaponLooks = 0;

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
        if (mWeaponForm != 0 && OFBase::Environment::get().hasFalloutScripts())
        {
            // Putting the weapon away has its sound, and the one that went on while it was held out stops (the move of
            // putting it away is not played: its key `Enum: Unequip` is taken for the moment of holstering)
            const bool playerBody = !mMotionSource.isEmpty() && mMotionSource == OFMechanics::getPlayer();
            const bool arms = mFirstPerson;
            if (!((playerBody || arms) && mFirstPerson != sFirstPersonArmsShown))
            {
                const OFWorld::Ptr actor = arms ? OFMechanics::getPlayer() : (playerBody ? mMotionSource : mPtr);
                OFScript::FalloutScripts* scripts = OFBase::Environment::get().getFalloutScripts();
                scripts->weaponHeld(actor, 0, false);
                scripts->weaponHandled(actor, mWeaponForm, false);
            }
        }
        mWeaponForm = 0;
        if (!mAimGroup.empty())
        {
            disable(mAimGroup);
            mAimGroup.clear();
        }
        removeEffects(true);
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
        flashMuzzle();
    }

    namespace
    {
        /// How much slower the effects play and how much longer they last, for a check (the environment variable
        /// OPENFALLOUT_EFFECT_HOLD); 1 for the game
        float effectHold()
        {
            static const float hold = [] {
                const char* text = std::getenv("OPENFALLOUT_EFFECT_HOLD");
                const float value = text != nullptr ? static_cast<float>(std::atof(text)) : 1.f;
                return value > 1.f ? value : 1.f;
            }();
            return hold;
        }
    }

    void FalloutActorAnimation::addEffect(const std::string& model, osg::Group* parent, float seconds, bool flash,
        const osg::Vec3f* facing, float lift)
    {
        if (parent == nullptr || model.empty() || mResourceSystem == nullptr)
            return;
        // A model that is turned hangs from a mount of its own, which is what is taken off the node when it is over
        osg::ref_ptr<osg::Group> mount;
        if (facing != nullptr)
        {
            osg::Vec3f local = *facing;
            const osg::NodePathList paths = parent->getParentalNodePaths();
            if (!paths.empty())
                local = osg::computeLocalToWorld(paths.front()).getRotate().inverse() * local;
            if (local.length2() < 1e-6f)
                local = osg::Vec3f(0.f, 0.f, 1.f);
            local.normalize();
            osg::Quat turn;
            turn.makeRotate(osg::Vec3f(0.f, 0.f, 1.f), local);
            osg::ref_ptr<osg::MatrixTransform> placed = new osg::MatrixTransform;
            placed->setMatrix(osg::Matrix::rotate(turn) * osg::Matrix::translate(local * lift));
            mount = placed;
        }
        osg::ref_ptr<osg::Node> node;
        try
        {
            node = mResourceSystem->getSceneManager()->getInstance(
                Misc::ResourceHelpers::correctMeshPath(VFS::Path::Normalized(model)),
                mount != nullptr ? mount.get() : parent);
        }
        catch (const std::exception& e)
        {
            static std::atomic<int> sFailed{ 0 };
            if (sFailed.fetch_add(1) < 4)
                Log(Debug::Warning) << "The effect " << model << " could not be loaded: " << e.what();
            return;
        }
        if (node == nullptr)
            return;
        if (mount != nullptr)
        {
            parent->addChild(mount);
            // (what is taken away when the effect ends is the mount)
        }
        // The controllers of the model run from the moment it is shown, by a time of its own
        auto time = std::make_shared<EffectAnimationTime>();
        const std::shared_ptr<SceneUtil::ControllerSource> source = time;
        SceneUtil::AssignControllerSourcesVisitor assign(source);
        node->accept(assign);
        // A check can make the effects play slower and last longer so that a picture catches them (by the number in the
        // environment variable OPENFALLOUT_EFFECT_HOLD; the games' own last a fraction of a second)
        seconds *= effectHold();
        // At most a handful are alive at once: a weapon that fires faster than its effects fade drops the oldest
        constexpr std::size_t most = 12;
        if (mEffects.size() >= most)
            removeEffects(false);
        mEffects.push_back({ mount != nullptr ? osg::ref_ptr<osg::Node>(mount) : node, parent, seconds, flash, std::move(time) });
    }

    void FalloutActorAnimation::updateEffects(float duration)
    {
        for (auto it = mEffects.begin(); it != mEffects.end();)
        {
            it->mLeft -= duration;
            if (it->mLeft > 0.f)
            {
                if (it->mTime != nullptr)
                    it->mTime->addTime(duration / effectHold());
                ++it;
                continue;
            }
            if (it->mParent != nullptr && it->mNode != nullptr)
                it->mParent->removeChild(it->mNode);
            it = mEffects.erase(it);
        }
    }

    void FalloutActorAnimation::removeEffects(bool flashesOnly)
    {
        for (auto it = mEffects.begin(); it != mEffects.end();)
        {
            if (flashesOnly && !it->mFlash)
            {
                ++it;
                continue;
            }
            if (it->mParent != nullptr && it->mNode != nullptr)
                it->mParent->removeChild(it->mNode);
            it = mEffects.erase(it);
            // (without the flag the oldest one goes, which is enough to make room)
            if (!flashesOnly)
                break;
        }
    }

    void FalloutActorAnimation::flashMuzzle()
    {
        if (mWeaponNode == nullptr || mWeaponForm == 0)
            return;
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        if (store == nullptr)
            return;
        const ESM4::Weapon* weapon
            = store->get<ESM4::Weapon>().search(ESM::RefId::formIdRefId(ESM::FormId::fromUint32(mWeaponForm)));
        if (weapon == nullptr || !weapon->mHasGameData || weapon->mGame.mProjectile.isZeroOrUnset())
            return;
        const ESM4::Projectile* projectile
            = store->get<ESM4::Projectile>().search(ESM::RefId::formIdRefId(weapon->mGame.mProjectile));
        // A projectile that says no seconds for its flash has none (the silenced guns' say 0)
        if (projectile == nullptr || projectile->mMuzzleFlashModel.empty())
            return;
        const float seconds = projectile->gameData().mMuzzleFlashDuration;
        if (seconds <= 0.f)
            return;
        // The weapon's model names the muzzle (the node `ProjectileNode`); one that does not has the flash on the weapon
        SceneUtil::FindByNameVisitor find("ProjectileNode");
        mWeaponNode->accept(find);
        osg::Group* muzzle = find.mFoundNode != nullptr ? find.mFoundNode : mWeaponNode->asGroup();
        if (muzzle == nullptr)
            return;
        // A flash that is still on is the one of the shot before: the new shot starts it again
        removeEffects(true);
        addEffect(projectile->mMuzzleFlashModel, muzzle, seconds, true);
        static std::atomic<int> sSaid{ 0 };
        if (sSaid.fetch_add(1) < 6)
            Log(Debug::Info) << "Muzzle flash of " << mPtr.getCellRef().getRefId() << ": "
                             << projectile->mMuzzleFlashModel << " on " << muzzle->getName() << " for " << seconds
                             << " s" << (find.mFoundNode == nullptr ? " (the weapon has no ProjectileNode)" : "");
    }

    std::size_t FalloutActorAnimation::impactMaterial() const
    {
        // The materials of an impact data set, in the order of its list: stone, dirt, grass, glass, metal, wood, organic
        // (flesh), cloth, water, hollow metal, organic bug, organic glow
        constexpr std::size_t metal = 4;
        constexpr std::size_t flesh = 6;
        constexpr std::size_t bug = 10;
        // (the type is asked first: get<> throws for a character)
        if (mPtr.getType() == ESM4::Creature::sRecordId)
        {
            // The creature types: 2 is a mutated insect, 6 a robot
            if (const OFWorld::LiveCellRef<ESM4::Creature>* ref = mPtr.get<ESM4::Creature>(); ref->mBase != nullptr)
            {
                if (ref->mBase->mCreatureType == 2)
                    return bug;
                if (ref->mBase->mCreatureType == 6)
                    return metal;
            }
        }
        return flesh;
    }

    void FalloutActorAnimation::showImpact(std::uint32_t weapon, const std::string& node, const osg::Vec3f& toward)
    {
        if (mObjectRoot == nullptr || weapon == 0)
            return;
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        if (store == nullptr)
            return;
        // Why a blow shows nothing is said for the first few (a weapon with no impact data set, a set that has no entry for
        // the body's material, an entry with no model)
        static std::atomic<int> sSkipped{ 0 };
        const auto nothing = [&](const char* why) {
            if (sSkipped.fetch_add(1) < 6)
                Log(Debug::Info) << "No impact on " << mPtr.getCellRef().getRefId() << " for the weapon 0x" << std::hex
                                 << weapon << std::dec << ": " << why;
        };
        const ESM4::Weapon* record
            = store->get<ESM4::Weapon>().search(ESM::RefId::formIdRefId(ESM::FormId::fromUint32(weapon)));
        if (record == nullptr)
            return nothing("the weapon has no record");
        if (record->mImpactDataSet.isZeroOrUnset())
            return nothing("the weapon names no impact data set");
        const ESM4::ImpactDataSet* set
            = store->get<ESM4::ImpactDataSet>().search(ESM::RefId::formIdRefId(record->mImpactDataSet));
        if (set == nullptr)
            return nothing("the impact data set is not in the store");
        const std::size_t material = impactMaterial();
        if (material >= set->mImpacts.size() || set->mImpacts[material].isZeroOrUnset())
            return nothing("the set has no impact for the material of the body");
        const ESM4::ImpactData* impact
            = store->get<ESM4::ImpactData>().search(ESM::RefId::formIdRefId(set->mImpacts[material]));
        if (impact == nullptr)
            return nothing("the impact is not in the store");
        if (impact->mModel.empty())
            return nothing("the impact has no model");
        // The first float of the impact's data is the seconds it lasts
        float seconds = 0.f;
        std::memcpy(&seconds, impact->mData.data(), sizeof(seconds));
        if (!(seconds > 0.f))
            seconds = 0.1f;
        osg::Group* parent = nullptr;
        if (!node.empty())
        {
            SceneUtil::FindByNameVisitor find(node);
            mObjectRoot->accept(find);
            parent = find.mFoundNode;
        }
        if (parent == nullptr)
            parent = mObjectRoot.get();
        // The splat lies on the body facing the one that struck, a hand's breadth out from the node
        constexpr float lift = 8.f;
        addEffect(impact->mModel.getOriginal(), parent, seconds, false, &toward, lift);
        static std::atomic<int> sSaid{ 0 };
        if (sSaid.fetch_add(1) < 8)
            Log(Debug::Info) << "Impact on " << mPtr.getCellRef().getRefId() << ": " << impact->mEditorId << " ("
                             << impact->mModel.getOriginal() << ") on " << parent->getName() << " for " << seconds
                             << " s";
    }

    void FalloutActorAnimation::setPose(int entryKind)
    {
        if (entryKind < 0)
            entryKind = 0;
        const bool lost = entryKind == mPoseKind && entryKind != 0 && !mPoseGroup.empty()
            && !getInfo(mPoseGroup, nullptr, nullptr, nullptr);
        if (!falloutPoseToStart(entryKind, mPoseKind, !mPoseGroup.empty(), !lost))
            return;
        if (lost && !mPoseLostReported)
        {
            mPoseLostReported = true;
            Log(Debug::Info) << mPtr.getCellRef().getRefId() << " lost the pose " << mPoseGroup
                             << " to an animation that played over it (gait " << static_cast<int>(mGait)
                             << "); it holds it again";
        }
        // It gets up: the idle animation plays on below, and the gait is the one it was in
        if (!mPoseGroup.empty())
        {
            disable(mPoseGroup);
            mPoseGroup.clear();
        }
        // Not yet drawn (a game that was loaded makes its characters before their animations): the pose is asked for
        // again in the next frame
        if (entryKind != 0 && (mObjectRoot == nullptr || mSkeletonPath.empty() || isDown()))
        {
            mPoseKind = 0;
            return;
        }
        mPoseKind = entryKind;
        if (entryKind == 0)
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
        if (group == mDefaultIdleGroup || group == mWalkGroup || group == mRunGroup || group == mPoseGroup
            || group == mMoveGroup)
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
