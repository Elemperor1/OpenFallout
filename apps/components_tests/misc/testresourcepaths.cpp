#include <components/esm/common.hpp>
#include <components/esm/refid.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/testing/util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <components/vfs/filemap.hpp>

#include <memory>
#include <stdexcept>
#include <string>

// The places where the game looks for icons, book art, models, materials, sounds and distant meshes. The files of the
// games name them in the way that the game of that time did, so the helpers have to find them wherever the files
// ended up.
namespace Misc::ResourceHelpers
{
    namespace
    {
        using namespace ::testing;

        std::unique_ptr<const VFS::Manager> makeVfs(std::initializer_list<std::string_view> paths)
        {
            VFS::FileMap files;
            for (const std::string_view path : paths)
                files.emplace(VFS::Path::Normalized(path), nullptr);
            return TestingOpenMW::createTestVFS(std::move(files));
        }

        TEST(MiscResourceHelpersIconPaths, iconsAreSearchedInTheIconDirectoryWithTheDdsExtension)
        {
            const auto vfs = makeVfs({ "icons/w/sword.dds" });

            EXPECT_EQ(correctIconPath(VFS::Path::NormalizedView("w/sword.tga"), *vfs), "icons/w/sword.dds");
            EXPECT_EQ(correctIconPath(VFS::Path::NormalizedView("icons/w/sword.tga"), *vfs), "icons/w/sword.dds");
        }

        TEST(MiscResourceHelpersIconPaths, iconWithTheOriginalExtensionIsUsedWhenThereIsNoDds)
        {
            const auto vfs = makeVfs({ "icons/w/sword.tga" });

            EXPECT_EQ(correctIconPath(VFS::Path::NormalizedView("icons/w/sword.tga"), *vfs), "icons/w/sword.tga");
        }

        TEST(MiscResourceHelpersIconPaths, bigIconHasTheNameOfTheIconWithAPrefix)
        {
            const auto vfs = makeVfs({ "icons/w/b_sword.dds" });

            EXPECT_EQ(correctBigIconPath(VFS::Path::NormalizedView("icons/w/sword.tga"), *vfs), "icons/w/b_sword.dds");
            EXPECT_EQ(correctBigIconPath(VFS::Path::NormalizedView("sword.tga"), *vfs), "icons/b_sword.dds");
        }

        TEST(
            MiscResourceHelpersBookartPaths, bookartIsSearchedInTheBookartDirectoryOrInTheTexturesThatTheNameStartsWith)
        {
            const auto vfs = makeVfs({ "textures/book/page.dds", "bookart/cover.dds" });

            EXPECT_EQ(correctBookartPath(VFS::Path::NormalizedView("textures/book/page.tga"), *vfs),
                "textures/book/page.dds");
            EXPECT_EQ(correctBookartPath(VFS::Path::NormalizedView("cover.tga"), *vfs), "bookart/cover.dds");
        }

        TEST(MiscResourceHelpersBookartPaths, bookartOfASizeKeepsTheSizeInItsNameWhenTheImageDoesNotExist)
        {
            const auto vfs = makeVfs({ "bookart/page.dds", "bookart/page_100_200.dds" });

            EXPECT_EQ(
                correctBookartPath(VFS::Path::NormalizedView("bookart/page.tga"), 100, 200, *vfs), "bookart/page.dds");

            const auto sized = makeVfs({ "bookart/page_100_200.dds" });
            EXPECT_EQ(correctBookartPath(VFS::Path::NormalizedView("bookart/page.tga"), 100, 200, *sized),
                "bookart/page_100_200.dds");
        }

        TEST(MiscResourceHelpersActorModelPaths, actorUsesTheModelThatHasTheXPrefixWhenItsAnimationExists)
        {
            const auto vfs = makeVfs({ "meshes/creature/xbear.kf" });

            EXPECT_EQ(correctActorModelPath(VFS::Path::NormalizedView("meshes/creature/bear.nif"), vfs.get()),
                "meshes/creature/xbear.nif");
            EXPECT_EQ(correctActorModelPath(VFS::Path::NormalizedView("meshes/creature/wolf.nif"), vfs.get()),
                "meshes/creature/wolf.nif");
        }

        TEST(MiscResourceHelpersActorModelPaths, actorModelWithoutADirectoryGetsThePrefixInFront)
        {
            const auto vfs = makeVfs({ "xbear.kf" });

            EXPECT_EQ(correctActorModelPath(VFS::Path::NormalizedView("bear.nif"), vfs.get()), "xbear.nif");
        }

        TEST(MiscResourceHelpersActorModelPaths, actorModelThatIsNotANifKeepsItsExtensionWhenLookingForTheAnimation)
        {
            const auto vfs = makeVfs({ "meshes/xbear.dae" });

            EXPECT_EQ(
                correctActorModelPath(VFS::Path::NormalizedView("meshes/bear.dae"), vfs.get()), "meshes/xbear.dae");
        }

        TEST(MiscResourceHelpersMaterialPaths, materialsAreSearchedInTheMaterialDirectory)
        {
            const auto vfs = makeVfs({ "materials/wall.bgsm" });

            EXPECT_EQ(correctMaterialPath(VFS::Path::NormalizedView("wall.bgsm"), *vfs), "materials/wall.bgsm");
            EXPECT_EQ(
                correctMaterialPath(VFS::Path::NormalizedView("materials/floor.bgsm"), *vfs), "materials/floor.bgsm");
        }

