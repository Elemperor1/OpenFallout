#include <apps/openfallout/mwrender/waterlook.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <components/esm4/loadwatr.hpp>

namespace
{
    using namespace OFRender;

    // The first 52 bytes of the settings of a water: the sun power, the reflectivity and the fresnel amount after 16
    // bytes that are not known, and the shallow, the deep and the reflection colour from the 40th byte on.
    std::vector<std::uint8_t> settings(float reflectivity, std::uint8_t shallowRed, std::uint8_t deepGreen)
    {
        std::vector<std::uint8_t> bytes(52, 0);
        const float sunPower = 826.f;
        const float fresnel = 0.75f;
        std::memcpy(bytes.data() + 16, &sunPower, sizeof(float));
        std::memcpy(bytes.data() + 20, &reflectivity, sizeof(float));
        std::memcpy(bytes.data() + 24, &fresnel, sizeof(float));
        bytes[40] = shallowRed;
        bytes[43] = 255; // the alpha byte of the shallow colour, which is not read
        bytes[45] = deepGreen;
        return bytes;
    }

    ESM4::Water makeWater(std::uint8_t opacity, float reflectivity)
    {
        ESM4::Water water;
        water.mOpacity = opacity;
        water.mVisualData = settings(reflectivity, 51, 102);
        return water;
    }

    TEST(OpenFalloutRenderWaterLook, standardWaterShowsTheGroundAsItIsAndAllOfTheReflection)
    {
        const WaterLook look = WaterLook::standard();
        EXPECT_EQ(look.mOpacity, 0.f);
        EXPECT_EQ(look.mReflectivity, 1.f);
        // The deep colour is the one the shader had for the water of Morrowind, WATER_COLOR of fog.glsl
        EXPECT_FLOAT_EQ(look.mDeepColour.x(), 0.090195f);
        EXPECT_FLOAT_EQ(look.mDeepColour.y(), 0.115685f);
        EXPECT_FLOAT_EQ(look.mDeepColour.z(), 0.12745f);
    }

    TEST(OpenFalloutRenderWaterLook, takesTheColoursOfTheSettingsAsFractions)
    {
        const std::optional<WaterLook> look = makeWaterLook(makeWater(50, 0.6f));
        ASSERT_TRUE(look.has_value());
        EXPECT_FLOAT_EQ(look->mShallowColour.x(), 51.f / 255.f);
        EXPECT_FLOAT_EQ(look->mShallowColour.y(), 0.f);
        EXPECT_FLOAT_EQ(look->mShallowColour.z(), 0.f);
        EXPECT_FLOAT_EQ(look->mDeepColour.x(), 0.f);
        EXPECT_FLOAT_EQ(look->mDeepColour.y(), 102.f / 255.f);
        EXPECT_FLOAT_EQ(look->mDeepColour.z(), 0.f);
    }

    TEST(OpenFalloutRenderWaterLook, takesTheOpacityAsAPercentageAndTheReflectivityAsItIs)
    {
        const std::optional<WaterLook> look = makeWaterLook(makeWater(50, 0.6f));
        ASSERT_TRUE(look.has_value());
        EXPECT_FLOAT_EQ(look->mOpacity, 0.5f);
        EXPECT_FLOAT_EQ(look->mReflectivity, 0.6f);
    }

    TEST(OpenFalloutRenderWaterLook, keepsTheOpacityAndTheReflectivityInTheRangeOfTheShader)
    {
        EXPECT_FLOAT_EQ(makeWaterLook(makeWater(255, 0.6f))->mOpacity, 1.f);
        EXPECT_FLOAT_EQ(makeWaterLook(makeWater(0, 0.6f))->mOpacity, 0.f);
        EXPECT_FLOAT_EQ(makeWaterLook(makeWater(50, 3.f))->mReflectivity, 1.f);
        EXPECT_FLOAT_EQ(makeWaterLook(makeWater(50, -1.f))->mReflectivity, 0.f);
    }

    TEST(OpenFalloutRenderWaterLook, hasNoLookWithoutSettings)
    {
        EXPECT_FALSE(makeWaterLook(ESM4::Water()).has_value());
    }
}
