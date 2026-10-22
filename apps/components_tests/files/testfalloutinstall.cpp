#include <components/files/falloutinstall.hpp>
#include <components/testing/util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace
{
    using namespace testing;
    using Files::FalloutGame;

    void touch(const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << "x";
    }

    void write(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << text;
    }

    TEST(FilesFalloutInstallTest, readsTheLibrariesOfTheCurrentSteamLayout)
    {
        const auto libraries = Files::parseSteamLibraries(
            "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n"
            "\t\t\"label\"\t\t\"\"\n\t\t\"apps\"\n\t\t{\n\t\t\t\"22380\"\t\t\"123\"\n\t\t}\n\t}\n"
            "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t}\n}\n");
        ASSERT_THAT(libraries, SizeIs(2));
        EXPECT_EQ(libraries[0], std::filesystem::path("C:\\Program Files (x86)\\Steam"));
        EXPECT_EQ(libraries[1], std::filesystem::path("D:\\SteamLibrary"));
    }

    TEST(FilesFalloutInstallTest, readsTheLibrariesOfTheOldSteamLayout)
    {
        const auto libraries = Files::parseSteamLibraries(
            "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\t\"1700000000\"\n\t\"ContentStatsID\"\t\t\"-1\"\n"
            "\t\"1\"\t\t\"/mnt/games/SteamLibrary\"\n\t\"2\"\t\t\"/home/me/lib\"\n}\n");
        ASSERT_THAT(libraries, SizeIs(2));
        EXPECT_EQ(libraries[0], "/mnt/games/SteamLibrary");
        EXPECT_EQ(libraries[1], "/home/me/lib");
    }

    TEST(FilesFalloutInstallTest, readsNothingFromTextThatIsNotAVdfFile)
    {
        EXPECT_THAT(Files::parseSteamLibraries(""), IsEmpty());
        EXPECT_THAT(Files::parseSteamLibraries("\"path\" \"unterminated"), SizeIs(1));
        EXPECT_THAT(Files::parseSteamLibraries("path = /somewhere\n// \"path\" \"/commented\"\n"), IsEmpty());
    }

    TEST(FilesFalloutInstallTest, findsTheGameInItsFolderOrInItsDataFolder)
    {
        const auto dir = TestingOpenMW::outputDirPath("falloutinstall_check");
        touch(dir / "nv" / "Data" / "FalloutNV.esm");
        touch(dir / "three" / "DATA" / "fallout3.ESM");
        touch(dir / "empty" / "Data" / "Other.esm");
        std::filesystem::create_directories(dir / "none");

        const auto nv = Files::checkGameFolder(dir / "nv");
        ASSERT_TRUE(nv.has_value());
        EXPECT_EQ(nv->mGame, FalloutGame::NewVegas);
        EXPECT_EQ(nv->mData, dir / "nv" / "Data");

        const auto three = Files::checkGameFolder(dir / "three");
        ASSERT_TRUE(three.has_value());
        EXPECT_EQ(three->mGame, FalloutGame::Fallout3);
        EXPECT_EQ(three->mData, dir / "three" / "DATA");

        const auto direct = Files::checkGameFolder(dir / "nv" / "Data");
        ASSERT_TRUE(direct.has_value());
        EXPECT_EQ(direct->mData, dir / "nv" / "Data");

        EXPECT_FALSE(Files::checkGameFolder(dir / "empty").has_value());
        EXPECT_FALSE(Files::checkGameFolder(dir / "none").has_value());
        EXPECT_FALSE(Files::checkGameFolder(dir / "gone").has_value());
    }

    TEST(FilesFalloutInstallTest, aFolderWithBothMastersIsNewVegas)
    {
        const auto dir = TestingOpenMW::outputDirPath("falloutinstall_ttw");
        touch(dir / "ttw" / "Fallout3.esm");
        touch(dir / "ttw" / "FalloutNV.esm");
        const auto install = Files::checkGameFolder(dir / "ttw");
        ASSERT_TRUE(install.has_value());
        EXPECT_EQ(install->mGame, FalloutGame::NewVegas);
        EXPECT_EQ(install->mData, dir / "ttw");
    }

    TEST(FilesFalloutInstallTest, findsTheGamesOfSteamLibrariesAndOfFolders)
    {
        const auto dir = TestingOpenMW::outputDirPath("falloutinstall_find");
        // A Steam installation with a second library on another drive
        const auto steam = dir / "Steam";
        const auto library = dir / "SteamLibrary";
        std::filesystem::create_directories(steam / "steamapps" / "common");
        write(steam / "steamapps" / "libraryfolders.vdf",
            "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + steam.generic_string()
                + "\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" + library.generic_string() + "\"\n\t}\n}\n");
        touch(steam / "steamapps" / "common" / "Fallout 3 goty" / "Data" / "Fallout3.esm");
        touch(steam / "steamapps" / "common" / "Fallout 4" / "Data" / "Fallout4.esm");
        touch(library / "steamapps" / "common" / "Fallout New Vegas" / "Data" / "FalloutNV.esm");
        touch(library / "steamapps" / "common" / "Skyrim" / "Data" / "FalloutNV.esm");
        // Folders that hold games installed another way
        const auto games = dir / "Games";
        touch(games / "FALLOUT NEW VEGAS copy" / "Data" / "FalloutNV.esm");
        touch(games / "Fallout3" / "data" / "Fallout3.esm");
        touch(games / "Morrowind" / "Data Files" / "Morrowind.esm");

        const auto installs = Files::findFalloutInstalls({ steam, dir / "no steam" }, { games, dir / "no games" });
        ASSERT_THAT(installs, SizeIs(4));
        // New Vegas first, each group in the order they were found
        EXPECT_EQ(installs[0].mGame, FalloutGame::NewVegas);
        EXPECT_EQ(installs[0].mData, library / "steamapps" / "common" / "Fallout New Vegas" / "Data");
        EXPECT_EQ(installs[1].mGame, FalloutGame::NewVegas);
        EXPECT_EQ(installs[1].mData, games / "FALLOUT NEW VEGAS copy" / "Data");
        EXPECT_EQ(installs[2].mGame, FalloutGame::Fallout3);
        EXPECT_EQ(installs[2].mData, steam / "steamapps" / "common" / "Fallout 3 goty" / "Data");
        EXPECT_EQ(installs[3].mGame, FalloutGame::Fallout3);
        EXPECT_EQ(installs[3].mData, games / "Fallout3" / "data");
    }

    TEST(FilesFalloutInstallTest, listsACopyOnceWhereTwoPlacesLeadToIt)
    {
        const auto dir = TestingOpenMW::outputDirPath("falloutinstall_twice");
        touch(dir / "Games" / "Fallout New Vegas" / "Data" / "FalloutNV.esm");
        const auto installs = Files::findFalloutInstalls({}, { dir / "Games", dir / "Games" / ".." / "Games" });
        EXPECT_THAT(installs, SizeIs(1));
    }

    TEST(FilesFalloutInstallTest, findsNothingInPlacesThatAreNotThere)
    {
        const auto dir = TestingOpenMW::outputDirPath("falloutinstall_nothing");
        EXPECT_THAT(Files::findFalloutInstalls({ dir / "a" }, { dir / "b" }), IsEmpty());
        EXPECT_THAT(Files::findFalloutInstalls({}, {}), IsEmpty());
    }

    TEST(FilesFalloutInstallTest, namesTheGames)
    {
        EXPECT_STREQ(Files::falloutGameName(FalloutGame::NewVegas), "Fallout: New Vegas");
        EXPECT_STREQ(Files::falloutGameName(FalloutGame::Fallout3), "Fallout 3");
    }
}
