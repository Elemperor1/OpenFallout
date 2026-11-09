#include <components/fallback/fallback.hpp>
#include <components/fallback/validate.hpp>

#include <boost/any.hpp>
#include <boost/program_options/errors.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

// The settings that were in the INI file of Morrowind and that the games after it keep in the same shape: the keys that
// are allowed, the check of a list of them in an option and the lookup of a value with its default.
namespace
{
    TEST(FallbackKeyTest, knowsTheKindOfEachKey)
    {
        EXPECT_TRUE(Fallback::isAllowedIntFallbackKey("LightAttenuation_UseConstant"));
        EXPECT_FALSE(Fallback::isAllowedIntFallbackKey("Water_World_Alpha"));
        EXPECT_TRUE(Fallback::isAllowedFloatFallbackKey("Water_World_Alpha"));
        EXPECT_FALSE(Fallback::isAllowedFloatFallbackKey("Fonts_Font_0"));
        EXPECT_TRUE(Fallback::isAllowedNonNumericFallbackKey("Fonts_Font_0"));
        EXPECT_TRUE(Fallback::isAllowedNonNumericFallbackKey("Blood_Texture_3"));
        EXPECT_TRUE(Fallback::isAllowedNonNumericFallbackKey("Level_Up_Level12"));
        EXPECT_FALSE(Fallback::isAllowedNonNumericFallbackKey("Level_Up"));
        EXPECT_TRUE(Fallback::isAllowedUnusedFallbackKey("Inventory_UniformScaling"));
        EXPECT_FALSE(Fallback::isAllowedUnusedFallbackKey("Fonts_Font_0"));
    }

    TEST(FallbackValidateTest, collectsTheKeysAndTheirValues)
    {
        boost::any value;
        Fallback::validate(value, { "Fonts_Font_0,magic_cards", "Water_World_Alpha,0.5,more" }, nullptr, 0);
        // a second option adds to the first, and the last value of a key wins
        Fallback::validate(value, { "Fonts_Font_0,daedric" }, nullptr, 0);

        const auto* map = boost::any_cast<Fallback::FallbackMap>(&value);
        ASSERT_NE(map, nullptr);
        EXPECT_EQ(map->mMap.size(), 2u);
        EXPECT_EQ(map->mMap.at("Fonts_Font_0"), "daedric");
        EXPECT_EQ(map->mMap.at("Water_World_Alpha"), "0.5,more");
    }

    TEST(FallbackValidateTest, refusesATokenWithoutAKeyOrWithoutAValue)
    {
        for (const std::string token : { "nothing", ",value", "key," })
        {
            boost::any value;
            EXPECT_THROW(Fallback::validate(value, { token }, nullptr, 0), boost::program_options::validation_error)
                << token;
        }
    }

    TEST(FallbackMapTest, keepsTheValuesOfTheKnownKeysAndAnswersTheOthersWithTheirDefault)
    {
        Fallback::Map::init({
            { "LightAttenuation_UseConstant", "1" },
            { "Water_NearWaterPoints", "x" },
            { "Water_World_Alpha", "0.25" },
            { "Water_Map_Alpha", "bad" },
            { "Fonts_Font_1", "daedric" },
            { "FontColor_color_normal", "202,165,96" },
            { "FontColor_color_header", "not a colour" },
            { "FontColor_color_link", "1,2" },
            { "Inventory_UniformScaling", "1" },
            { "Not_A_Key_At_All", "1" },
        });

        EXPECT_EQ(Fallback::Map::getInt("LightAttenuation_UseConstant"), 1);
        EXPECT_TRUE(Fallback::Map::getBool("LightAttenuation_UseConstant"));
        // a number that cannot be read becomes 0
        EXPECT_EQ(Fallback::Map::getInt("Water_NearWaterPoints"), 0);
        EXPECT_EQ(Fallback::Map::getFloat("Water_World_Alpha"), 0.25f);
        EXPECT_EQ(Fallback::Map::getFloat("Water_Map_Alpha"), 0.f);
        EXPECT_EQ(Fallback::Map::getString("Fonts_Font_1"), "daedric");
        // a key that is allowed and not set is empty
        EXPECT_EQ(Fallback::Map::getString("Fonts_Font_2"), "");
        EXPECT_EQ(Fallback::Map::getFloat("Water_RippleLifetime"), 0.f);
        EXPECT_EQ(Fallback::Map::getInt("Water_RippleFrameCount"), 0);
        EXPECT_FALSE(Fallback::Map::getBool("Water_RippleFrameCount"));
        EXPECT_EQ(Fallback::Map::getIntFallbackMap().count("LightAttenuation_UseConstant"), 1u);
        EXPECT_EQ(Fallback::Map::getFloatFallbackMap().count("Water_World_Alpha"), 1u);
        EXPECT_EQ(Fallback::Map::getNonNumericFallbackMap().count("Fonts_Font_1"), 1u);
        EXPECT_EQ(Fallback::Map::getNonNumericFallbackMap().count("Not_A_Key_At_All"), 0u);
    }

    TEST(FallbackMapTest, refusesAKeyThatItDoesNotKnow)
    {
        EXPECT_THROW(Fallback::Map::getInt("Fonts_Font_0"), std::logic_error);
        EXPECT_THROW(Fallback::Map::getFloat("Fonts_Font_0"), std::logic_error);
        EXPECT_THROW(Fallback::Map::getString("Water_World_Alpha"), std::logic_error);
        EXPECT_THROW(Fallback::Map::getBool("Nothing"), std::logic_error);
    }

    TEST(FallbackMapTest, readsAColourAsThreeBytes)
    {
        Fallback::Map::init({
            { "FontColor_color_normal", "202,165,96" },
            { "FontColor_color_header", "not a colour" },
            { "FontColor_color_link", "1,2" },
        });

        const osg::Vec4f colour = Fallback::Map::getColour("FontColor_color_normal");
        EXPECT_FLOAT_EQ(colour.r(), 202.f / 255.f);
        EXPECT_FLOAT_EQ(colour.g(), 165.f / 255.f);
        EXPECT_FLOAT_EQ(colour.b(), 96.f / 255.f);
        EXPECT_FLOAT_EQ(colour.a(), 1.f);
        // a colour that is not there, or is not three numbers, is a middle gray
        EXPECT_EQ(Fallback::Map::getColour("FontColor_color_misc"), osg::Vec4f(0.5f, 0.5f, 0.5f, 1.f));
        EXPECT_EQ(Fallback::Map::getColour("FontColor_color_header"), osg::Vec4f(0.5f, 0.5f, 0.5f, 1.f));
        EXPECT_EQ(Fallback::Map::getColour("FontColor_color_link"), osg::Vec4f(0.5f, 0.5f, 0.5f, 1.f));
    }
}
