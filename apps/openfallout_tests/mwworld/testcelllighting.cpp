#include <gtest/gtest.h>

#include <components/esm4/lighting.hpp>

#include "apps/openfallout/mwworld/cell.hpp"

namespace OFWorld
{
    namespace
    {
        constexpr std::uint32_t sAmbient = 0x01;
        constexpr std::uint32_t sDirectional = 0x02;
        constexpr std::uint32_t sFogColor = 0x04;
        constexpr std::uint32_t sFogNear = 0x08;
        constexpr std::uint32_t sFogFar = 0x10;
        constexpr std::uint32_t sRotation = 0x20;
        constexpr std::uint32_t sFogDirFade = 0x40;
        constexpr std::uint32_t sFogClipDist = 0x80;
        constexpr std::uint32_t sFogPower = 0x100;

        ESM4::Lighting makeLighting(std::uint32_t seed)
        {
            ESM4::Lighting result;
            result.ambient = seed + 1;
            result.directional = seed + 2;
            result.fogColor = seed + 3;
            result.fogNear = static_cast<float>(seed + 4);
            result.fogFar = static_cast<float>(seed + 5);
            result.rotationXY = static_cast<std::int32_t>(seed + 6);
            result.rotationZ = static_cast<std::int32_t>(seed + 7);
            result.fogDirFade = static_cast<float>(seed + 8);
            result.fogClipDist = static_cast<float>(seed + 9);
            result.fogPower = static_cast<float>(seed + 10);
            return result;
        }

        TEST(OFWorldCellLightingTest, withoutTemplateTheCellKeepsItsOwnLighting)
        {
            const ESM4::Lighting own = makeLighting(100);
            const ESM4::Lighting result = resolveLighting(own, nullptr, 0x1ff);
            EXPECT_EQ(result.ambient, own.ambient);
            EXPECT_EQ(result.fogFar, own.fogFar);
            EXPECT_EQ(result.fogPower, own.fogPower);
        }

        TEST(OFWorldCellLightingTest, withoutFlagsTheCellKeepsItsOwnLighting)
        {
            const ESM4::Lighting own = makeLighting(100);
            const ESM4::Lighting lightingTemplate = makeLighting(200);
            const ESM4::Lighting result = resolveLighting(own, &lightingTemplate, 0);
            EXPECT_EQ(result.ambient, own.ambient);
            EXPECT_EQ(result.directional, own.directional);
            EXPECT_EQ(result.fogColor, own.fogColor);
            EXPECT_EQ(result.fogNear, own.fogNear);
            EXPECT_EQ(result.fogFar, own.fogFar);
            EXPECT_EQ(result.rotationXY, own.rotationXY);
            EXPECT_EQ(result.rotationZ, own.rotationZ);
            EXPECT_EQ(result.fogDirFade, own.fogDirFade);
            EXPECT_EQ(result.fogClipDist, own.fogClipDist);
            EXPECT_EQ(result.fogPower, own.fogPower);
        }

        TEST(OFWorldCellLightingTest, allFlagsTakeEverythingFromTheTemplate)
        {
            const ESM4::Lighting own = makeLighting(100);
            const ESM4::Lighting lightingTemplate = makeLighting(200);
            const ESM4::Lighting result = resolveLighting(own, &lightingTemplate, 0x1ff);
            EXPECT_EQ(result.ambient, lightingTemplate.ambient);
            EXPECT_EQ(result.directional, lightingTemplate.directional);
            EXPECT_EQ(result.fogColor, lightingTemplate.fogColor);
            EXPECT_EQ(result.fogNear, lightingTemplate.fogNear);
            EXPECT_EQ(result.fogFar, lightingTemplate.fogFar);
            EXPECT_EQ(result.rotationXY, lightingTemplate.rotationXY);
            EXPECT_EQ(result.rotationZ, lightingTemplate.rotationZ);
            EXPECT_EQ(result.fogDirFade, lightingTemplate.fogDirFade);
            EXPECT_EQ(result.fogClipDist, lightingTemplate.fogClipDist);
            EXPECT_EQ(result.fogPower, lightingTemplate.fogPower);
        }

