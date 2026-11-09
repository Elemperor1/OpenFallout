#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/lua/inputactions.hpp>
#include <components/lua/luastate.hpp>
#include <components/lua/scriptscontainer.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using LuaUtil::InputAction::Type;

    /// A callback of a script that is there, or of one that was removed.
    LuaUtil::Callback makeCallback(sol::state& lua, const std::string& code, bool scriptIsThere = true)
    {
        sol::table hiddenData(lua, sol::create);
        hiddenData[std::string(LuaUtil::ScriptsContainer::sScriptDebugNameKey)] = "test.lua";
        if (scriptIsThere)
            hiddenData[std::string(LuaUtil::ScriptsContainer::sScriptIdKey)] = LuaUtil::ScriptId{};
        return LuaUtil::Callback{ lua.load(code)(), hiddenData };
    }

    LuaUtil::InputAction::Info action(
        sol::state& lua, const std::string& key, Type type, sol::object defaultValue, bool persistent = false)
    {
        return { key, type, "test", key + "_name", key + "_description", sol::main_object(defaultValue), persistent };
    }

    std::string insertFailure(LuaUtil::InputAction::Registry& registry, const LuaUtil::InputAction::Info& info)
    {
        try
        {
            registry.insert(info);
        }
        catch (const std::exception& e)
        {
            return e.what();
        }
        return {};
    }

    TEST(LuaInputRegistryTest, anActionHasAUniqueKeyALocalizationContextAndAValueOfItsType)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "jump", Type::Boolean, sol::make_object(lua, false)));

        EXPECT_THAT(insertFailure(registry, action(lua, "jump", Type::Boolean, sol::make_object(lua, true))),
            HasSubstr("already in use"));
        EXPECT_THAT(insertFailure(registry, action(lua, "", Type::Boolean, sol::make_object(lua, true))),
            HasSubstr("can't be an empty string"));
        LuaUtil::InputAction::Info withoutContext = action(lua, "look", Type::Boolean, sol::make_object(lua, true));
        withoutContext.mL10n.clear();
        EXPECT_THAT(insertFailure(registry, withoutContext), HasSubstr("Localization context can't be empty"));

        EXPECT_THAT(insertFailure(registry, action(lua, "a", Type::Boolean, sol::make_object(lua, 1))),
            HasSubstr("Invalid value"));
        EXPECT_THAT(insertFailure(registry, action(lua, "b", Type::Number, sol::make_object(lua, "text"))),
            HasSubstr("Invalid value"));
        EXPECT_THAT(insertFailure(registry, action(lua, "c", Type::Range, sol::make_object(lua, "text"))),
            HasSubstr("Invalid value"));
        EXPECT_THAT(insertFailure(registry, action(lua, "d", Type::Range, sol::make_object(lua, 1.5))),
            HasSubstr("Invalid value"));
        EXPECT_THAT(insertFailure(registry, action(lua, "e", Type::Range, sol::make_object(lua, -0.5))),
            HasSubstr("Invalid value"));

        EXPECT_EQ(insertFailure(registry, action(lua, "f", Type::Number, sol::make_object(lua, 12.5))), "");
        EXPECT_EQ(insertFailure(registry, action(lua, "g", Type::Range, sol::make_object(lua, 0.5))), "");
        EXPECT_EQ(insertFailure(registry, action(lua, "h", Type::Range, sol::make_object(lua, 1.0))), "");
    }

    TEST(LuaInputRegistryTest, theKeysOfTheActionsAreWalkedInTheOrderTheyWereInserted)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        EXPECT_EQ(registry.firstKey(), std::nullopt);

        registry.insert(action(lua, "first", Type::Boolean, sol::make_object(lua, false)));
        registry.insert(action(lua, "second", Type::Number, sol::make_object(lua, 0)));
        registry.insert(action(lua, "third", Type::Range, sol::make_object(lua, 0.25)));

        EXPECT_EQ(registry.size(), 3u);
        EXPECT_EQ(registry.firstKey(), "first");
        EXPECT_EQ(registry.nextKey("first"), "second");
        EXPECT_EQ(registry.nextKey("second"), "third");
        EXPECT_EQ(registry.nextKey("third"), std::nullopt);
        EXPECT_EQ(registry.nextKey("unknown"), std::nullopt);

        const std::optional<LuaUtil::InputAction::Info> info = registry["second"];
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->mKey, "second");
        EXPECT_EQ(info->mType, Type::Number);
        EXPECT_EQ(info->mName, "second_name");
        EXPECT_EQ(registry["unknown"], std::nullopt);
    }

    TEST(LuaInputRegistryTest, anActionThatIsNotRegisteredCannotBeBoundOrHandled)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "jump", Type::Boolean, sol::make_object(lua, false)));
        const LuaUtil::Callback callback = makeCallback(lua, "return function() return true end");

        EXPECT_THAT([&] { registry.bind("crouch", callback, {}); },
            ThrowsMessage<std::logic_error>(HasSubstr("Unknown action key: \"crouch\"")));
        EXPECT_THAT([&] { registry.bind("jump", callback, { "crouch" }); },
            ThrowsMessage<std::logic_error>(HasSubstr("Unknown action key: \"crouch\"")));
        EXPECT_THAT([&] { registry.registerHandler("crouch", callback); },
            ThrowsMessage<std::logic_error>(HasSubstr("Unknown action key")));
        EXPECT_THAT([&] { registry.valueOfType("crouch", Type::Boolean); },
            ThrowsMessage<std::logic_error>(HasSubstr("Unknown action key")));
    }

    TEST(LuaInputRegistryTest, aValueIsOnlyGivenAsTheTypeOfItsAction)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "jump", Type::Boolean, sol::make_object(lua, false)));
        registry.insert(action(lua, "speed", Type::Number, sol::make_object(lua, 3)));
        registry.insert(action(lua, "trigger", Type::Range, sol::make_object(lua, 0.5)));

        EXPECT_THAT([&] { registry.valueOfType("jump", Type::Number); },
            ThrowsMessage<std::logic_error>(
                AllOf(HasSubstr("type \"Number\""), HasSubstr("action \"jump\""), HasSubstr("type \"Boolean\""))));
        EXPECT_THAT([&] { registry.valueOfType("speed", Type::Range); },
            ThrowsMessage<std::logic_error>(AllOf(HasSubstr("type \"Range\""), HasSubstr("type \"Number\""))));
        EXPECT_THAT([&] { registry.valueOfType("trigger", Type::Boolean); },
            ThrowsMessage<std::logic_error>(AllOf(HasSubstr("type \"Boolean\""), HasSubstr("type \"Range\""))));

        EXPECT_EQ(registry.valueOfType("speed", Type::Number).as<int>(), 3);
        EXPECT_EQ(registry.valueOfType("trigger", Type::Range).as<double>(), 0.5);
    }

    TEST(LuaInputRegistryTest, bindingsAreRunBeforeTheBindingsThatDependOnThem)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "double", Type::Number, sol::make_object(lua, 0.0)));
        registry.insert(action(lua, "speed", Type::Number, sol::make_object(lua, 1.0)));

        // dt, the value of the action and the values of what it depends on
        EXPECT_TRUE(registry.bind("speed", makeCallback(lua, "return function(dt, value) return value + dt end"), {}));
        EXPECT_TRUE(registry.bind(
            "double", makeCallback(lua, "return function(dt, value, speed) return speed * 2 end"), { "speed" }));

        registry.update(0.5);
        EXPECT_EQ(registry.valueOfType("speed", Type::Number).as<double>(), 1.5);
        EXPECT_EQ(registry.valueOfType("double", Type::Number).as<double>(), 3.0);

        registry.update(0.5);
        EXPECT_EQ(registry.valueOfType("speed", Type::Number).as<double>(), 2.0);
        EXPECT_EQ(registry.valueOfType("double", Type::Number).as<double>(), 4.0);
    }

    TEST(LuaInputRegistryTest, aBindingThatWouldMakeACycleIsRefused)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "a", Type::Boolean, sol::make_object(lua, false)));
        registry.insert(action(lua, "b", Type::Boolean, sol::make_object(lua, false)));
        const LuaUtil::Callback callback = makeCallback(lua, "return function() return true end");

        EXPECT_TRUE(registry.bind("a", callback, { "b" }));
        EXPECT_FALSE(registry.bind("b", callback, { "a" }));
    }

    TEST(LuaInputRegistryTest, aBindingThatFailsLeavesTheValueAsItWas)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "speed", Type::Number, sol::make_object(lua, 4.0)));
        registry.insert(action(lua, "jump", Type::Boolean, sol::make_object(lua, false)));
        // The first fails with an error, the second returns something that is not a number
        EXPECT_TRUE(registry.bind("speed", makeCallback(lua, "return function() error('boom') end"), {}));
        EXPECT_TRUE(registry.bind("jump", makeCallback(lua, "return function() return 'not a boolean' end"), {}));

        EXPECT_NO_THROW(registry.update(1.0));

        EXPECT_EQ(registry.valueOfType("speed", Type::Number).as<double>(), 4.0);
    }

    TEST(LuaInputRegistryTest, aBindingOfAnActionThatHasAnInvalidValueAlreadyAndFailsIsLogged)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "speed", Type::Number, sol::make_object(lua, 4.0)));
        // The first binding makes the value of the action invalid, which the second one fails to improve
        EXPECT_TRUE(registry.bind("speed", makeCallback(lua, "return function() return 'text' end"), {}));
        EXPECT_TRUE(registry.bind("speed", makeCallback(lua, "return function() error('boom') end"), {}));

        EXPECT_NO_THROW(registry.update(1.0));
    }

    TEST(LuaInputRegistryTest, theBindingsOfAScriptThatWasRemovedAreDropped)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "speed", Type::Number, sol::make_object(lua, 4.0)));
        EXPECT_TRUE(registry.bind("speed", makeCallback(lua, "return function() return 9 end", false), {}));

        registry.update(1.0);

        EXPECT_EQ(registry.valueOfType("speed", Type::Number).as<double>(), 4.0);
    }

    TEST(LuaInputRegistryTest, aHandlerIsCalledWhenTheValueOfItsActionChanges)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        lua["calls"] = 0;
        lua["last"] = sol::nil;
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "speed", Type::Number, sol::make_object(lua, 1.0)));
        EXPECT_TRUE(registry.bind("speed", makeCallback(lua, "return function(dt, value) return value + dt end"), {}));
        registry.registerHandler(
            "speed", makeCallback(lua, "return function(value) calls = calls + 1; last = value end"));
        // A handler of a script that was removed is dropped without being called
        registry.registerHandler("speed", makeCallback(lua, "return function() calls = calls + 100 end", false));

        registry.update(0.5);
        EXPECT_EQ(lua["calls"].get<int>(), 1);
        EXPECT_EQ(lua["last"].get<double>(), 1.5);

        registry.update(0.5);
        EXPECT_EQ(lua["calls"].get<int>(), 2);
        EXPECT_EQ(lua["last"].get<double>(), 2.0);
    }

    TEST(LuaInputRegistryTest, aHandlerIsNotCalledWhileTheValueStaysTheSame)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        lua["calls"] = 0;
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "jump", Type::Boolean, sol::make_object(lua, false)));
        registry.registerHandler("jump", makeCallback(lua, "return function() calls = calls + 1 end"));

        registry.update(1.0);
        registry.update(1.0);

        EXPECT_EQ(lua["calls"].get<int>(), 0);
    }

    TEST(LuaInputRegistryTest, clearingKeepsTheActionsThatArePersistentUnlessForced)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputAction::Registry registry;
        registry.insert(action(lua, "kept", Type::Boolean, sol::make_object(lua, false), true));
        registry.insert(action(lua, "dropped", Type::Boolean, sol::make_object(lua, false), false));

        registry.clear();
        EXPECT_EQ(registry.size(), 1u);
        EXPECT_EQ(registry.firstKey(), "kept");
        EXPECT_EQ(registry["dropped"], std::nullopt);

        registry.clear(true);
        EXPECT_EQ(registry.size(), 0u);
        EXPECT_EQ(registry.firstKey(), std::nullopt);
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Triggers

    LuaUtil::InputTrigger::Info trigger(const std::string& key, bool persistent = false)
    {
        return { key, "test", key + "_name", key + "_description", persistent };
    }

    TEST(LuaInputTriggerTest, aTriggerHasAUniqueKeyAndALocalizationContext)
    {
        LuaUtil::InputTrigger::Registry registry;
        registry.insert(trigger("fire"));

        EXPECT_THAT(
            [&] { registry.insert(trigger("fire")); }, ThrowsMessage<std::domain_error>(HasSubstr("already in use")));
        EXPECT_THAT([&] { registry.insert(trigger("")); },
            ThrowsMessage<std::domain_error>(HasSubstr("can't be an empty string")));
        LuaUtil::InputTrigger::Info withoutContext = trigger("reload");
        withoutContext.mL10n.clear();
        EXPECT_THAT([&] { registry.insert(withoutContext); },
            ThrowsMessage<std::domain_error>(HasSubstr("Localization context can't be empty")));
    }

    TEST(LuaInputTriggerTest, theKeysOfTheTriggersCanBeWalkedAndLookedUp)
    {
        LuaUtil::InputTrigger::Registry registry;
        EXPECT_EQ(registry.firstKey(), std::nullopt);

        registry.insert(trigger("fire"));
        registry.insert(trigger("reload"));

        std::vector<std::string> keys;
        for (std::optional<std::string> key = registry.firstKey(); key; key = registry.nextKey(*key))
            keys.push_back(*key);
        EXPECT_THAT(keys, UnorderedElementsAre("fire", "reload"));
        EXPECT_EQ(registry.nextKey("unknown"), std::nullopt);

        const std::optional<LuaUtil::InputTrigger::Info> info = registry["reload"];
        ASSERT_TRUE(info.has_value());
        EXPECT_EQ(info->mDescription, "reload_description");
        EXPECT_EQ(registry["unknown"], std::nullopt);
    }

    TEST(LuaInputTriggerTest, activatingATriggerCallsItsHandlersAndDropsTheOnesOfRemovedScripts)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        lua["calls"] = 0;
        LuaUtil::InputTrigger::Registry registry;
        registry.insert(trigger("fire"));
        registry.registerHandler("fire", makeCallback(lua, "return function() calls = calls + 1 end"));
        registry.registerHandler("fire", makeCallback(lua, "return function() calls = calls + 100 end", false));
        registry.registerHandler("fire", makeCallback(lua, "return function() error('boom') end"));

        registry.activate("fire");
        registry.activate("fire");

        EXPECT_EQ(lua["calls"].get<int>(), 2);
    }

    TEST(LuaInputTriggerTest, aTriggerThatIsNotRegisteredCannotBeHandledOrActivated)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        LuaUtil::InputTrigger::Registry registry;
        const LuaUtil::Callback callback = makeCallback(lua, "return function() end");

        EXPECT_THAT([&] { registry.registerHandler("fire", callback); },
            ThrowsMessage<std::domain_error>(HasSubstr("Unknown trigger key \"fire\"")));
        EXPECT_THAT(
            [&] { registry.activate("fire"); }, ThrowsMessage<std::domain_error>(HasSubstr("Unknown trigger key")));
    }

    TEST(LuaInputTriggerTest, clearingKeepsTheTriggersThatArePersistentUnlessForced)
    {
        LuaUtil::InputTrigger::Registry registry;
        registry.insert(trigger("kept", true));
        registry.insert(trigger("dropped", false));

        registry.clear();
        EXPECT_EQ(registry.firstKey(), "kept");
        EXPECT_EQ(registry["dropped"], std::nullopt);

        registry.clear(true);
        EXPECT_EQ(registry.firstKey(), std::nullopt);
    }
}
