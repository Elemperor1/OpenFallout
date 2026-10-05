#ifndef OPENFALLOUT_MWCLASS_CLASSMODEL_H
#define OPENFALLOUT_MWCLASS_CLASSMODEL_H

#include "../mwworld/livecellref.hpp"
#include "../mwworld/ptr.hpp"

#include <components/esm/path.hpp>
#include <components/vfs/pathutil.hpp>

namespace OFClass
{
    template <class Class>
    VFS::Path::NormalizedView getClassModel(const OFWorld::ConstPtr& ptr)
    {
        const OFWorld::LiveCellRef<Class>* ref = ptr.get<Class>();
        return ref->mBase->mModel.getNormalized();
    }
}

#endif
