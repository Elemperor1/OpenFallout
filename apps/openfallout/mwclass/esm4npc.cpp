#include "esm4npc.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadclot.hpp>
#include <components/esm4/loadhair.hpp>
#include <components/esm4/loadhdpt.hpp>
#include <components/esm4/loadlvli.hpp>
#include <components/esm4/loadlvln.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadotft.hpp>
#include <components/esm4/loadrace.hpp>

#include <components/misc/resourcehelpers.hpp>

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/esmstore.hpp"

#include "esm4base.hpp"

namespace OFClass
{
    osg::Vec3f npcBodyHalfExtents(float raceHeight)
    {
        // A race that is a quarter as tall, or four times, is the limit of what the data has meant
        constexpr float sSmallest = 0.25f;
        constexpr float sLargest = 4.f;
        const float scale
            = std::isfinite(raceHeight) && raceHeight > 0.f ? std::clamp(raceHeight, sSmallest, sLargest) : 1.f;
        return osg::Vec3f(20.f, 20.f, 64.f) * scale;
    }

    std::vector<std::string> falloutNpcModels(const ESM4::Race& race, bool isFemale, const ESM4::Hair* hair,
        const std::vector<const ESM4::HeadPart*>& headParts, const std::vector<const ESM4::Armor*>& armor)
    {
        // The part of the body that a piece covers are its biped flags. A piece is worn if no piece before it covers
        // any of them, so that a character with two suits wears the first.
        constexpr std::uint32_t sBipedSlots = 0x000FFFFF;
        std::uint32_t covered = 0;
        std::vector<std::string> worn;
        for (const ESM4::Armor* piece : armor)
        {
            if (piece == nullptr)
                continue;
            const std::uint32_t slots = piece->mArmorFlags & sBipedSlots;
            const ESM::Path& model = isFemale && !piece->mModelFemale.empty() ? piece->mModelFemale : piece->mModelMale;
            if (slots == 0 || model.empty() || (slots & covered) != 0)
                continue;
            covered |= slots;
            worn.push_back(model.getOriginal());
        }

        std::vector<std::string> models;
        // The index of a part of the body in the race, and the biped slot that hides it: the upper body, left hand and
        // right hand (the fourth is only the texture of the upper body)
        constexpr std::array<std::uint32_t, 3> sBodySlots{ ESM4::Armor::FO3_UpperBody, ESM4::Armor::FO3_LeftHand,
            ESM4::Armor::FO3_RightHand };
        const std::vector<ESM4::Race::BodyPart>& body = isFemale ? race.mBodyPartsFemale : race.mBodyPartsMale;
        for (std::size_t i = 0; i < body.size() && i < sBodySlots.size(); ++i)
            if (!body[i].mesh.empty() && (covered & sBodySlots[i]) == 0)
                models.push_back(body[i].mesh);

        // Everything of the face goes with the head: it is not there under a piece that covers the head
        if ((covered & ESM4::Armor::FO3_Head) == 0)
        {
            for (const ESM4::Race::BodyPart& part : isFemale ? race.mHeadPartsFemale : race.mHeadParts)
                if (!part.mesh.empty())
                    models.push_back(part.mesh);
            for (const ESM4::HeadPart* part : headParts)
                if (part != nullptr && !part->mModel.empty())
                    models.push_back(part->mModel.getOriginal());
        }
        if (hair != nullptr && !hair->mModel.empty() && (covered & ESM4::Armor::FO3_Hair) == 0)
            models.push_back(hair->mModel.getOriginal());

        models.insert(models.end(), worn.begin(), worn.end());
        return models;
    }

    template <class LevelledRecord, class TargetRecord>
    static std::vector<const TargetRecord*> withBaseTemplates(
        const TargetRecord* rec, int level = OFClass::ESM4Impl::sDefaultLevel)
    {
        std::vector<const TargetRecord*> res{ rec };
        while (true)
        {
            const TargetRecord* newRec
                = OFClass::ESM4Impl::resolveLevelled<ESM4::LevelledNpc, ESM4::Npc>(rec->mBaseTemplate, level);
            if (!newRec || newRec == rec)
                return res;
            res.push_back(rec = newRec);
        }
    }

