#include "furniture.hpp"

#include <cmath>
#include <string_view>

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

    FurnitureDeltas FurnitureDeltas::fromGameSettings(const std::map<std::string, double>& settings)
    {
        constexpr std::string_view prefix = "ffurnituremarker";
        FurnitureDeltas result;
        for (auto it = settings.lower_bound(std::string(prefix)); it != settings.end(); ++it)
        {
            const std::string_view name = it->first;
            if (!name.starts_with(prefix))
                break;
            std::size_t i = prefix.size();
            int ref = 0;
            std::size_t digits = 0;
            while (i < name.size() && name[i] >= '0' && name[i] <= '9' && digits < 4)
            {
                ref = ref * 10 + (name[i] - '0');
                ++i;
                ++digits;
            }
            if (digits == 0 || digits > 3 || ref > 255)
                continue;
            const std::string_view rest = name.substr(i);
            const auto value = static_cast<float>(it->second);
            // A setting of the plugin that is not a number, or too large for one, moves no one
            if (!std::isfinite(value))
                continue;
            FurnitureDelta delta = result.of(static_cast<std::uint8_t>(ref));
            if (rest.find("head") != std::string_view::npos || rest.find("angle") != std::string_view::npos)
                delta.mHeading = value;
            else if (rest.ends_with('x'))
                delta.mX = value;
            else if (rest.ends_with('y'))
                delta.mY = value;
            else if (rest.ends_with('z'))
                delta.mZ = value;
            else
                continue;
            result.mDeltas[static_cast<std::uint8_t>(ref)] = delta;
        }
        return result;
    }

    FurnitureDelta FurnitureDeltas::of(std::uint8_t ref) const
    {
        const auto found = mDeltas.find(ref);
        return found != mDeltas.end() ? found->second : FurnitureDelta{};
    }

    FurnitureSeat settledSeat(const FurnitureSeat& entry, const FurnitureDelta& delta, float scale)
    {
        // Like the offset of an entry: clockwise seen from above, by the heading the character has at the entry
        const float c = std::cos(entry.mHeading);
        const float s = std::sin(entry.mHeading);
        FurnitureSeat seat = entry;
        seat.mPosition.x += delta.mX * c + delta.mY * s;
        seat.mPosition.y += -delta.mX * s + delta.mY * c;
        seat.mPosition.z += (scale - 1.f) * delta.mZ;
        seat.mHeading = entry.mHeading + delta.mHeading;
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
