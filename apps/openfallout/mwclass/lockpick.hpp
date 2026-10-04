#ifndef GAME_MWCLASS_LOCKPICK_H
#define GAME_MWCLASS_LOCKPICK_H

#include "../mwworld/registeredclass.hpp"

namespace ESM
{
    class RefId;
}

namespace OFClass
{
    class Lockpick : public OFWorld::RegisteredClass<Lockpick>
    {
        friend OFWorld::RegisteredClass<Lockpick>;

        Lockpick();

        OFWorld::Ptr copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const override;

    public:
        void insertObjectRendering(const OFWorld::Ptr& ptr, const std::string& model,
            OFRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool isItem(const OFWorld::ConstPtr&) const override { return true; }

        std::string_view getWerewolfRefusalSoundId() const override { return "WolfItem"; }

        std::unique_ptr<OFWorld::Action> activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const override;
        ///< Generate action for activation

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

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

        std::pair<int, std::string_view> canBeEquipped(
            const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const override;

        std::unique_ptr<OFWorld::Action> use(const OFWorld::Ptr& ptr, bool force = false) const override;
        ///< Generate action for using via inventory menu

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        bool canSell(const OFWorld::ConstPtr& item, int npcServices) const override;

        float getWeight(const OFWorld::ConstPtr& ptr) const override;

        int getItemMaxHealth(const OFWorld::ConstPtr& ptr) const override;
        ///< Return item max health or throw an exception, if class does not have item health

        bool hasItemHealth(const OFWorld::ConstPtr& ptr) const override { return true; }
        ///< \return Item health data available? (default implementation: false)
    };
}

#endif
