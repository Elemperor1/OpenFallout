#ifndef MWGUI_INVENTORY_ITEM_MODEL_H
#define MWGUI_INVENTORY_ITEM_MODEL_H

#include "itemmodel.hpp"

namespace OFGui
{

    class InventoryItemModel : public ItemModel
    {
    public:
        InventoryItemModel(const OFWorld::Ptr& actor);

        ItemStack getItem(ModelIndex index) override;
        ModelIndex getIndex(const ItemStack& item) override;
        size_t getItemCount() override;

        bool onTakeItem(const OFWorld::Ptr& item, int count) override;

        /// Move items from this model to \a otherModel.
        OFWorld::Ptr moveItem(
            const ItemStack& item, size_t count, ItemModel* otherModel, bool allowAutoEquip = true) override;

        void update() override;

        bool usesContainer(const OFWorld::Ptr& container) override;

    protected:
        OFWorld::Ptr addItem(const ItemStack& item, size_t count, bool allowAutoEquip = true) override;
        OFWorld::Ptr copyItem(const ItemStack& item, size_t count, bool allowAutoEquip = true) override;
        void removeItem(const ItemStack& item, size_t count) override;

        OFWorld::Ptr mActor;

    private:
        std::vector<ItemStack> mItems;
    };

}

#endif
