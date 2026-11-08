#include "esm4npcanimation.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <set>
#include <sstream>

#include <components/esm4/loadarma.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadeyes.hpp>
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

    osg::ref_ptr<osg::Image> ESM4NpcAnimation::loadFalloutImage(const std::string& path)
    {
        if (path.empty())
            return nullptr;
        const VFS::Path::Normalized normalized(path);
        if (!mResourceSystem->getVFS()->exists(normalized))
            return nullptr;
        return mResourceSystem->getImageManager()->getImage(normalized);
    }

    namespace
    {
        // The skin of a character, as opposed to the underwear that the body model holds and the gore under it
        bool isSkinPicture(const osg::Image* image)
        {
            return image != nullptr
                && Misc::StringUtils::lowerCase(image->getFileName()).starts_with("textures/characters/");
        }
    }

    ESM4NpcAnimation::FalloutPartLook ESM4NpcAnimation::shapeFalloutPart(osg::Node& part,
        const OFClass::FalloutNpcPart& npcPart, const std::vector<float>& symmetric,
        const std::vector<float>& asymmetric, const FalloutLookImages& images)
    {
        using Kind = OFClass::FalloutPartKind;
        const Kind kind = npcPart.mKind;
        const std::string& model = npcPart.mModel;
        FalloutPartLook look;

        // The hair has the shape for a bare head and the shape for a head with a hat; the character has one
        std::string_view variant;
        if (kind == Kind::Hair)
        {
            chooseHairShape(part, images.mHat);
            variant = images.mHat ? "hat" : "nohat";
        }

        // The shape: the morphs of the file beside the model, moved by the coefficients of the face. The eyes are the
        // first vertices of their file, as the head is
        if (!symmetric.empty() || !asymmetric.empty())
        {
            std::shared_ptr<const ESM4::FaceMorphs> morphs;
            if (!variant.empty())
                morphs = getFaceMorphs(*mResourceSystem->getVFS(), model, variant);
            if (morphs == nullptr)
                morphs = getFaceMorphs(*mResourceSystem->getVFS(), model);
            if (morphs != nullptr)
            {
                look.mMorphed
                    = morphFaceMeshes(part, *morphs, symmetric, asymmetric, kind == Kind::Head || kind == Kind::Eye);
                if (look.mMorphed == 0)
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
        }

        // The picture: what the record names (for the skin that of the race, for the hair that of the hair, for the
        // eyes that of the eyes of the character), and for the skin the tint that the editor wrote for the character
        osg::ref_ptr<osg::Image> named;
        osg::ref_ptr<osg::Image> tint;
        bool average = false;
        bool skin = false;
        std::string source = npcPart.mTexture;
        switch (kind)
        {
            case Kind::Head:
                skin = true;
                tint = images.mFaceTint;
                if (tint == nullptr && !images.mTone.empty())
                {
                    // A face with no texture of the editor (the player's, made by the game itself, and any character
                    // whose files are not in the archives) gets its colour from the coefficients of FGTS
                    // (the picture has no file name, so that the skin made with it is not shared with a face that
                    // has other coefficients)
                    tint = makeFaceToneImage(*mResourceSystem->getVFS(), model, images.mTone);
                }
                else if (tint != nullptr && !images.mTone.empty())
                    reportFaceToneCheck(*mResourceSystem->getVFS(), model, images.mTone, *tint, images.mFormId);
                break;
            case Kind::Body:
                skin = true;
                tint = images.mBodyTint;
                break;
            case Kind::Hand:
                skin = true;
                tint = images.mBodyTint;
                average = true;
                break;
            case Kind::Eye:
                named = images.mEyes;
                source = images.mEyes != nullptr ? images.mEyes->getFileName() : std::string();
                break;
            case Kind::Worn:
                // The skin that shows from clothes (the arms of an outfit, the neck) has the picture of the skin of the
                // race, and gets the tint of the body like the body does; the rest of the outfit is left as it is
                skin = true;
                tint = images.mBodyTint;
                break;
            case Kind::Face:
                // The mouth, the teeth and the tongue have the picture that the race names for them
                break;
            case Kind::Hair:
            case Kind::HeadPart:
                break;
            default:
                return look;
        }
        if (kind != Kind::Eye)
            named = loadFalloutImage(npcPart.mTexture);
        if (kind == Kind::Worn && tint == nullptr)
            return look;

        // The hair, and the beard and eyebrows that go with it, have the colour of the character
        if ((kind == Kind::Hair || kind == Kind::HeadPart) && images.mHairColour.has_value())
            if (tintHair(part, *images.mHairColour) != 0)
                look.mTexture = "colour";
        if (kind == Kind::HeadPart)
            return look;

        if (named == nullptr && tint == nullptr)
            return look;

        const std::size_t replaced
            = replaceDiffuseMap(part, [&](const osg::Image* original) -> osg::ref_ptr<osg::Image> {
                  if (skin && !isSkinPicture(original))
                      return nullptr;
                  const osg::ref_ptr<osg::Image> base
                      = named != nullptr ? named : osg::ref_ptr<osg::Image>(const_cast<osg::Image*>(original));
                  if (base == nullptr)
                      return nullptr;
                  if (tint != nullptr)
                  {
                      const osg::ref_ptr<osg::Image> tinted
                          = average ? tintSkinImageByAverage(*base, *tint) : tintSkinImage(*base, *tint);
                      if (tinted != nullptr)
                          return tinted;
                  }
                  return base.get() == original ? nullptr : base;
              });
        if (replaced != 0)
        {
            look.mTexture = source + (look.mTexture.empty() ? "" : " in colour");
            if (tint != nullptr)
                look.mTexture += (average ? " + average of " : " + ")
                    + (tint->getFileName().empty() ? std::string("the face tone of its coefficients")
                                                   : tint->getFileName());
        }
        return look;
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

        const ESM4::Eyes* eyes = nullptr;
        if (!traits.mEyes.isZeroOrUnset())
        {
            eyes = store->get<ESM4::Eyes>().search(traits.mEyes);
            if (eyes == nullptr)
                Log(Debug::Error) << "Eyes not found: " << ESM::RefId(traits.mEyes);
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

        // The face of the character is that of its race for its sex, and the numbers of the record on top
        const std::vector<float> symmetric
            = ESM4::addFaceCoefficients(isFemale ? race->mSymShapeModeCoeffFemale : race->mSymShapeModeCoefficients,
                traits.mSymShapeModeCoefficients);
        const std::vector<float> asymmetric
            = ESM4::addFaceCoefficients(isFemale ? race->mAsymShapeModeCoeffFemale : race->mAsymShapeModeCoefficients,
                traits.mAsymShapeModeCoefficients);

        // The skin: textures that the editor wrote for the character. It writes them in the folder of the plugin that
        // saved the character last, so a plugin that changes the character has its own, and for a character that no
        // plugin changed it is the plugin that made it
        FalloutLookImages images;
        images.mTone
            = ESM4::addFaceCoefficients(isFemale ? race->mSymTextureModeCoeffFemale : race->mSymTextureModeCoefficients,
                traits.mSymTextureModeCoefficients);
        images.mFormId = traits.mId.mIndex;
        {
            const std::vector<std::string>& contentFiles = OFBase::Environment::get().getWorld()->getContentFiles();
            const auto pluginOf = [&](std::int32_t contentFile) {
                if (contentFile < 0 || static_cast<std::size_t>(contentFile) >= contentFiles.size())
                    return std::string();
                return Misc::StringUtils::lowerCase(std::filesystem::path(contentFiles[contentFile]).stem().string());
            };
            const ESM4::FaceTextureIndex& textures = getFaceTextures(*mResourceSystem->getVFS());
            const auto find = [&](ESM4::FaceTextureIndex::Kind kind) -> osg::ref_ptr<osg::Image> {
                const std::string* path = textures.findIn(kind, pluginOf(traits.mSourceFile), traits.mId.mIndex);
                if (path == nullptr)
                    path = textures.find(kind, pluginOf(traits.mId.mContentFile), traits.mId.mIndex);
                // A character that gets its race when the game makes it has the files of the race it got instead
                if (path == nullptr)
                    path = textures.findForRace(
                        kind, pluginOf(traits.mSourceFile), race->mId.mIndex, traits.mId.mIndex, isFemale);
                if (path == nullptr)
                    path = textures.findForRace(
                        kind, pluginOf(traits.mId.mContentFile), race->mId.mIndex, traits.mId.mIndex, isFemale);
                if (path == nullptr)
                    return nullptr;
                return mResourceSystem->getImageManager()->getImage(VFS::Path::Normalized(*path));
            };
            images.mFaceTint = find(ESM4::FaceTextureIndex::Kind::Face);
            images.mBodyTint
                = find(isFemale ? ESM4::FaceTextureIndex::Kind::BodyFemale : ESM4::FaceTextureIndex::Kind::BodyMale);
        }
        // The player (the record 7 of the master) is the one character the game makes the face of while it runs, from
        // the numbers the player chose, and so is a character whose look the screen of the face gave (it shows a
        // character of the data with the numbers of the player's look on top): a texture of the editor for such a
        // record, if the archives hold one, is the face of the record as it ships and not the one made
        if (traits.mId.mIndex == 0x7 || OFClass::ESM4Npc::isLookGiven(mPtr))
        {
            images.mFaceTint = nullptr;
            images.mBodyTint = nullptr;
        }
        if (eyes != nullptr)
            images.mEyes = loadFalloutImage(OFClass::falloutTexturePath(eyes->mIcon));

        const OFClass::FalloutNpcParts parts
            = OFClass::falloutNpcParts(*race, isFemale, hair, headParts, OFClass::ESM4Npc::getEquippedArmor(mPtr));
        images.mHat = parts.mHatOn;
        // A body with no texture of the editor gets its tone from the coefficients as the face does, from the .egt of
        // the model of its body; the hands and the skin of the clothes take the one picture (as in the games). The
        // model is the race's, not the one in the list of parts: a piece that covers the upper body takes that part
        // out of the list, and the skin that shows from the piece still has to have the tone
        if (images.mBodyTint == nullptr && !images.mTone.empty())
        {
            const std::string bodyModel = OFClass::falloutBodyModel(*race, isFemale);
            if (!bodyModel.empty())
                images.mBodyTint = makeBodyToneImage(*mResourceSystem->getVFS(), bodyModel, isFemale, images.mTone);
        }
        // A colour of black is the colour of a record that has none, which then keeps the picture it has
        if (traits.mHairColour.red != 0 || traits.mHairColour.green != 0 || traits.mHairColour.blue != 0)
            images.mHairColour = osg::Vec3f(
                traits.mHairColour.red / 255.f, traits.mHairColour.green / 255.f, traits.mHairColour.blue / 255.f);

        std::size_t inserted = 0;
        std::size_t missing = 0;
        std::size_t morphed = 0;
        std::string faceTexture;
        std::string bodyTexture;
        bool bodyShown = false;
        std::size_t wornSkin = 0;
        std::string hairTexture;
        std::string eyeTexture;
        for (const OFClass::FalloutNpcPart& npcPart : parts.mParts)
        {
            const osg::ref_ptr<osg::Node> part = insertPart(npcPart.mModel, npcPart.mForHead);
            if (part == nullptr)
            {
                ++missing;
                continue;
            }
            ++inserted;
            const FalloutPartLook look = shapeFalloutPart(*part, npcPart, symmetric, asymmetric, images);
            morphed += look.mMorphed;
            switch (npcPart.mKind)
            {
                case OFClass::FalloutPartKind::Head:
                    faceTexture = look.mTexture;
                    break;
                case OFClass::FalloutPartKind::Body:
                    bodyShown = true;
                    bodyTexture = look.mTexture;
                    break;
                case OFClass::FalloutPartKind::Worn:
                    if (!look.mTexture.empty())
                        ++wornSkin;
                    break;
                case OFClass::FalloutPartKind::Hair:
                    hairTexture = look.mTexture;
                    break;
                case OFClass::FalloutPartKind::Eye:
                    eyeTexture = look.mTexture;
                    break;
                default:
                    break;
            }
        }

        // One line for each of the first characters of a run: what the face and the body were made of, which is what
        // to read when a character looks wrong
        static std::atomic<int> sLooksTold{ 0 };
        constexpr int maxLooksTold = 150;
        const auto valueOr = [](const std::string& text, const char* otherwise) {
            return text.empty() ? std::string(otherwise) : text;
        };
        if (sLooksTold.fetch_add(1) < maxLooksTold)
        {
            // The form id is written in hexadecimal and leaves the stream in it; the numbers here are not
            std::ostringstream ref;
            ref << mPtr.getCellRef().getRefId();
            // (the stream of the log stays in the base that an earlier line left it in)
            Log(Debug::Info) << std::dec << "Look of " << ref.str() << ": " << race->mEditorId
                             << (isFemale ? " female" : " male") << ", " << inserted << " parts"
                             << (missing != 0 ? " (" + std::to_string(missing) + " not found)" : std::string())
                             << ", hair " << (hair != nullptr ? hair->mEditorId : std::string("none"))
                             << (parts.mHatOn ? " (under a hat)" : "") << ", " << morphed << " meshes morphed, face "
                             << valueOr(faceTexture, "of its model") << ", body "
                             << (bodyShown ? valueOr(bodyTexture, "of its model") : std::string("under clothes"))
                             << ", skin on clothes tinted " << wornSkin << ", hair texture "
                             << valueOr(hairTexture, "of its model") << ", eyes "
                             << valueOr(eyeTexture, "of its model");
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
