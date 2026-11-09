#include <components/detournavigator/collisionshapetype.hpp>
#include <components/misc/strings/conversion.hpp>
#include <components/settings/parser.hpp>
#include <components/settings/settings.hpp>
#include <components/settings/values.hpp>
#include <components/testing/util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifndef OPENFALLOUT_PROJECT_SOURCE_DIR
#define OPENFALLOUT_PROJECT_SOURCE_DIR "."
#endif

// The settings that the game reads: the user's value wins over the default, every value is stored as text and the
// changes made at run time are remembered until the settings are saved.
namespace Settings
{
    namespace
    {
        using namespace testing;

        struct SettingsManagerTest : Test
        {
            SettingsManagerTest()
            {
                Manager::mDefaultSettings.clear();
                Manager::mUserSettings.clear();
                Manager::mChangedSettings.clear();
            }

            ~SettingsManagerTest()
            {
                Manager::mDefaultSettings.clear();
                Manager::mUserSettings.clear();
                Manager::mChangedSettings.clear();
            }

            static void setDefault(const std::string& category, const std::string& name, const std::string& value)
            {
                Manager::mDefaultSettings[{ category, name }] = value;
            }

            static void setUser(const std::string& category, const std::string& name, const std::string& value)
            {
                Manager::mUserSettings[{ category, name }] = value;
            }
        };

        TEST_F(SettingsManagerTest, theUserValueWinsOverTheDefault)
        {
            setDefault("Video", "mode", "1");
            EXPECT_EQ(Manager::getString("mode", "Video"), "1");

            setUser("Video", "mode", "2");
            EXPECT_EQ(Manager::getString("mode", "Video"), "2");
            EXPECT_EQ(Manager::getInt("mode", "Video"), 2);
        }

