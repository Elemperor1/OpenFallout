#ifndef OPENFALLOUT_COMPONENTS_ESM4_CONDITIONPARAMS_H
#define OPENFALLOUT_COMPONENTS_ESM4_CONDITIONPARAMS_H

#include <cstdint>

namespace ESM4
{
    class Reader;
    struct TargetCondition;

    /// The types of the first two parameters of a condition function (the numbers of the table of commands of the
    /// games: 14 is a quest, 23 a quest stage ...), or none for a parameter it does not have.
    struct ConditionParameterTypes
    {
        static constexpr std::uint32_t none = 0xFFFF;
        std::uint32_t mFirst = none;
        std::uint32_t mSecond = none;
    };

    /// The parameter types of the function of that number (a condition names its function by the code of the command
    /// less 0x1000). Functions the table of the games' commands does not have give two none.
    ConditionParameterTypes conditionParameterTypes(std::uint32_t function);

    /// A condition of a plugin of Fallout 3 or New Vegas names forms in its parameters by the index of the file they
    /// are in, which counts the masters of the plugin, not the load order. This gives the parameters that are forms (by
    /// the types of the function) the index the reader gives every other form it reads. A condition of any other game
    /// is left as it is.
    void adjustConditionParameters(const Reader& reader, TargetCondition& condition);

    /// The comparison value of a condition that has the flag for a global variable is the form id of the global, as a
    /// float with the bytes of the id. This adjusts it the same way.
    void adjustConditionComparison(const Reader& reader, TargetCondition& condition);
}

#endif
