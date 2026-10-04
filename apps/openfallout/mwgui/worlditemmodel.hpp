#ifndef OPENFALLOUT_APPS_OPENFALLOUT_MWGUI_WORLDITEMMODEL_H
#define OPENFALLOUT_APPS_OPENFALLOUT_MWGUI_WORLDITEMMODEL_H

#include "itemmodel.hpp"

#include <apps/openfallout/mwbase/environment.hpp>
#include <apps/openfallout/mwbase/world.hpp>

#include <components/esm/refid.hpp>

#include <MyGUI_InputManager.h>
#include <MyGUI_RenderManager.h>

#include <stdexcept>

namespace OFGui
{
    // Makes it possible to use ItemModel::moveItem to move an item from an inventory to the world.
    class WorldItemModel : public ItemModel
    {
        OFWorld::Ptr dropItemImpl(const ItemStack& item, int count, bool copy)
        {
            OFBase::World& world = *OFBase::Environment::get().getWorld();

            const OFWorld::Ptr player = world.getPlayerPtr();

            world.breakInvisibility(player);

            const OFWorld::Ptr dropped = world.canPlaceObject(mCursorX, mCursorY)
                ? world.placeObject(item.mBase, mCursorX, mCursorY, count, copy)
                : world.dropObjectOnGround(player, item.mBase, count, copy);

            dropped.getCellRef().setOwner(ESM::RefId());

            return dropped;
        }

    public:
        explicit WorldItemModel(float cursorX, float cursorY)
            : mCursorX(cursorX)
            , mCursorY(cursorY)
        {
        }

        ModelIndex getIndex(const ItemStack& /*item*/) override
        {
            throw std::runtime_error("WorldItemModel::getIndex is not implemented");
        }

        void update() override {}

        size_t getItemCount() override { return 0; }

        ItemStack getItem(ModelIndex /*index*/) override
        {
            throw std::runtime_error("WorldItemModel::getItem is not implemented");
        }

        bool usesContainer(const OFWorld::Ptr&) override { return false; }

    protected:
        OFWorld::Ptr addItem(const ItemStack& item, size_t count, bool /*allowAutoEquip*/) override
        {
            const int prevCount = item.mBase.getCellRef().getCount(false);
            const int intCount = static_cast<int>(count);
            item.mBase.getCellRef().setCount(intCount);
            OFWorld::Ptr ptr = dropItemImpl(item, intCount, false);
            item.mBase.getCellRef().setCount(prevCount);
            return ptr;
        }

        OFWorld::Ptr copyItem(const ItemStack& item, size_t count, bool /*allowAutoEquip*/) override
        {
            return dropItemImpl(item, static_cast<int>(count), true);
        }

        void removeItem(const ItemStack& /*item*/, size_t /*count*/) override
        {
            throw std::runtime_error("WorldItemModel::removeItem is not implemented");
        }

    private:
        float mCursorX;
        float mCursorY;
    };
}

#endif
