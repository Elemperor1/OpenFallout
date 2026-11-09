#include <components/fallback/validate.hpp>
#include <components/files/configfileparser.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/testing/util.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

// How the settings of the games are found and merged: the files of the configuration, the paths that name a place by
// a token and the options that add up instead of replacing one another.
namespace
{
    using namespace testing;
    namespace bpo = boost::program_options;

    bpo::options_description describe()
    {
        bpo::options_description description;
        Files::ConfigurationManager::addCommonOptions(description);
        description.add_options()("data", bpo::value<Files::MaybeQuotedPathContainer>()->multitoken()->composing(), "")(
            "content", bpo::value<std::vector<std::string>>()->multitoken()->composing(), "")("fallback",
            bpo::value<Fallback::FallbackMap>()->multitoken()->composing(),
            "")("name", bpo::value<std::string>()->default_value("none"), "")(
            "flag", bpo::value<bool>()->default_value(false)->implicit_value(true), "");
        return description;
    }

    bpo::variables_map parse(const std::string& text, const bpo::options_description& description)
    {
        std::istringstream stream(text);
        bpo::variables_map variables;
        Files::parseConfig(stream, variables, description);
        bpo::notify(variables);
        return variables;
    }

    TEST(FilesConfigFileParserTest, readsTheOptionsOfAFileWithItsSections)
    {
        bpo::options_description description;
        description.add_options()("name", bpo::value<std::string>(), "")("shape.width", bpo::value<int>(), "")(
            "list", bpo::value<std::vector<std::string>>()->composing(), "");
        std::istringstream stream(R"(# a comment
   # an indented comment
name = The Name   # not a comment, the line is read as it is
list=a
list = b

[shape]
width=12
)");

        bpo::variables_map variables;
        bpo::store(Files::parse_config_file(stream, description), variables);

        EXPECT_EQ(variables["name"].as<std::string>(), "The Name   # not a comment, the line is read as it is");
        EXPECT_THAT(variables["list"].as<std::vector<std::string>>(), ElementsAre("a", "b"));
        EXPECT_EQ(variables["shape.width"].as<int>(), 12);
    }

    TEST(FilesConfigFileParserTest, refusesWhatItDoesNotKnowUnlessAskedNotTo)
    {
        bpo::options_description description;
        description.add_options()("name", bpo::value<std::string>(), "");
        {
            std::istringstream stream("unknown=1\n");
            EXPECT_THROW(Files::parse_config_file(stream, description), bpo::unknown_option);
        }
        {
            std::istringstream stream("unknown=1\nname=x\n");
            const auto parsed = Files::parse_config_file(stream, description, true);

            ASSERT_EQ(parsed.options.size(), 2u);
            EXPECT_TRUE(parsed.options[0].unregistered);
            EXPECT_EQ(parsed.options[0].string_key, "unknown");
            EXPECT_FALSE(parsed.options[1].unregistered);
        }
        {
            std::istringstream stream("this line is not an option\n");
            EXPECT_THROW(Files::parse_config_file(stream, description), bpo::invalid_config_file_syntax);
        }
    }

    TEST(FilesConfigFileParserTest, matchesTheOptionsWithAPrefix)
    {
        bpo::options_description description;
        description.add_options()("fallback-*", bpo::value<std::string>(), "");
        std::istringstream stream("fallback-one=1\n");

        // a name that ends with a star stands for every key that begins with the rest of it
        const auto parsed = Files::parse_config_file(stream, description, true);

        ASSERT_EQ(parsed.options.size(), 1u);
        EXPECT_EQ(parsed.options[0].string_key, "fallback-one");
    }

    TEST(FilesConfigFileParserTest, refusesPrefixesThatWouldBothMatchTheSameKey)
    {
        bpo::options_description description;
        description.add_options()("a-*", bpo::value<std::string>(), "")("a-b-*", bpo::value<std::string>(), "");
        std::istringstream stream("a-b-c=1\n");

        EXPECT_THROW(Files::parse_config_file(stream, description, true), bpo::error);
    }

    TEST(FilesConfigFileParserTest, refusesAnOptionWithOnlyAShortName)
    {
        bpo::options_description description;
        description.add_options()(",s", bpo::value<std::string>(), "");
        std::istringstream stream("s=1\n");

        EXPECT_THROW(Files::parse_config_file(stream, description), bpo::error);
    }

