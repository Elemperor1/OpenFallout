#ifndef GAME_MWCLASS_CONTAINER_H
#define GAME_MWCLASS_CONTAINER_H

#include "../mwworld/containerstore.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/registeredclass.hpp"

namespace ESM
{
    struct Container;
    struct InventoryState;
}

namespace OFClass
{
    class ContainerCustomData : public OFWorld::TypedCustomData<ContainerCustomData>
    {
        OFWorld::ContainerStore mStore;

    public:
        ContainerCustomData(const ESM::Container& container, OFWorld::CellStore* cell);
        ContainerCustomData(const ESM::InventoryState& inventory);

        ContainerCustomData& asContainerCustomData() override;
        const ContainerCustomData& asContainerCustomData() const override;

        friend class Container;
    };

    class Container : public OFWorld::RegisteredClass<Container>
    {
        friend OFWorld::RegisteredClass<Container>;

        Container();

        void ensureCustomData(const OFWorld::Ptr& ptr) const;

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

        bool canBeHarvested(const OFWorld::ConstPtr& ptr) const;

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;
        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        std::string_view getWerewolfRefusalSoundId() const override { return "WolfContainer"; }

        std::unique_ptr<OFWorld::Action> activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const override;
        ///< Generate action for activation

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        OFWorld::ContainerStore& getContainerStore(const OFWorld::Ptr& ptr) const override;
        ///< Return container store

        ESM::RefId getScript(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        float getCapacity(const OFWorld::Ptr& ptr) const override;
        ///< Return total weight that fits into the object. Throws an exception, if the object can't
        /// hold other objects.

        float getEncumbrance(const OFWorld::Ptr& ptr) const override;
        ///< Returns total weight of objects inside this object (including modifications from magic
        /// effects). Throws an exception, if the object can't hold other objects.

        bool canLock(const OFWorld::ConstPtr& ptr) const override;

        void readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const override;
        ///< Read additional state from \a state into \a ptr.

        void writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const override;
        ///< Write additional state from \a ptr into \a state.

        void respawn(const OFWorld::Ptr& ptr) const override;

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        bool useAnim() const override;

        void modifyBaseInventory(const ESM::RefId& containerId, const ESM::RefId& itemId, int amount) const override;
    };
}

#endif
