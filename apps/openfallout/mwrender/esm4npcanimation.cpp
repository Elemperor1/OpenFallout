#include "esm4npcanimation.hpp"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <set>

#include <osg/Vec2f>

#include <components/esm4/loadarma.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>

#include <components/debug/debuglog.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/pathutil.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwmechanics/character.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"

#include "blendmask.hpp"
#include "falloutface.hpp"

namespace OFRender
{
    /// The time of the animations of a character moves with the frames of the scene: a character of Fallout has no
    /// mechanics yet (that is the work of the next slices) to say how long a frame was.
    class ESM4NpcAnimation::AdvanceCallback : public SceneUtil::NodeCallback<AdvanceCallback>
    {
    public:
        explicit AdvanceCallback(ESM4NpcAnimation& animation)
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
        ESM4NpcAnimation* mAnimation;
        double mLastTime = -1.0;
    };

    ESM4NpcAnimation::ESM4NpcAnimation(
        const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : Animation(ptr, std::move(parentNode), resourceSystem)
    {
        setObjectRoot(mPtr.getClass().getCorrectedModel(mPtr), true, true, false);
        updateParts();
        startAnimations();
    }

    ESM4NpcAnimation::~ESM4NpcAnimation()
    {
        if (mAdvanceCallback != nullptr)
            mInsert->removeUpdateCallback(mAdvanceCallback);
    }

    void ESM4NpcAnimation::advance(float duration)
    {
        updateGait(duration);
        runAnimation(duration);
    }

    std::string ESM4NpcAnimation::addAnimation(const std::string& file, const std::string& skeleton)
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

    void ESM4NpcAnimation::startAnimations()
    {
        if (mObjectRoot == nullptr)
            return;
        const ESM4::Npc* traits = OFClass::ESM4Npc::getTraitsRecord(mPtr);
        if (traits == nullptr || traits->mIsTES4 || !traits->mIsFONV)
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

        // The animations that play over the idle one while the character is moved
        const FalloutLocomotion locomotion = chooseFalloutLocomotion(folder, files, OFClass::ESM4Npc::isFemale(mPtr));
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

    void ESM4NpcAnimation::updateGait(float duration)
    {
        if (!mGaits.mHasWalk && !mGaits.mHasRun)
            return;

        // The character has no mechanics to say how fast it moves, so the speed is that at which whatever moves it
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

    void ESM4NpcAnimation::setGait(FalloutGait gait)
    {
        if (gait == mGait)
            return;

        // The idle animation plays on below the others, so the character goes back to it when they stop
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

    void ESM4NpcAnimation::updateParts()
    {
        if (mObjectRoot == nullptr)
            return;
        const ESM4::Npc* traits = OFClass::ESM4Npc::getTraitsRecord(mPtr);
        if (traits == nullptr)
            return;
        if (traits->mIsTES4)
            updatePartsTES4(*traits);
        else if (traits->mIsFONV)
            updatePartsFallout(*traits);
        else
            updatePartsTES5(*traits);
    }

    osg::ref_ptr<osg::Node> ESM4NpcAnimation::insertPart(std::string_view model)
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

    void ESM4NpcAnimation::shapeFalloutPart(osg::Node& part, std::string_view model, const ESM4::Npc& traits,
        const std::vector<float>& symmetric, const std::vector<float>& asymmetric, bool isHead, bool isBody)
    {
        // The shape: the morphs of the file beside the model, moved by the coefficients of the face
        if (!symmetric.empty() || !asymmetric.empty())
        {
            const std::shared_ptr<const ESM4::FaceMorphs> morphs = getFaceMorphs(*mResourceSystem->getVFS(), model);
            if (morphs != nullptr && morphFaceMeshes(part, *morphs, symmetric, asymmetric, isHead) == 0)
            {
                // Said once for each model, whatever number of characters wear it
                static std::mutex sReportedMutex;
                static std::set<std::string> sReported;
                const std::lock_guard lock(sReportedMutex);
                if (sReported.insert(Misc::StringUtils::lowerCase(model)).second)
                    Log(Debug::Warning) << "FaceGen: the meshes of " << model << " do not have the "
                                        << morphs->mVertexCount << " vertices of its morph file";
            }
        }

        // The skin: a texture that the editor wrote for the character, of the plugin that made the character
        if (!isHead && !isBody)
            return;
        const std::vector<std::string>& contentFiles = OFBase::Environment::get().getWorld()->getContentFiles();
        std::string plugin;
        if (traits.mId.mContentFile >= 0 && static_cast<std::size_t>(traits.mId.mContentFile) < contentFiles.size())
            plugin = Misc::StringUtils::lowerCase(
                std::filesystem::path(contentFiles[traits.mId.mContentFile]).stem().string());
        const ESM4::FaceTextureIndex& textures = getFaceTextures(*mResourceSystem->getVFS());
        const ESM4::FaceTextureIndex::Kind kind = isHead ? ESM4::FaceTextureIndex::Kind::Face
            : OFClass::ESM4Npc::isFemale(mPtr)           ? ESM4::FaceTextureIndex::Kind::BodyFemale
                                                         : ESM4::FaceTextureIndex::Kind::BodyMale;
        const std::string* path = textures.find(kind, plugin, traits.mId.mIndex);
        if (path == nullptr)
            return;
        const osg::ref_ptr<osg::Image> image
            = mResourceSystem->getImageManager()->getImage(VFS::Path::Normalized(*path));
        if (replaceDiffuseMap(part, image) == 0)
            Log(Debug::Verbose) << "FaceGen: " << model << " has no diffuse map to give " << *path;
    }

    void ESM4NpcAnimation::updatePartsFallout(const ESM4::Npc& traits)
    {
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        const ESM4::Race* race = OFClass::ESM4Npc::getRace(mPtr);
        if (race == nullptr)
            return;

        const ESM4::Hair* hair = nullptr;
        if (!traits.mHair.isZeroOrUnset())
        {
            hair = store->get<ESM4::Hair>().search(traits.mHair);
            if (hair == nullptr)
                Log(Debug::Error) << "Hair not found: " << ESM::RefId(traits.mHair);
        }

        const std::vector<const ESM4::HeadPart*> headParts
            = OFClass::expandHeadParts(traits.mHeadParts, [store](ESM::FormId partId) -> const ESM4::HeadPart* {
                  const ESM4::HeadPart* part = store->get<ESM4::HeadPart>().search(partId);
                  if (part == nullptr)
                      Log(Debug::Error) << "Head part not found: " << ESM::RefId(partId);
                  return part;
              });

        // The body parts are skinned to the bones of the skeleton and need no placing, unlike those of Oblivion
        const bool isFemale = OFClass::ESM4Npc::isFemale(mPtr);
        const std::string headModel = OFClass::falloutHeadModel(*race, isFemale);
        const std::string bodyModel = OFClass::falloutBodyModel(*race, isFemale);

        // The face of the character is that of its race for its sex, and the numbers of the record on top
        const std::vector<float> symmetric
            = ESM4::addFaceCoefficients(isFemale ? race->mSymShapeModeCoeffFemale : race->mSymShapeModeCoefficients,
                traits.mSymShapeModeCoefficients);
        const std::vector<float> asymmetric
            = ESM4::addFaceCoefficients(isFemale ? race->mAsymShapeModeCoeffFemale : race->mAsymShapeModeCoefficients,
                traits.mAsymShapeModeCoefficients);

        for (const std::string& model :
            OFClass::falloutNpcModels(*race, isFemale, hair, headParts, OFClass::ESM4Npc::getEquippedArmor(mPtr)))
        {
            const osg::ref_ptr<osg::Node> part = insertPart(model);
            if (part != nullptr)
                shapeFalloutPart(*part, model, traits, symmetric, asymmetric,
                    !headModel.empty() && Misc::StringUtils::ciEqual(model, headModel),
                    !bodyModel.empty() && Misc::StringUtils::ciEqual(model, bodyModel));
        }
    }

    template <class Record>
    static std::string_view chooseTes4EquipmentModel(const Record* rec, bool isFemale)
    {
        if (isFemale && !rec->mModelFemale.empty())
            return rec->mModelFemale.getOriginal();
        else if (!isFemale && !rec->mModelMale.empty())
            return rec->mModelMale.getOriginal();
        else
            return rec->mModel.getOriginal();
    }

    void ESM4NpcAnimation::updatePartsTES4(const ESM4::Npc& traits)
    {
        const ESM4::Race* race = OFClass::ESM4Npc::getRace(mPtr);
        bool isFemale = OFClass::ESM4Npc::isFemale(mPtr);

        // TODO: Body and head parts are placed incorrectly, need to attach to bones

        for (const ESM4::Race::BodyPart& bodyPart : (isFemale ? race->mBodyPartsFemale : race->mBodyPartsMale))
            insertPart(bodyPart.mesh);
        for (const ESM4::Race::BodyPart& bodyPart : race->mHeadParts)
            insertPart(bodyPart.mesh);
        if (!traits.mHair.isZeroOrUnset())
        {
            const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
            if (const ESM4::Hair* hair = store->get<ESM4::Hair>().search(traits.mHair))
                insertPart(hair->mModel.getOriginal());
            else
                Log(Debug::Error) << "Hair not found: " << ESM::RefId(traits.mHair);
        }

        for (const ESM4::Armor* armor : OFClass::ESM4Npc::getEquippedArmor(mPtr))
            insertPart(chooseTes4EquipmentModel(armor, isFemale));
        for (const ESM4::Clothing* clothing : OFClass::ESM4Npc::getEquippedClothing(mPtr))
            insertPart(chooseTes4EquipmentModel(clothing, isFemale));
    }

    void ESM4NpcAnimation::insertHeadParts(
        const std::vector<ESM::FormId>& partIds, std::set<uint32_t>& usedHeadPartTypes)
    {
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        for (ESM::FormId partId : partIds)
        {
            if (partId.isZeroOrUnset())
                continue;
            const ESM4::HeadPart* part = store->get<ESM4::HeadPart>().search(partId);
            if (!part)
            {
                Log(Debug::Error) << "Head part not found: " << ESM::RefId(partId);
                continue;
            }
            if (usedHeadPartTypes.emplace(part->mType).second)
                insertPart(part->mModel.getOriginal());
        }
    }

    void ESM4NpcAnimation::updatePartsTES5(const ESM4::Npc& traits)
    {
        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();

        const ESM4::Race* race = OFClass::ESM4Npc::getRace(mPtr);
        bool isFemale = OFClass::ESM4Npc::isFemale(mPtr);

        std::vector<const ESM4::ArmorAddon*> armorAddons;

        auto findArmorAddons = [&](const ESM4::Armor* armor) {
            for (ESM::FormId armaId : armor->mAddOns)
            {
                if (armaId.isZeroOrUnset())
                    continue;
                const ESM4::ArmorAddon* arma = store->get<ESM4::ArmorAddon>().search(armaId);
                if (!arma)
                {
                    Log(Debug::Error) << "ArmorAddon not found: " << ESM::RefId(armaId);
                    continue;
                }
                bool compatibleRace = arma->mRacePrimary == traits.mRace;
                for (auto r : arma->mRaces)
                    if (r == traits.mRace)
                        compatibleRace = true;
                if (compatibleRace)
                    armorAddons.push_back(arma);
            }
        };

        for (const ESM4::Armor* armor : OFClass::ESM4Npc::getEquippedArmor(mPtr))
            findArmorAddons(armor);
        if (!traits.mWornArmor.isZeroOrUnset())
        {
            if (const ESM4::Armor* armor = store->get<ESM4::Armor>().search(traits.mWornArmor))
                findArmorAddons(armor);
            else
                Log(Debug::Error) << "Worn armor not found: " << ESM::RefId(traits.mWornArmor);
        }
        if (!race->mSkin.isZeroOrUnset())
        {
            if (const ESM4::Armor* armor = store->get<ESM4::Armor>().search(race->mSkin))
                findArmorAddons(armor);
            else
                Log(Debug::Error) << "Skin not found: " << ESM::RefId(race->mSkin);
        }

        if (isFemale)
            std::sort(armorAddons.begin(), armorAddons.end(),
                [](auto x, auto y) { return x->mFemalePriority > y->mFemalePriority; });
        else
            std::sort(armorAddons.begin(), armorAddons.end(),
                [](auto x, auto y) { return x->mMalePriority > y->mMalePriority; });

        uint32_t usedParts = 0;
        for (const ESM4::ArmorAddon* arma : armorAddons)
        {
            const uint32_t covers = arma->mBodyTemplate.bodyPart;
            // if body is already covered, skip to avoid clipping
            if (covers & usedParts & ESM4::Armor::TES5_Body)
                continue;
            // if covers at least something that wasn't covered before - add model
            if (covers & ~usedParts)
            {
                usedParts |= covers;
                insertPart(isFemale ? arma->mModelFemale.getOriginal() : arma->mModelMale.getOriginal());
            }
        }

        std::set<uint32_t> usedHeadPartTypes;
        if (usedParts & ESM4::Armor::TES5_Hair)
            usedHeadPartTypes.insert(ESM4::HeadPart::Type_Hair);
        insertHeadParts(traits.mHeadParts, usedHeadPartTypes);
        insertHeadParts(isFemale ? race->mHeadPartIdsFemale : race->mHeadPartIdsMale, usedHeadPartTypes);
    }
}
