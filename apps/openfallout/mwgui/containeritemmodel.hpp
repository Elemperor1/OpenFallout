#ifndef MWGUI_CONTAINER_ITEM_MODEL_H
#define MWGUI_CONTAINER_ITEM_MODEL_H

#include <utility>
#include <vector>

#include "itemmodel.hpp"

#include "../mwworld/containerstore.hpp"

namespace OFGui
{

    /// @brief The container item model supports multiple item sources, which are needed for
    /// making NPCs sell items from containers owned by them
    class ContainerItemModel : public ItemModel
    {
    public:
        ContainerItemModel(const std::vector<OFWorld::Ptr>& itemSources, const std::vector<OFWorld::Ptr>& worldItems);
        ///< @note The order of elements \a itemSources matters here. The first element has the highest priority for
        ///< removal,
        ///  while the last element will be used to add new items to.

        ContainerItemModel(const OFWorld::Ptr& source);

        bool allowedToUseItems() const override;

        bool onDropItem(const OFWorld::Ptr& item, int count) override;
        bool onTakeItem(const OFWorld::Ptr& item, int count) override;

        ItemStack getItem(ModelIndex index) override;
        ModelIndex getIndex(const ItemStack& item) override;
        size_t getItemCount() override;

        void update() override;

        bool usesContainer(const OFWorld::Ptr& container) override;

    protected:
        OFWorld::Ptr addItem(const ItemStack& item, size_t count, bool allowAutoEquip = true) override;
        OFWorld::Ptr copyItem(const ItemStack& item, size_t count, bool allowAutoEquip = true) override;
        void removeItem(const ItemStack& item, size_t count) override;

    private:
        std::vector<std::pair<OFWorld::Ptr, OFWorld::ResolutionHandle>> mItemSources;
        std::vector<OFWorld::Ptr> mWorldItems;
        const bool mTrading;
        std::vector<ItemStack> mItems;
    };

}

#endif
