#include "esm4npcanimation.hpp"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <set>

#include <components/esm4/loadarma.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrace.hpp>

#include <components/debug/debuglog.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/imagemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"

#include "falloutface.hpp"

namespace OFRender
{
    ESM4NpcAnimation::ESM4NpcAnimation(
        const OFWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem)
        : FalloutActorAnimation(ptr, std::move(parentNode), resourceSystem)
    {
        setObjectRoot(mPtr.getClass().getCorrectedModel(mPtr), true, true, false);
        updateParts();

        // Only the characters of Fallout have animation files that are read
        const ESM4::Npc* traits = OFClass::ESM4Npc::getTraitsRecord(mPtr);
        if (traits != nullptr && !traits->mIsTES4 && traits->mIsFONV)
            startAnimations(OFClass::ESM4Npc::isFemale(mPtr));
    }

    ESM4NpcAnimation::~ESM4NpcAnimation() = default;

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

        // The skin: a texture that the editor wrote for the character. It writes it in the folder of the plugin that
        // saved the character last, so a plugin that changes the character has its own, and for a character that no
        // plugin changed it is the plugin that made it
        if (!isHead && !isBody)
            return;
        const std::vector<std::string>& contentFiles = OFBase::Environment::get().getWorld()->getContentFiles();
        const auto pluginOf = [&](std::int32_t contentFile) {
            if (contentFile < 0 || static_cast<std::size_t>(contentFile) >= contentFiles.size())
                return std::string();
            return Misc::StringUtils::lowerCase(std::filesystem::path(contentFiles[contentFile]).stem().string());
        };
        const ESM4::FaceTextureIndex& textures = getFaceTextures(*mResourceSystem->getVFS());
        const ESM4::FaceTextureIndex::Kind kind = isHead ? ESM4::FaceTextureIndex::Kind::Face
            : OFClass::ESM4Npc::isFemale(mPtr)           ? ESM4::FaceTextureIndex::Kind::BodyFemale
                                                         : ESM4::FaceTextureIndex::Kind::BodyMale;
        const std::string* path = textures.findIn(kind, pluginOf(traits.mSourceFile), traits.mId.mIndex);
        if (path == nullptr)
            path = textures.find(kind, pluginOf(traits.mId.mContentFile), traits.mId.mIndex);
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
