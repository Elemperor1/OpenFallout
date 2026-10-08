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
    constexpr VFS::Path::NormalizedView folderWithDot("meshes/characters/head.d/headhuman.nif");
    constexpr VFS::Path::NormalizedView folderWithDotMorphs("meshes/characters/head.egm");
    constexpr VFS::Path::NormalizedView noExtension("meshes/characters/noextension");
    constexpr VFS::Path::NormalizedView noExtensionMorphs("meshes/characters/noextension.egm");

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
}
