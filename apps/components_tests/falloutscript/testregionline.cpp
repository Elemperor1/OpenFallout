#include <components/falloutscript/regionline.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace FalloutScript::RegionLine;

    Candidate region(
        FalloutScript::FormId id, const char* name, bool weather = false, bool override = false, int priority = 0)
    {
        Candidate candidate;
        candidate.mRegion = id;
        candidate.mName = name;
        candidate.mHasWeather = weather;
        candidate.mOverride = override;
        candidate.mPriority = priority;
        return candidate;
    }

    TEST(FalloutScriptRegionLineTest, nothingIsPickedWhenNoRegionCoversThePlace)
    {
        EXPECT_FALSE(pick({}).has_value());
    }

    TEST(FalloutScriptRegionLineTest, theFirstRegionIsPickedWhenNoneHasWeather)
    {
        const std::vector<Candidate> covering = { region(1, "A"), region(2, "B"), region(3, "C") };
        EXPECT_EQ(pick(covering), 0u);
    }

    TEST(FalloutScriptRegionLineTest, aLaterRegionNeedsWeatherToReplaceTheFirst)
    {
        // The second has none, the third has: it replaces the first that has none
        const std::vector<Candidate> covering = { region(1, "A"), region(2, "B"), region(3, "C", true) };
        EXPECT_EQ(pick(covering), 2u);
        // A region with no weather never replaces one that has
        const std::vector<Candidate> kept = { region(1, "A", true, false, 1), region(2, "B", false, true, 9) };
        EXPECT_EQ(pick(kept), 0u);
    }

    TEST(FalloutScriptRegionLineTest, theOverrideDecidesThenThePriorityAndTiesKeepTheEarlierOne)
    {
        // Override beats priority
        const std::vector<Candidate> overriding = { region(1, "A", true, false, 9), region(2, "B", true, true, 1) };
        EXPECT_EQ(pick(overriding), 1u);
        // Equal overrides: the higher priority wins, an equal one does not replace
        const std::vector<Candidate> priority
            = { region(1, "A", true, false, 3), region(2, "B", true, false, 4), region(3, "C", true, false, 4) };
        EXPECT_EQ(pick(priority), 1u);
        // A region whose override is not set does not replace one that has it
        const std::vector<Candidate> held = { region(1, "A", true, true, 1), region(2, "B", true, false, 9) };
        EXPECT_EQ(pick(held), 0u);
    }

    TEST(FalloutScriptRegionLineTest, aNamedRegionIsShownOnceAndNotAgainWhileThePlayerStaysInIt)
    {
        Announcer line;
        const std::vector<Candidate> mojave = { region(7, "Mojave Wasteland") };
        const std::optional<std::string> first = line.enter(true, mojave);
        ASSERT_TRUE(first.has_value());
        EXPECT_EQ(*first, "Mojave Wasteland");
        EXPECT_EQ(line.region(), 7u);
        // The next cell of the same region
        EXPECT_FALSE(line.enter(true, mojave).has_value());
    }

    TEST(FalloutScriptRegionLineTest, aBuildingKeepsTheRegionAndShowsNothing)
    {
        Announcer line;
        ASSERT_TRUE(line.enter(true, { region(7, "Strip") }).has_value());
        EXPECT_FALSE(line.enter(false, {}).has_value());
        EXPECT_EQ(line.region(), 7u);
        // Out of the building into the same region: not shown again
        EXPECT_FALSE(line.enter(true, { region(7, "Strip") }).has_value());
    }

    TEST(FalloutScriptRegionLineTest, aPlaceOfNoRegionOrOfANamelessOneForgetsWhatWasShownLast)
    {
        Announcer line;
        ASSERT_TRUE(line.enter(true, { region(7, "A") }).has_value());
        // No region covers the next cell: nothing is shown, and the one before is shown again when the player is back
        // in it
        EXPECT_FALSE(line.enter(true, {}).has_value());
        EXPECT_EQ(line.region(), 0u);
        const std::optional<std::string> again = line.enter(true, { region(7, "A") });
        ASSERT_TRUE(again.has_value());
        EXPECT_EQ(*again, "A");
        // A region with no name shows nothing but is the one that was last
        EXPECT_FALSE(line.enter(true, { region(8, "") }).has_value());
        EXPECT_TRUE(line.enter(true, { region(7, "A") }).has_value());
    }

    TEST(FalloutScriptRegionLineTest, aNewGameShowsTheRegionAgain)
    {
        Announcer line;
        ASSERT_TRUE(line.enter(true, { region(7, "A") }).has_value());
        line.reset();
        EXPECT_TRUE(line.enter(true, { region(7, "A") }).has_value());
    }
}
