#include "furniture.hpp"

#include <cmath>

namespace FalloutScript
{
    namespace
    {
        /// The way the rotation of a reference turns what is in the space of its model: about x first, then y, then z,
        /// each clockwise seen from the end of its axis
        Point turned(const Point& point, const float (&rotation)[3])
        {
            Point result = point;
            {
                const float c = std::cos(rotation[0]);
                const float s = std::sin(rotation[0]);
                const Point from = result;
                result.y = from.y * c + from.z * s;
                result.z = -from.y * s + from.z * c;
            }
            {
                const float c = std::cos(rotation[1]);
                const float s = std::sin(rotation[1]);
                const Point from = result;
                result.x = from.x * c - from.z * s;
                result.z = from.x * s + from.z * c;
            }
            {
                const float c = std::cos(rotation[2]);
                const float s = std::sin(rotation[2]);
                const Point from = result;
                result.x = from.x * c + from.y * s;
                result.y = -from.x * s + from.y * c;
            }
            return result;
        }

        float distanceSquared(const Point& a, const Point& b)
        {
            const float x = a.x - b.x;
            const float y = a.y - b.y;
            const float z = a.z - b.z;
            return x * x + y * y + z * z;
        }
    }

    bool furnitureEntryEnabled(std::uint32_t flags, std::size_t index)
    {
        return index < maxFurnitureEntries && ((flags >> index) & 1u) != 0;
    }

    FurnitureSeat furnitureSeat(const FurnitureEntry& entry, std::size_t index, const FurniturePlacement& placement)
    {
        const Point scaled{ entry.mOffset.x * placement.mScale, entry.mOffset.y * placement.mScale,
            entry.mOffset.z * placement.mScale };
        const Point offset = turned(scaled, placement.mRotation);
        FurnitureSeat seat;
        seat.mPosition
            = { placement.mPosition.x + offset.x, placement.mPosition.y + offset.y, placement.mPosition.z + offset.z };
        seat.mHeading = placement.mRotation[2] + entry.mHeading;
        seat.mRef = entry.mRef;
        seat.mIndex = index;
        return seat;
    }

    std::optional<FurnitureSeat> chooseFurnitureSeat(const std::vector<FurnitureEntry>& entries, std::uint32_t flags,
        std::uint32_t used, const FurniturePlacement& placement, const Point& from)
    {
        std::optional<FurnitureSeat> best;
        float bestDistance = 0;
        for (std::size_t i = 0; i < entries.size() && i < maxFurnitureEntries; ++i)
        {
            if (!furnitureEntryEnabled(flags, i) || ((used >> i) & 1u) != 0)
                continue;
            const FurnitureSeat seat = furnitureSeat(entries[i], i, placement);
            const float distance = distanceSquared(seat.mPosition, from);
            if (!best || distance < bestDistance)
            {
                best = seat;
                bestDistance = distance;
            }
        }
        return best;
    }
}
