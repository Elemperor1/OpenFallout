#include <gtest/gtest.h>

#include <cmath>

#include "apps/openfallout/mwworld/timestamp.hpp"

namespace OFWorld
{
    namespace
    {
        TEST(OFWorldTimeStampTest, operatorPlusShouldNotChangeTimeStampForZero)
        {
            TimeStamp timeStamp;
            timeStamp += 0;
            EXPECT_EQ(timeStamp.getDay(), 0);
            EXPECT_EQ(timeStamp.getHour(), 0);
        }

        TEST(OFWorldTimeStampTest, operatorPlusShouldProperlyHandleDoubleValuesCloseTo24)
        {
            TimeStamp timeStamp;
            timeStamp += std::nextafter(24.0, 0.0);
            EXPECT_EQ(timeStamp.getDay(), 0);
            EXPECT_LT(timeStamp.getHour(), 24);
            EXPECT_FLOAT_EQ(timeStamp.getHour(), 24);
        }

        TEST(OFWorldTimeStampTest, operatorPlusShouldProperlyHandleFloatValuesCloseTo24)
        {
            TimeStamp timeStamp;
            timeStamp += std::nextafter(24.0f, 0.0f);
            EXPECT_EQ(timeStamp.getDay(), 0);
            EXPECT_LT(timeStamp.getHour(), 24);
            EXPECT_FLOAT_EQ(timeStamp.getHour(), 24);
        }

        TEST(OFWorldTimeStampTest, operatorPlusShouldAddDaysForEach24Hours)
        {
            TimeStamp timeStamp;
            timeStamp += 24.0 * 42;
            EXPECT_EQ(timeStamp.getDay(), 42);
            EXPECT_EQ(timeStamp.getHour(), 0);
        }

        TEST(OFWorldTimeStampTest, operatorPlusShouldAddDaysForEach24HoursAndSetRemainderToHours)
        {
            TimeStamp timeStamp;
            timeStamp += 24.0 * 42 + 13.0;
            EXPECT_EQ(timeStamp.getDay(), 42);
            EXPECT_EQ(timeStamp.getHour(), 13);
        }

        TEST(OFWorldTimeStampTest, operatorPlusShouldAccumulateExistingValue)
        {
            TimeStamp timeStamp(13, 42);
            timeStamp += 24.0 * 2 + 17.0;
            EXPECT_EQ(timeStamp.getDay(), 45);
            EXPECT_EQ(timeStamp.getHour(), 6);
        }

        TEST(OFWorldTimeStampTest, operatorPlusShouldThrowExceptionForNegativeValue)
        {
            TimeStamp timeStamp(13, 42);
            EXPECT_THROW(timeStamp += -1, std::runtime_error);
        }
    }
}
