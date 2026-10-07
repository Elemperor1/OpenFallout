#include <components/esmterrain/texturepath.hpp>

#include <components/testing/util.hpp>

#include <gtest/gtest.h>

namespace ESMTerrain
{
    namespace
    {
        using namespace testing;
        using namespace TestingOpenMW;

        VFSTestFile file("");

        TEST(ESMTerrainTexturePath, a_path_of_a_texture_set_is_relative_to_the_textures_directory)
        {
            const auto vfs
                = createTestVFS({ { VFS::Path::NormalizedView("textures/landscape/dirtwasteland01.dds"), &file } });

            // Fallout writes the paths with backslashes and in any case
            EXPECT_EQ(findTexturePath(vfs.get(), "Landscape\\DirtWasteland01.dds", false).value(),
                "textures/landscape/dirtwasteland01.dds");
        }

        TEST(ESMTerrainTexturePath, a_path_that_has_the_textures_directory_is_not_given_it_again)
        {
            const auto vfs = createTestVFS({ { VFS::Path::NormalizedView("textures/landscape/sand.dds"), &file } });

            EXPECT_EQ(findTexturePath(vfs.get(), "textures\\landscape\\sand.dds", false).value(),
                "textures/landscape/sand.dds");
        }

        TEST(ESMTerrainTexturePath, the_filename_of_an_oblivion_record_is_relative_to_the_landscape_directory)
        {
            const auto vfs = createTestVFS({ { VFS::Path::NormalizedView("textures/landscape/cliff01.dds"), &file } });

            EXPECT_EQ(findTexturePath(vfs.get(), "Cliff01.dds", true).value(), "textures/landscape/cliff01.dds");
        }

        TEST(ESMTerrainTexturePath, the_filename_of_a_fallout_icon_is_found_in_the_textures_directory_too)
        {
            // A path with the landscape directory in it, relative to the textures directory
            const auto vfs = createTestVFS({ { VFS::Path::NormalizedView("textures/landscape/dirt.dds"), &file } });

            EXPECT_EQ(findTexturePath(vfs.get(), "Landscape\\Dirt.dds", true).value(), "textures/landscape/dirt.dds");
        }

        TEST(ESMTerrainTexturePath, a_texture_that_no_data_file_has_is_looked_for_where_the_oldest_games_have_it)
        {
            const auto vfs = createTestVFS({ { VFS::Path::NormalizedView("textures/other.dds"), &file } });

            EXPECT_EQ(
                findTexturePath(vfs.get(), "landscape/missing.dds", false).value(), "textures/landscape/missing.dds");
            EXPECT_EQ(findTexturePath(vfs.get(), "missing.dds", true).value(), "textures/landscape/missing.dds");
            EXPECT_EQ(findTexturePath(nullptr, "missing.dds", false).value(), "textures/missing.dds");
        }

        TEST(ESMTerrainTexturePath, a_path_that_has_the_landscape_directory_is_not_given_it_again)
        {
            const auto vfs = createTestVFS({ { VFS::Path::NormalizedView("textures/other.dds"), &file } });

            // Not textures/landscape/landscape/, which no game has, in the path that the log says is missing
            EXPECT_EQ(
                findTexturePath(vfs.get(), "Landscape\\missing.dds", true).value(), "textures/landscape/missing.dds");
        }
    }
}
