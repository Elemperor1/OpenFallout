#include "scriptbuilder.hpp"

#include <components/falloutscript/mapmarkers.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::MapMarkers;

    constexpr FormId goodsprings = 0x0009289B;
    constexpr FormId hidden = 0x000CDF13;
    constexpr FormId seen = 0x000CDF06;
    constexpr FormId reachable = 0x0013A66A;
    constexpr FormId barrel = 0x00101010;

    constexpr std::uint16_t OpShowMap = 0x1055;
    constexpr std::uint16_t OpEnableFastTravel = 0x1111;
    constexpr std::uint16_t OpGetMapMarkerVisible = 0x11EC;

    class FakeRecords : public FalloutScript::MapMarkerRecords
    {
    public:
        std::map<FormId, std::uint8_t> mFlags{ { hidden, 0 }, { seen, 1 }, { reachable, 3 }, { goodsprings, 0 } };

        bool markerFlags(FormId marker, std::uint8_t& flags) override
        {
            const auto found = mFlags.find(marker);
            if (found == mFlags.end())
                return false;
            flags = found->second;
            return true;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeRecords mRecords;
        MapMarkers mMarkers{ mRecords };
        std::vector<FormId> mShown;

        Runner()
        {
            FalloutScript::addMapMarkerCommands(
                mInterpreter, mMarkers, [this](FormId marker) { mShown.push_back(marker); });
        }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    TEST(FalloutScriptMapMarkersTest, a_marker_is_shown_as_its_record_says)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, hidden), 0.0);
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, seen), 1.0);
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, reachable), 2.0);
        // What is no marker is not visible, and neither is no reference
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, barrel), 0.0);
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, 0), 0.0);
    }

    TEST(FalloutScriptMapMarkersTest, showing_a_marker_uncovers_it_and_tells_the_player_once)
    {
        Runner run;
        run.call(OpShowMap, 0, { hidden });
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, hidden), 1.0);
        EXPECT_THAT(run.mShown, ElementsAre(hidden));
        // Nothing changes the second time, and the player is not told again
        run.call(OpShowMap, 0, { hidden });
        EXPECT_THAT(run.mShown, ElementsAre(hidden));
    }

    TEST(FalloutScriptMapMarkersTest, a_second_argument_makes_the_marker_a_place_to_travel_to)
    {
        Runner run;
        run.call(OpShowMap, 0, { seen, 0 });
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, seen), 1.0);
        EXPECT_THAT(run.mShown, IsEmpty());
        run.call(OpShowMap, 0, { seen, 1 });
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, seen), 2.0);
        EXPECT_THAT(run.mShown, ElementsAre(seen));
        run.call(OpShowMap, 0, { goodsprings, 1 });
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, goodsprings), 2.0);
    }

    TEST(FalloutScriptMapMarkersTest, what_is_no_marker_cannot_be_shown)
    {
        Runner run;
        run.call(OpShowMap, 0, { barrel });
        run.call(OpShowMap, 0, {});
        EXPECT_THAT(run.mShown, IsEmpty());
        EXPECT_EQ(run.call(OpGetMapMarkerVisible, barrel), 0.0);
    }

    TEST(FalloutScriptMapMarkersTest, fast_travel_is_allowed_in_a_new_game)
    {
        Runner run;
        EXPECT_TRUE(run.mMarkers.fastTravelAllowed());
        EXPECT_TRUE(run.mMarkers.restAllowed());
        EXPECT_FALSE(run.mMarkers.travelSwitch());
    }

    TEST(FalloutScriptMapMarkersTest, the_switches_of_fast_travel_follow_the_arguments)
    {
        Runner run;
        run.call(OpEnableFastTravel, 0, { 0, 0 });
        EXPECT_FALSE(run.mMarkers.fastTravelAllowed());
        EXPECT_FALSE(run.mMarkers.restAllowed());
        run.call(OpEnableFastTravel, 0, { 0 });
        EXPECT_TRUE(run.mMarkers.restAllowed());
        // Turning travel on allows resting again, and the third switch is set when it is 1
        run.call(OpEnableFastTravel, 0, { 0, 0, 1 });
        EXPECT_TRUE(run.mMarkers.travelSwitch());
        run.call(OpEnableFastTravel, 0, { 0, 0, 0 });
        EXPECT_TRUE(run.mMarkers.travelSwitch());
        run.call(OpEnableFastTravel, 0, { 1, 0, 0 });
        EXPECT_TRUE(run.mMarkers.fastTravelAllowed());
        EXPECT_TRUE(run.mMarkers.restAllowed());
        EXPECT_FALSE(run.mMarkers.travelSwitch());
    }

    TEST(FalloutScriptMapMarkersTest, a_saved_game_brings_back_the_markers_and_the_switches)
    {
        Runner run;
        run.call(OpShowMap, 0, { hidden, 1 });
        run.call(OpEnableFastTravel, 0, { 0, 0, 1 });
        FalloutScript::SaveWriter out;
        FalloutScript::writeMapMarkers(run.mMarkers, out);

        Runner again;
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readMapMarkers(again.mMarkers, in, FalloutScript::mapMarkersSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(again.call(OpGetMapMarkerVisible, hidden), 2.0);
        EXPECT_FALSE(again.mMarkers.fastTravelAllowed());
        EXPECT_FALSE(again.mMarkers.restAllowed());
        EXPECT_TRUE(again.mMarkers.travelSwitch());

        again.mMarkers.clear();
        EXPECT_EQ(again.call(OpGetMapMarkerVisible, hidden), 0.0);
        EXPECT_TRUE(again.mMarkers.fastTravelAllowed());
    }
}
