#include "conditionparams.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "reader.hpp"
#include "script.hpp"
#include "scriptargs.hpp"

namespace ESM4
{
    namespace
    {
        struct Entry
        {
            std::uint32_t mFunction;
            std::uint32_t mFirst;
            std::uint32_t mSecond;
        };

        // The table is made by scripts/openfallout/generate_command_table.py, in the order of the functions
        constexpr Entry entries[] = {
#include "conditionparams.inc"
        };

        void adjustForm(const Reader& reader, std::uint32_t& value)
        {
            // Zero is the null form. Adjusting it would give it the index of the file that holds it.
            if (value != 0)
                reader.adjustFormId(value);
        }
    }

    ConditionParameterTypes conditionParameterTypes(std::uint32_t function)
    {
        const Entry* end = std::end(entries);
        const Entry* found = std::lower_bound(std::begin(entries), end, function,
            [](const Entry& entry, std::uint32_t value) { return entry.mFunction < value; });
        if (found == end || found->mFunction != function)
            return {};
        return { found->mFirst, found->mSecond };
    }

    void adjustConditionParameters(const Reader& reader, TargetCondition& condition)
    {
        if (!reader.isFalloutFile())
            return;
        const ConditionParameterTypes types = conditionParameterTypes(condition.functionIndex);
        if (types.mFirst != ConditionParameterTypes::none
            && ScriptCode::classifyParameter(types.mFirst) == ScriptCode::ParamClass::Form)
            adjustForm(reader, condition.param1);
        if (types.mSecond != ConditionParameterTypes::none
            && ScriptCode::classifyParameter(types.mSecond) == ScriptCode::ParamClass::Form)
            adjustForm(reader, condition.param2);
    }

    void adjustConditionComparison(const Reader& reader, TargetCondition& condition)
    {
        if ((condition.condition & CTF_UseGlobal) == 0)
            return;
        std::uint32_t global = 0;
        static_assert(sizeof(global) == sizeof(condition.comparison));
        std::memcpy(&global, &condition.comparison, sizeof(global));
        adjustForm(reader, global);
        std::memcpy(&condition.comparison, &global, sizeof(global));
    }
}
