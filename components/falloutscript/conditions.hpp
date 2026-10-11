#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CONDITIONS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_CONDITIONS_H

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include <components/esm4/script.hpp>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// Who a condition is about
    struct ConditionContext
    {
        /// The object a condition runs on when it does not say otherwise: the character a line of dialogue is said by,
        /// the package's owner. The player when it is 0.
        FormId mSubject = 0;
        /// The other object of the exchange: the character the line is said to. The player when it is 0.
        FormId mTarget = 0;
    };

    /// A condition function that is not a command of scripts, called with the parameters as the record has them
    struct ConditionCall
    {
        Host& mHost;
        FormId mReference = 0; // the object it runs on
        bool mOnReference = false;
        std::uint32_t mFirst = 0;
        std::uint32_t mSecond = 0;
    };
    using ConditionFunction = std::function<double(ConditionCall&)>;

    /// Tells whether the conditions (CTDA) of a record hold. A condition names a function by its number (the code of
    /// the command less 0x1000), gives it up to two parameters, and compares what it tells with a number or with a
    /// global variable. The functions are the commands of the interpreter, which has the handlers for them; a function
    /// that is only a condition is given with setFunction. A function without a handler tells 0 (that is reported
    /// once).
    class ConditionEvaluator
    {
    public:
        explicit ConditionEvaluator(Interpreter& interpreter);

        void setFunction(std::uint32_t number, ConditionFunction function);

        /// What the function of the condition tells, before it is compared
        double value(const ESM4::TargetCondition& condition, const ConditionContext& context);
        /// Whether the condition holds
        bool holds(const ESM4::TargetCondition& condition, const ConditionContext& context);
        /// Whether the conditions hold together: they all must, but a condition that is marked OR is joined with the
        /// one after it, so that a, b OR, c is a and (b or c). No conditions at all hold.
        bool holds(const std::vector<ESM4::TargetCondition>& conditions, const ConditionContext& context);

    private:
        Interpreter& mInterpreter;
        std::map<std::uint32_t, ConditionFunction> mFunctions;
    };

    /// Gives the evaluator the functions that are only conditions and that need nothing of the engine: the variables of
    /// the script of an object or a quest (GetScriptVariable, GetQuestVariable).
    void addConditionFunctions(ConditionEvaluator& evaluator);
}

#endif