    TEST(FilesConfigurationManagerTest, readsPathsThatAreInQuotes)
    {
        const auto readPath = [](const std::string& text) {
            std::istringstream stream(text);
            Files::MaybeQuotedPath path;
            stream >> path;
            return static_cast<std::filesystem::path>(path);
        };

        EXPECT_EQ(readPath(R"(C:\Games\Fallout)"), std::filesystem::path(u8R"(C:\Games\Fallout)"));
        EXPECT_EQ(readPath(R"("C:\Games\Fallout 3")"), std::filesystem::path(u8R"(C:\Games\Fallout 3)"));
        // the ampersand escapes the quote and itself
        EXPECT_EQ(readPath(R"("a&"b&&c")"), std::filesystem::path(u8R"(a"b&c)"));
        // what comes after the closing quote is dropped
        EXPECT_EQ(readPath(R"("quoted" and the rest)"), std::filesystem::path(u8"quoted"));
    }

    TEST(FilesConfigurationManagerTest, convertsAListOfPathsThatAreMaybeInQuotes)
    {
        Files::MaybeQuotedPathContainer container(2);
        static_cast<std::filesystem::path&>(container[0]) = "one";
        static_cast<std::filesystem::path&>(container[1]) = "two/three";

        const Files::PathContainer paths = Files::asPathContainer(container);

        EXPECT_THAT(paths, ElementsAre(std::filesystem::path("one"), std::filesystem::path("two/three")));
    }

    TEST(FilesConfigurationManagerTest, parsesTheOptionsOfTheCommandLineAndKeepsThoseItDoesNotKnow)
    {
        const bpo::options_description description = describe();
        const char* args[] = { "program", "--name=cli", "--unknown=1", "--flag" };
        bpo::variables_map variables;

        Files::parseArgs(4, args, variables, description);
        bpo::notify(variables);

        EXPECT_EQ(variables["name"].as<std::string>(), "cli");
        EXPECT_EQ(variables.count("unknown"), 0u);
    }

    TEST(FilesConfigurationManagerTest, readsTheOptionsOfAFileWithPathsAndLists)
    {
        const bpo::options_description description = describe();

        const bpo::variables_map variables = parse(R"(user-data="/games/my data"
data=/one
data="/two"
content=A.esm
content=B.esm
fallback=Fonts_Font_0,magic
unknown=1
)",
            description);

