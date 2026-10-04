#ifndef GAME_MWCLASS_ITEMLEVLIST_H
#define GAME_MWCLASS_ITEMLEVLIST_H

#include "../mwworld/registeredclass.hpp"

namespace OFClass
{
    class ItemLevList : public OFWorld::RegisteredClass<ItemLevList>
    {
        friend OFWorld::RegisteredClass<ItemLevList>;

        ItemLevList();

    public:
        std::string_view getName(const OFWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const OFWorld::ConstPtr& ptr) const override;
        ///< @return true if this object has a tooltip when focused (default implementation: true)
    };
}

#endif
