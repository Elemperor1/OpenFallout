#include "worldmath.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"

namespace FalloutScript::WorldMath
{
    namespace
    {
        // The two constants of the executables: pi/180 as a double (FalloutNV.exe 0x1023128) and the factor that makes the
        // library's angle degrees (0x102F248, a double that holds 57.2957763671875)
        constexpr double radiansPerDegree = 0.01745329238474369;
        constexpr double degreesPerRadian = 57.2957763671875;

        float asFloat(double value)
        {
            return static_cast<float>(value);
        }

        /// The function the executable calls for the arc functions: a double function whose result is kept as a float
        float arc(double (*function)(double), float value)
        {
            return asFloat(function(static_cast<double>(value)));
        }

        float clampedToUnit(float value)
        {
            // Below -1 it is -1, above 1 it is 1 (a value that is not a number is left as it is)
            if (value < -1.f)
                return -1.f;
            if (value > 1.f)
                return 1.f;
            return value;
        }

        float naturalLog(float value)
        {
            return asFloat(std::log(static_cast<double>(value)));
        }
    }

    double sine(double angle, bool inverse)
    {
        const float value = asFloat(angle);
        if (inverse)
            return static_cast<double>(arc(std::asin, clampedToUnit(value))) * degreesPerRadian;
        return static_cast<double>(std::sin(asFloat(static_cast<double>(value) * radiansPerDegree)));
    }

    double cosine(double angle, bool inverse)
    {
        const float value = asFloat(angle);
        if (inverse)
            return static_cast<double>(arc(std::acos, clampedToUnit(value))) * degreesPerRadian;
        return static_cast<double>(std::cos(asFloat(static_cast<double>(value) * radiansPerDegree)));
    }

    double tangent(double angle, bool inverse)
    {
        const float value = asFloat(angle);
        if (inverse)
            return static_cast<double>(arc(std::atan, value)) * degreesPerRadian;
        return static_cast<double>(std::tan(asFloat(static_cast<double>(value) * radiansPerDegree)));
    }

    double squareRoot(double value)
    {
        return static_cast<double>(std::sqrt(asFloat(value)));
    }

    double logarithm(double value, double base)
    {
        const float x = asFloat(value);
        const float b = asFloat(base);
        if (b == 1.f)
            return 0.0;
        return static_cast<double>(naturalLog(x)) / static_cast<double>(naturalLog(b));
    }

    void addMathCommands(Interpreter& interpreter)
    {
        interpreter.setHandler("Sin", [](CallContext& call) { return sine(argument(call, 0), argument(call, 1) != 0); });
        interpreter.setHandler("Cos", [](CallContext& call) { return cosine(argument(call, 0), argument(call, 1) != 0); });
        interpreter.setHandler("Tan", [](CallContext& call) { return tangent(argument(call, 0), argument(call, 1) != 0); });
        interpreter.setHandler("Sqrt", [](CallContext& call) { return squareRoot(argument(call, 0)); });
        interpreter.setHandler("Log", [](CallContext& call) {
            // The base is the second argument, which a call may leave out
            const double base = call.mArguments.size() > 1 ? argument(call, 1) : static_cast<double>(defaultLogBase);
            return logarithm(argument(call, 0), base);
        });
    }
}
