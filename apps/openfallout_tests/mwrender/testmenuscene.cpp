#include <gtest/gtest.h>

#include <array>
#include <cmath>

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

    TEST(OFRenderMenuScenePlacement, turnsAPointByTheRowsOfAMatrixOnAColumnVector)
    {
        // The turn of the scene root of the lockpick screen: the y of a point becomes the x, the z the y and the x the
        // z
        const osg::Matrixf m
            = OFRender::placementFromRows({ 125, 0, 0 }, std::array<float, 9>{ 0, 1, 0, 0, 0, 1, 1, 0, 0 });
        expectNear(placed(m, { 1, 0, 0 }), { 125, 0, 1 });
        expectNear(placed(m, { 0, 1, 0 }), { 126, 0, 0 });
        expectNear(placed(m, { 0, 0, 1 }), { 125, 1, 0 });
    }

    TEST(OFRenderMenuScenePlacement, givesTheSameTurnFromRowsAsFromAnglesOfAFile)
    {
        const float a = 30.f * osg::PI / 180.f;
        // About Y, rows [cos, 0, -sin], [0, 1, 0], [sin, 0, cos] (the pin of the lockpick screen), is -30 degrees
        // about Y as a rotation of the right-handed axes
        const osg::Matrixf byRows = OFRender::placementFromRows(
            { 0, 0, 0 }, std::array<float, 9>{ std::cos(a), 0, -std::sin(a), 0, 1, 0, std::sin(a), 0, std::cos(a) });
        const osg::Matrixf byAngles = OFRender::placement({ 0, 0, 0 }, { 0, -30, 0 });
        for (const osg::Vec3f& p : { osg::Vec3f(1, 0, 0), osg::Vec3f(0, 1, 0), osg::Vec3f(0, 0, 1) })
            expectNear(placed(byRows, p), placed(byAngles, p));
    }

    // A point in front of the camera (the camera looks down -z in OSG's own convention) above its axis
    osg::Vec3f onScreen(const osg::Matrixf& projection, const osg::Vec3f& point)
    {
        const osg::Vec4f clip = osg::Vec4f(point, 1.f) * projection;
        return osg::Vec3f(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
    }

    TEST(OFRenderMenuSceneProjection, putsAPointAboveTheAxisBelowTheMiddleOfTheUpsideDownPicture)
    {
        const osg::Matrixf projection = osg::Matrixf::perspective(60.0, 16.0 / 9.0, 1.0, 1000.0);
        const osg::Vec3f above(0.f, 1.f, -10.f);
        EXPECT_GT(onScreen(projection, above).y(), 0.f);
        EXPECT_LT(onScreen(OFRender::upsideDown(projection), above).y(), 0.f);
    }

    TEST(OFRenderMenuSceneProjection, keepsTheOtherAxesAndTheDepthOfAPointWhenTurnedUpsideDown)
    {
        const osg::Matrixf projection = osg::Matrixf::perspective(45.0, 1.5, 0.5, 200.0);
        const osg::Vec3f point(2.f, 3.f, -17.f);
        const osg::Vec3f upright = onScreen(projection, point);
        const osg::Vec3f turned = onScreen(OFRender::upsideDown(projection), point);
        EXPECT_NEAR(turned.x(), upright.x(), 1e-5f);
        EXPECT_NEAR(turned.y(), -upright.y(), 1e-5f);
        EXPECT_NEAR(turned.z(), upright.z(), 1e-5f);
    }

    TEST(OFRenderMenuSceneProjection, turnsAnUpsideDownProjectionBackWhenTurnedAgain)
    {
        const osg::Matrixf projection = osg::Matrixf::perspective(75.0, 1.0, 5.0, 2000.0);
        const osg::Matrixf twice = OFRender::upsideDown(OFRender::upsideDown(projection));
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                EXPECT_EQ(twice(row, column), projection(row, column));
    }
}
