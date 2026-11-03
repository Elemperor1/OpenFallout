#include <gtest/gtest.h>

#include <apps/openfallout/mwrender/menuscene.hpp>

namespace
{
    using namespace testing;

    // A point as it is placed: the OSG order of the matrix applied to it
    osg::Vec3f placed(const osg::Matrixf& m, const osg::Vec3f& p)
    {
        return p * m;
    }

    void expectNear(const osg::Vec3f& got, const osg::Vec3f& want, float epsilon = 1e-3f)
    {
        EXPECT_NEAR(got.x(), want.x(), epsilon);
        EXPECT_NEAR(got.y(), want.y(), epsilon);
        EXPECT_NEAR(got.z(), want.z(), epsilon);
    }

    TEST(OFRenderMenuScenePlacement, turnsAboutXByTheFirstAngle)
    {
        const osg::Matrixf m = OFRender::placement({ 0, 0, 0 }, { 90, 0, 0 });
        expectNear(placed(m, { 0, 1, 0 }), { 0, 0, 1 });
    }

    TEST(OFRenderMenuScenePlacement, turnsAboutZByTheThirdAngle)
    {
        const osg::Matrixf m = OFRender::placement({ 0, 0, 0 }, { 0, 0, 90 });
        expectNear(placed(m, { 1, 0, 0 }), { 0, 1, 0 });
    }

    TEST(OFRenderMenuScenePlacement, turnsAboutXFirstThenYThenZ)
    {
        // About X by 90 degrees takes Y to Z and about Z by 90 degrees leaves Z: the other order would end elsewhere
        const osg::Matrixf m = OFRender::placement({ 0, 0, 0 }, { 90, 0, 90 });
        expectNear(placed(m, { 0, 1, 0 }), { 0, 0, 1 });
        expectNear(placed(m, { 1, 0, 0 }), { 0, 1, 0 });
    }

    TEST(OFRenderMenuScenePlacement, agreesWithTheRowsOfAFileNode)
    {
        // The node of a model whose rotation is stored as the angles (161.494, 56.173, 74.097) and as the rows
        // (0.1525, 0.9842, 0.0894), (0.5354, -0.0063, -0.8446), (-0.8307, 0.1767, -0.5279): the images of the axes
        // are the columns of those rows
        const osg::Matrixf m = OFRender::placement({ 0, 0, 0 }, { 161.494f, 56.173f, 74.097f });
        expectNear(placed(m, { 1, 0, 0 }), { 0.1525f, 0.5354f, -0.8307f }, 2e-3f);
        expectNear(placed(m, { 0, 1, 0 }), { 0.9842f, -0.0063f, 0.1767f }, 2e-3f);
        expectNear(placed(m, { 0, 0, 1 }), { 0.0894f, -0.8446f, -0.5279f }, 2e-3f);
    }

    TEST(OFRenderMenuScenePlacement, scalesThenTurnsThenMoves)
    {
        const osg::Matrixf m = OFRender::placement({ 10, 20, 30 }, { 0, 0, 90 }, 2.f);
        expectNear(placed(m, { 1, 0, 0 }), { 10, 22, 30 });
    }
}
