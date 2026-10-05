#include <gtest/gtest.h>

#include <cmath>

#include "apps/openfallout/mwworld/duration.hpp"

namespace OFWorld
{
    namespace
    {
        TEST(OFWorldDurationTest, fromHoursShouldProduceZeroDaysAndHoursFor0)
        {
            const Duration duration = Duration::fromHours(0);
            EXPECT_EQ(duration.getDays(), 0);
            EXPECT_EQ(duration.getHours(), 0);
        }

        TEST(OFWorldDurationTest, fromHoursShouldProduceOneDayAndZeroHoursFor24)
        {
            const Duration duration = Duration::fromHours(24);
            EXPECT_EQ(duration.getDays(), 1);
            EXPECT_EQ(duration.getHours(), 0);
        }

        TEST(OFWorldDurationTest, fromHoursShouldProduceOneDayAndRemainderHoursFor42)
        {
            const Duration duration = Duration::fromHours(42);
            EXPECT_EQ(duration.getDays(), 1);
            EXPECT_EQ(duration.getHours(), 18);
        }

        TEST(OFWorldDurationTest, fromHoursShouldProduceZeroDaysAndZeroHoursForMinDouble)
        {
            const Duration duration = Duration::fromHours(std::numeric_limits<double>::min());
            EXPECT_EQ(duration.getDays(), 0);
            EXPECT_EQ(duration.getHours(), 0);
        }

        TEST(OFWorldDurationTest, fromHoursShouldProduceZeroDaysAndSomeHoursForMinFloat)
        {
            const Duration duration = Duration::fromHours(std::numeric_limits<float>::min());
            EXPECT_EQ(duration.getDays(), 0);
            EXPECT_GT(duration.getHours(), 0);
            EXPECT_FLOAT_EQ(duration.getHours(), std::numeric_limits<float>::min());
        }

        TEST(OFWorldDurationTest, fromHoursShouldProduceZeroDaysAndRemainderHoursForValueJustBelow24InDoublePrecision)
        {
            const Duration duration = Duration::fromHours(std::nextafter(24.0, 0.0));
            EXPECT_EQ(duration.getDays(), 0);
            EXPECT_LT(duration.getHours(), 24);
            EXPECT_FLOAT_EQ(duration.getHours(), 24);
        }

        TEST(OFWorldDurationTest, fromHoursShouldProduceZeroDaysAndRemainderHoursForValueJustBelow24InFloatPrecision)
        {
            const Duration duration = Duration::fromHours(std::nextafter(24.0f, 0.0f));
            EXPECT_EQ(duration.getDays(), 0);
            EXPECT_LT(duration.getHours(), 24);
            EXPECT_FLOAT_EQ(duration.getHours(), 24);
        }
    }
}
