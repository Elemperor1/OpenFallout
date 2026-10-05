#include "ptr.hpp"

#include "apps/openfallout/mwbase/environment.hpp"

#include "worldmodel.hpp"

namespace OFWorld
{
    SafePtr::SafePtr(const Ptr& ptr)
        : mId(ptr.getCellRef().getRefNum())
        , mPtr(ptr)
        , mLastUpdate(OFBase::Environment::get().getWorldModel()->getPtrRegistryRevision())
    {
    }

    std::string SafePtr::toString() const
    {
        update();
        if (mPtr.isEmpty())
            return "object" + mId.toString() + " (not found)";
        else
            return mPtr.toString();
    }

    void SafePtr::update() const
    {
        const WorldModel& worldModel = *OFBase::Environment::get().getWorldModel();
        if (mLastUpdate != worldModel.getPtrRegistryRevision())
        {
            mPtr = worldModel.getPtr(mId);
            mLastUpdate = worldModel.getPtrRegistryRevision();
        }
    }
}
