#include <components/bsa/bsafile.hpp>
#include <components/files/collections.hpp>
#include <components/files/istreamptr.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/archiveinvalidation.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/registerarchives.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    using namespace testing;
    using VFS::ArchiveInvalidation;

    const auto noDirectories = [](std::string_view) { return false; };
    const auto everyDirectory = [](std::string_view) { return true; };

    TEST(VFSArchiveInvalidationTest, aLineWithABackslashIsADirectoryAndAnyOtherIsAFileName)
    {
        const ArchiveInvalidation invalidation = ArchiveInvalidation::parse(
            "\xEF\xBB\xBF"
            "\\Textures\\Armor\\\r\nmeshes/weapons\nSkin.dds\r\n  \r\n");
        EXPECT_FALSE(invalidation.empty());
        // A directory counts for the files directly in it, not for a directory below it, whatever the case or the slash
        EXPECT_TRUE(invalidation.invalidates("textures/armor/a.dds", noDirectories));
        EXPECT_TRUE(invalidation.invalidates("meshes/weapons/a.nif", noDirectories));
        EXPECT_FALSE(invalidation.invalidates("textures/armor/sub/a.dds", noDirectories));
        EXPECT_FALSE(invalidation.invalidates("textures/a.dds", noDirectories));
        // A file name counts in the directories the Data folder has
        EXPECT_FALSE(invalidation.invalidates("textures/skin.dds", noDirectories));
        EXPECT_TRUE(invalidation.invalidates("textures/skin.dds", everyDirectory));
        EXPECT_TRUE(invalidation.invalidates("skin.dds", everyDirectory));
        EXPECT_FALSE(invalidation.invalidates("textures/other.dds", everyDirectory));
    }

    TEST(VFSArchiveInvalidationTest, anEmptyFileInvalidatesNothing)
    {
        const ArchiveInvalidation invalidation = ArchiveInvalidation::parse("\r\n\n  \t\n");
        EXPECT_TRUE(invalidation.empty());
        EXPECT_FALSE(invalidation.invalidates("textures/a.dds", everyDirectory));
    }

    void writeFile(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    void makeArchive(const std::filesystem::path& path)
    {
        Bsa::BSAFile bsa;
        bsa.open(path);
        std::istringstream first("archive");
        bsa.addFile("textures\\armor\\a.txt", first);
        std::istringstream second("archive");
        bsa.addFile("meshes\\b.txt", second);
        std::istringstream third("archive");
        bsa.addFile("only\\c.txt", third);
    }

    std::string read(const VFS::Manager& vfs, std::string_view path)
    {
        const VFS::Path::Normalized name(path);
        if (!vfs.exists(name))
            return "missing";
        const Files::IStreamPtr stream = vfs.get(name);
        std::string text;
        std::getline(*stream, text, '\0');
        return text;
    }

    struct VFSRegisterArchivesTest : Test
    {
        std::filesystem::path mDir;
        Files::Collections mCollections;

        VFSRegisterArchivesTest()
        {
            mDir = TestingOpenMW::outputDirPath("registerarchives_" + std::to_string(sCounter++));
            makeArchive(mDir / "Test.bsa");
            writeFile(mDir / "textures" / "armor" / "a.txt", "loose");
            writeFile(mDir / "meshes" / "other.txt", "loose");
            mCollections = Files::Collections(Files::PathContainer{ mDir });
        }

        std::unique_ptr<VFS::Manager> build(const VFS::ArchiveOptions& options)
        {
            auto vfs = std::make_unique<VFS::Manager>();
            VFS::registerArchives(vfs.get(), mCollections, { "Test.bsa" }, true, nullptr, options);
            return vfs;
        }

        static inline int sCounter = 0;
    };

    TEST_F(VFSRegisterArchivesTest, aLooseFileWinsOverAnArchiveByDefault)
    {
        const auto vfs = build({});
        EXPECT_EQ(read(*vfs, "textures/armor/a.txt"), "loose");
        EXPECT_EQ(read(*vfs, "meshes/b.txt"), "archive");
    }

    TEST_F(VFSRegisterArchivesTest, anArchiveWinsOverALooseFileWhenAsked)
    {
        VFS::ArchiveOptions options;
        options.mArchivesWin = true;
        const auto vfs = build(options);
        EXPECT_EQ(read(*vfs, "textures/armor/a.txt"), "archive");
        EXPECT_EQ(read(*vfs, "meshes/other.txt"), "loose");
    }

    TEST_F(VFSRegisterArchivesTest, anInvalidatedFileIsNotFoundInTheArchive)
    {
        VFS::ArchiveOptions options;
        // meshes\b.txt by name (the Data folder has a meshes directory), the directory only\ (it has no loose copy)
        options.mInvalidation = std::make_shared<ArchiveInvalidation>(ArchiveInvalidation::parse("b.txt\nonly\\\n"));
        options.mArchivesWin = true;
        const auto vfs = build(options);
        EXPECT_EQ(read(*vfs, "meshes/b.txt"), "missing");
        EXPECT_EQ(read(*vfs, "only/c.txt"), "missing");
        // Not listed: still the archive's
        EXPECT_EQ(read(*vfs, "textures/armor/a.txt"), "archive");
    }

    TEST_F(VFSRegisterArchivesTest, aFileNameDoesNotInvalidateInADirectoryTheDataFolderLacks)
    {
        VFS::ArchiveOptions options;
        options.mInvalidation = std::make_shared<ArchiveInvalidation>(ArchiveInvalidation::parse("c.txt\n"));
        const auto vfs = build(options);
        // There is no directory only in the Data folder: the file is the archive's
        EXPECT_EQ(read(*vfs, "only/c.txt"), "archive");
    }
}
