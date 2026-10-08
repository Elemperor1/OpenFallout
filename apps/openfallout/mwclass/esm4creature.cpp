#include "esm4creature.hpp"

#include <algorithm>
#include <cmath>

#include <components/debug/debuglog.hpp>

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFClass
{
    osg::Vec3f creatureBodyHalfExtents(const ESM4::Creature& creature)
    {
        // The size of a person's body, for a creature that does not say; and the limits of what the data has meant
        const osg::Vec3f sUnknown(20.f, 20.f, 64.f);
        constexpr float sSmallestRadius = 8.f;
        constexpr float sLargestRadius = 160.f;
        constexpr float sSmallestHeight = 8.f;
        constexpr float sLargestHeight = 250.f;

        if (!creature.mHasBounds)
            return sUnknown;
        const ESM4::ObjectBounds& box = creature.mBounds;
        const float width = static_cast<float>(std::min(std::abs(box.mX2 - box.mX1), std::abs(box.mY2 - box.mY1)));
        const float height = static_cast<float>(std::abs(box.mZ2 - box.mZ1));
        if (width <= 0.f || height <= 0.f)
            return sUnknown;
        const float radius = std::clamp(width / 2.f, sSmallestRadius, sLargestRadius);
        return osg::Vec3f(radius, radius, std::clamp(height / 2.f, sSmallestHeight, sLargestHeight));
    }

    namespace
    {
        /// The creatures of the store, for the templates of a creature
        class StoreCreatureSource final : public ESM4::CreatureSource
        {
        public:
            explicit StoreCreatureSource(const OFWorld::ESMStore& store)
                : mStore(store)
            {
            }

            const ESM4::Creature* findCreature(ESM::FormId id) const override
            {
                return mStore.get<ESM4::Creature>().search(id);
            }

            const ESM4::LevelledCreature* findLevelledCreature(ESM::FormId id) const override
            {
                return mStore.get<ESM4::LevelledCreature>().search(id);
            }

        private:
            const OFWorld::ESMStore& mStore;
        };
    }

    class ESM4CreatureCustomData : public OFWorld::TypedCustomData<ESM4CreatureCustomData>
    {
    public:
        ESM4::CreatureModel mModel;
        /// The record that has the name of the creature, if any has one
        const ESM4::Creature* mBaseData = nullptr;

        ESM4CreatureCustomData& asESM4CreatureCustomData() override { return *this; }
        const ESM4CreatureCustomData& asESM4CreatureCustomData() const override { return *this; }
    };

    ESM4CreatureCustomData& ESM4Creature::getCustomData(const OFWorld::ConstPtr& ptr)
    {
        // Note: the argument is ConstPtr because this function is used in `getModel` and `getName`, which are virtual
        // and work with ConstPtr, and they need the results of the templates and levelled lists, which are cached
        OFWorld::RefData& refData = const_cast<OFWorld::RefData&>(ptr.getRefData());

        if (auto* data = refData.getCustomData())
            return data->asESM4CreatureCustomData();

        auto data = std::make_unique<ESM4CreatureCustomData>();

        const OFWorld::ESMStore* store = OFBase::Environment::get().getESMStore();
        const ESM4::Creature& base = *ptr.get<ESM4::Creature>()->mBase;
        const StoreCreatureSource source(*store);
        const std::vector<const ESM4::Creature*> chain
            = ESM4::creatureTemplateChain(source, base, ESM4Impl::sDefaultLevel, ESM4Impl::actorSeed(ptr, base.mId));

        data->mModel = ESM4::creatureModel(chain);
        // Fallout names a marker as the model of a creature that is not in the world
        if (ESM4Impl::isMarkerModel(data->mModel.mSkeleton))
            data->mModel = {};
        if (data->mModel.mOwner == nullptr)
            Log(Debug::Verbose) << "No model is found for ESM4 creature base record: \"" << base.mEditorId << "\" ("
                                << ESM::RefId(base.mId) << ")";

        data->mBaseData = ESM4::creatureTemplateOwner(chain, ESM4::Creature::Template_UseBaseData);

        ESM4CreatureCustomData& res = *data;
        refData.setCustomData(std::move(data));
        return res;
    }

    const ESM4::CreatureModel& ESM4Creature::getCreatureModel(const OFWorld::Ptr& ptr)
    {
        return getCustomData(ptr).mModel;
    }

    VFS::Path::NormalizedView ESM4Creature::getModel(const OFWorld::ConstPtr& ptr) const
    {
        const ESM4CreatureCustomData& data = getCustomData(ptr);
        if (data.mModel.mOwner == nullptr)
            return {};
        return data.mModel.mOwner->mModel.getNormalized();
    }

    std::string_view ESM4Creature::getName(const OFWorld::ConstPtr& ptr) const
    {
        const ESM4::Creature* baseData = getCustomData(ptr).mBaseData;
        return baseData == nullptr ? std::string_view() : std::string_view(baseData->mFullName);
    }

    void ESM4Creature::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
            renderingInterface.getObjects().insertESM4Creature(ptr);
    }

    void ESM4Creature::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        // The box is the one of the record that has the model
        const ESM4::Creature* boxOwner = getCustomData(ptr).mModel.mOwner;
        if (boxOwner == nullptr)
            boxOwner = ptr.get<ESM4::Creature>()->mBase;
        physics.addActorBody(ptr, creatureBodyHalfExtents(*boxOwner), rotation);
    }
}
