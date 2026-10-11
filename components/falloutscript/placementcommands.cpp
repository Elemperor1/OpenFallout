#include "placementcommands.hpp"

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

        /// A number that a script computed can be anything; a place is not
        double finite(double value)
        {
            return std::isfinite(value) ? value : 0.0;
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
        interpreter.setHandler("SetPos", [&world](CallContext& call) {
            const int axis = axisOf(call, 0);
            Placement where;
            if (call.mReference == 0 || axis < 0 || !world.placement(call.mReference, where))
                return 0.0;
            where.mPosition[axis] = finite(argument(call, 1));
            world.place(call.mReference, where);
            return 0.0;
        });
        interpreter.setHandler("SetAngle", [&world](CallContext& call) {
            const int axis = axisOf(call, 0);
            Placement where;
            if (call.mReference == 0 || axis < 0 || !world.placement(call.mReference, where))
                return 0.0;
            where.mAngle[axis] = toRadians(finite(argument(call, 1)));
            world.place(call.mReference, where);
            return 0.0;
        });
    }
}
