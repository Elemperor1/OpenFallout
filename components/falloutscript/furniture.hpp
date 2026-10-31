#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FURNITURE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_FURNITURE_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
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

    /// How far the games move a character from the entry of a marker to where it stays when it has sat down or lain
    /// down: the net movement of the animation that carries it onto the furniture (the settings
    /// fFurnitureMarker<ref>Delta... of the files, for the kind of use the entry says). The marker is on the floor
    /// where the character stands to begin; the seat is a step away from it.
    struct FurnitureDelta
    {
        /// In the space of the character at the entry, x to its right and y to its front
        float mX = 0;
        float mY = 0;
        float mZ = 0;
        /// The turn in radians, added to the heading of the entry
        float mHeading = 0;
    };

    /// The deltas by the entry ref (the byte of the marker)
    class FurnitureDeltas
    {
    public:
        /// Reads the settings of the files named fFurnitureMarker<ref><something that ends in X, Y or Z, or says
        /// heading>, by their names in lower case. The ones that are not there are zero, as in the exe.
        static FurnitureDeltas fromGameSettings(const std::map<std::string, double>& settings);

        FurnitureDelta of(std::uint8_t ref) const;

        /// How many refs have a setting
        std::size_t size() const { return mDeltas.size(); }

    private:
        std::map<std::uint8_t, FurnitureDelta> mDeltas;
    };

    /// Where a character stays once it has settled on the furniture that it began to use at `entry`: the entry moved
    /// by the delta, turned like the entry (the same turn as the reference of a furniture has), the vertical part
    /// scaled by the size of the reference less one as the exe does, and the heading of the delta added.
    FurnitureSeat settledSeat(const FurnitureSeat& entry, const FurnitureDelta& delta, float scale);

    /// The entry that a character at `from` uses: the nearest of those the record enables that are not marked in
    /// `used` (one bit for each entry). None when there is no such entry.
    std::optional<FurnitureSeat> chooseFurnitureSeat(const std::vector<FurnitureEntry>& entries, std::uint32_t flags,
        std::uint32_t used, const FurniturePlacement& placement, const Point& from);
}

#endif
