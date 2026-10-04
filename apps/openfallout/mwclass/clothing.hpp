#ifndef GAME_MWCLASS_CLOTHING_H
#define GAME_MWCLASS_CLOTHING_H

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{
    class Clothing : public OFWorld::RegisteredClass<Clothing>
    {
        friend OFWorld::RegisteredClass<Clothing>;

        Clothing();

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

        ESM::RefId getScript(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        std::pair<std::vector<int>, bool> getEquipmentSlots(const OFWorld::ConstPtr& ptr) const override;
        ///< \return first: Return IDs of the slot this object can be equipped in; second: can object
        /// stay stacked when equipped?

        ESM::RefId getEquipmentSkill(const OFWorld::ConstPtr& ptr, bool useLuaInterfaceIfAvailable) const override;

        OFGui::ToolTipInfo getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        int getValue(const OFWorld::ConstPtr& ptr) const override;
        ///< Return trade value of the object. Throws an exception, if the object can't be traded.

        const ESM::RefId& getUpSoundId(const OFWorld::ConstPtr& ptr) const override;
        ///< Return the pick up sound Id

        const ESM::RefId& getDownSoundId(const OFWorld::ConstPtr& ptr) const override;
        ///< Return the put down sound Id

        VFS::Path::NormalizedView getInventoryIcon(const OFWorld::ConstPtr& ptr) const override;
        ///< Return name of inventory icon.

        ESM::RefId getEnchantment(const OFWorld::ConstPtr& ptr) const override;
        ///< @return the enchantment ID if the object is enchanted, otherwise an empty string

        const ESM::RefId& applyEnchantment(const OFWorld::ConstPtr& ptr, const ESM::RefId& enchId, int enchCharge,
            const std::string& newName) const override;
        ///< Creates a new record using \a ptr as template, with the given name and the given enchantment applied to it.

        std::pair<int, std::string_view> canBeEquipped(
            const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const override;
        ///< Return 0 if player cannot equip item. 1 if can equip. 2 if it's twohanded weapon. 3 if twohanded weapon
        ///< conflicts with that.
        ///  Second item in the pair specifies the error message

        std::unique_ptr<OFWorld::Action> use(const OFWorld::Ptr& ptr, bool force = false) const override;
        ///< Generate action for using via inventory menu

        VFS::Path::NormalizedView getModel(const OFWorld::ConstPtr& ptr) const override;

        int getEnchantmentPoints(const OFWorld::ConstPtr& ptr) const override;

        float getWeight(const OFWorld::ConstPtr& ptr) const override;

        bool canSell(const OFWorld::ConstPtr& item, int npcServices) const override;
    };
}

#endif
