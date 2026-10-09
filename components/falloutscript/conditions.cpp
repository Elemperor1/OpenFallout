#include "conditions.hpp"

#include <cstring>

#include <components/esm4/conditionparams.hpp>
#include <components/esm4/scriptargs.hpp>

namespace FalloutScript
{
    namespace
    {
        // The numbers of the condition functions that are not commands of scripts
        constexpr std::uint32_t getScriptVariable = 53;
        constexpr std::uint32_t getQuestVariable = 79;

        // The types of the parameters of a command in the order a condition has them
        double parameterValue(const CommandInfo& command, std::size_t index, std::uint32_t raw)
        {
            if (index >= command.mParameters.size())
                return 0;
            switch (ESM4::ScriptCode::classifyParameter(command.mParameters[index].mType))
            {
                case ESM4::ScriptCode::ParamClass::Form:
                    return raw;
                case ESM4::ScriptCode::ParamClass::Float:
                case ESM4::ScriptCode::ParamClass::Double:
                {
                    float value = 0;
                    std::memcpy(&value, &raw, sizeof(value));
                    return value;
                }
                case ESM4::ScriptCode::ParamClass::Integer:
                case ESM4::ScriptCode::ParamClass::Short:
                case ESM4::ScriptCode::ParamClass::Byte:
                    return static_cast<std::int32_t>(raw);
                default:
                    return 0;
            }
        }

        bool compare(std::uint32_t type, double left, double right)
        {
            switch (type & 0xE0)
            {
                case ESM4::CTF_EqualTo:
                    return left == right;
                case ESM4::CTF_NotEqualTo:
                    return left != right;
                case ESM4::CTF_GreaterThan:
                    return left > right;
                case ESM4::CTF_GrThOrEqTo:
                    return left >= right;
                case ESM4::CTF_LessThan:
                    return left < right;
                case ESM4::CTF_LeThOrEqTo:
                    return left <= right;
                default:
                    return false;
            }
        }
    }

    ConditionEvaluator::ConditionEvaluator(Interpreter& interpreter)
        : mInterpreter(interpreter)
    {
    }

    void ConditionEvaluator::setFunction(std::uint32_t number, ConditionFunction function)
    {
        mFunctions[number] = std::move(function);
    }

    double ConditionEvaluator::value(const ESM4::TargetCondition& condition, const ConditionContext& context)
    {
        // The older way to say that a condition is about the target, which the run on replaced
        std::uint32_t runOn = condition.runOn;
        if ((condition.condition & ESM4::CTF_RunOnTarget) != 0)
            runOn = 1;
        FormId reference = 0;
        bool onReference = false;
        switch (runOn)
        {
            case 0:
                reference = context.mSubject != 0 ? context.mSubject : mInterpreter.host().player();
                break;
            case 1:
                reference = context.mTarget != 0 ? context.mTarget : mInterpreter.host().player();
                break;
            case 2:
                reference = condition.reference;
                onReference = condition.reference != 0;
                break;
            default: // the combat target and the linked reference, which nothing tells yet
                break;
        }

        if (const auto function = mFunctions.find(condition.functionIndex); function != mFunctions.end())
        {
            ConditionCall call{ mInterpreter.host(), reference, onReference, condition.param1, condition.param2 };
            return function->second(call);
        }

        const std::uint16_t opcode
            = static_cast<std::uint16_t>(ESM4::ScriptCode::firstCommand + condition.functionIndex);
        const CommandInfo* command = mInterpreter.commands().find(opcode);
        std::vector<Value> arguments;
        if (command != nullptr)
        {
            const std::size_t count = std::min<std::size_t>(command->mParameters.size(), 2);
            const std::uint32_t raw[2] = { condition.param1, condition.param2 };
            for (std::size_t i = 0; i < count; ++i)
                arguments.push_back(Value{ parameterValue(*command, i, raw[i]), {}, false });
        }
        return mInterpreter.call(opcode, reference, onReference, std::move(arguments));
    }

    bool ConditionEvaluator::holds(const ESM4::TargetCondition& condition, const ConditionContext& context)
    {
        double right = condition.comparison;
        if ((condition.condition & ESM4::CTF_UseGlobal) != 0)
        {
            std::uint32_t global = 0;
            std::memcpy(&global, &condition.comparison, sizeof(global));
            right = 0;
            mInterpreter.host().getGlobal(global, right);
        }
        return compare(condition.condition, value(condition, context), right);
    }

    bool ConditionEvaluator::holds(
        const std::vector<ESM4::TargetCondition>& conditions, const ConditionContext& context)
    {
        if (conditions.empty())
            return true;
        // The conditions marked OR are joined with the one after them, and the groups that makes with AND
        bool group = false;
        for (std::size_t i = 0; i < conditions.size(); ++i)
        {
            group = group || holds(conditions[i], context);
            const bool joinsNext = (conditions[i].condition & ESM4::CTF_Combine) != 0 && i + 1 < conditions.size();
            if (joinsNext)
                continue;
            if (!group)
                return false;
            group = false;
        }
        return true;
    }

    void addConditionFunctions(ConditionEvaluator& evaluator)
    {
        // The value of a variable of the script of an object or a quest: the second parameter is the index of the
        // variable (counted from 1)
        const ConditionFunction variableOf = [](ConditionCall& call) {
            Instance* instance = call.mHost.findInstance(call.mFirst);
            return instance != nullptr ? instance->variable(call.mSecond) : 0.0;
        };
        evaluator.setFunction(getScriptVariable, variableOf);
        evaluator.setFunction(getQuestVariable, variableOf);
    }
}