        TEST(OFWorldCellLightingTest, eachFlagTakesOnlyItsOwnValue)
        {
            const ESM4::Lighting own = makeLighting(100);
            const ESM4::Lighting lightingTemplate = makeLighting(200);

            const ESM4::Lighting ambient = resolveLighting(own, &lightingTemplate, sAmbient);
            EXPECT_EQ(ambient.ambient, lightingTemplate.ambient);
            EXPECT_EQ(ambient.directional, own.directional);

            const ESM4::Lighting directional = resolveLighting(own, &lightingTemplate, sDirectional);
            EXPECT_EQ(directional.directional, lightingTemplate.directional);
            EXPECT_EQ(directional.ambient, own.ambient);

            const ESM4::Lighting fogColor = resolveLighting(own, &lightingTemplate, sFogColor);
            EXPECT_EQ(fogColor.fogColor, lightingTemplate.fogColor);
            EXPECT_EQ(fogColor.fogNear, own.fogNear);

            const ESM4::Lighting fogNear = resolveLighting(own, &lightingTemplate, sFogNear);
            EXPECT_EQ(fogNear.fogNear, lightingTemplate.fogNear);
            EXPECT_EQ(fogNear.fogFar, own.fogFar);

            const ESM4::Lighting fogFar = resolveLighting(own, &lightingTemplate, sFogFar);
            EXPECT_EQ(fogFar.fogFar, lightingTemplate.fogFar);
            EXPECT_EQ(fogFar.fogNear, own.fogNear);

            const ESM4::Lighting rotation = resolveLighting(own, &lightingTemplate, sRotation);
            EXPECT_EQ(rotation.rotationXY, lightingTemplate.rotationXY);
            EXPECT_EQ(rotation.rotationZ, lightingTemplate.rotationZ);
            EXPECT_EQ(rotation.fogDirFade, own.fogDirFade);

            const ESM4::Lighting fogDirFade = resolveLighting(own, &lightingTemplate, sFogDirFade);
            EXPECT_EQ(fogDirFade.fogDirFade, lightingTemplate.fogDirFade);
            EXPECT_EQ(fogDirFade.fogClipDist, own.fogClipDist);

            const ESM4::Lighting fogClipDist = resolveLighting(own, &lightingTemplate, sFogClipDist);
            EXPECT_EQ(fogClipDist.fogClipDist, lightingTemplate.fogClipDist);
            EXPECT_EQ(fogClipDist.fogPower, own.fogPower);

            const ESM4::Lighting fogPower = resolveLighting(own, &lightingTemplate, sFogPower);
            EXPECT_EQ(fogPower.fogPower, lightingTemplate.fogPower);
            EXPECT_EQ(fogPower.ambient, own.ambient);
        }

        TEST(OFWorldCellLightingTest, unknownFlagBitsAreIgnored)
        {
            const ESM4::Lighting own = makeLighting(100);
            const ESM4::Lighting lightingTemplate = makeLighting(200);
            const ESM4::Lighting result = resolveLighting(own, &lightingTemplate, 0xfffffe00);
            EXPECT_EQ(result.ambient, own.ambient);
            EXPECT_EQ(result.fogPower, own.fogPower);
        }

        TEST(OFWorldCellLightingTest, fogRangeNeedsAFarDistanceBeyondTheNearOne)
        {
            MoodData mood{};
            EXPECT_FALSE(mood.hasFogRange());

            mood.mFogNear = 500.f;
            mood.mFogFar = 500.f;
            EXPECT_FALSE(mood.hasFogRange());

            mood.mFogFar = 100.f;
            EXPECT_FALSE(mood.hasFogRange());

            mood.mFogFar = 3000.f;
            EXPECT_TRUE(mood.hasFogRange());

            mood.mFogNear = 0.f;
            EXPECT_TRUE(mood.hasFogRange());
        }
    }
}
