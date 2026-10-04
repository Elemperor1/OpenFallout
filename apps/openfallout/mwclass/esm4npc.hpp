#ifndef GAME_MWCLASS_ESM4ACTOR_H
#define GAME_MWCLASS_ESM4ACTOR_H

#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadnpc.hpp>

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/registeredclass.hpp"

#include "esm4base.hpp"

namespace OFClass
{
    class ESM4Npc final : public OFWorld::RegisteredClass<ESM4Npc>
    {
    public:
        ESM4Npc()
            : OFWorld::RegisteredClass<ESM4Npc>(ESM4::Npc::sRecordId)
        {
        }

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override
        {
            const OFWorld::LiveCellRef<ESM4::Npc>* ref = ptr.get<ESM4::Npc>();
            return OFWorld::Ptr(cell.insert(ref), &cell);
        }

        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override
        {
            renderingInterface.getObjects().insertNPC(ptr);
        }

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override
        {
            insertObjectPhysics(ptr, model, rotation, physics);
        }

        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override
        {
            // ESM4Impl::insertObjectPhysics(ptr, getModel(ptr), rotation, physics);
        }

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override { return true; }
        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override
        {
            return ESM4Impl::getToolTipInfo(getName(ptr), count);
        }

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;
        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;

        static const ESM4::Npc* getTraitsRecord(const OFWorld::Ptr& ptr);
        static const ESM4::Race* getRace(const OFWorld::Ptr& ptr);
        static bool isFemale(const OFWorld::Ptr& ptr);
        static const std::vector<const ESM4::Armor*>& getEquippedArmor(const OFWorld::Ptr& ptr);
        static const std::vector<const ESM4::Clothing*>& getEquippedClothing(const OFWorld::Ptr& ptr);

    private:
        static ESM4NpcCustomData& getCustomData(const OFWorld::ConstPtr& ptr);
    };
}

#endif // GAME_MWCLASS_ESM4ACTOR_H
