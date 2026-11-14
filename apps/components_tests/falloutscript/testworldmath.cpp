#include "scriptbuilder.hpp"

#include <components/falloutscript/vanillacommands.hpp>
#include <components/falloutscript/worldmath.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{
    using namespace FalloutScriptTest;
    namespace Math = FalloutScript::WorldMath;
    using FalloutScript::Value;

    Value number(double value)
    {
        Value result;
        result.mNumber = value;
        return result;
    }

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };

        Runner() { Math::addMathCommands(mInterpreter); }

        double call(const char* name, std::vector<Value> arguments)
        {
            const FalloutScript::CommandInfo* info = mCommands.find(name);
            EXPECT_NE(info, nullptr) << name;
            return info == nullptr ? 0.0 : mInterpreter.call(info->mOpcode, 0, false, std::move(arguments));
        }
    };

    TEST(FalloutScriptWorldMathTest, the_angles_of_the_sine_and_cosine_are_degrees)
    {
        EXPECT_NEAR(Math::sine(30, false), 0.5, 1e-6);
        EXPECT_NEAR(Math::sine(90, false), 1.0, 1e-6);
        EXPECT_NEAR(Math::cosine(60, false), 0.5, 1e-6);
        EXPECT_NEAR(Math::cosine(0, false), 1.0, 1e-9);
        EXPECT_NEAR(Math::tangent(45, false), 1.0, 1e-6);
    }

    TEST(FalloutScriptWorldMathTest, the_inverse_gives_an_angle_in_degrees)
    {
        EXPECT_NEAR(Math::sine(0.5, true), 30.0, 1e-4);
        EXPECT_NEAR(Math::cosine(0.5, true), 60.0, 1e-4);
        EXPECT_NEAR(Math::tangent(1.0, true), 45.0, 1e-4);
    }

    TEST(FalloutScriptWorldMathTest, the_inverse_of_the_sine_and_cosine_takes_a_value_into_the_range)
    {
        EXPECT_NEAR(Math::sine(2.0, true), 90.0, 1e-4);
        EXPECT_NEAR(Math::sine(-7.0, true), -90.0, 1e-4);
        EXPECT_NEAR(Math::cosine(5.0, true), 0.0, 1e-4);
        EXPECT_NEAR(Math::cosine(-5.0, true), 180.0, 1e-3);
    }

    TEST(FalloutScriptWorldMathTest, the_arguments_are_32_bit_floats)
    {
        // 0.1 is not a float: the angle the library gets is the float's value
        const float angle = 0.1f;
        const float radians = static_cast<float>(static_cast<double>(angle) * 0.01745329238474369);
        EXPECT_EQ(Math::sine(0.1, false), static_cast<double>(std::sin(radians)));
    }

    TEST(FalloutScriptWorldMathTest, the_square_root_is_a_float)
    {
        EXPECT_EQ(Math::squareRoot(16), 4.0);
        EXPECT_EQ(Math::squareRoot(2), static_cast<double>(std::sqrt(2.0f)));
        EXPECT_TRUE(std::isnan(Math::squareRoot(-1)));
    }

    TEST(FalloutScriptWorldMathTest, the_logarithm_is_the_quotient_of_two_natural_ones)
    {
        EXPECT_NEAR(Math::logarithm(8, 2), 3.0, 1e-5);
        EXPECT_NEAR(Math::logarithm(100, 10), 2.0, 1e-5);
        EXPECT_NEAR(Math::logarithm(1, 10), 0.0, 1e-9);
    }

    TEST(FalloutScriptWorldMathTest, the_logarithm_to_the_base_one_is_zero)
    {
        EXPECT_EQ(Math::logarithm(10, 1), 0.0);
    }

    TEST(FalloutScriptWorldMathTest, the_commands_give_the_same_answers)
    {
        Runner runner;
        EXPECT_NEAR(runner.call("Sin", { number(30) }), 0.5, 1e-6);
        EXPECT_NEAR(runner.call("Sin", { number(0.5), number(1) }), 30.0, 1e-4);
        EXPECT_NEAR(runner.call("Cos", { number(60) }), 0.5, 1e-6);
        EXPECT_NEAR(runner.call("Tan", { number(45) }), 1.0, 1e-6);
        EXPECT_EQ(runner.call("Sqrt", { number(25) }), 5.0);
        EXPECT_NEAR(runner.call("Log", { number(8), number(2) }), 3.0, 1e-5);
    }

    TEST(FalloutScriptWorldMathTest, a_logarithm_with_no_base_is_the_natural_one)
    {
        Runner runner;
        EXPECT_NEAR(runner.call("Log", { number(std::exp(2.0)) }), 2.0, 1e-5);
        EXPECT_NEAR(Math::logarithm(1, Math::defaultLogBase), 0.0, 1e-9);
    }

    TEST(FalloutScriptWorldMathTest, the_command_of_the_table_is_the_one_that_has_the_handler)
    {
        Runner runner;
        const FalloutScript::CommandInfo* info = runner.mCommands.find("Sin");
        ASSERT_NE(info, nullptr);
        EXPECT_TRUE(runner.mInterpreter.hasHandler(info->mOpcode));
    }
}
