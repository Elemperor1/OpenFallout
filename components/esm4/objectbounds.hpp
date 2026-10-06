#ifndef OPENFALLOUT_COMPONENTS_ESM4_OBJECTBOUNDS_H
#define OPENFALLOUT_COMPONENTS_ESM4_OBJECTBOUNDS_H

#include <cstdint>

namespace ESM4
{
#pragma pack(push, 1)
    /// OBND: the box that surrounds an object, which the editor and the game use for culling.
    struct ObjectBounds
    {
        std::int16_t mX1 = 0;
        std::int16_t mY1 = 0;
        std::int16_t mZ1 = 0;
        std::int16_t mX2 = 0;
        std::int16_t mY2 = 0;
        std::int16_t mZ2 = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(ObjectBounds) == 12);
}

#endif // OPENFALLOUT_COMPONENTS_ESM4_OBJECTBOUNDS_H
