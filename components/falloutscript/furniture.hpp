#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FURNITURE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FURNITURE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "triggervolumes.hpp"

namespace FalloutScript
{
    /// An entry of the furniture marker of a model (the BSFurnitureMarker of its nif): where a character stands to use
    /// the furniture
    struct FurnitureEntry
    {
        /// In the space of the model, at the level of the floor
        Point mOffset;
        /// In radians, turned like the rotation of a reference (clockwise seen from above)
        float mHeading = 0;
        /// The kind of use: 1 to 6 sleep, 10 to 20 sit, 22 and more other
        std::uint8_t mRef = 0;
    };

    /// Where a reference of the furniture stands: its position, its rotation as the file has it (radians, the x, y and
    /// z of the reference) and its scale
    struct FurniturePlacement
    {
        Point mPosition;
        float mRotation[3] = { 0, 0, 0 };
        float mScale = 1;
    };

    /// An entry in the world
    struct FurnitureSeat
    {
        Point mPosition;
        /// The rotation of the reference about z that a character at the entry has
        float mHeading = 0;
        std::uint8_t mRef = 0;
        std::size_t mIndex = 0;
    };

    /// The record of a furniture says which of the entries of its model can be used, one bit for each, and only the
    /// first thirty are numbered
    constexpr std::size_t maxFurnitureEntries = 30;

    bool furnitureEntryEnabled(std::uint32_t flags, std::size_t index);

    /// An entry of the model of a reference: the offset turned and scaled like the reference, from its position, with
    /// the heading of the entry added to that of the reference. This is what the games do to the entry before a
    /// character is put on it.
    FurnitureSeat furnitureSeat(const FurnitureEntry& entry, std::size_t index, const FurniturePlacement& placement);

    /// The entry that a character at `from` uses: the nearest of those the record enables that are not marked in
    /// `used` (one bit for each entry). None when there is no such entry.
    std::optional<FurnitureSeat> chooseFurnitureSeat(const std::vector<FurnitureEntry>& entries, std::uint32_t flags,
        std::uint32_t used, const FurniturePlacement& placement, const Point& from);
}

#endif
