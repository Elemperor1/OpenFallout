#include <components/resource/speedtree.hpp>
#include <components/testing/util.hpp>

#include <gtest/gtest.h>

#include <osg/Geometry>

#include <algorithm>
#include <cmath>

namespace
{
    using namespace testing;

    TEST(ResourceSpeedTreeTest, cardsStandOnTheGroundAndHaveTheSizeOfTheBillboard)
    {
        const osg::ref_ptr<osg::Geometry> geometry = Resource::makeCrossedCards(400.f, 700.f, 3);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(geometry->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_EQ(vertices->size(), 12u);

        float lowest = 1e9f;
        float highest = -1e9f;
        float widest = 0.f;
        for (const osg::Vec3f& vertex : *vertices)
        {
            lowest = std::min(lowest, vertex.z());
            highest = std::max(highest, vertex.z());
            widest = std::max(widest, std::hypot(vertex.x(), vertex.y()));
        }
        EXPECT_FLOAT_EQ(lowest, 0.f);
        EXPECT_FLOAT_EQ(highest, 700.f);
        // the cards cross at the trunk, so no vertex is more than half a card away from it
        EXPECT_FLOAT_EQ(widest, 200.f);
    }

    TEST(ResourceSpeedTreeTest, cardsAreTurnedEvenlyAboutTheTrunk)
    {
        const osg::ref_ptr<osg::Geometry> geometry = Resource::makeCrossedCards(100.f, 100.f, 2);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(geometry->getVertexArray());
        ASSERT_NE(vertices, nullptr);
        ASSERT_EQ(vertices->size(), 8u);
        // the first card is along x, the second along y
        EXPECT_FLOAT_EQ((*vertices)[1].x() - (*vertices)[0].x(), 100.f);
        EXPECT_NEAR((*vertices)[1].y() - (*vertices)[0].y(), 0.f, 1e-3f);
        EXPECT_NEAR((*vertices)[5].x() - (*vertices)[4].x(), 0.f, 1e-3f);
        EXPECT_FLOAT_EQ((*vertices)[5].y() - (*vertices)[4].y(), 100.f);
    }

    TEST(ResourceSpeedTreeTest, theImageRunsFromTheTopOfTheCardToItsBottom)
    {
        const osg::ref_ptr<osg::Geometry> geometry = Resource::makeCrossedCards(100.f, 200.f, 1);
        const auto* vertices = dynamic_cast<const osg::Vec3Array*>(geometry->getVertexArray());
        const auto* coordinates = dynamic_cast<const osg::Vec2Array*>(geometry->getTexCoordArray(0));
        ASSERT_NE(coordinates, nullptr);
        ASSERT_EQ(coordinates->size(), vertices->size());
        for (std::size_t i = 0; i < vertices->size(); ++i)
            EXPECT_FLOAT_EQ((*coordinates)[i].y(), (*vertices)[i].z() > 0.f ? 0.f : 1.f);
    }

    TEST(ResourceSpeedTreeTest, everyCardIsTwoTrianglesOfItsOwnVertices)
    {
        const osg::ref_ptr<osg::Geometry> geometry = Resource::makeCrossedCards(100.f, 100.f, 3);
        ASSERT_EQ(geometry->getNumPrimitiveSets(), 1u);
        const auto* indices = dynamic_cast<const osg::DrawElementsUShort*>(geometry->getPrimitiveSet(0));
        ASSERT_NE(indices, nullptr);
        ASSERT_EQ(indices->size(), 18u);
        for (std::size_t card = 0; card < 3; ++card)
            for (std::size_t i = 0; i < 6; ++i)
            {
                const unsigned index = (*indices)[card * 6 + i];
                EXPECT_GE(index, card * 4);
                EXPECT_LT(index, card * 4 + 4);
            }
    }
    TEST(ResourceSpeedTreeTest, theImagesOfTheGamesHoldClustersOf512Pixels)
    {
        const auto count = [](int width, int height) {
            const Resource::SpeedTreeClusters clusters = Resource::getSpeedTreeClusters(width, height);
            return std::pair(clusters.mColumns, clusters.mRows);
        };
        EXPECT_EQ(count(1024, 512), std::pair(2, 1));
        EXPECT_EQ(count(512, 1024), std::pair(1, 2));
        EXPECT_EQ(count(1024, 1024), std::pair(2, 2));
        EXPECT_EQ(count(512, 512), std::pair(1, 1));
        EXPECT_EQ(count(256, 128), std::pair(1, 1));
        EXPECT_EQ(count(0, 0), std::pair(1, 1));
    }

    TEST(ResourceSpeedTreeTest, everyCardShowsOneClusterOfTheImage)
    {
        // Two clusters one above the other and three cards: top, bottom, top again
        const osg::ref_ptr<osg::Geometry> geometry = Resource::makeCrossedCards(100.f, 100.f, 3, { 1, 2 });
        const auto* coordinates = dynamic_cast<const osg::Vec2Array*>(geometry->getTexCoordArray(0));
        ASSERT_NE(coordinates, nullptr);
        ASSERT_EQ(coordinates->size(), 12u);
        const float expectedTop[] = { 0.f, 0.5f, 0.f };
        for (std::size_t card = 0; card < 3; ++card)
        {
            // the corners at the base come first, then those at the top
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 0].x(), 0.f);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 1].x(), 1.f);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 3].y(), expectedTop[card]);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 2].y(), expectedTop[card]);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 0].y(), expectedTop[card] + 0.5f);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 1].y(), expectedTop[card] + 0.5f);
        }
    }

    TEST(ResourceSpeedTreeTest, theClustersAreTakenRowByRowFromTheTopLeft)
    {
        const osg::ref_ptr<osg::Geometry> geometry = Resource::makeCrossedCards(100.f, 100.f, 4, { 2, 2 });
        const auto* coordinates = dynamic_cast<const osg::Vec2Array*>(geometry->getTexCoordArray(0));
        ASSERT_NE(coordinates, nullptr);
        const float expectedLeft[] = { 0.f, 0.5f, 0.f, 0.5f };
        const float expectedTop[] = { 0.f, 0.f, 0.5f, 0.5f };
        for (std::size_t card = 0; card < 4; ++card)
        {
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 3].x(), expectedLeft[card]);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 3].y(), expectedTop[card]);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 1].x(), expectedLeft[card] + 0.5f);
            EXPECT_FLOAT_EQ((*coordinates)[card * 4 + 1].y(), expectedTop[card] + 0.5f);
        }
    }
    TEST(ResourceSpeedTreeTest, theImageOfARecordIsInTheFolderOfTheLeaves)
    {
        TestingOpenMW::VFSTestFile file("image");
        const auto vfs = TestingOpenMW::createTestVFS({
            { VFS::Path::NormalizedView("textures/trees/leaves/pineleaves01.dds"), &file },
            { VFS::Path::NormalizedView("textures/pineleaves01.dds"), &file },
        });
        EXPECT_EQ(Resource::findSpeedTreeImage("PineLeaves01.dds", *vfs).value(), "textures/trees/leaves/pineleaves01.dds");
    }

    TEST(ResourceSpeedTreeTest, theLeavesComeBeforeTheBillboardsWhenBothHaveTheFile)
    {
        TestingOpenMW::VFSTestFile file("image");
        const auto vfs = TestingOpenMW::createTestVFS({
            { VFS::Path::NormalizedView("textures/trees/billboards/euonymusbush01.dds"), &file },
            { VFS::Path::NormalizedView("textures/trees/leaves/euonymusbush01.dds"), &file },
        });
        EXPECT_EQ(Resource::findSpeedTreeImage("EuonymusBush01.dds", *vfs).value(),
            "textures/trees/leaves/euonymusbush01.dds");
    }

    TEST(ResourceSpeedTreeTest, theBillboardsAreUsedWhenTheLeavesLackTheFile)
    {
        TestingOpenMW::VFSTestFile file("image");
        const auto vfs = TestingOpenMW::createTestVFS({
            { VFS::Path::NormalizedView("textures/trees/billboards/oak.dds"), &file },
        });
        EXPECT_EQ(Resource::findSpeedTreeImage("Oak.dds", *vfs).value(), "textures/trees/billboards/oak.dds");
    }

    TEST(ResourceSpeedTreeTest, aNameWithAFolderIsAnOrdinaryTexture)
    {
        TestingOpenMW::VFSTestFile file("image");
        const auto vfs = TestingOpenMW::createTestVFS({
            { VFS::Path::NormalizedView("textures/trees/oftesttree.dds"), &file },
            { VFS::Path::NormalizedView("textures/trees/leaves/oftesttree.dds"), &file },
        });
        EXPECT_EQ(Resource::findSpeedTreeImage("trees\\OFTestTree.dds", *vfs).value(), "textures/trees/oftesttree.dds");
    }

    TEST(ResourceSpeedTreeTest, aNameInNoFolderIsLookedForUnderTextures)
    {
        const auto vfs = TestingOpenMW::createTestVFS({});
        EXPECT_EQ(Resource::findSpeedTreeImage("Lost.dds", *vfs).value(), "textures/lost.dds");
    }
}
