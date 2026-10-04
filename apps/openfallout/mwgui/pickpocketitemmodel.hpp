#ifndef MWGUI_PICKPOCKET_ITEM_MODEL_H
#define MWGUI_PICKPOCKET_ITEM_MODEL_H

#include "itemmodel.hpp"

namespace OFGui
{

    /// @brief The pickpocket item model randomly hides item stacks based on a specified chance. Equipped items are
    /// always hidden.
    class PickpocketItemModel : public ProxyItemModel
    {
    public:
        PickpocketItemModel(const OFWorld::Ptr& thief, std::unique_ptr<ItemModel> sourceModel, bool hideItems = true);

        bool allowedToUseItems() const override;
        ItemStack getItem(ModelIndex index) override;
        size_t getItemCount() override;
        void update() override;
        void onClose() override;
        bool onDropItem(const OFWorld::Ptr& item, int count) override;
        bool onTakeItem(const OFWorld::Ptr& item, int count) override;

    protected:
        OFWorld::Ptr mActor;
        bool mPickpocketDetected;
        bool stealItem(const OFWorld::Ptr& item, int count);

    private:
        std::vector<ItemStack> mHiddenItems;
        std::vector<ItemStack> mItems;
    };

}

#endif
