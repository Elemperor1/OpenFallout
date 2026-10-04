#ifndef GAME_MWCLASS_DOOR_H
#define GAME_MWCLASS_DOOR_H

#include <components/esm3/loaddoor.hpp>

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{
    class Door : public OFWorld::RegisteredClass<Door>
    {
        friend OFWorld::RegisteredClass<Door>;

        Door();

        void ensureCustomData(const OFWorld::Ptr& ptr) const;

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;
        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        bool isDoor() const override;

        bool useAnim() const override;

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        std::unique_ptr<OFWorld::Action> activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const override;
        ///< Generate action for activation

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        static std::string getDestination(const OFWorld::LiveCellRef<ESM::Door>& door);
        ///< @return destination cell name or token

        bool canLock(const OFWorld::ConstPtr& ptr) const override;

        bool allowTelekinesis(const OFWorld::ConstPtr& ptr) const override;
        ///< Return whether this class of object can be activated with telekinesis

        ESM::RefId getScript(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        OFWorld::DoorState getDoorState(const OFWorld::ConstPtr& ptr) const override;
        /// This does not actually cause the door to move. Use World::activateDoor instead.
        void setDoorState(const OFWorld::Ptr& ptr, OFWorld::DoorState state) const override;

        void readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const override;
        ///< Read additional state from \a state into \a ptr.

        void writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const override;
        ///< Write additional state from \a ptr into \a state.
    };
}

#endif
