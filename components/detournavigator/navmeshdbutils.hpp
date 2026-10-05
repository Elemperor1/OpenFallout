#ifndef OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_NAVMESHDBUTILS_H
#define OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_NAVMESHDBUTILS_H

#include "navmeshdb.hpp"

#include <optional>

namespace DetourNavigator
{
    struct MeshSource;

    ShapeId resolveMeshSource(NavMeshDb& db, const MeshSource& source, ShapeId& nextShapeId);

    std::optional<ShapeId> resolveMeshSource(NavMeshDb& db, const MeshSource& source);
}

#endif
