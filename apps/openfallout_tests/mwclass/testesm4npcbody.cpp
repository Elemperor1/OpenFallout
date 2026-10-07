#include <gtest/gtest.h>

#include <limits>

#include "apps/openfallout/mwclass/esm4npc.hpp"

namespace OFClass
{
    namespace
    {
        TEST(OFClassNpcBodyTest, aRaceOfTheUsualHeightIsTheBoxOfThePlaceholderPlayer)
        {
            const osg::Vec3f extents = npcBodyHalfExtents(1.f);
            EXPECT_EQ(extents, osg::Vec3f(20.f, 20.f, 64.f));
        }

        TEST(OFClassNpcBodyTest, aTallerRaceHasAWiderAndTallerBody)
        {
            const osg::Vec3f extents = npcBodyHalfExtents(1.25f);
            EXPECT_FLOAT_EQ(extents.x(), 25.f);
            EXPECT_FLOAT_EQ(extents.y(), 25.f);
            EXPECT_FLOAT_EQ(extents.z(), 80.f);
        }

        TEST(OFClassNpcBodyTest, aHeightThatIsNoNumberOrNotPositiveCountsAsOne)
        {
            const osg::Vec3f usual = npcBodyHalfExtents(1.f);
            EXPECT_EQ(npcBodyHalfExtents(0.f), usual);
            EXPECT_EQ(npcBodyHalfExtents(-2.f), usual);
            EXPECT_EQ(npcBodyHalfExtents(std::numeric_limits<float>::quiet_NaN()), usual);
            EXPECT_EQ(npcBodyHalfExtents(std::numeric_limits<float>::infinity()), usual);
        }

        TEST(OFClassNpcBodyTest, theHeightOfARaceIsLimitedToWhatTheDataHasMeant)
        {
            EXPECT_EQ(npcBodyHalfExtents(1000.f), npcBodyHalfExtents(4.f));
            EXPECT_EQ(npcBodyHalfExtents(0.001f), npcBodyHalfExtents(0.25f));
        }
    }
}
