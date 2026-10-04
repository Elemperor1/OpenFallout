#ifndef OPENFALLOUT_COMPONENTS_MISC_TUPLEHELPERS_H
#define OPENFALLOUT_COMPONENTS_MISC_TUPLEHELPERS_H

#include <tuple>

namespace Misc
{
    template <typename TupleType, typename Callable>
    void tupleForEach(TupleType& tuple, Callable&& f)
    {
        std::apply([&f](auto&... x) { (f(x), ...); }, tuple);
    }
}

#endif
