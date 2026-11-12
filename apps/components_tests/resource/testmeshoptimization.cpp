#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/optimizer.hpp>
#include <components/testing/util.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace TestingOpenMW;

    // the options that change the nodes and the vertices of a mesh
    constexpr unsigned int changing = SceneUtil::Optimizer::MERGE_GEOMETRY
        | SceneUtil::Optimizer::REMOVE_REDUNDANT_NODES | SceneUtil::Optimizer::FLATTEN_STATIC_TRANSFORMS;

    constexpr VFS::Path::NormalizedView hairOne("meshes/characters/hair/hairone.nif");
    constexpr VFS::Path::NormalizedView hairOneMorphs("meshes/characters/hair/hairone.egm");
    constexpr VFS::Path::NormalizedView hairTwo("meshes/characters/hair/hairtwo.nif");
    constexpr VFS::Path::NormalizedView hairShapes("meshes/characters/hair/hairshapes.nif");
    constexpr VFS::Path::NormalizedView hairShapesHat("meshes/characters/hair/hairshapeshat.egm");
    constexpr VFS::Path::NormalizedView hairShapesNoHat("meshes/characters/hair/hairshapesnohat.egm");
    constexpr VFS::Path::NormalizedView hairHatOnly("meshes/characters/hair/hairhatonly.nif");
    constexpr VFS::Path::NormalizedView hairHatOnlyHat("meshes/characters/hair/hairhatonlyhat.egm");
    constexpr VFS::Path::NormalizedView folderWithDot("meshes/characters/head.d/headhuman.nif");
    constexpr VFS::Path::NormalizedView folderWithDotMorphs("meshes/characters/head.egm");
    constexpr VFS::Path::NormalizedView noExtension("meshes/characters/noextension");
    constexpr VFS::Path::NormalizedView noExtensionMorphs("meshes/characters/noextension.egm");

    TEST(ResourceMeshOptimization, leavesTheNodesAndVerticesOfAHairWithMorphFilesForItsShapesAsTheyAre)
    {
        VFSTestFile file("");
        const std::unique_ptr<VFS::Manager> vfs = createTestVFS({
            { hairShapes, &file },
            { hairShapesHat, &file },
            { hairShapesNoHat, &file },
            { hairHatOnly, &file },
            { hairHatOnlyHat, &file },
            { hairTwo, &file },
        });

        const unsigned int plain = Resource::getMeshOptimizationOptions(*vfs, hairTwo);
        EXPECT_EQ(plain & changing, changing);

        // the shapes of a hair for a head with a hat and one without have a file each, named after the model
        EXPECT_EQ(Resource::getMeshOptimizationOptions(*vfs, hairShapes) & changing, 0u);
        EXPECT_EQ(Resource::getMeshOptimizationOptions(*vfs, hairHatOnly) & changing, 0u);
        EXPECT_EQ(Resource::getMeshOptimizationOptions(*vfs, hairShapes), plain & ~changing);
    }

    TEST(ResourceMeshOptimization, leavesTheNodesAndVerticesOfAMeshWithAFaceGenMorphFileAsTheyAre)
    {
        VFSTestFile file("");
        const std::unique_ptr<VFS::Manager> vfs = createTestVFS({
            { hairOne, &file },
            { hairOneMorphs, &file },
            { hairTwo, &file },
            { folderWithDot, &file },
            { folderWithDotMorphs, &file },
            { noExtension, &file },
            { noExtensionMorphs, &file },
        });

        const unsigned int plain = Resource::getMeshOptimizationOptions(*vfs, hairTwo);
        EXPECT_EQ(plain & changing, changing);

        // geometries are not merged, groups are not merged and transforms are not flattened
        const unsigned int morphed = Resource::getMeshOptimizationOptions(*vfs, hairOne);
        EXPECT_EQ(morphed & changing, 0u);
        // nothing else is left out
        EXPECT_EQ(morphed, plain & ~changing);

        // a dot in the name of a folder is not the extension of the file, and a file with none has no morph file
        EXPECT_EQ(Resource::getMeshOptimizationOptions(*vfs, folderWithDot) & changing, changing);
        EXPECT_EQ(Resource::getMeshOptimizationOptions(*vfs, noExtension) & changing, changing);
    }

    TEST(ResourceMeshOptimization, leavesTheNodesThatAFalloutSkeletonAndAWeaponAreHeldByAlone)
    {
        // the node that a weapon is put on in a character's hands and the one a bullet comes out of hold no shape
        EXPECT_TRUE(Resource::isReservedNodeName("Weapon"));
        EXPECT_TRUE(Resource::isReservedNodeName("weapon"));
        EXPECT_TRUE(Resource::isReservedNodeName("ProjectileNode"));
        // the bones, the camera of the arms and what Morrowind names
        EXPECT_TRUE(Resource::isReservedNodeName("Bip01 R Hand"));
        EXPECT_TRUE(Resource::isReservedNodeName("Camera1st"));
        EXPECT_TRUE(Resource::isReservedNodeName("Weapon Bone"));
        EXPECT_TRUE(Resource::isReservedNodeName("Weapon_Bone"));
        EXPECT_TRUE(Resource::isReservedNodeName("Tri Weapon Bone"));
        EXPECT_TRUE(Resource::isReservedNodeName("Right Hand"));
        // a name is reserved when it starts with a reserved one, however far the search has to go for it
        EXPECT_TRUE(Resource::isReservedNodeName("Weapon Bone 01"));
        EXPECT_TRUE(Resource::isReservedNodeName("WeaponMod"));

        EXPECT_FALSE(Resource::isReservedNodeName(""));
        EXPECT_FALSE(Resource::isReservedNodeName("Wea"));
        EXPECT_FALSE(Resource::isReservedNodeName("Scene Root"));
        EXPECT_FALSE(Resource::isReservedNodeName("10mmPistol"));
    }
}
