#include <components/files/gameini.hpp>
#include <components/testing/util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace
{
    using namespace testing;
    using Files::GameIni;

    void write(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream(path, std::ios::binary) << text;
    }

    TEST(FilesGameIniTest, readsKeysBySection)
    {
        const GameIni ini = GameIni::parse(
            "[General]\r\nsTestFile1=FalloutNV.esm\r\n; comment\r\n\r\n[Archive]\r\n"
            "bInvalidateOlderFiles = 1 \r\nSArchiveList=A.bsa, B.bsa\r\n");
        EXPECT_EQ(ini.get("General", "sTestFile1"), "FalloutNV.esm");
        EXPECT_EQ(ini.get("Archive", "bInvalidateOlderFiles"), "1");
        EXPECT_EQ(ini.get("Archive", "SArchiveList"), "A.bsa, B.bsa");
        EXPECT_FALSE(ini.get("General", "SArchiveList").has_value());
        EXPECT_FALSE(ini.get("Archive", "sTestFile1").has_value());
    }

    TEST(FilesGameIniTest, comparesSectionsAndKeysWithoutCase)
    {
        const GameIni ini = GameIni::parse("[ARCHIVE]\nsarchivelist=A.bsa\n");
        EXPECT_EQ(ini.get("Archive", "SArchiveList"), "A.bsa");
    }

    TEST(FilesGameIniTest, takesTheLastOfAKeyThatAppearsTwice)
    {
        EXPECT_EQ(GameIni::parse("[A]\nk=1\nk=2\n").get("A", "k"), "2");
    }

    TEST(FilesGameIniTest, keepsAnEqualsSignInAValueAndAnEmptyValue)
    {
        const GameIni ini = GameIni::parse("[A]\nk=a=b\nempty=\n");
        EXPECT_EQ(ini.get("A", "k"), "a=b");
        EXPECT_EQ(ini.get("A", "empty"), "");
    }

    TEST(FilesGameIniTest, skipsLinesItDoesNotUnderstandAndAByteOrderMark)
    {
        const GameIni ini = GameIni::parse(
            "\xEF\xBB\xBF"
            "[A]\nnonsense\n=novalue\n[broken\nk=1\n");
        EXPECT_EQ(ini.get("A", "k"), "1");
    }

    TEST(FilesGameIniTest, keysBeforeAnySectionBelongToNoSection)
    {
        EXPECT_EQ(GameIni::parse("k=1\n").get("", "k"), "1");
    }

    TEST(FilesGameIniTest, mergedSettingsTakeThePlaceOfTheOnesBefore)
    {
        GameIni ini = GameIni::parse("[A]\nk=1\nonly=base\n");
        ini.merge(GameIni::parse("[A]\nk=2\n[B]\nj=3\n"));
        EXPECT_EQ(ini.get("A", "k"), "2");
        EXPECT_EQ(ini.get("A", "only"), "base");
        EXPECT_EQ(ini.get("B", "j"), "3");
    }

    TEST(FilesGameIniTest, splitsAListAtTheCommasAndTrimsTheNames)
    {
        EXPECT_THAT(GameIni::splitList("Fallout - Textures.bsa, Fallout - Meshes.bsa,  Fallout - Misc.bsa"),
            ElementsAre("Fallout - Textures.bsa", "Fallout - Meshes.bsa", "Fallout - Misc.bsa"));
        EXPECT_THAT(GameIni::splitList(" , A.bsa,,"), ElementsAre("A.bsa"));
        EXPECT_THAT(GameIni::splitList(""), IsEmpty());
    }

    TEST(FilesGameIniTest, loadsFilesInOrderAndSkipsOnesThatAreNotThere)
    {
        const auto dir = TestingOpenMW::outputDirPath("gameini_load");
        write(dir / "a.ini", "[A]\nk=1\nj=1\n");
        write(dir / "b.ini", "[A]\nk=2\n");
        const GameIni ini = GameIni::load({ dir / "a.ini", dir / "gone.ini", dir / "b.ini" });
        EXPECT_EQ(ini.get("A", "k"), "2");
        EXPECT_EQ(ini.get("A", "j"), "1");
    }

    TEST(FilesGameIniTest, findsTheIniFilesInTheFolderAboveTheDataDirectories)
    {
        const auto dir = TestingOpenMW::outputDirPath("gameini_find");
        std::filesystem::create_directories(dir / "game" / "Data");
        std::filesystem::create_directories(dir / "other" / "Data");
        write(dir / "game" / "fallout_DEFAULT.ini", "");
        write(dir / "game" / "Fallout.ini", "");
        write(dir / "game" / "low.ini", "");
        write(dir / "other" / "FalloutPrefs.ini", "");
        const auto found
            = Files::findGameInis({ dir / "game" / "Data", dir / "other" / "Data", dir / "game" / "Data" });
        ASSERT_THAT(found, SizeIs(3));
        EXPECT_EQ(found[0].filename(), "fallout_DEFAULT.ini");
        EXPECT_EQ(found[1].filename(), "Fallout.ini");
        EXPECT_EQ(found[2].filename(), "FalloutPrefs.ini");
    }
}
