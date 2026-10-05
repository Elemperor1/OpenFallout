#ifndef OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_BOUNDS_H
#define OPENFALLOUT_COMPONENTS_DETOURNAVIGATOR_BOUNDS_H

#include <osg/Vec3f>

namespace DetourNavigator
{
    struct Bounds
    {
        osg::Vec3f mMin;
        osg::Vec3f mMax;
    };

    inline bool isEmpty(const Bounds& value)
    {
        return value.mMin == value.mMax;
    }
}

#endif
