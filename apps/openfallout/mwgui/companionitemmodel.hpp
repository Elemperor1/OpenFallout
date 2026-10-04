#ifndef MWGUI_COMPANION_ITEM_MODEL_H
#define MWGUI_COMPANION_ITEM_MODEL_H

#include "inventoryitemmodel.hpp"

namespace OFGui
{

    /// @brief The companion item model keeps track of the companion's profit by
    /// monitoring which items are being added to and removed from the model.
    class CompanionItemModel : public InventoryItemModel
    {
    public:
        CompanionItemModel(const OFWorld::Ptr& actor);

        bool hasProfit(const OFWorld::Ptr& actor);

    protected:
        OFWorld::Ptr addItem(const ItemStack& item, size_t count, bool allowAutoEquip = true) override;
        OFWorld::Ptr copyItem(const ItemStack& item, size_t count, bool allowAutoEquip = true) override;
        void removeItem(const ItemStack& item, size_t count) override;
    };

}

#endif
