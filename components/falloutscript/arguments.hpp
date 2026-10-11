#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ARGUMENTS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ARGUMENTS_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <string>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// The arguments of a call as the handlers of commands want them. An argument the call leaves out is 0.
    inline double argument(const CallContext& call, std::size_t index)
    {
        return index < call.mArguments.size() ? call.mArguments[index].mNumber : 0;
    }

    inline FormId formArgument(const CallContext& call, std::size_t index)
    {
        const double value = argument(call, index);
        return value > 0 && value < 4294967296.0 ? static_cast<FormId>(value) : 0;
    }

    inline int intArgument(const CallContext& call, std::size_t index)
    {
        const double value = std::trunc(argument(call, index));
        if (std::isnan(value))
            return 0;
        return static_cast<int>(std::clamp(value, -2147483648.0, 2147483647.0));
    }

    inline std::string hex(FormId id)
    {
        std::ostringstream stream;
        stream << "0x" << std::hex << id;
        return stream.str();
    }
}

#endif
