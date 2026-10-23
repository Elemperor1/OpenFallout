#include "scriptbuilder.hpp"

#include <components/falloutscript/extendercommands.hpp>
#include <components/falloutscript/queries.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::SkyWeather;

    constexpr FormId clerk = 0x01000100;
    constexpr FormId item = 0x01000101;
    constexpr FormId zoneGoodsprings = 0x01000200;
    constexpr FormId zoneOther = 0x01000201;

    constexpr std::uint16_t OpIsRaining = 0x103E;
    constexpr std::uint16_t OpIsPCSleeping = 0x10AF;
    constexpr std::uint16_t OpGetGameSetting = 0x1100;
    constexpr std::uint16_t OpIsInInterior = 0x112C;
    constexpr std::uint16_t OpIsActorTalkingThroughActivator = 0x11AE;
    constexpr std::uint16_t OpGetInZone = 0x11BE;
    constexpr std::uint16_t OpHasLoaded3D = 0x122E;
    constexpr std::uint16_t OpGetParentCell = 0x146D;
    constexpr std::uint16_t OpGetParentWorldspace = 0x146E;
    constexpr std::uint16_t OpGetGameRestarted = 0x14A8;
    constexpr std::uint16_t OpGetGameLoaded = 0x14AC;

    class FakeQueries : public FalloutScript::QueryWorld
    {
    public:
        std::set<FormId> mInteriors;
        std::set<FormId> mLoaded;
        SkyWeather mSky;
        bool mSleeping = false;
        std::map<FormId, FormId> mZones;
        std::set<FormId> mTalking;
        std::map<std::string, double> mSettings;
        std::map<FormId, FormId> mCells;
        std::map<FormId, FormId> mWorldspaces;
        std::set<FormId> mRestarted;
        std::set<FormId> mLoaded3;

        bool inInterior(FormId reference) override { return mInteriors.count(reference) != 0; }
        bool hasLoaded3D(FormId reference) override { return mLoaded.count(reference) != 0; }
        SkyWeather sky() override { return mSky; }
        bool playerSleeping() override { return mSleeping; }
        bool inZone(FormId reference, FormId zone) override
        {
            const auto found = mZones.find(reference);
            return found != mZones.end() && found->second == zone;
        }
        bool talksThroughActivator(FormId actor) override { return mTalking.count(actor) != 0; }
        FormId parentCell(FormId reference) override
        {
            const auto found = mCells.find(reference);
            return found == mCells.end() ? 0 : found->second;
        }
        FormId parentWorldspace(FormId reference) override
        {
            const auto found = mWorldspaces.find(reference);
            return found == mWorldspaces.end() ? 0 : found->second;
        }
        bool gameRestarted(FormId script) override { return mRestarted.insert(script).second; }
        bool gameLoaded(FormId script) override { return mLoaded3.insert(script).second; }
        std::optional<double> gameSetting(std::string_view name) override
        {
            const auto found = mSettings.find(std::string(name));
            if (found == mSettings.end())
                return std::nullopt;
            return found->second;
        }
    };

    struct Runner
    {
        static FalloutScript::CommandTable table()
        {
            FalloutScript::CommandTable result = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
            FalloutScript::addExtenderCommands(result, FalloutScript::Game::NewVegas);
            return result;
        }

        FalloutScript::CommandTable mCommands = table();
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeQueries mWorld;

        Runner() { FalloutScript::addQueryCommands(mInterpreter, mWorld); }

        double call(std::uint16_t opcode, FormId reference, std::vector<FalloutScript::Value> arguments = {})
        {
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    SkyWeather::Weather weather(std::uint8_t classification, std::uint8_t begin = 0, std::uint8_t end = 0)
    {
        SkyWeather::Weather result;
        result.mPresent = true;
        result.mClassification = classification;
        result.mPrecipitationBeginFadeIn = begin;
        result.mPrecipitationEndFadeOut = end;
        return result;
    }

    constexpr std::uint8_t pleasant = 0x01;
    constexpr std::uint8_t cloudy = 0x02;
    constexpr std::uint8_t rainyWeather = 0x04;
    constexpr std::uint8_t snow = 0x08;

    TEST(FalloutScriptQueriesTest, it_rains_when_the_weather_in_force_is_rainy)
    {
        SkyWeather sky;
        sky.mCurrent = weather(rainyWeather);
        EXPECT_TRUE(FalloutScript::isRaining(sky));
        sky.mCurrent = weather(pleasant);
        EXPECT_FALSE(FalloutScript::isRaining(sky));
        // Snow and clouds are not rain, and a weather that is also rainy is
        sky.mCurrent = weather(snow | cloudy);
        EXPECT_FALSE(FalloutScript::isRaining(sky));
        sky.mCurrent = weather(snow | rainyWeather);
        EXPECT_TRUE(FalloutScript::isRaining(sky));
    }

    TEST(FalloutScriptQueriesTest, a_sky_with_no_weather_does_not_rain)
    {
        EXPECT_FALSE(FalloutScript::isRaining(SkyWeather()));
    }

    TEST(FalloutScriptQueriesTest, the_rain_of_a_new_weather_starts_where_the_record_says)
    {
        SkyWeather sky;
        sky.mOutgoing = weather(pleasant);
        // The rain begins at half of the change
        sky.mCurrent = weather(rainyWeather, 128);
        sky.mFraction = 0.4;
        EXPECT_FALSE(FalloutScript::isRaining(sky));
        sky.mFraction = 0.6;
        EXPECT_TRUE(FalloutScript::isRaining(sky));
        // A rain that begins at once is there as soon as the change has started
        sky.mCurrent = weather(rainyWeather, 0);
        sky.mFraction = 0.01;
        EXPECT_TRUE(FalloutScript::isRaining(sky));
    }

    TEST(FalloutScriptQueriesTest, the_rain_of_the_old_weather_goes_on_until_the_record_says_it_ends)
    {
        SkyWeather sky;
        sky.mCurrent = weather(pleasant, 0, 0);
        // The rain ends at a quarter of the change
        sky.mOutgoing = weather(rainyWeather, 0, 64);
        sky.mFraction = 0.1;
        EXPECT_TRUE(FalloutScript::isRaining(sky));
        sky.mFraction = 0.5;
        EXPECT_FALSE(FalloutScript::isRaining(sky));
        // A weather that was settled is not the one that is left
        sky.mOutgoing = SkyWeather::Weather();
        sky.mFraction = 0.1;
        EXPECT_FALSE(FalloutScript::isRaining(sky));
    }

    TEST(FalloutScriptQueriesTest, the_command_tells_whether_it_rains_for_any_object_it_is_called_on)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsRaining, 0), 0.0);
        run.mWorld.mSky.mCurrent = weather(rainyWeather);
        EXPECT_EQ(run.call(OpIsRaining, 0), 1.0);
        EXPECT_EQ(run.call(OpIsRaining, clerk), 1.0);
    }

    TEST(FalloutScriptQueriesTest, a_reference_is_in_an_interior_when_the_world_says_so)
    {
        Runner run;
        run.mWorld.mInteriors.insert(clerk);
        EXPECT_EQ(run.call(OpIsInInterior, clerk), 1.0);
        EXPECT_EQ(run.call(OpIsInInterior, item), 0.0);
        EXPECT_EQ(run.call(OpIsInInterior, 0), 0.0);
    }

    TEST(FalloutScriptQueriesTest, a_reference_has_loaded_3d_when_the_world_has_its_model)
    {
        Runner run;
        run.mWorld.mLoaded.insert(clerk);
        EXPECT_EQ(run.call(OpHasLoaded3D, clerk), 1.0);
        EXPECT_EQ(run.call(OpHasLoaded3D, item), 0.0);
        EXPECT_EQ(run.call(OpHasLoaded3D, 0), 0.0);
    }

    TEST(FalloutScriptQueriesTest, the_player_sleeps_whatever_the_command_is_called_on)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsPCSleeping, clerk), 0.0);
        run.mWorld.mSleeping = true;
        EXPECT_EQ(run.call(OpIsPCSleeping, clerk), 1.0);
        EXPECT_EQ(run.call(OpIsPCSleeping, 0), 1.0);
    }

    TEST(FalloutScriptQueriesTest, a_reference_is_in_the_zone_the_world_gives_it)
    {
        Runner run;
        run.mWorld.mZones[clerk] = zoneGoodsprings;
        EXPECT_EQ(run.call(OpGetInZone, clerk, { { zoneGoodsprings, {}, false } }), 1.0);
        EXPECT_EQ(run.call(OpGetInZone, clerk, { { zoneOther, {}, false } }), 0.0);
        EXPECT_EQ(run.call(OpGetInZone, item, { { zoneGoodsprings, {}, false } }), 0.0);
        // No zone and no reference are in no zone
        EXPECT_EQ(run.call(OpGetInZone, clerk, { { 0, {}, false } }), 0.0);
        EXPECT_EQ(run.call(OpGetInZone, 0, { { zoneGoodsprings, {}, false } }), 0.0);
    }

    TEST(FalloutScriptQueriesTest, an_actor_talks_through_an_activator_while_one_speaks_for_it)
    {
        Runner run;
        run.mWorld.mTalking.insert(clerk);
        EXPECT_EQ(run.call(OpIsActorTalkingThroughActivator, clerk), 1.0);
        EXPECT_EQ(run.call(OpIsActorTalkingThroughActivator, item), 0.0);
        EXPECT_EQ(run.call(OpIsActorTalkingThroughActivator, 0), 0.0);
    }

    TEST(FalloutScriptQueriesTest, a_game_setting_is_its_number_and_a_setting_without_one_is_zero)
    {
        Runner run;
        run.mWorld.mSettings["fReputationThresholdOne"] = 0.15;
        EXPECT_DOUBLE_EQ(run.call(OpGetGameSetting, 0, { { 0, "fReputationThresholdOne", true } }), 0.15);
        EXPECT_EQ(run.call(OpGetGameSetting, 0, { { 0, "fNoSuchSetting", true } }), 0.0);
        // The name has to be a name
        EXPECT_EQ(run.call(OpGetGameSetting, 0, { { 5, {}, false } }), 0.0);
        EXPECT_EQ(run.call(OpGetGameSetting, 0, {}), 0.0);
        EXPECT_THAT(run.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptQueriesTest, the_cell_and_the_worldspace_of_a_reference_are_what_the_host_says)
    {
        Runner run;
        run.mWorld.mCells[clerk] = zoneGoodsprings;
        run.mWorld.mWorldspaces[clerk] = zoneOther;
        EXPECT_EQ(run.call(OpGetParentCell, clerk), static_cast<double>(zoneGoodsprings));
        EXPECT_EQ(run.call(OpGetParentWorldspace, clerk), static_cast<double>(zoneOther));
        // A reference in a building has no worldspace, and none has no cell
        run.mWorld.mWorldspaces.erase(clerk);
        EXPECT_EQ(run.call(OpGetParentWorldspace, clerk), 0.0);
        EXPECT_EQ(run.call(OpGetParentCell, item), 0.0);
        EXPECT_EQ(run.call(OpGetParentCell, 0), 0.0);
    }

    TEST(FalloutScriptQueriesTest, a_script_is_told_once_that_the_game_started_and_once_that_it_was_loaded)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetGameRestarted, 0), 1.0);
        EXPECT_EQ(run.call(OpGetGameRestarted, 0), 0.0);
        EXPECT_EQ(run.call(OpGetGameLoaded, 0), 1.0);
        EXPECT_EQ(run.call(OpGetGameLoaded, 0), 0.0);
    }
}
