#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLACEMENTCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLACEMENTCOMMANDS_H

#include <array>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// Where a reference stands: its position in game units and its angles around the x, y and z axes in radians, and
    /// how large it is made
    struct Placement
    {
        std::array<double, 3> mPosition{};
        std::array<double, 3> mAngle{};
        double mScale = 1.0;
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
        /// Puts one coordinate of the reference (axis 0, 1 and 2 for x, y and z) at the value, in the cell it is in (or
        /// the exterior cell that the position belongs to, for a reference that stands outside). The rest of where the
        /// reference stands is left as the world has it when this is done, which is later than the script.
        virtual void setPosition(FormId reference, int axis, double value) = 0;
        /// The same for an angle around the axis, in radians
        virtual void setAngle(FormId reference, int axis, double radians) = 0;
        /// Makes the reference as large as the scale says (already within the limits of the games, 0.01 to 10)
        virtual void setScale(FormId reference, double scale) = 0;
    };

    /// Gives the commands that move things to the interpreter: MoveToMarker (also written MoveTo),
    /// MoveToMarkerWithFade, GetPos, SetPos, GetAngle, SetAngle, GetHeadingAngle, GetScale and SetScale. Angles in
    /// scripts are degrees. Those that the command table does not have are left out.
    void addPlacementCommands(Interpreter& interpreter, PlacementWorld& world);
}

#endif
