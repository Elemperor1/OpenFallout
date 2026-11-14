#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WORLDMATH_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_WORLDMATH_H

#include "interpreter.hpp"

namespace FalloutScript::WorldMath
{
    /// The maths commands of the games (FalloutNV.exe 0x5DC0F0 Sin, 0x5DC1B0 Cos, 0x5DC290 Tan, 0x5DC350 Sqrt, 0x5DC3B0
    /// Log; Fallout3.exe 0x52FF60, 0x530030, 0x530100, 0x5301A0, 0x530200). The arguments are 32 bit floats and the
    /// angles are in degrees, as the executables do them: an angle is multiplied by pi/180 (as a double, then made a
    /// float) before the C library's float function, and the inverse of a function (a second argument that is not 0)
    /// gives an angle back, in degrees (the result of the library's function multiplied by the double 57.2957763671875).

    /// Sin of an angle in degrees; with `inverse` the arc sine in degrees of a value taken into -1..1
    double sine(double angle, bool inverse);
    double cosine(double angle, bool inverse);
    /// Tan of an angle in degrees; with `inverse` the arc tangent in degrees
    double tangent(double angle, bool inverse);
    double squareRoot(double value);
    /// The logarithm of `value` to the base `base` (the base of the natural logarithm when the call gives none): the
    /// quotient of the two natural logarithms, each taken in double precision and kept as a float. A base of 1 gives 0.
    double logarithm(double value, double base);

    /// The base a Log call that gives none uses: the float the executable reads at FalloutNV.exe 0x103D018
    constexpr float defaultLogBase = 2.7182817f;

    /// Gives the commands Sin, Cos, Tan, Sqrt and Log to the interpreter
    void addMathCommands(Interpreter& interpreter);
}

#endif
