#include <components/falloutscript/enableparents.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::EnableParents;
    using FalloutScript::FormId;
    using Followers = std::vector<std::pair<FormId, bool>>;

    constexpr FormId marker = 0x0109D44C;
    constexpr FormId amata = 0x010230E4;
    constexpr FormId dad = 0x01023500;
    constexpr FormId door = 0x01023600;

    TEST(FalloutScriptEnableParents, children_are_enabled_and_disabled_with_the_parent)
    {
        EnableParents parents;
        parents.add(amata, marker, false);
        parents.add(dad, marker, false);
        EXPECT_THAT(parents.followers(marker, false), ElementsAre(Pair(amata, false), Pair(dad, false)));
        EXPECT_THAT(parents.followers(marker, true), ElementsAre(Pair(amata, true), Pair(dad, true)));
    }

    TEST(FalloutScriptEnableParents, an_inversed_child_is_in_the_other_state)
    {
        EnableParents parents;
        parents.add(amata, marker, false);
        parents.add(door, marker, true);
        EXPECT_THAT(parents.followers(marker, false), ElementsAre(Pair(amata, false), Pair(door, true)));
        EXPECT_THAT(parents.followers(marker, true), ElementsAre(Pair(amata, true), Pair(door, false)));
    }

    TEST(FalloutScriptEnableParents, the_children_of_the_children_follow_the_state_they_were_given)
    {
        EnableParents parents;
        parents.add(door, amata, false);
        parents.add(amata, marker, true);
        parents.add(dad, amata, true);
        // Enabling the marker disables Amata, and her own children follow: the door is disabled, Dad is enabled
        EXPECT_THAT(
            parents.followers(marker, false), ElementsAre(Pair(amata, true), Pair(door, true), Pair(dad, false)));
    }

    TEST(FalloutScriptEnableParents, a_reference_that_is_nobodys_parent_has_no_followers)
    {
        EnableParents parents;
        parents.add(amata, marker, false);
        EXPECT_TRUE(parents.followers(amata, true).empty());
        EXPECT_TRUE(parents.followers(dad, true).empty());
        EXPECT_FALSE(parents.empty());
    }

    TEST(FalloutScriptEnableParents, a_loop_of_parents_lists_each_reference_once)
    {
        EnableParents parents;
        parents.add(amata, marker, false);
        parents.add(marker, amata, false);
        parents.add(marker, marker, false);
        parents.add(0, marker, false);
        EXPECT_THAT(parents.followers(marker, true), ElementsAre(Pair(amata, true)));
    }
}
