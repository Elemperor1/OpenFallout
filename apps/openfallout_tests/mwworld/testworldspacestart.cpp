#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>

#include "apps/openfallout/mwworld/worldspacestart.hpp"

namespace OFWorld
{
    namespace
    {
        TEST(OFWorldWorldspaceStartTest, readsWorldspaceAndCoordinates)
        {
            const std::optional<WorldspaceStart> start = parseWorldspaceStart("WastelandNV:-2,3");
            ASSERT_TRUE(start.has_value());
            EXPECT_EQ(start->mWorldspace, "WastelandNV");
            EXPECT_EQ(start->mX, -2);
            EXPECT_EQ(start->mY, 3);
        }

        TEST(OFWorldWorldspaceStartTest, readsZeroAndLargeCoordinates)
        {
            const std::optional<WorldspaceStart> start = parseWorldspaceStart("Wasteland:0,-120");
            ASSERT_TRUE(start.has_value());
            EXPECT_EQ(start->mWorldspace, "Wasteland");
            EXPECT_EQ(start->mX, 0);
            EXPECT_EQ(start->mY, -120);
        }

        TEST(OFWorldWorldspaceStartTest, ignoresTextThatIsNotAWorldspaceStart)
        {
            EXPECT_FALSE(parseWorldspaceStart("").has_value());
            EXPECT_FALSE(parseWorldspaceStart("Balmora").has_value());
            EXPECT_FALSE(parseWorldspaceStart("-2,3").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:").has_value());
            EXPECT_FALSE(parseWorldspaceStart(":1,2").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:12").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:1,").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:,2").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:one,two").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:1,2x").has_value());
            EXPECT_FALSE(parseWorldspaceStart("WastelandNV:1,2,3").has_value());
        }

        TEST(OFWorldWorldspaceStartTest, rejectsCoordinatesThatDoNotFit)
        {
            const std::string tooBig = std::to_string(std::numeric_limits<long long>::max());
            EXPECT_THROW(parseWorldspaceStart("WastelandNV:" + tooBig + ",1"), std::runtime_error);
        }
    }
}
