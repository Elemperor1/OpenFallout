#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/lua/luastate.hpp>
#include <components/lua/utilpackage.hpp>
#include <components/testing/expecterror.hpp>

// The parts of the util package of the Lua scripts that the first tests do not reach: boxes, colors written as text,
// the angles of a transform, the transforms that cannot be inverted and the loading of code
namespace
{
    using namespace testing;

    struct LuaUtilPackageMoreTest : Test
    {
        LuaUtil::LuaState mLuaState{ nullptr, nullptr };

        LuaUtilPackageMoreTest()
        {
            mLuaState.addInternalLibSearchPath(
                std::filesystem::path{ OPENFALLOUT_PROJECT_SOURCE_DIR } / "components" / "lua");
            sol::state_view sol = mLuaState.unsafeState();
            sol["util"] = LuaUtil::initUtilPackage(sol);
        }

        template <typename T>
        T get(const std::string& luaCode)
        {
            sol::state_view lua = mLuaState.unsafeState();
            return lua.safe_script("return " + luaCode).template get<T>();
        }

        std::string asString(const std::string& luaCode)
        {
            sol::state_view lua = mLuaState.unsafeState();
            return LuaUtil::toString(lua.safe_script("return " + luaCode));
        }
    };

    TEST_F(LuaUtilPackageMoreTest, makesABoxFromACenterATransformOrARotation)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("T = util.transform");
        lua.safe_script("box = util.box(util.vector3(1, 2, 3), util.vector3(4, 5, 6))");
        EXPECT_EQ(asString("box"), "Box{ center(1, 2, 3) halfSize(4, 5, 6) }");
        EXPECT_EQ(asString("box.center"), "(1, 2, 3)");
        EXPECT_EQ(asString("box.halfSize"), "(4, 5, 6)");
        EXPECT_EQ(get<int>("#box.vertices"), 8);
        EXPECT_THAT(asString("box.transform"), StartsWith("TransformM{ "));
        EXPECT_TRUE(get<bool>("box == util.box(util.vector3(1, 2, 3), util.vector3(4, 5, 6))"));
        EXPECT_FALSE(get<bool>("box == util.box(util.vector3(1, 2, 3), util.vector3(4, 5, 7))"));

