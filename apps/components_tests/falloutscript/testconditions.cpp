#include "scriptbuilder.hpp"

#include <components/falloutscript/conditions.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ConditionContext;
    using FalloutScript::ConditionEvaluator;
    using FalloutScript::FormId;
    using FalloutScript::Interpreter;

    constexpr FormId quest = 0x00001000;
    constexpr std::uint32_t functionGetStage = OpGetStage - 0x1000;
    constexpr std::uint32_t functionTwice = OpTwice - 0x1000;
    constexpr std::uint32_t functionTouch = OpTouch - 0x1000;
    constexpr std::uint32_t functionNothing = OpNothing - 0x1000;
    constexpr std::uint32_t functionGetQuestVariable = 79;

    ESM4::TargetCondition condition(std::uint32_t function, std::uint32_t type, float comparison,
        std::uint32_t param1 = 0, std::uint32_t param2 = 0)
    {
        ESM4::TargetCondition result{};
        result.condition = type;
        result.comparison = comparison;
        result.functionIndex = function;
        result.param1 = param1;
        result.param2 = param2;
        return result;
    }

    struct Evaluator
    {
        FalloutScript::CommandTable mCommands = testCommands();
        TestHost mHost;
        Interpreter mInterpreter{ mHost, mCommands };
        ConditionEvaluator mEvaluator{ mInterpreter };
        std::map<FormId, double> mStages;
        // what the last command was called on
        FormId mReference = 0;
        bool mOnReference = false;

        Evaluator()
        {
            mInterpreter.setHandler("GetStage", [this](FalloutScript::CallContext& call) {
                mReference = call.mReference;
                mOnReference = call.mOnReference;
                return mStages[static_cast<FormId>(call.mArguments.at(0).mNumber)];
            });
            mInterpreter.setHandler(
                "Twice", [](FalloutScript::CallContext& call) { return call.mArguments.at(0).mNumber * 2; });
            mInterpreter.setHandler("Touch", [this](FalloutScript::CallContext& call) {
                mReference = call.mReference;
                mOnReference = call.mOnReference;
                return static_cast<double>(call.mArguments.size());
            });
            FalloutScript::addConditionFunctions(mEvaluator);
        }

        bool holds(const ESM4::TargetCondition& one, const ConditionContext& context = {})
        {
            return mEvaluator.holds(one, context);
        }
    };

    TEST(FalloutScriptConditionsTest, comparesTheValueOfAFunctionWithANumber)
    {
        Evaluator test;
        test.mStages[quest] = 10;
        const std::uint32_t equal = ESM4::CTF_EqualTo;
        EXPECT_TRUE(test.holds(condition(functionGetStage, equal, 10, quest)));
        EXPECT_FALSE(test.holds(condition(functionGetStage, equal, 11, quest)));
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_NotEqualTo, 11, quest)));
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_GreaterThan, 9, quest)));
        EXPECT_FALSE(test.holds(condition(functionGetStage, ESM4::CTF_GreaterThan, 10, quest)));
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_GrThOrEqTo, 10, quest)));
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_LessThan, 11, quest)));
        EXPECT_FALSE(test.holds(condition(functionGetStage, ESM4::CTF_LessThan, 10, quest)));
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_LeThOrEqTo, 10, quest)));
    }

    TEST(FalloutScriptConditionsTest, comparesWithAGlobalVariableWhenTheFlagSaysSo)
    {
        Evaluator test;
        test.mStages[quest] = 10;
        test.mHost.mGlobals[0x00002000] = 10;
        test.mHost.mGlobals[0x00002001] = 11;
        float global = 0;
        const std::uint32_t id = 0x00002000;
        std::memcpy(&global, &id, sizeof(global));
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_EqualTo | ESM4::CTF_UseGlobal, global, quest)));
        const std::uint32_t other = 0x00002001;
        std::memcpy(&global, &other, sizeof(global));
        EXPECT_FALSE(test.holds(condition(functionGetStage, ESM4::CTF_EqualTo | ESM4::CTF_UseGlobal, global, quest)));
    }

    TEST(FalloutScriptConditionsTest, joinsTheConditionsThatAreMarkedOrWithTheOneAfterThem)
    {
        Evaluator test;
        test.mStages[quest] = 10;
        const auto yes = condition(functionGetStage, ESM4::CTF_EqualTo, 10, quest);
        const auto no = condition(functionGetStage, ESM4::CTF_EqualTo, 99, quest);
        auto orWithNext = [](ESM4::TargetCondition value) {
            value.condition |= ESM4::CTF_Combine;
            return value;
        };
        const ConditionContext context;

        // all of them: AND
        EXPECT_TRUE(test.mEvaluator.holds({ yes, yes }, context));
        EXPECT_FALSE(test.mEvaluator.holds({ yes, no }, context));
        // a OR b
        EXPECT_TRUE(test.mEvaluator.holds({ orWithNext(no), yes }, context));
        EXPECT_FALSE(test.mEvaluator.holds({ orWithNext(no), no }, context));
        // (no OR yes) AND no is false, (no OR yes) AND yes true: the OR is tighter than the AND
        EXPECT_FALSE(test.mEvaluator.holds({ orWithNext(no), yes, no }, context));
        EXPECT_TRUE(test.mEvaluator.holds({ orWithNext(no), yes, yes }, context));
        // no AND (yes OR yes) is false, even though yes OR yes is true
        EXPECT_FALSE(test.mEvaluator.holds({ no, orWithNext(yes), yes }, context));
        // a chain of ORs, and an OR on the last condition, which has nothing to join
        EXPECT_TRUE(test.mEvaluator.holds({ orWithNext(no), orWithNext(no), yes }, context));
        EXPECT_FALSE(test.mEvaluator.holds({ yes, orWithNext(no) }, context));
        // no conditions at all hold
        EXPECT_TRUE(test.mEvaluator.holds(std::vector<ESM4::TargetCondition>(), context));
    }

    TEST(FalloutScriptConditionsTest, runsAConditionOnTheObjectItSaysItRunsOn)
    {
        Evaluator test;
        ConditionContext context;
        context.mSubject = 0x00003000;
        context.mTarget = 0x00003001;

        auto touch = condition(functionTouch, ESM4::CTF_EqualTo, 0);
        test.holds(touch, context);
        EXPECT_EQ(test.mReference, 0x00003000u);
        EXPECT_FALSE(test.mOnReference);

        touch.runOn = 1;
        test.holds(touch, context);
        EXPECT_EQ(test.mReference, 0x00003001u);

        // the older flag that says the same
        touch.runOn = 0;
        touch.condition |= ESM4::CTF_RunOnTarget;
        test.holds(touch, context);
        EXPECT_EQ(test.mReference, 0x00003001u);

        touch.condition = ESM4::CTF_EqualTo;
        touch.runOn = 2;
        touch.reference = 0x00003002;
        test.holds(touch, context);
        EXPECT_EQ(test.mReference, 0x00003002u);
        EXPECT_TRUE(test.mOnReference);

        // without a subject or a target the condition is about the player
        touch.runOn = 0;
        test.holds(touch, {});
        EXPECT_EQ(test.mReference, FalloutScript::playerReference);
        touch.runOn = 1;
        test.holds(touch, {});
        EXPECT_EQ(test.mReference, FalloutScript::playerReference);
    }

    TEST(FalloutScriptConditionsTest, givesTheParametersTheTypesTheCommandHasThem)
    {
        Evaluator test;
        // Twice takes a float, which a record writes as the bytes of a float
        float parameter = 2.25f;
        std::uint32_t raw = 0;
        std::memcpy(&raw, &parameter, sizeof(raw));
        EXPECT_TRUE(test.holds(condition(functionTwice, ESM4::CTF_EqualTo, 4.5f, raw)));

        // Touch has an object and an integer: both parameters are passed, and the integer is signed
        auto touch = condition(functionTouch, ESM4::CTF_EqualTo, 2, 0x00003000, 0xFFFFFFFF);
        EXPECT_TRUE(test.holds(touch));
        // a command with one parameter is given one
        EXPECT_TRUE(test.holds(condition(functionGetStage, ESM4::CTF_EqualTo, 0, quest, 123)));
    }

    TEST(FalloutScriptConditionsTest, aFunctionWithNoHandlerOrNoCommandTellsZeroAndSaysSoOnce)
    {
        Evaluator test;
        const auto nothing = condition(functionNothing, ESM4::CTF_EqualTo, 0);
        EXPECT_TRUE(test.holds(nothing));
        EXPECT_TRUE(test.holds(nothing));
        const auto unknown = condition(900, ESM4::CTF_EqualTo, 0);
        EXPECT_TRUE(test.holds(unknown));
        EXPECT_TRUE(test.holds(unknown));
        EXPECT_THAT(test.mHost.mLog, SizeIs(2));
        EXPECT_THAT(test.mHost.mLog[0], HasSubstr("Nothing"));
        EXPECT_THAT(test.mHost.mLog[1], HasSubstr("not in the table"));
    }

    TEST(FalloutScriptConditionsTest, readsAVariableOfTheScriptOfAQuest)
    {
        Evaluator test;
        ScriptBuilder builder;
        builder.variable(1, 1, "iCount").variable(2, 1, "iOther").code(scriptName());
        auto script = prepare(builder, test.mCommands);
        FalloutScript::Instance instance(script, quest);
        instance.setVariable(2, 7);
        test.mHost.mInstances[quest] = &instance;

        EXPECT_TRUE(test.holds(condition(functionGetQuestVariable, ESM4::CTF_EqualTo, 7, quest, 2)));
        EXPECT_TRUE(test.holds(condition(functionGetQuestVariable, ESM4::CTF_EqualTo, 0, quest, 1)));
        // a quest with no script has no variables, and tells 0
        EXPECT_TRUE(test.holds(condition(functionGetQuestVariable, ESM4::CTF_EqualTo, 0, 0x00001777, 1)));
    }
}
