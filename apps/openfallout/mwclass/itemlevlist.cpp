#include "itemlevlist.hpp"

#include <components/esm3/loadlevlist.hpp>

namespace OFClass
{
    ItemLevList::ItemLevList()
        : OFWorld::RegisteredClass<ItemLevList>(ESM::ItemLevList::sRecordId)
    {
    }

    std::string_view ItemLevList::getName(const OFWorld::ConstPtr& ptr) const
    {
        return {};
    }

    bool ItemLevList::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        return false;
    }
}