        EXPECT_EQ(variables["user-data"].as<Files::MaybeQuotedPath>(), std::filesystem::path(u8"/games/my data"));
        const auto data = Files::asPathContainer(variables["data"].as<Files::MaybeQuotedPathContainer>());
        EXPECT_THAT(data, ElementsAre(std::filesystem::path("/one"), std::filesystem::path("/two")));
        EXPECT_THAT(variables["content"].as<std::vector<std::string>>(), ElementsAre("A.esm", "B.esm"));
        EXPECT_EQ(variables["fallback"].as<Fallback::FallbackMap>().mMap.at("Fonts_Font_0"), "magic");
    }

    TEST(FilesConfigurationManagerTest, replacesTheTokensOfAPathWithThePlacesTheyNameAndAddsTheBaseToTheOthers)
    {
        const Files::ConfigurationManager manager(true);
        const std::filesystem::path base = std::filesystem::path("/base") / "dir";

        const auto process = [&](std::filesystem::path path) {
            manager.processPath(path, base);
            return path;
        };

        EXPECT_EQ(process("relative/file"), base / "relative/file");
        EXPECT_EQ(process(""), base / "");
        EXPECT_EQ(process("/absolute/file"), std::filesystem::path("/absolute/file"));
        EXPECT_EQ(process("?local?"), manager.getLocalPath());
        EXPECT_EQ(process("?local?data"), manager.getLocalPath() / "data");
        EXPECT_EQ(process("?userconfig?data"), manager.getUserConfigPath() / "data");
        EXPECT_EQ(process("?userdata?data").filename(), "data");
        // a token that is not known leaves nothing, and a question mark that is not closed is a path like the others
        EXPECT_TRUE(process("?nowhere?/data").empty());
        EXPECT_EQ(process("?open"), std::filesystem::path("?open"));
        (void)process("?global?data");
    }

    TEST(FilesConfigurationManagerTest, processesAListOfPathsAndThoseOfTheVariables)
    {
        const Files::ConfigurationManager manager(true);
        const bpo::options_description description = describe();
        const std::filesystem::path base = std::filesystem::path("/base");
        Files::PathContainer paths{ "one", "/two", "?nowhere?/three" };

        manager.processPaths(paths, base);

        EXPECT_THAT(paths, ElementsAre(base / "one", std::filesystem::path("/two"), std::filesystem::path()));

        bpo::variables_map variables = parse("user-data=data\ndata=a\ndata=b\nname=text\n", description);
        manager.processPaths(variables, base);

        EXPECT_EQ(variables["user-data"].as<Files::MaybeQuotedPath>(), base / "data");
        EXPECT_THAT(Files::asPathContainer(variables["data"].as<Files::MaybeQuotedPathContainer>()),
            ElementsAre(base / "a", base / "b"));
        // what is only a default is not a path of the file, and what is not a path is left as it is
        EXPECT_TRUE(variables["resources"].as<Files::MaybeQuotedPath>().empty());
        EXPECT_EQ(variables["name"].as<std::string>(), "text");
    }

    TEST(FilesConfigurationManagerTest, dropsTheDirectoriesThatDoNotExist)
    {
        const Files::ConfigurationManager manager(true);
        const std::filesystem::path dir = TestingOpenMW::outputDirPath("configuration/existing");
        Files::PathContainer paths{ dir, dir / "missing", dir };

        manager.filterOutNonExistingPaths(paths);

        EXPECT_THAT(paths, ElementsAre(dir, dir));
    }

    TEST(FilesConfigurationManagerTest, givesThePlacesOfTheGameThatAreNeverEmpty)
    {
        const Files::ConfigurationManager manager(true);

        EXPECT_FALSE(manager.getUserConfigPath().empty());
        EXPECT_FALSE(manager.getUserDataPath().empty());
        EXPECT_FALSE(manager.getCachePath().empty());
        EXPECT_EQ(manager.getLogPath(), manager.getUserConfigPath());
        EXPECT_EQ(manager.getScreenshotPath(), manager.getUserDataPath() / "screenshots");
        EXPECT_TRUE(manager.getActiveConfigPaths().empty());
        (void)manager.getGlobalPath();
        (void)manager.getInstallPaths();
    }

    TEST(FilesConfigurationManagerTest, addsUpTheOptionsThatComposeAndLetsTheLastOneWinForTheOthers)
    {
        const bpo::options_description description = describe();
        bpo::variables_map first = parse(R"(data=/first
content=A.esm
fallback=Fonts_Font_0,first
fallback=Fonts_Font_1,only first
name=first
)",
            description);
        bpo::variables_map second = parse(R"(data=/second
content=B.esm
fallback=Fonts_Font_0,second
name=second
)",
            description);

        auto composing = Files::separateComposingVariables(second, description);

        EXPECT_EQ(second.count("data"), 0u);
        EXPECT_EQ(second.count("name"), 1u);
        EXPECT_EQ(composing.count("content"), 1u);
        EXPECT_EQ(composing.count("name"), 0u);

        bpo::variables_map firstComposing = Files::separateComposingVariables(first, description);
        Files::mergeComposingVariables(firstComposing, composing, description);

        EXPECT_THAT(Files::asPathContainer(firstComposing["data"].as<Files::MaybeQuotedPathContainer>()),
            ElementsAre(std::filesystem::path("/first"), std::filesystem::path("/second")));
        EXPECT_THAT(firstComposing["content"].as<std::vector<std::string>>(), ElementsAre("A.esm", "B.esm"));
        // the map has the keys of both, and the second one wins for a key that both have
        const auto& map = firstComposing["fallback"].as<Fallback::FallbackMap>().mMap;
        EXPECT_EQ(map.at("Fonts_Font_0"), "second");
        EXPECT_EQ(map.at("Fonts_Font_1"), "only first");
    }

    TEST(FilesConfigurationManagerTest, lettingASourceReplaceWhatTheOthersAdded)
    {
        const bpo::options_description description = describe();
        bpo::variables_map first = parse("content=A.esm\ncontent=B.esm\n", description);
        bpo::variables_map second = parse("replace=content\ncontent=C.esm\n", description);
        bpo::variables_map firstComposing = Files::separateComposingVariables(first, description);
        bpo::variables_map secondComposing = Files::separateComposingVariables(second, description);

        Files::mergeComposingVariables(firstComposing, secondComposing, description);

        EXPECT_THAT(firstComposing["content"].as<std::vector<std::string>>(), ElementsAre("C.esm"));
    }

    TEST(FilesConfigurationManagerTest, keepsWhatTheFirstHasWhenTheSecondHasNothing)
    {
        const bpo::options_description description = describe();
        bpo::variables_map first = parse("content=A.esm\n", description);
        bpo::variables_map second = parse("name=x\n", description);
        bpo::variables_map firstComposing = Files::separateComposingVariables(first, description);
        bpo::variables_map secondComposing = Files::separateComposingVariables(second, description);

        Files::mergeComposingVariables(firstComposing, secondComposing, description);

        EXPECT_THAT(firstComposing["content"].as<std::vector<std::string>>(), ElementsAre("A.esm"));
        EXPECT_TRUE(firstComposing["data"].empty());
    }
}
