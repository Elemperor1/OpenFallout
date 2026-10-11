#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLACEMENTCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLACEMENTCOMMANDS_H

#include <array>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// Where a reference stands: its position in game units and its angles around the x, y and z axes in radians
    struct Placement
    {
        std::array<double, 3> mPosition{};
        std::array<double, 3> mAngle{};
    };

    /// What the commands that move things ask of the game world. A reference is told by its form id; the host knows
    /// which of them are in loaded cells and where the others are by their records.
    class PlacementWorld
    {
    public:
        virtual ~PlacementWorld() = default;

        /// Where the reference is, false when the host does not know the reference
        virtual bool placement(FormId reference, Placement& result) = 0;
        /// Moves the reference to the place of the target, in the cell of the target, with the offsets added to the
        /// position. The reference turns to face as the target does.
        virtual void moveTo(FormId reference, FormId target, const std::array<double, 3>& offset) = 0;
        /// Puts the reference at the placement, in the cell it is in (or the exterior cell that the position belongs
        /// to, for a reference that stands outside)
        virtual void place(FormId reference, const Placement& where) = 0;
    };

    /// Gives the commands that move things to the interpreter: MoveToMarker (also written MoveTo),
    /// MoveToMarkerWithFade, GetPos, SetPos, GetAngle and SetAngle. Angles in scripts are degrees. Those that the
    /// command table does not have are left out.
    void addPlacementCommands(Interpreter& interpreter, PlacementWorld& world);
}

#endif
