#ifndef OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_CELLGRIDBOUNDS_H
#define OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_CELLGRIDBOUNDS_H

#include <osg/Vec2i>

namespace DetourNavigator
{
    struct CellGridBounds
    {
        osg::Vec2i mCenter;
        int mHalfSize;
    };
}

#endif
