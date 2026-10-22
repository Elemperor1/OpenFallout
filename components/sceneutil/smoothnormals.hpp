#ifndef OPENFALLOUT_COMPONENTS_SCENEUTIL_SMOOTHNORMALS_H
#define OPENFALLOUT_COMPONENTS_SCENEUTIL_SMOOTHNORMALS_H

namespace osg
{
    class Geometry;
    class Node;
}

namespace SceneUtil
{
    /// Gives a geometry of triangles that has no normals smooth ones: the normal of a vertex is the mean of the
    /// normals of the triangles around it, the larger triangles counting more.
    /// @param skirtHeight when above 0, a triangle whose points are further apart than that in height is a skirt
    /// that hangs under the edge of a piece of land to hide the cracks, and does not count for the vertices it shares
    /// with the land.
    /// @return whether normals were added
    bool generateSmoothNormals(osg::Geometry& geometry, float skirtHeight = 0.f);

    /// Does that for every geometry below the node that has no normals.
    void generateMissingNormals(osg::Node& node, float skirtHeight = 0.f);
}

#endif
