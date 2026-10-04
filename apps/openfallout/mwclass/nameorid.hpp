#ifndef OPENFALLOUT_MWCLASS_NAMEORID_H
#define OPENFALLOUT_MWCLASS_NAMEORID_H

#include <components/esm/refid.hpp>

#include "../mwworld/livecellref.hpp"
#include "../mwworld/ptr.hpp"

#include <string_view>

namespace OFClass
{
    template <class Class>
    std::string_view getNameOrId(const OFWorld::ConstPtr& ptr)
    {
        const OFWorld::LiveCellRef<Class>* ref = ptr.get<Class>();
        if (!ref->mBase->mName.empty())
            return ref->mBase->mName;
        if (const auto* id = ref->mBase->mId.template getIf<ESM::StringRefId>())
            return id->getValue();
        return {};
    }
}

#endif
