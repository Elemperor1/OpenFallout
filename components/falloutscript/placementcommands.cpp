#include "placementcommands.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        /// The axis a script names: the letter X, Y or Z in either case (as the games write it), or 0, 1 and 2.
        /// -1 for anything else.
        int axisOf(const CallContext& call, std::size_t index)
        {
            switch (intArgument(call, index))
            {
                case 'X':
                case 'x':
                case 0:
                    return 0;
                case 'Y':
                case 'y':
                case 1:
                    return 1;
                case 'Z':
                case 'z':
                case 2:
                    return 2;
                default:
                    return -1;
            }
        }

        double toDegrees(double radians, int axis)
        {
            const double degrees = radians * 180.0 / pi;
            if (axis != 2)
                return degrees;
            // The heading is told from 0 to 360, however the record writes it
            const double turned = std::fmod(degrees, 360.0);
            return turned < 0 ? turned + 360.0 : turned;
        }

        double toRadians(double degrees)
        {
            return degrees * pi / 180.0;
        }

        /// The largest coordinate or angle (units, degrees) that a script can give: a unit is about a centimetre and a
        /// half, so this is a world of ten thousand kilometres, far outside any map, and still a number that a float
        /// holds with room for the sum of two of them and for the index of an exterior cell that fits an int
        constexpr double largest = 1.0e9;

        /// A number that a script computed can be anything; a place is not
        double finite(double value)
        {
            return std::isfinite(value) ? std::clamp(value, -largest, largest) : 0.0;
        }

        /// The angle in degrees from where the reference faces to the direction of the target, from -180 to 180 and
        /// positive when the target is to the right (clockwise seen from above). The heights do not count. Two
        /// references on one place give the direction of 270 degrees, which is what the games do.
        double headingAngle(const Placement& self, const Placement& target)
        {
            const double dx = target.mPosition[0] - self.mPosition[0];
            const double dy = target.mPosition[1] - self.mPosition[1];
            // The bearing runs from the y axis towards the x axis
            double bearing;
            if (dy == 0)
                bearing = dx > 0 ? pi / 2 : 3 * pi / 2;
            else
                bearing = std::atan(dx / dy) + (dy < 0 ? pi : 0.0);
            return std::remainder(bearing - self.mAngle[2], 2 * pi) * 180.0 / pi;
        }

        void moveToMarker(PlacementWorld& world, CallContext& call)
        {
            const FormId target = formArgument(call, 0);
            if (call.mReference == 0 || target == 0)
                return;
            const std::array<double, 3> offset{ finite(argument(call, 1)), finite(argument(call, 2)),
                finite(argument(call, 3)) };
            world.moveTo(call.mReference, target, offset);
        }
    }

    void addPlacementCommands(Interpreter& interpreter, PlacementWorld& world)
    {
        interpreter.setHandler("MoveToMarker", [&world](CallContext& call) {
            moveToMarker(world, call);
            return 0.0;
        });
        // The same without the fade of the screen, which is the host's to show when it has one
        interpreter.setHandler("MoveToMarkerWithFade", [&world](CallContext& call) {
            moveToMarker(world, call);
            return 0.0;
        });
        interpreter.setHandler("GetPos", [&world](CallContext& call) {
            const int axis = axisOf(call, 0);
            Placement where;
            if (call.mReference == 0 || axis < 0 || !world.placement(call.mReference, where))
                return 0.0;
            return where.mPosition[axis];
        });
        interpreter.setHandler("GetAngle", [&world](CallContext& call) {
            const int axis = axisOf(call, 0);
            Placement where;
            if (call.mReference == 0 || axis < 0 || !world.placement(call.mReference, where))
                return 0.0;
            return toDegrees(where.mAngle[axis], axis);
        });
        interpreter.setHandler("GetHeadingAngle", [&world](CallContext& call) {
            Placement self;
            Placement target;
            const FormId other = formArgument(call, 0);
            if (call.mReference == 0 || other == 0 || !world.placement(call.mReference, self)
                || !world.placement(other, target))
                return 0.0;
            return headingAngle(self, target);
        });
        interpreter.setHandler("GetScale", [&world](CallContext& call) {
            Placement where;
            if (call.mReference == 0 || !world.placement(call.mReference, where))
                return 0.0;
            return where.mScale;
        });
        // The games hold the scale of a reference between a hundredth and ten
        interpreter.setHandler("SetScale", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.setScale(call.mReference, std::clamp(finite(argument(call, 0)), 0.01, 10.0));
            return 0.0;
        });
        interpreter.setHandler("SetPos", [&world](CallContext& call) {
            const int axis = axisOf(call, 0);
            if (call.mReference != 0 && axis >= 0)
                world.setPosition(call.mReference, axis, finite(argument(call, 1)));
            return 0.0;
        });
        interpreter.setHandler("SetAngle", [&world](CallContext& call) {
            const int axis = axisOf(call, 0);
            if (call.mReference != 0 && axis >= 0)
                world.setAngle(call.mReference, axis, toRadians(finite(argument(call, 1))));
            return 0.0;
        });
    }
}