        // a transform is the box that it turns the unit box into
        lua.safe_script("moved = util.box(T.move(1, 2, 3) * T.scale(2, 2, 2))");
        EXPECT_EQ(asString("moved.center"), "(1, 2, 3)");
        EXPECT_EQ(asString("moved.halfSize"), "(2, 2, 2)");
        // and a rotation is the unit box turned
        lua.safe_script("turned = util.box(T.rotateZ(math.pi / 2))");
        EXPECT_EQ(asString("turned.halfSize"), "(1, 1, 1)");
        EXPECT_EQ(asString("turned.center"), "(0, 0, 0)");
        EXPECT_LT(get<float>("(turned.vertices[1] - turned.vertices[2]):length() - 2"), 1e-5f);
    }

    TEST_F(LuaUtilPackageMoreTest, readsAColorWrittenAsCommaSeparatedNumbers)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("c = util.color.commaString('255, 0, 51')");
        EXPECT_FLOAT_EQ(get<float>("c.r"), 1.f);
        EXPECT_FLOAT_EQ(get<float>("c.g"), 0.f);
        EXPECT_FLOAT_EQ(get<float>("c.b"), 0.2f);
        EXPECT_FLOAT_EQ(get<float>("c.a"), 1.f);
        lua.safe_script("c = util.color.commaString('0,255,0,51')");
        EXPECT_FLOAT_EQ(get<float>("c.g"), 1.f);
        EXPECT_FLOAT_EQ(get<float>("c.a"), 0.2f);

        EXPECT_ERROR(lua.safe_script("util.color.commaString('1, 2')"), "Invalid comma-separated color: 1, 2");
        EXPECT_ERROR(lua.safe_script("util.color.commaString('1, 2, 3, 4, 5')"), "Invalid comma-separated color");
        EXPECT_ERROR(
            lua.safe_script("util.color.commaString('red, 2, 3')"), "Invalid comma-separated color: red, 2, 3");
    }

    TEST_F(LuaUtilPackageMoreTest, describesATransformWithEachOfItsParts)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("T = util.transform");
        lua.safe_script("rotated = T.move(1, 2, 3) * T.rotateZ(math.pi / 2) * T.scale(2, 3, 4)");
        const std::string text = asString("rotated");
        EXPECT_THAT(text, HasSubstr("move("));
        EXPECT_THAT(text, HasSubstr("rotation(angle="));
        EXPECT_THAT(text, HasSubstr("scale("));
        EXPECT_THAT(asString("T.rotateX(1)"), StartsWith("TransformQ{ rotation(angle=1"));
        EXPECT_THAT(asString("T.rotateY(1) * T.rotateX(1)"), StartsWith("TransformQ{ rotation(angle="));
        EXPECT_THAT(asString("T.rotateX(1) * T.rotateY(1)"), StartsWith("TransformQ{ rotation(angle="));
        EXPECT_EQ(asString("T.scale(1, 1, 1) * T.move(0, 0, 0)"), "TransformM{ }");
    }

    TEST_F(LuaUtilPackageMoreTest, multipliesTransformsOfBothKindsInBothOrders)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("T = util.transform");
        lua.safe_script("v = util.vector3");
        // the transform on the right is applied first
        lua.safe_script("moveOfTurn = T.move(1, 0, 0) * T.rotateZ(-math.pi / 2)");
        lua.safe_script("turnOfMove = T.rotateZ(-math.pi / 2) * T.move(1, 0, 0)");
        EXPECT_LT(get<float>("(moveOfTurn * v(0, 0, 0) - v(1, 0, 0)):length()"), 1e-5f);
        EXPECT_LT(get<float>("(turnOfMove * v(0, 0, 0) - v(0, 1, 0)):length()"), 1e-5f);
        EXPECT_LT(get<float>("(T.move(1, 2, 3):apply(v(0, 0, 0)) - v(1, 2, 3)):length()"), 1e-5f);
    }

    TEST_F(LuaUtilPackageMoreTest, invertsATransformThatIsAMatrix)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("T = util.transform");
        lua.safe_script("v = util.vector3");
        lua.safe_script("m = T.move(1, 2, 3) * T.scale(2, 2, 2)");
        EXPECT_LT(get<float>("(m:inverse() * (m * v(5, 6, 7)) - v(5, 6, 7)):length()"), 1e-5f);
    }

    TEST_F(LuaUtilPackageMoreTest, tellsTheAnglesOfATransform)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("T = util.transform");
        lua.safe_script("yawed = T.rotateZ(0.5)");
        lua.safe_script("pitched = T.rotateX(0.25)");
        EXPECT_NEAR(get<float>("yawed:getYaw()"), 0.5f, 1e-5f);
        EXPECT_NEAR(get<float>("yawed:getPitch()"), 0.f, 1e-5f);
        EXPECT_NEAR(get<float>("pitched:getPitch()"), 0.25f, 1e-5f);
        lua.safe_script("pitch, yaw = pitched:getAnglesXZ()");
        EXPECT_NEAR(get<float>("pitch"), 0.25f, 1e-5f);
        EXPECT_NEAR(get<float>("yaw"), 0.f, 1e-5f);
        lua.safe_script("z, y, x = yawed:getAnglesZYX()");
        EXPECT_NEAR(get<float>("z"), 0.5f, 1e-5f);
        EXPECT_NEAR(get<float>("y"), 0.f, 1e-5f);
        EXPECT_NEAR(get<float>("x"), 0.f, 1e-5f);

        // the same through a transform that is a matrix
        lua.safe_script("matrix = T.move(1, 2, 3) * yawed");
        EXPECT_NEAR(get<float>("matrix:getYaw()"), 0.5f, 1e-5f);
        EXPECT_NEAR(get<float>("matrix:getPitch()"), 0.f, 1e-5f);
        lua.safe_script("pitchM, yawM = matrix:getAnglesXZ()");
        EXPECT_NEAR(get<float>("yawM"), 0.5f, 1e-5f);
        lua.safe_script("zM, yM, xM = matrix:getAnglesZYX()");
        EXPECT_NEAR(get<float>("zM"), 0.5f, 1e-5f);
    }

    TEST_F(LuaUtilPackageMoreTest, loadsCodeInAnEnvironmentOfItsOwn)
    {
        sol::state_view lua = mLuaState.unsafeState();
        lua.safe_script("env = { x = 2 }");
        lua.safe_script("fn = util.loadCode('y = x + 1; return y * 2', env)");
        EXPECT_EQ(get<int>("fn()"), 6);
        // what the code sets goes to the environment it was given, and not to the globals
        EXPECT_EQ(get<int>("env.y"), 3);
        EXPECT_TRUE(get<bool>("y == nil"));
        EXPECT_ERROR(lua.safe_script("util.loadCode('this is not Lua', {})"), "Lua error");
    }

    TEST_F(LuaUtilPackageMoreTest, picksComponentsOfEveryKindOfVectorBySwizzle)
    {
        EXPECT_EQ(asString("util.vector2(1, 2).yx"), "(2, 1)");
        EXPECT_EQ(asString("util.vector2(1, 2).xyyx"), "(1, 2, 2, 1)");
        EXPECT_EQ(asString("util.vector3(1, 2, 3).zyx"), "(3, 2, 1)");
        EXPECT_EQ(asString("util.vector3(1, 2, 3).zzyx"), "(3, 3, 2, 1)");
        EXPECT_EQ(asString("util.vector4(1, 2, 3, 4).wzyx"), "(4, 3, 2, 1)");
        EXPECT_EQ(asString("util.vector4(1, 2, 3, 4).wwxy"), "(4, 4, 1, 2)");
    }
}