    static const ESM4::Npc* chooseTemplate(const std::vector<const ESM4::Npc*>& recs, uint16_t flag)
    {
        for (const auto* rec : recs)
        {
            if (rec->mIsTES4)
                return rec;
            else if (rec->mIsFONV)
            {
                // TODO: FO3 should use this branch as well. But it is not clear how to distinguish FO3 from
                // TES5. Currently FO3 uses wrong template flags that can lead to "ESM4 NPC traits not found"
                // exception the NPC will not be added to the scene. But in any way it shouldn't cause a crash.
                if (!(rec->mBaseConfig.fo3.templateFlags & flag))
                    return rec;
            }
            else if (rec->mIsFO4)
            {
                if (!(rec->mBaseConfig.fo4.templateFlags & flag))
                    return rec;
            }
            else if (!(rec->mBaseConfig.tes5.templateFlags & flag))
                return rec;
        }
        return nullptr;
    }

    class ESM4NpcCustomData : public OFWorld::TypedCustomData<ESM4NpcCustomData>
    {
    public:
        const ESM4::Npc* mTraits;
        const ESM4::Npc* mBaseData;
        // The record that has the skeleton of a Fallout character (the race of a Fallout character has none), if any
        const ESM4::Npc* mSkeletonRecord = nullptr;
        const ESM4::Race* mRace;
        bool mIsFemale;

        // TODO: Use InventoryStore instead (currently doesn't support ESM4 objects)
        std::vector<const ESM4::Armor*> mEquippedArmor;
        std::vector<const ESM4::Clothing*> mEquippedClothing;

        ESM4NpcCustomData& asESM4NpcCustomData() override { return *this; }
        const ESM4NpcCustomData& asESM4NpcCustomData() const override { return *this; }
    };

