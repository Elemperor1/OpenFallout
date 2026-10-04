#ifndef GAME_MWCLASS_LIGHT_H
#define GAME_MWCLASS_LIGHT_H

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{
    class Light : public OFWorld::RegisteredClass<Light>
    {
        friend OFWorld::RegisteredClass<Light>;

        Light();

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        void insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;
        void insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
            OFPhysics::PhysicsSystem& physics) const override;

        bool useAnim() const override;

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        bool showsInInventory(const OFWorld::ConstPtr& ptr) const override;

        bool isItem(const OFWorld::ConstPtr&) const override;

        std::string_view getWerewolfRefusalSoundId() const override { return "WolfItem"; }

        std::unique_ptr<OFWorld::Action> activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const override;
        ///< Generate action for activation

        ESM::RefId getScript(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        std::pair<std::vector<int>, bool> getEquipmentSlots(const OFWorld::ConstPtr& ptr) const override;
        ///< \return first: Return IDs of the slot this object can be equipped in; second: can object
        /// stay stacked when equipped?

        int getValue(const OFWorld::ConstPtr& ptr) const override;
        ///< Return trade value of the object. Throws an exception, if the object can't be traded.

        const ESM::RefId& getUpSoundId(const OFWorld::ConstPtr& ptr) const override;
        ///< Return the pick up sound Id

        const ESM::RefId& getDownSoundId(const OFWorld::ConstPtr& ptr) const override;
        ///< Return the put down sound Id

        VFS::Path::NormalizedView getInventoryIcon(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of inventory icon.

        std::unique_ptr<OFWorld::Action> use(const OFWorld::Ptr& ptr, bool force = false) const override;
        ///< Generate action for using via inventory menu

        void setRemainingUsageTime(const OFWorld::Ptr& ptr, float duration) const override;
        ///< Sets the remaining duration of the object.

        float getRemainingUsageTime(const OFWorld::ConstPtr& ptr) const override;
        ///< Returns the remaining duration of the object.

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        float getWeight(const OFWorld::ConstPtr& ptr) const override;

        bool canSell(const OFWorld::ConstPtr& item, int npcServices) const override;

        std::pair<int, std::string_view> canBeEquipped(
            const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const override;

        ESM::RefId getSound(const OFWorld::ConstPtr& ptr) const override;
    };

}

#endif
