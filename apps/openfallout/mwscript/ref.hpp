#ifndef GAME_MWSCRIPT_REF_H
#define GAME_MWSCRIPT_REF_H

#include "../mwworld/ptr.hpp"

namespace Interpreter
{
    class Runtime;
}

namespace OFScript
{
    struct ExplicitRef
    {
        static constexpr bool implicit = false;

        OFWorld::Ptr operator()(Interpreter::Runtime& runtime, bool required = true, bool activeOnly = false) const;
    };

    struct ImplicitRef
    {
        static constexpr bool implicit = true;

        OFWorld::Ptr operator()(Interpreter::Runtime& runtime, bool required = true, bool activeOnly = false) const;
    };
}

#endif
