#include <apps/openfallout/mwrender/falloutanimation.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace OFRender;

    TEST(OpenFalloutRenderFalloutAnimationFolder, isTheFolderOfTheSkeleton)
    {
        EXPECT_EQ(falloutAnimationFolder("meshes/characters/_male/skeleton.nif"), "meshes/characters/_male/");
        EXPECT_EQ(falloutAnimationFolder("Meshes\\Characters\\_Male\\Skeleton.NIF"), "meshes/characters/_male/");
        EXPECT_EQ(falloutAnimationFolder("skeleton.nif"), "");
    }

    struct OpenFalloutRenderChooseFalloutIdle : Test
    {
        const std::string mFolder = "meshes/characters/_male/";
    };

    TEST_F(OpenFalloutRenderChooseFalloutIdle, takesMtidleBeforeIdle)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/idle.kf", "meshes/characters/_male/mtidle.kf",
            "meshes/characters/_male/h2hidle.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "meshes/characters/_male/mtidle.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, takesIdleBeforeH2hidle)
    {
        const std::vector<std::string> files
            = { "meshes/characters/_male/h2hidle.kf", "meshes/characters/_male/idle.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "meshes/characters/_male/idle.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, takesTheFirstIdleByNameWhenNoneIsPreferred)
    {
        const std::vector<std::string> files = { "meshes/characters/_male/2hridle.kf",
            "meshes/characters/_male/1hpidle.kf", "meshes/characters/_male/forward.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "meshes/characters/_male/1hpidle.kf");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, ignoresFoldersBelowAndOtherFolders)
    {
        const std::vector<std::string> files
            = { "meshes/characters/_male/idleanims/mtidle.kf", "meshes/characters/_female/mtidle.kf",
                  "meshes/characters/_male/idle.nif", "meshes/characters/_male/forward.kf" };
        EXPECT_EQ(chooseFalloutIdle(mFolder, files), "");
    }

    TEST_F(OpenFalloutRenderChooseFalloutIdle, hasNothingWithNoFiles)
    {
        EXPECT_EQ(chooseFalloutIdle(mFolder, {}), "");
    }
}