        TEST_F(SettingsManagerTest, aSettingThatIsNotThereIsAnError)
        {
            EXPECT_THAT([] { Manager::getString("missing", "Nowhere"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("non-existing setting: [Nowhere] missing")));
            EXPECT_THAT([] { Manager::getInt("missing", "Nowhere"); }, Throws<std::runtime_error>());
        }

        TEST_F(SettingsManagerTest, getOrDefaultAnswersForSettingsThatAreNotThere)
        {
            EXPECT_EQ(Manager::getOrDefault<int>("missing", "Nowhere", 42), 42);

            setDefault("Nowhere", "missing", "7");
            EXPECT_EQ(Manager::getOrDefault<int>("missing", "Nowhere", 42), 7);

            setUser("Nowhere", "missing", "9");
            EXPECT_EQ(Manager::getOrDefault<int>("missing", "Nowhere", 42), 9);
        }

        TEST_F(SettingsManagerTest, readsTheNumbers)
        {
            setDefault("Numbers", "int", "-12");
            setDefault("Numbers", "uint64", "18446744073709551615");
            setDefault("Numbers", "size", "4096");
            setDefault("Numbers", "unsigned", "4000000000");
            setDefault("Numbers", "float", "2.5");
            setDefault("Numbers", "double", "-0.125");

            EXPECT_EQ(Manager::getInt("int", "Numbers"), -12);
            EXPECT_EQ(Manager::getUInt64("uint64", "Numbers"), 18446744073709551615ull);
            EXPECT_EQ(Manager::getSize("size", "Numbers"), 4096u);
            EXPECT_EQ(Manager::getUnsigned("unsigned", "Numbers"), 4000000000u);
            EXPECT_EQ(Manager::getUnsignedLong("size", "Numbers"), 4096ul);
            EXPECT_EQ(Manager::getUnsignedLongLong("size", "Numbers"), 4096ull);
            EXPECT_FLOAT_EQ(Manager::getFloat("float", "Numbers"), 2.5f);
            EXPECT_DOUBLE_EQ(Manager::getDouble("double", "Numbers"), -0.125);

            EXPECT_EQ(Manager::get<int>("int", "Numbers"), -12);
            EXPECT_EQ(Manager::get<unsigned>("unsigned", "Numbers"), 4000000000u);
            EXPECT_EQ(Manager::get<unsigned long>("size", "Numbers"), 4096ul);
            EXPECT_EQ(Manager::get<unsigned long long>("size", "Numbers"), 4096ull);
            EXPECT_FLOAT_EQ(Manager::get<float>("float", "Numbers"), 2.5f);
            EXPECT_DOUBLE_EQ(Manager::get<double>("double", "Numbers"), -0.125);
            EXPECT_EQ(Manager::get<std::string>("float", "Numbers"), "2.5");
        }

        TEST_F(SettingsManagerTest, aNumberThatIsNotANumberNamesTheSetting)
        {
            setDefault("Numbers", "broken", "twelve");
            setDefault("Numbers", "empty", "");

            EXPECT_THAT([] { Manager::getInt("broken", "Numbers"); },
                ThrowsMessage<std::system_error>(HasSubstr("[Numbers] broken value \"twelve\"")));
            EXPECT_THAT([] { Manager::getFloat("broken", "Numbers"); }, Throws<std::system_error>());
            EXPECT_THAT([] { Manager::getDouble("empty", "Numbers"); }, Throws<std::system_error>());
            EXPECT_THAT([] { Manager::getUnsigned("empty", "Numbers"); }, Throws<std::system_error>());
        }

        TEST_F(SettingsManagerTest, readsBooleansWithoutCaringAboutTheCase)
        {
            setDefault("Flags", "a", "true");
            setDefault("Flags", "b", "TRUE");
            setDefault("Flags", "c", "false");
            setDefault("Flags", "d", "yes");
            setDefault("Flags", "e", "");

            EXPECT_TRUE(Manager::getBool("a", "Flags"));
            EXPECT_TRUE(Manager::getBool("b", "Flags"));
            EXPECT_FALSE(Manager::getBool("c", "Flags"));
            EXPECT_FALSE(Manager::getBool("d", "Flags"));
            EXPECT_FALSE(Manager::get<bool>("e", "Flags"));
        }

        TEST_F(SettingsManagerTest, readsArraysOfStrings)
        {
            setDefault("Lists", "plain", "one, two ,three");
            setDefault("Lists", "single", "alone");
            setDefault("Lists", "empty", "");

            EXPECT_THAT(Manager::getStringArray("plain", "Lists"), ElementsAre("one", "two", "three"));
            EXPECT_THAT(Manager::get<std::vector<std::string>>("single", "Lists"), ElementsAre("alone"));
            EXPECT_THAT(Manager::getStringArray("empty", "Lists"), IsEmpty());
        }

        TEST_F(SettingsManagerTest, readsVectors)
        {
            setDefault("Vectors", "two", "1.5 -2");
            setDefault("Vectors", "three", "1 2.5 -3");
            setDefault("Vectors", "tooFew", "1");
            setDefault("Vectors", "tooMany", "1 2 3 4");
            setDefault("Vectors", "text", "a b c");

            EXPECT_EQ(Manager::getVector2("two", "Vectors"), osg::Vec2f(1.5f, -2.f));
            EXPECT_EQ(Manager::getVector3("three", "Vectors"), osg::Vec3f(1.f, 2.5f, -3.f));
            EXPECT_EQ(Manager::get<osg::Vec3f>("three", "Vectors"), osg::Vec3f(1.f, 2.5f, -3.f));
            EXPECT_THAT([] { Manager::getVector2("tooFew", "Vectors"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("Can't parse 2d vector")));
            EXPECT_THAT([] { Manager::getVector2("text", "Vectors"); }, Throws<std::runtime_error>());
            EXPECT_THAT([] { Manager::getVector3("tooMany", "Vectors"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("Can't parse 3d vector")));
            EXPECT_THAT([] { Manager::getVector3("text", "Vectors"); }, Throws<std::runtime_error>());
        }

        TEST_F(SettingsManagerTest, readsTheEnumerations)
        {
            setDefault("Enums", "hrtfOff", "0");
            setDefault("Enums", "hrtfOn", "3");
            setDefault("Enums", "hrtfAuto", "-1");
            setDefault("Enums", "windowed", "2");
            setDefault("Enums", "windowBelow", "-1");
            setDefault("Enums", "windowAbove", "3");
            setDefault("Enums", "vsyncAdaptive", "2");
            setDefault("Enums", "vsyncAbove", "5");
            setDefault("Enums", "shape", "2");
            setDefault("Enums", "badShape", "9");
            setDefault("Enums", "axis", "-y");
            setDefault("Enums", "navmesh", "update frequency");
            setDefault("Enums", "colour", "0.25 0.5 0.75 1");
            setDefault("Enums", "path", "Textures\\Some.dds");

            EXPECT_EQ(Manager::get<HrtfMode>("hrtfOff", "Enums"), HrtfMode::Disable);
            EXPECT_EQ(Manager::get<HrtfMode>("hrtfOn", "Enums"), HrtfMode::Enable);
            EXPECT_EQ(Manager::get<HrtfMode>("hrtfAuto", "Enums"), HrtfMode::Auto);
            EXPECT_EQ(Manager::get<WindowMode>("windowed", "Enums"), WindowMode::Windowed);
            EXPECT_EQ(Manager::get<WindowMode>("windowBelow", "Enums"), WindowMode::Fullscreen);
            EXPECT_EQ(Manager::get<WindowMode>("windowAbove", "Enums"), WindowMode::Fullscreen);
            EXPECT_EQ(Manager::get<SDLUtil::VSyncMode>("vsyncAdaptive", "Enums"), SDLUtil::VSyncMode::Adaptive);
            EXPECT_EQ(Manager::get<SDLUtil::VSyncMode>("vsyncAbove", "Enums"), SDLUtil::VSyncMode::Disabled);
            EXPECT_EQ(Manager::get<DetourNavigator::CollisionShapeType>("shape", "Enums"),
                DetourNavigator::CollisionShapeType::Cylinder);
            EXPECT_THAT([] { Manager::get<DetourNavigator::CollisionShapeType>("badShape", "Enums"); },
                Throws<std::invalid_argument>());
            EXPECT_EQ(Manager::get<GyroscopeAxis>("axis", "Enums"), GyroscopeAxis::MinusY);
            EXPECT_EQ(Manager::get<NavMeshRenderMode>("navmesh", "Enums"), NavMeshRenderMode::UpdateFrequency);

            const MyGUI::Colour colour = Manager::get<MyGUI::Colour>("colour", "Enums");
            EXPECT_FLOAT_EQ(colour.red, 0.25f);
            EXPECT_FLOAT_EQ(colour.green, 0.5f);
            EXPECT_FLOAT_EQ(colour.blue, 0.75f);
            EXPECT_FLOAT_EQ(colour.alpha, 1.f);

            EXPECT_EQ(Manager::get<VFS::Path::Normalized>("path", "Enums"), VFS::Path::Normalized("textures/some.dds"));
        }

        TEST(SettingsParseEnumTest, knowsEveryAxisAndMode)
        {
            EXPECT_EQ(parseGyroscopeAxis("x"), GyroscopeAxis::X);
            EXPECT_EQ(parseGyroscopeAxis("y"), GyroscopeAxis::Y);
            EXPECT_EQ(parseGyroscopeAxis("z"), GyroscopeAxis::Z);
            EXPECT_EQ(parseGyroscopeAxis("-x"), GyroscopeAxis::MinusX);
            EXPECT_EQ(parseGyroscopeAxis("-y"), GyroscopeAxis::MinusY);
            EXPECT_EQ(parseGyroscopeAxis("-z"), GyroscopeAxis::MinusZ);
            EXPECT_THAT([] { parseGyroscopeAxis("w"); },
                ThrowsMessage<std::runtime_error>(HasSubstr("Invalid gyroscope axis: w")));

            EXPECT_EQ(parseNavMeshRenderMode("area type"), NavMeshRenderMode::AreaType);
            EXPECT_EQ(parseNavMeshRenderMode("update frequency"), NavMeshRenderMode::UpdateFrequency);
            EXPECT_THAT([] { parseNavMeshRenderMode("by height"); },
                ThrowsMessage<std::invalid_argument>(HasSubstr("Invalid navigation mesh rendering mode: by height")));
        }

        TEST_F(SettingsManagerTest, writesEveryKindOfValueAsText)
        {
            Manager::setInt("int", "Out", -3);
            Manager::setUInt64("uint64", "Out", 18446744073709551615ull);
            Manager::setFloat("float", "Out", 0.5f);
            Manager::setDouble("double", "Out", 0.25);
            Manager::setBool("on", "Out", true);
            Manager::setBool("off", "Out", false);
            Manager::setVector2("two", "Out", osg::Vec2f(1.f, 2.5f));
            Manager::setVector3("three", "Out", osg::Vec3f(1.f, 2.f, 3.5f));
            Manager::setStringArray("list", "Out", { " a ", "b", "c" });
            Manager::setStringArray("none", "Out", {});

            EXPECT_EQ(Manager::getString("int", "Out"), "-3");
            EXPECT_EQ(Manager::getString("uint64", "Out"), "18446744073709551615");
            EXPECT_EQ(Manager::getString("float", "Out"), "0.5");
            EXPECT_EQ(Manager::getString("double", "Out"), "0.25");
            EXPECT_EQ(Manager::getString("on", "Out"), "true");
            EXPECT_EQ(Manager::getString("off", "Out"), "false");
            EXPECT_EQ(Manager::getString("two", "Out"), "1 2.5");
            EXPECT_EQ(Manager::getString("three", "Out"), "1 2 3.5");
            EXPECT_EQ(Manager::getString("list", "Out"), "a,b,c");
            EXPECT_EQ(Manager::getString("none", "Out"), "");
            EXPECT_EQ(Manager::getVector3("three", "Out"), osg::Vec3f(1.f, 2.f, 3.5f));
        }

        TEST_F(SettingsManagerTest, theOverloadsOfSetChooseTheRightEncoding)
        {
            Manager::set("int", "Set", -7);
            Manager::set("unsigned", "Set", 7u);
            Manager::set("unsignedLong", "Set", 8ul);
            Manager::set("unsignedLongLong", "Set", 9ull);
            Manager::set("float", "Set", 1.5f);
            Manager::set("double", "Set", 2.5);
            Manager::set("string", "Set", std::string("text"));
            Manager::set("bool", "Set", true);
            Manager::set("two", "Set", osg::Vec2f(3.f, 4.f));
            Manager::set("three", "Set", osg::Vec3f(5.f, 6.f, 7.f));
            Manager::set("shape", "Set", DetourNavigator::CollisionShapeType::RotatingBox);
            Manager::set("list", "Set", std::vector<std::string>{ "x", "y" });
            Manager::set("colour", "Set", MyGUI::Colour(0.f, 0.5f, 1.f, 1.f));
            Manager::set("window", "Set", WindowMode::WindowedFullscreen);
            Manager::set("vsync", "Set", SDLUtil::VSyncMode::Adaptive);

            EXPECT_EQ(Manager::getString("int", "Set"), "-7");
            EXPECT_EQ(Manager::getString("unsigned", "Set"), "7");
            EXPECT_EQ(Manager::getString("unsignedLong", "Set"), "8");
            EXPECT_EQ(Manager::getString("unsignedLongLong", "Set"), "9");
            EXPECT_EQ(Manager::getString("float", "Set"), "1.5");
            EXPECT_EQ(Manager::getString("double", "Set"), "2.5");
            EXPECT_EQ(Manager::getString("string", "Set"), "text");
            EXPECT_EQ(Manager::getString("bool", "Set"), "true");
            EXPECT_EQ(Manager::getString("two", "Set"), "3 4");
            EXPECT_EQ(Manager::getString("three", "Set"), "5 6 7");
            EXPECT_EQ(Manager::getString("shape", "Set"), "1");
            EXPECT_EQ(Manager::getString("list", "Set"), "x,y");
            EXPECT_EQ(Manager::get<MyGUI::Colour>("colour", "Set"), MyGUI::Colour(0.f, 0.5f, 1.f, 1.f));
            EXPECT_EQ(Manager::getString("window", "Set"), "1");
            EXPECT_EQ(Manager::getString("vsync", "Set"), "2");
        }

        TEST_F(SettingsManagerTest, theModeOfTheHeadRelatedTransferIsStoredAsASignedNumber)
        {
            Manager::set("off", "Hrtf", HrtfMode::Disable);
            Manager::set("on", "Hrtf", HrtfMode::Enable);
            Manager::set("auto", "Hrtf", HrtfMode::Auto);
            Manager::set("unknown", "Hrtf", static_cast<HrtfMode>(42));

            EXPECT_EQ(Manager::getString("off", "Hrtf"), "0");
            EXPECT_EQ(Manager::getString("on", "Hrtf"), "1");
            EXPECT_EQ(Manager::getString("auto", "Hrtf"), "-1");
            EXPECT_EQ(Manager::getString("unknown", "Hrtf"), "-1");
        }

        TEST_F(SettingsManagerTest, aChangeIsRememberedOnlyWhenTheValueIsDifferent)
        {
            Manager::setString("a", "Changes", "1");
            ASSERT_THAT(Manager::getPendingChanges(), ElementsAre(CategorySetting("Changes", "a")));

            Manager::resetPendingChanges();
            EXPECT_THAT(Manager::getPendingChanges(), IsEmpty());

            Manager::setString("a", "Changes", "1");
            EXPECT_THAT(Manager::getPendingChanges(), IsEmpty());

            Manager::setString("a", "Changes", "2");
            EXPECT_THAT(Manager::getPendingChanges(), ElementsAre(CategorySetting("Changes", "a")));
        }

        TEST_F(SettingsManagerTest, pendingChangesCanBeFilteredAndResetOneByOne)
        {
            Manager::setInt("a", "Video", 1);
            Manager::setInt("b", "Video", 2);
            Manager::setInt("c", "Sound", 3);

            const CategorySettingVector filter{ { "Video", "b" }, { "Sound", "c" }, { "Sound", "other" } };
            EXPECT_THAT(Manager::getPendingChanges(filter),
                ElementsAre(CategorySetting("Sound", "c"), CategorySetting("Video", "b")));
            EXPECT_THAT(Manager::getPendingChanges(), SizeIs(3));

            Manager::resetPendingChanges(filter);
            EXPECT_THAT(Manager::getPendingChanges(), ElementsAre(CategorySetting("Video", "a")));
        }

        TEST_F(SettingsManagerTest, clearForgetsEverything)
        {
            setDefault("A", "a", "1");
            Manager::setInt("a", "A", 2);
            Manager::get<int>("a", "A");

            Manager::clear();

            EXPECT_TRUE(Manager::mDefaultSettings.empty());
            EXPECT_TRUE(Manager::mUserSettings.empty());
            EXPECT_THAT(Manager::getPendingChanges(), IsEmpty());

            // The other tests use the values that the main function made, so put them back.
            SettingsFileParser().loadSettingsFile(std::filesystem::path{ OPENFALLOUT_PROJECT_SOURCE_DIR } / "files"
                    / Misc::StringUtils::stringToU8String("settings-default.cfg"),
                Manager::mDefaultSettings);
            StaticValues::initDefaults();
            Manager::mUserSettings = Manager::mDefaultSettings;
            StaticValues::init();
        }

        // The file of the user keeps the comments of the player and only the settings that were changed.
        struct SettingsSaveTest : SettingsManagerTest
        {
            static std::string slurp(const std::filesystem::path& path)
            {
                std::ifstream stream(path);
                return std::string(std::istreambuf_iterator<char>(stream), {});
            }

            static void spit(const std::filesystem::path& path, const std::string& text)
            {
                std::ofstream(path) << text;
            }

            std::filesystem::path pathFor(const std::string& name) const
            {
                return TestingOpenMW::currentTestDirPath() / Misc::StringUtils::stringToU8String(name);
            }
        };

        TEST_F(SettingsSaveTest, aNewFileStartsWithAnExplanation)
        {
            setUser("Video", "mode", "2");
            const auto path = pathFor("any.cfg");

            Manager::saveUser(path);

            const std::string text = slurp(path);
            EXPECT_THAT(text, HasSubstr("# This is the OpenFallout user 'any.cfg' file."));
            EXPECT_THAT(text, HasSubstr("# to its default, simply remove it from this file."));
            EXPECT_THAT(text, Not(HasSubstr("documentation at")));
            EXPECT_THAT(text, HasSubstr("\n[Video]\nmode = 2\n"));
        }

        TEST_F(SettingsSaveTest, theGameSettingsFilePointsToTheDocumentation)
        {
            setUser("Video", "mode", "2");
            const auto path = pathFor("settings.cfg");

            Manager::saveUser(path);

            EXPECT_THAT(slurp(path), HasSubstr("documentation at:"));
        }

        TEST_F(SettingsSaveTest, theEditorSettingsFilePointsToTheSource)
        {
            setUser("Video", "mode", "2");
            const auto path = pathFor("openfallout-cs.cfg");

            Manager::saveUser(path);

            EXPECT_THAT(slurp(path), HasSubstr("apps/opencs/model/prefs/values.hpp"));
        }

        TEST_F(SettingsSaveTest, anExistingFileKeepsItsCommentsAndGetsNewValues)
        {
            const auto path = pathFor("settings.cfg");
            spit(path,
                "# my own words\n"
                "[Video]\n"
                "# the resolution\n"
                "width = 800\n"
                "\n"
                "[Sound]\n"
                "volume = 1\n");
            setUser("Video", "width", "1024");
            setUser("Video", "height", "768");
            setUser("Sound", "volume", "1");
            setUser("Input", "invert", "true");

            Manager::saveUser(path);

            EXPECT_EQ(slurp(path),
                "# my own words\n"
                "[Video]\n"
                "# the resolution\n"
                "width = 1024\n"
                "height = 768\n"
                "\n"
                "[Sound]\n"
                "volume = 1\n"
                "\n"
                "[Input]\n"
                "invert = true\n");
        }

        TEST_F(SettingsSaveTest, aSettingAddedToTheLastCategoryGoesBeforeTheNewCategories)
        {
            const auto path = pathFor("settings.cfg");
            spit(path,
                "[Sound]\n"
                "volume = 1\n");
            setUser("Sound", "volume", "1");
            setUser("Sound", "music", "0.5");
            setUser("Video", "mode", "2");

            Manager::saveUser(path);

            EXPECT_EQ(slurp(path),
                "[Sound]\n"
                "volume = 1\n"
                "music = 0.5\n"
                "\n"
                "[Video]\n"
                "mode = 2\n");
        }

        TEST_F(SettingsSaveTest, aFileThatIsAlreadyRightIsNotWrittenAgain)
        {
            const auto path = pathFor("settings.cfg");
            spit(path,
                "[Sound]\n"
                "volume = 1\n");
            const auto old = std::filesystem::file_time_type::clock::now() - std::chrono::hours(48);
            std::filesystem::last_write_time(path, old);
            setUser("Sound", "volume", "1");

            Manager::saveUser(path);

            EXPECT_EQ(std::filesystem::last_write_time(path), old);
            EXPECT_EQ(slurp(path),
                "[Sound]\n"
                "volume = 1\n");
        }

        TEST_F(SettingsSaveTest, aSettingTheProgramDoesNotKnowIsCommentedOut)
        {
            const auto path = pathFor("settings.cfg");
            spit(path,
                "[Sound]\n"
                "volume = 1\n"
                "added by hand = 3\n");
            setUser("Sound", "volume", "2");

            Manager::saveUser(path);

            EXPECT_EQ(slurp(path),
                "[Sound]\n"
                "volume = 2\n"
                "# ignoring setting modified while the program was running: added by hand = 3\n");
        }

        TEST_F(SettingsSaveTest, brokenLinesAreCommentedOutInsteadOfLost)
        {
            const auto path = pathFor("settings.cfg");
            spit(path,
                "orphan = 1\n"
                "[Sound\n"
                "[Sound]\n"
                "no equals sign\n"
                "volume = 1\n");
            setUser("Sound", "volume", "1");

            Manager::saveUser(path);

            EXPECT_EQ(slurp(path),
                "# empty category name: orphan = 1\n"
                "# unterminated category: [Sound\n"
                "[Sound]\n"
                "# unterminated setting name: no equals sign\n"
                "volume = 1\n");
        }

        TEST_F(SettingsSaveTest, aSettingOnTheLineOfItsCategoryIsKept)
        {
            const auto path = pathFor("settings.cfg");
            spit(path, "[Sound] volume = 1\n");
            setUser("Sound", "volume", "3");

            Manager::saveUser(path);

            EXPECT_EQ(slurp(path),
                "[Sound]\n"
                "volume = 3\n");
        }

        TEST_F(SettingsSaveTest, savedSettingsCanBeLoadedAgain)
        {
            const auto path = pathFor("round.cfg");
            setUser("Video", "mode", "2");
            setUser("Video", "name", "two words");
            setUser("Sound", "volume", "0.25");

            Manager::saveUser(path);

            CategorySettingValueMap loaded;
            SettingsFileParser().loadSettingsFile(path, loaded);
            EXPECT_EQ(loaded, Manager::mUserSettings);
        }
    }
}
