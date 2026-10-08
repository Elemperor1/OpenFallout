#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/optimizer.hpp>
#include <components/testing/util.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace TestingOpenMW;

    constexpr unsigned int merge = SceneUtil::Optimizer::MERGE_GEOMETRY;

    constexpr VFS::Path::NormalizedView hairOne("meshes/characters/hair/hairone.nif");
    constexpr VFS::Path::NormalizedView hairOneMorphs("meshes/characters/hair/hairone.egm");
    constexpr VFS::Path::NormalizedView hairTwo("meshes/characters/hair/hairtwo.nif");
    constexpr VFS::Path::NormalizedView folderWithDot("meshes/characters/head.d/headhuman.nif");
    constexpr VFS::Path::NormalizedView folderWithDotMorphs("meshes/characters/head.egm");
    constexpr VFS::Path::NormalizedView noExtension("meshes/characters/noextension");
    constexpr VFS::Path::NormalizedView noExtensionMorphs("meshes/characters/noextension.egm");

    TEST(ResourceMeshOptimization, keepsTheGeometriesOfAMeshWithAFaceGenMorphFileApart)
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
        EXPECT_NE(plain & merge, 0u);

        const unsigned int morphed = Resource::getMeshOptimizationOptions(*vfs, hairOne);
        EXPECT_EQ(morphed & merge, 0u);
        // nothing else is left out
        EXPECT_EQ(morphed, plain & ~merge);

        // a dot in the name of a folder is not the extension of the file, and a file with none has no morph file
        EXPECT_NE(Resource::getMeshOptimizationOptions(*vfs, folderWithDot) & merge, 0u);
        EXPECT_NE(Resource::getMeshOptimizationOptions(*vfs, noExtension) & merge, 0u);
    }
}
