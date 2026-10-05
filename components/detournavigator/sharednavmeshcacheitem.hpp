#ifndef OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_SHAREDNAVMESHCACHEITEM_H
#define OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_SHAREDNAVMESHCACHEITEM_H

#include "guardednavmeshcacheitem.hpp"

#include <memory>

namespace DetourNavigator
{
    using SharedNavMeshCacheItem = std::shared_ptr<GuardedNavMeshCacheItem>;
}

#endif
