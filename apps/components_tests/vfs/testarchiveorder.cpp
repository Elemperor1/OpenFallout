#include <components/files/collections.hpp>
#include <components/files/gameini.hpp>
#include <components/testing/util.hpp>
#include <components/vfs/archiveorder.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using VFS::ArchiveRules;

    void write(const std::filesystem::path& path, const std::string& bytes)
    {
        std::ofstream(path, std::ios::binary) << bytes;
    }

    TEST(VFSArchiveOrderTest, fallout3KeepsTheOrderTheArchivesWereOpenedIn)
    {
        const std::vector<std::string> opened = { "Fallout - Meshes.bsa", "Anchorage - Main.bsa", "Mod.bsa" };
        EXPECT_THAT(VFS::orderArchives(opened, ArchiveRules::Fallout3), ElementsAreArray(opened));
    }

    TEST(VFSArchiveOrderTest, newVegasLooksInTheArchivesOfTheLaterDlcFirst)
    {
        const std::vector<std::string> opened = { "Fallout - Textures.bsa", "Fallout - Meshes.bsa", "Update.bsa",
            "DeadMoney - Main.bsa", "DeadMoney - Sounds.bsa", "HonestHearts - Main.bsa", "HonestHearts - Sounds.bsa",
            "OldWorldBlues - Main.bsa", "OldWorldBlues - Sounds.bsa", "LonesomeRoad - Main.bsa",
            "LonesomeRoad - Sounds.bsa" };
        // Update.bsa goes in front of the first of the lists's Fallout archives and the DLC in front of the ones before
        // them, each by its rank (DeadM 1, Hones 2, Lones 3, OldWo 4, Updat 5); within a rank the one opened later is
        // first
        EXPECT_THAT(VFS::orderArchives(opened, ArchiveRules::NewVegas),
            ElementsAre("OldWorldBlues - Sounds.bsa", "OldWorldBlues - Main.bsa", "Update.bsa",
                "LonesomeRoad - Sounds.bsa", "LonesomeRoad - Main.bsa", "HonestHearts - Sounds.bsa",
                "HonestHearts - Main.bsa", "DeadMoney - Sounds.bsa", "DeadMoney - Main.bsa", "Fallout - Textures.bsa",
                "Fallout - Meshes.bsa"));
    }

    TEST(VFSArchiveOrderTest, newVegasPutsTheArchivesOfModsInFrontOfAllOthersAfterEarlierOnes)
    {
        const std::vector<std::string> opened
            = { "Fallout - Meshes.bsa", "Mod A.bsa", "DeadMoney - Main.bsa", "Mod B.bsa", "Fallout3 - Meshes.bsa" };
        // "Fallout3 - Meshes.bsa" has Fallo in its name: it goes to the end like the archives of the ini file
        EXPECT_THAT(VFS::orderArchives(opened, ArchiveRules::NewVegas),
            ElementsAre(
                "Mod A.bsa", "Mod B.bsa", "DeadMoney - Main.bsa", "Fallout - Meshes.bsa", "Fallout3 - Meshes.bsa"));
    }

    TEST(VFSArchiveOrderTest, newVegasRanksTheArchivesOfTaleOfTwoWastelandsAsTheMacReadItInTheGame)
    {
        // The Tale of Two Wastelands output folder alone, the plugins in the order of their indexes (round 8A, part D)
        const std::vector<std::string> opened = { "Fallout - Textures.bsa", "Fallout - Textures2.bsa",
            "Fallout - Meshes.bsa", "Fallout - Sound.bsa", "Fallout - Misc.bsa", "DeadMoney - Sounds.bsa",
            "HonestHearts - Sounds.bsa", "OldWorldBlues - Sounds.bsa", "LonesomeRoad - Sounds.bsa",
            "Fallout3 - MenuVoices.bsa", "Fallout3 - Meshes.bsa", "Fallout3 - Sound.bsa", "Fallout3 - Textures.bsa",
            "Fallout3 - Voices.bsa", "Anchorage - Main.bsa", "Anchorage - Sounds.bsa", "ThePitt - Main.bsa",
            "ThePitt - Sounds.bsa", "BrokenSteel - Main.bsa", "BrokenSteel - Sounds.bsa", "PointLookout - Main.bsa",
            "PointLookout - Sounds.bsa", "Zeta - Main.bsa", "Zeta - Sounds.bsa", "TaleOfTwoWastelands - Main.bsa",
            "TaleOfTwoWastelands - Textures.bsa", "YUPTTW - Main.bsa", "YUPTTW - Sounds.bsa" };
        EXPECT_THAT(VFS::orderArchives(opened, ArchiveRules::NewVegas),
            ElementsAre("Anchorage - Main.bsa", "Anchorage - Sounds.bsa", "ThePitt - Main.bsa", "ThePitt - Sounds.bsa",
                "BrokenSteel - Main.bsa", "BrokenSteel - Sounds.bsa", "PointLookout - Main.bsa",
                "PointLookout - Sounds.bsa", "Zeta - Main.bsa", "Zeta - Sounds.bsa", "TaleOfTwoWastelands - Main.bsa",
                "TaleOfTwoWastelands - Textures.bsa", "YUPTTW - Main.bsa", "YUPTTW - Sounds.bsa",
                "OldWorldBlues - Sounds.bsa", "LonesomeRoad - Sounds.bsa", "HonestHearts - Sounds.bsa",
                "DeadMoney - Sounds.bsa", "Fallout - Textures.bsa", "Fallout - Textures2.bsa", "Fallout - Meshes.bsa",
                "Fallout - Sound.bsa", "Fallout - Misc.bsa", "Fallout3 - MenuVoices.bsa", "Fallout3 - Meshes.bsa",
                "Fallout3 - Sound.bsa", "Fallout3 - Textures.bsa", "Fallout3 - Voices.bsa"));
    }

    TEST(VFSArchiveOrderTest, readsTheArchiveSettingsOfAnIniFile)
    {
        const Files::GameIni ini = Files::GameIni::parse(
            "[Archive]\nbUseArchives=1\nbInvalidateOlderFiles=0\n"
            "SArchiveList=A.bsa,  B.bsa\nSInvalidationFile=Mine.txt\n");
        const VFS::ArchiveSettings settings = VFS::ArchiveSettings::read(ini, ArchiveRules::NewVegas);
        EXPECT_TRUE(settings.mUseArchives);
        EXPECT_FALSE(settings.mInvalidateOlderFiles);
        EXPECT_THAT(settings.mArchiveList, ElementsAre("A.bsa", "B.bsa"));
        EXPECT_EQ(settings.mInvalidationFile, "Mine.txt");
    }

    TEST(VFSArchiveOrderTest, usesTheRetailArchiveListWhereAnIniFileHasNone)
    {
        const VFS::ArchiveSettings newVegas = VFS::ArchiveSettings::read(Files::GameIni(), ArchiveRules::NewVegas);
        EXPECT_TRUE(newVegas.mUseArchives);
        EXPECT_TRUE(newVegas.mInvalidateOlderFiles);
        EXPECT_EQ(newVegas.mInvalidationFile, "ArchiveInvalidation.txt");
        EXPECT_THAT(newVegas.mArchiveList,
            ElementsAre("Fallout - Textures.bsa", "Fallout - Textures2.bsa", "Fallout - Meshes.bsa",
                "Fallout - Voices1.bsa", "Fallout - Sound.bsa", "Fallout - Misc.bsa"));
        const VFS::ArchiveSettings fallout3 = VFS::ArchiveSettings::read(Files::GameIni(), ArchiveRules::Fallout3);
        EXPECT_THAT(fallout3.mArchiveList, Contains("Fallout - MenuVoices.bsa"));
    }

    TEST(VFSArchiveOrderTest, opensTheListedArchivesThenTheExplicitOnesUpdateAndThoseOfThePlugins)
    {
        const auto dir = TestingOpenMW::outputDirPath("archiveorder_open");
        for (const char* name : { "Fallout - Meshes.bsa", "Explicit.bsa", "Update.bsa", "Plugin - Main.bsa" })
            write(dir / name, "x");
        const Files::Collections collections(Files::PathContainer{ dir });

        VFS::ArchiveSettings settings;
        settings.mArchiveList = { "Fallout - Meshes.bsa", "Gone.bsa", "explicit.bsa" };
        // A name once, whatever its case; an archive the ini file lists and the data directories lack is skipped
        EXPECT_THAT(VFS::archivesToOpen(
                        settings, ArchiveRules::NewVegas, { "Explicit.bsa" }, { "Plugin - Main.bsa" }, collections),
            ElementsAre("Fallout - Meshes.bsa", "explicit.bsa", "Update.bsa", "Plugin - Main.bsa"));
        // Fallout 3 has no Update.bsa
        EXPECT_THAT(VFS::archivesToOpen(settings, ArchiveRules::Fallout3, {}, {}, collections),
            ElementsAre("Fallout - Meshes.bsa", "explicit.bsa"));
        // Turned off, only what was asked for stays
        settings.mUseArchives = false;
        EXPECT_THAT(VFS::archivesToOpen(
                        settings, ArchiveRules::NewVegas, { "Explicit.bsa" }, { "Plugin - Main.bsa" }, collections),
            ElementsAre("Explicit.bsa"));
    }
}