    ESM4NpcCustomData& ESM4Npc::getCustomData(const OFWorld::ConstPtr& ptr)
    {
        // Note: the argument is ConstPtr because this function is used in `getModel` and `getName`
        // which are virtual and work with ConstPtr. `getModel` and `getName` use custom data
        // because they require a lot of work including levelled records resolving and it would be
        // stupid to not to cache the results. Maybe we should stop using ConstPtr at all
        // to avoid such workarounds.
        OFWorld::RefData& refData = const_cast<OFWorld::RefData&>(ptr.getRefData());

        if (auto* data = refData.getCustomData())
            return data->asESM4NpcCustomData();

        auto data = std::make_unique<ESM4NpcCustomData>();

        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        const ESM4::Npc* const base = ptr.get<ESM4::Npc>()->mBase;
        auto npcRecs = withBaseTemplates<ESM4::LevelledNpc, ESM4::Npc>(base);

        data->mTraits = chooseTemplate(npcRecs, ESM4::Npc::Template_UseTraits);

        if (data->mTraits == nullptr)
            Log(Debug::Warning) << "Traits are not found for ESM4 NPC base record: \"" << base->mEditorId << "\" ("
                                << ESM::RefId(base->mId) << ")";

        data->mBaseData = chooseTemplate(npcRecs, ESM4::Npc::Template_UseBaseData);

        // The one that the template flags leave the model with, else the first of the records that has one. Fallout
        // names a marker as the model of a character that is not in the world.
        const auto hasSkeleton = [](const ESM4::Npc* rec) {
            return !rec->mModel.empty() && !ESM4Impl::isMarkerModel(rec->mModel.getNormalized().value());
        };
        if (const ESM4::Npc* rec = chooseTemplate(npcRecs, ESM4::Npc::Template_UseModel); rec && hasSkeleton(rec))
            data->mSkeletonRecord = rec;
        else if (const auto found = std::find_if(npcRecs.begin(), npcRecs.end(), hasSkeleton); found != npcRecs.end())
            data->mSkeletonRecord = *found;

        if (data->mBaseData == nullptr)
            Log(Debug::Warning) << "Base data is not found for ESM4 NPC base record: \"" << base->mEditorId << "\" ("
                                << ESM::RefId(base->mId) << ")";

        if (data->mTraits != nullptr)
        {
            data->mRace = store->get<ESM4::Race>().find(data->mTraits->mRace);
            if (data->mTraits->mIsTES4)
                data->mIsFemale = data->mTraits->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female;
            else if (data->mTraits->mIsFONV)
                data->mIsFemale = data->mTraits->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female;
            else if (data->mTraits->mIsFO4)
                data->mIsFemale
                    = data->mTraits->mBaseConfig.fo4.flags & ESM4::Npc::TES5_Female; // FO4 flags are the same as TES5
            else
                data->mIsFemale = data->mTraits->mBaseConfig.tes5.flags & ESM4::Npc::TES5_Female;
        }

        if (auto inv = chooseTemplate(npcRecs, ESM4::Npc::Template_UseInventory))
        {
            for (const ESM4::InventoryItem& item : inv->mInventory)
            {
                if (auto* armor
                    = ESM4Impl::resolveLevelled<ESM4::LevelledItem, ESM4::Armor>(ESM::FormId::fromUint32(item.item)))
                    data->mEquippedArmor.push_back(armor);
                else if (data->mTraits != nullptr && data->mTraits->mIsTES4)
                {
                    const auto* clothing = ESM4Impl::resolveLevelled<ESM4::LevelledItem, ESM4::Clothing>(
                        ESM::FormId::fromUint32(item.item));
                    if (clothing)
                        data->mEquippedClothing.push_back(clothing);
                }
            }
            if (!inv->mDefaultOutfit.isZeroOrUnset())
            {
                if (const ESM4::Outfit* outfit = store->get<ESM4::Outfit>().search(inv->mDefaultOutfit))
                {
                    for (ESM::FormId itemId : outfit->mInventory)
                        if (auto* armor = ESM4Impl::resolveLevelled<ESM4::LevelledItem, ESM4::Armor>(itemId))
                            data->mEquippedArmor.push_back(armor);
                }
                else
                    Log(Debug::Error) << "Outfit not found: " << ESM::RefId(inv->mDefaultOutfit);
            }
        }

        ESM4NpcCustomData& res = *data;
        refData.setCustomData(std::move(data));
        return res;
    }

    const std::vector<const ESM4::Armor*>& ESM4Npc::getEquippedArmor(const OFWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mEquippedArmor;
    }

    const std::vector<const ESM4::Clothing*>& ESM4Npc::getEquippedClothing(const OFWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mEquippedClothing;
    }

    const ESM4::Npc* ESM4Npc::getTraitsRecord(const OFWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mTraits;
    }

    const ESM4::Race* ESM4Npc::getRace(const OFWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mRace;
    }

    bool ESM4Npc::isFemale(const OFWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mIsFemale;
    }

    VFS::Path::NormalizedView ESM4Npc::getModel(const OFWorld::ConstPtr& ptr) const
    {
        const ESM4NpcCustomData& data = getCustomData(ptr);
        if (data.mTraits == nullptr)
            return {};
        if (data.mTraits->mIsTES4)
            return data.mTraits->mModel.getNormalized();
        const ESM::Path& raceModel = data.mIsFemale ? data.mRace->mModelFemale : data.mRace->mModelMale;
        if (!raceModel.empty())
            return raceModel.getNormalized();
        // Fallout: the skeleton is in the record of the character
        if (data.mSkeletonRecord != nullptr)
            return data.mSkeletonRecord->mModel.getNormalized();
        return {};
    }

    void ESM4Npc::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        const ESM4::Race* race = getRace(ptr);
        const float raceHeight = race == nullptr ? 1.f : (isFemale(ptr) ? race->mHeightFemale : race->mHeightMale);
        physics.addActorBody(ptr, npcBodyHalfExtents(raceHeight), rotation);
    }

    std::string_view ESM4Npc::getName(const OFWorld::ConstPtr& ptr) const
    {
        const ESM4::Npc* const baseData = getCustomData(ptr).mBaseData;
        if (baseData == nullptr)
            return {};
        return baseData->mFullName;
    }
}
