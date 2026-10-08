#include "sequence.hpp"

#include <components/nif/bspline.hpp>

#include <gtest/gtest.h>

#include <osg/Quat>
#include <osg/Vec3f>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace
{
    using namespace Nif;
    using namespace Nif::Testing;

    constexpr float tolerance = 1e-5f;

    constexpr VFS::Path::NormalizedView testKf("meshes/characters/_male/mtidle.kf");

    void expectNear(const osg::Vec3f& actual, const osg::Vec3f& expected)
    {
        EXPECT_NEAR(actual.x(), expected.x(), tolerance);
        EXPECT_NEAR(actual.y(), expected.y(), tolerance);
        EXPECT_NEAR(actual.z(), expected.z(), tolerance);
    }

    const NiBSplineTransformInterpolator& lastInterpolator(const NiControllerSequence& sequence)
    {
        return *static_cast<const NiBSplineTransformInterpolator*>(
            sequence.mControlledBlocks.back().mInterpolator.getPtr());
    }

    TEST(NifBSplineCurve, isEmptyWithoutPoints)
    {
        EXPECT_TRUE(BSplineCurve().empty());
        EXPECT_TRUE(BSplineCurve({}, 3).empty());
        EXPECT_TRUE(BSplineCurve({ 1.f, 2.f }, 3).empty());
        EXPECT_TRUE(BSplineCurve({ 1.f, 2.f, 3.f }, 0).empty());
        EXPECT_TRUE(BSplineCurve({ 1.f, 2.f, 3.f, 4.f, 5.f }, 5).empty());
    }

    TEST(NifBSplineCurve, staysAtTheOnlyPoint)
    {
        const BSplineCurve curve({ 4.f, 5.f }, 2);
        ASSERT_FALSE(curve.empty());
        for (const float position : { 0.f, 0.3f, 1.f })
        {
            const auto value = curve.evaluate(position);
            EXPECT_EQ(value[0], 4.f);
            EXPECT_EQ(value[1], 5.f);
        }
    }

    TEST(NifBSplineCurve, runsStraightBetweenTwoPoints)
    {
        const BSplineCurve curve({ 1.f, 2.f, 3.f, 5.f, 6.f, 7.f }, 3);
        const auto value = curve.evaluate(0.25f);
        EXPECT_NEAR(value[0], 2.f, tolerance);
        EXPECT_NEAR(value[1], 3.f, tolerance);
        EXPECT_NEAR(value[2], 4.f, tolerance);
    }

    // With three points the curve is a Bezier curve of the second degree
    TEST(NifBSplineCurve, isAQuadraticBezierCurveThroughThreePoints)
    {
        const BSplineCurve curve({ 0.f, 4.f, 2.f }, 1);
        EXPECT_NEAR(curve.evaluate(0.5f)[0], (0.f + 2.f * 4.f + 2.f) / 4.f, tolerance);
        EXPECT_NEAR(curve.evaluate(0.f)[0], 0.f, tolerance);
        EXPECT_NEAR(curve.evaluate(1.f)[0], 2.f, tolerance);
    }

    // With four points there is one piece, a Bezier curve of the third degree
    TEST(NifBSplineCurve, isACubicBezierCurveThroughFourPoints)
    {
        const BSplineCurve curve({ 0.f, 1.f, 2.f, 4.f }, 1);
        EXPECT_NEAR(curve.evaluate(0.5f)[0], (0.f + 3.f * 1.f + 3.f * 2.f + 4.f) / 8.f, tolerance);
        EXPECT_NEAR(curve.evaluate(0.25f)[0], (27.f * 0.f + 27.f * 1.f + 9.f * 2.f + 1.f * 4.f) / 64.f, tolerance);
    }

    TEST(NifBSplineCurve, startsAtTheFirstPointAndEndsAtTheLast)
    {
        const BSplineCurve curve({ 3.f, -1.f, 8.f, 2.f, 5.f, 7.f }, 1);
        EXPECT_NEAR(curve.evaluate(0.f)[0], 3.f, tolerance);
        EXPECT_NEAR(curve.evaluate(1.f)[0], 7.f, tolerance);
    }

    TEST(NifBSplineCurve, clampsThePosition)
    {
        const BSplineCurve curve({ 3.f, -1.f, 8.f, 2.f, 5.f, 7.f }, 1);
        EXPECT_EQ(curve.evaluate(-4.f)[0], curve.evaluate(0.f)[0]);
        EXPECT_EQ(curve.evaluate(2.f)[0], curve.evaluate(1.f)[0]);
        EXPECT_EQ(curve.evaluate(std::nanf(""))[0], curve.evaluate(0.f)[0]);
    }

    // The points of a straight line at the places the knots average to make the curve the line itself. The knots of
    // five points are 0, 0, 0, 0, 1, 2, 2, 2, 2.
    TEST(NifBSplineCurve, followsALineThroughPointsPlacedAtTheKnotAverages)
    {
        const BSplineCurve curve({ 0.f, 1.f / 3.f, 1.f, 5.f / 3.f, 2.f }, 1);
        for (const float position : { 0.f, 0.1f, 0.25f, 0.4f, 0.5f, 0.6f, 0.75f, 0.9f, 1.f })
            EXPECT_NEAR(curve.evaluate(position)[0], 2.f * position, tolerance) << position;
    }

    TEST(NifBSplineCurve, stayStillWhenAllPointsAreTheSame)
    {
        std::vector<float> points;
        for (int i = 0; i < 9; ++i)
            points.insert(points.end(), { 1.f, 2.f, 3.f });
        const BSplineCurve curve(points, 3);
        for (const float position : { 0.f, 0.17f, 0.5f, 0.83f, 1.f })
        {
            const auto value = curve.evaluate(position);
            EXPECT_NEAR(value[0], 1.f, tolerance);
            EXPECT_NEAR(value[1], 2.f, tolerance);
            EXPECT_NEAR(value[2], 3.f, tolerance);
        }
    }

    // A curve has no jump where one piece ends and the next begins, and stays within its points
    TEST(NifBSplineCurve, isContinuousAndStaysWithinThePoints)
    {
        std::mt19937 generator(7);
        std::uniform_real_distribution<float> distribution(0.f, 1.f);
        for (const std::size_t numPoints : { 5, 10, 33 })
        {
            std::vector<float> points(numPoints);
            for (float& point : points)
                point = distribution(generator);
            const BSplineCurve curve(points, 1);
            const auto [lowest, highest] = std::minmax_element(points.begin(), points.end());

            // between two positions the curve moves by at most the third degree times the largest step between points
            // for every piece they cross
            const float step = 1.f / 2000.f;
            const float largestMove = 3.f * static_cast<float>(numPoints - 3) * step + tolerance;
            float previous = curve.evaluate(0.f)[0];
            for (int i = 1; i <= 2000; ++i)
            {
                const float value = curve.evaluate(step * static_cast<float>(i))[0];
                EXPECT_LE(std::abs(value - previous), largestMove) << numPoints << " points, step " << i;
                EXPECT_GE(value, *lowest - tolerance);
                EXPECT_LE(value, *highest + tolerance);
                previous = value;
            }
        }
    }

    // A transform interpolator holds the control points of its channels in the data it shares with others, and says
    // where they start
    TEST(NifBSplineTransform, readsTheCompactPointsOfEachChannelAtItsHandle)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 2.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mStop = 2.f;
        track.mNumControlPoints = 2;
        // some points of another channel come first
        track.mCompactPoints = { 7, 7, 7,
            // translation: two points of three components
            32767, 0, -32767, 0, 32767, 0,
            // rotation: two points of four components, w first
            32767, 0, 0, 0, 0, 0, 0, 32767,
            // scale: two points
            -32767, 32767 };
        track.mTranslationHandle = 3;
        track.mRotationHandle = 9;
        track.mScaleHandle = 17;
        track.mTranslationOffset = 10.f;
        track.mTranslationHalfRange = 2.f;
        track.mRotationOffset = 0.f;
        track.mRotationHalfRange = 1.f;
        track.mScaleOffset = 1.f;
        track.mScaleHalfRange = 0.5f;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        EXPECT_FALSE(transform.empty());

        expectNear(*transform.getTranslation(0.f), osg::Vec3f(12.f, 10.f, 8.f));
        expectNear(*transform.getTranslation(1.f), osg::Vec3f(11.f, 11.f, 9.f));
        expectNear(*transform.getTranslation(2.f), osg::Vec3f(10.f, 12.f, 10.f));

        const osg::Quat start = *transform.getRotation(0.f);
        EXPECT_NEAR(start.w(), 1.f, tolerance);
        EXPECT_NEAR(start.z(), 0.f, tolerance);
        // half way between no turn and a turn of a half circle about the z axis is a quarter of a circle
        const osg::Quat middle = *transform.getRotation(1.f);
        EXPECT_NEAR(middle.w(), std::sqrt(0.5f), tolerance);
        EXPECT_NEAR(middle.z(), std::sqrt(0.5f), tolerance);
        EXPECT_NEAR(middle.x(), 0.f, tolerance);

        EXPECT_NEAR(*transform.getScale(0.f), 0.5f, tolerance);
        EXPECT_NEAR(*transform.getScale(1.f), 1.f, tolerance);
        EXPECT_NEAR(*transform.getScale(2.f), 1.5f, tolerance);
    }

    TEST(NifBSplineTransform, usesTheFloatPointsOfAnInterpolatorThatIsNotCompact)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 1.f, 2.f, 3.f, 5.f, 6.f, 7.f };
        track.mTranslationHandle = 0;
        // the offset and the range belong to the compact points only
        track.mTranslationOffset = 100.f;
        track.mTranslationHalfRange = 100.f;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        expectNear(*transform.getTranslation(0.f), osg::Vec3f(1.f, 2.f, 3.f));
        expectNear(*transform.getTranslation(0.5f), osg::Vec3f(3.f, 4.f, 5.f));
        EXPECT_FALSE(transform.getRotation(0.f).has_value());
        EXPECT_FALSE(transform.getScale(0.f).has_value());
    }

    TEST(NifBSplineTransform, makesAUnitRotationOfTheCurve)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 3.f, 0.f, 4.f, 0.f, 3.f, 0.f, 4.f, 0.f };
        track.mRotationHandle = 0;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        const osg::Quat rotation = *transform.getRotation(0.7f);
        EXPECT_NEAR(rotation.length(), 1.0, tolerance);
        EXPECT_NEAR(rotation.w(), 0.6f, tolerance);
        EXPECT_NEAR(rotation.y(), 0.8f, tolerance);
    }

    TEST(NifBSplineTransform, leavesOutAChannelThatHasNoHandle)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mNumControlPoints = 2;
        track.mCompactPoints = { 0, 0, 0, 100, 100, 100 };
        track.mTranslationHandle = 0;
        // the files use the largest value of an unsigned 32 bit number as well as that of a 16 bit one
        track.mRotationHandle = 0xFFFFFFFF;
        track.mScaleHandle = SplineTrack::sNoHandle;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        EXPECT_TRUE(transform.getTranslation(0.f).has_value());
        EXPECT_FALSE(transform.getRotation(0.f).has_value());
        EXPECT_FALSE(transform.getScale(0.f).has_value());
    }

    TEST(NifBSplineTransform, leavesOutAChannelThatReachesPastTheData)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mNumControlPoints = 2;
        track.mCompactPoints = { 0, 0, 0, 100, 100, 100 };
        // the points of the translation start one component too late for the data and those of the scale are past it
        track.mTranslationHandle = 1;
        track.mScaleHandle = 6;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        EXPECT_TRUE(transform.empty());
    }

    TEST(NifBSplineTransform, isEmptyWithoutTheBasis)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mNumControlPoints = 2;
        track.mHasBasis = false;
        track.mCompactPoints = { 0, 0, 0, 100, 100, 100 };
        track.mTranslationHandle = 0;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        EXPECT_TRUE(transform.empty());
    }

    TEST(NifBSplineTransform, holdsTheEndsOutsideOfItsTimes)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 3.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mStart = 1.f;
        track.mStop = 3.f;
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 0.f, 0.f, 0.f, 4.f, 4.f, 4.f };
        track.mTranslationHandle = 0;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        expectNear(*transform.getTranslation(-1.f), osg::Vec3f(0.f, 0.f, 0.f));
        expectNear(*transform.getTranslation(1.f), osg::Vec3f(0.f, 0.f, 0.f));
        expectNear(*transform.getTranslation(2.f), osg::Vec3f(2.f, 2.f, 2.f));
        expectNear(*transform.getTranslation(3.f), osg::Vec3f(4.f, 4.f, 4.f));
        expectNear(*transform.getTranslation(9.f), osg::Vec3f(4.f, 4.f, 4.f));
    }

    TEST(NifBSplineTransform, staysAtTheStartWhenItHasNoDuration)
    {
        NIFFile file(testKf);
        auto& sequence = addSequence(file, "Idle", 0.f, 1.f, NiTimeController::ExtrapolationMode::Constant, {});

        SplineTrack track;
        track.mNode = "Bip01";
        track.mStart = 2.f;
        track.mStop = 2.f;
        track.mCompact = false;
        track.mNumControlPoints = 2;
        track.mFloatPoints = { 1.f, 1.f, 1.f, 4.f, 4.f, 4.f };
        track.mTranslationHandle = 0;
        addSplineBlock(file, sequence, track);

        const BSplineTransform transform(lastInterpolator(sequence));
        expectNear(*transform.getTranslation(5.f), osg::Vec3f(1.f, 1.f, 1.f));
    }
}
