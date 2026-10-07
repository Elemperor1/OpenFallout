#include <apps/openfallout/mwrender/fogmanager.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace OFRender;

    TEST(OpenFalloutRenderFitFogRange, keepsARangeThatEndsInsideTheViewDistance)
    {
        const auto [fogNear, fogFar] = fitFogRange(300.f, 2500.f, 7168.f);
        EXPECT_FLOAT_EQ(fogNear, 300.f);
        EXPECT_FLOAT_EQ(fogFar, 2500.f);
    }

    TEST(OpenFalloutRenderFitFogRange, keepsARangeThatEndsAtTheViewDistance)
    {
        const auto [fogNear, fogFar] = fitFogRange(300.f, 7168.f, 7168.f);
        EXPECT_FLOAT_EQ(fogNear, 300.f);
        EXPECT_FLOAT_EQ(fogFar, 7168.f);
    }

    TEST(OpenFalloutRenderFitFogRange, scalesARangeThatEndsBeyondTheViewDistanceToEndAtIt)
    {
        const auto [fogNear, fogFar] = fitFogRange(3000.f, 14336.f, 7168.f);
        EXPECT_FLOAT_EQ(fogFar, 7168.f);
        EXPECT_FLOAT_EQ(fogNear, 1500.f);
    }

    TEST(OpenFalloutRenderFitFogRange, hasTheEndOfTheRangeContinuousAcrossTheViewDistance)
    {
        const float below = fitFogRange(1000.f, 7167.f, 7168.f).second;
        const float above = fitFogRange(1000.f, 7169.f, 7168.f).second;
        EXPECT_NEAR(below, above, 2.f);
        EXPECT_NEAR(fitFogRange(1000.f, 7167.f, 7168.f).first, fitFogRange(1000.f, 7169.f, 7168.f).first, 0.5f);
    }

    TEST(OpenFalloutRenderFitFogRange, keepsTheNearDistanceBeforeTheFarOne)
    {
        for (const float fogFar : { 7000.f, 8000.f, 20000.f, 100000.f })
        {
            const auto [fogNear, fitted] = fitFogRange(0.5f * fogFar, fogFar, 7168.f);
            EXPECT_LE(fogNear, fitted) << fogFar;
        }
    }

    TEST(OpenFalloutRenderFitFogRange, leavesARangeAloneWhenThereIsNoViewDistanceToFitTo)
    {
        const auto [fogNear, fogFar] = fitFogRange(300.f, 9000.f, 0.f);
        EXPECT_FLOAT_EQ(fogNear, 300.f);
        EXPECT_FLOAT_EQ(fogFar, 9000.f);
    }
}