        TEST(MiscResourceHelpersPrefixes, meshesSoundsAndMusicHaveTheirDirectoriesInFront)
        {
            EXPECT_EQ(correctMeshPath(VFS::Path::NormalizedView("a/b.nif")), "meshes/a/b.nif");
            EXPECT_EQ(correctSoundPath(VFS::Path::NormalizedView("a/b.wav")), "sound/a/b.wav");
            EXPECT_EQ(correctMusicPath(VFS::Path::NormalizedView("a/b.mp3")), "music/a/b.mp3");
        }

        TEST(MiscResourceHelpersPrefixes, pathsOfOldPluginsLoseTheDirectoryThatTheyStartWith)
        {
            EXPECT_EQ(meshPathForESM3("meshes\\a\\b.nif"), "a\\b.nif");
            EXPECT_EQ(meshPathForESM3("MESHES/a/b.nif"), "a/b.nif");
            EXPECT_EQ(soundPathForESM3("Sound\\fx\\boom.wav"), "fx\\boom.wav");
        }

        TEST(MiscResourceHelpersPrefixes, pathThatDoesNotStartWithTheDirectoryIsAnError)
        {
            EXPECT_THROW(meshPathForESM3("textures\\a\\b.nif"), std::runtime_error);
            EXPECT_THROW(meshPathForESM3("meshes"), std::runtime_error);
            EXPECT_THROW(meshPathForESM3("meshesx\\b.nif"), std::runtime_error);
            EXPECT_THROW(soundPathForESM3("fx\\boom.wav"), std::runtime_error);
        }

        TEST(MiscResourceHelpersMarkers, hiddenMarkersAreTheOnesWithAFunctionInTheGameLogic)
        {
            for (const char* marker : { "prisonmarker", "divinemarker", "templemarker", "northmarker" })
                EXPECT_TRUE(isHiddenMarker(ESM::RefId::stringRefId(marker))) << marker;
            EXPECT_FALSE(isHiddenMarker(ESM::RefId::stringRefId("chest")));
            EXPECT_FALSE(isHiddenMarker(ESM::RefId()));
        }

        struct DistantMeshCase
        {
            int mVersion;
            const char* mSuffix;
        };

        struct MiscResourceHelpersDistantMeshes : TestWithParam<DistantMeshCase>
        {
        };

        TEST_P(MiscResourceHelpersDistantMeshes, meshOfTheLevelOfDetailIsTheBestOneThatExists)
        {
            const std::string suffix = GetParam().mSuffix;
            const auto vfs = makeVfs({ "meshes/tree.nif", "meshes/tree" + suffix + "_1.nif" });

            // The level that is asked for does not exist, so the next level down is used.
            EXPECT_EQ(getLODMeshName(GetParam().mVersion, VFS::Path::NormalizedView("meshes/tree.nif"), *vfs, 3),
                "meshes/tree" + suffix + "_1.nif");
        }

        TEST_P(MiscResourceHelpersDistantMeshes, meshWithoutALevelOfDetailIsTheMeshItself)
        {
            const auto vfs = makeVfs({ "meshes/tree.nif" });

            EXPECT_EQ(getLODMeshName(GetParam().mVersion, VFS::Path::NormalizedView("meshes/tree.nif"), *vfs, 2),
                "meshes/tree.nif");
        }

        TEST_P(MiscResourceHelpersDistantMeshes, meshThatHasOnlyTheGenericLevelIsUsedLast)
        {
            const std::string suffix = GetParam().mSuffix;
            const auto vfs = makeVfs({ "meshes/tree.nif", "meshes/tree" + suffix + ".nif" });

            EXPECT_EQ(getLODMeshName(GetParam().mVersion, VFS::Path::NormalizedView("meshes/tree.nif"), *vfs),
                "meshes/tree" + suffix + ".nif");
        }

        TEST_P(MiscResourceHelpersDistantMeshes, meshWithoutAnExtensionHasNoLevelOfDetail)
        {
            const auto vfs = makeVfs({ "meshes/tree" });

            EXPECT_EQ(
                getLODMeshName(GetParam().mVersion, VFS::Path::NormalizedView("meshes/tree"), *vfs, 1), "meshes/tree");
        }

        INSTANTIATE_TEST_SUITE_P(Games, MiscResourceHelpersDistantMeshes,
            Values(DistantMeshCase{ ESM::VER_120, "_dist" }, DistantMeshCase{ ESM::VER_130, "_dist" },
                DistantMeshCase{ ESM::VER_080, "_far" }, DistantMeshCase{ ESM::VER_100, "_far" },
                DistantMeshCase{ ESM::VER_094, "_lod" }, DistantMeshCase{ ESM::VER_170, "_lod" }));

        TEST(MiscResourceHelpersDistantMeshes, gameWithoutAPatternKeepsTheLevelNumberOnly)
        {
            const auto vfs = makeVfs({ "meshes/tree.nif", "meshes/tree_1.nif" });

            EXPECT_EQ(getLODMeshName(0, VFS::Path::NormalizedView("meshes/tree.nif"), *vfs, 1), "meshes/tree_1.nif");
        }
    }
}
