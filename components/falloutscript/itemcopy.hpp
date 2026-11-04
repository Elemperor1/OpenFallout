#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ITEMCOPY_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ITEMCOPY_H

namespace FalloutScript
{
    /// A list shows a copy of a weapon or armor by one number: its condition (tenths of a percent of its health, 0 to
    /// 1000) with the mask of the mods installed in it over the ten low bits (a copy with no mods is its condition). A
    /// negative number names no copy (the best one, or the one that is on).
    constexpr int copyOf(int condition, int mods)
    {
        return condition | (mods << 10);
    }
    constexpr int copyCondition(int copy)
    {
        return copy < 0 ? copy : copy & 1023;
    }
    constexpr int copyMods(int copy)
    {
        return copy < 0 ? 0 : copy >> 10;
    }
}

#endif
