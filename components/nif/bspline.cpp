#include "bspline.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "data.hpp"

namespace Nif
{
    namespace
    {
        // The handle of a channel that has no control points: the files write 0xFFFF, and the all-ones word is the same
        // thing in a 32 bit field
        constexpr uint32_t invalidHandle = 0xFFFF;
        constexpr uint32_t invalidWideHandle = 0xFFFFFFFF;

        // The compact control points are shorts, scaled to the range of the channel
        constexpr float compactScale = 1.f / 32767.f;
    }

    BSplineCurve::BSplineCurve(std::vector<float> controlPoints, std::size_t components)
    {
        if (components == 0 || components > sMaxComponents || controlPoints.size() < components)
            return;

        mComponents = components;
        mNumPoints = controlPoints.size() / components;
        mPoints = std::move(controlPoints);
        mPoints.resize(mNumPoints * mComponents);
        mDegree = std::min<std::size_t>(3, mNumPoints - 1);
    }

    std::array<float, BSplineCurve::sMaxComponents> BSplineCurve::getPoint(std::size_t index) const
    {
        std::array<float, sMaxComponents> result{};
        std::copy_n(&mPoints.at(index * mComponents), mComponents, result.begin());
        return result;
    }

    std::array<float, BSplineCurve::sMaxComponents> BSplineCurve::evaluate(float position) const
    {
        std::array<float, sMaxComponents> result{};
        if (empty())
            return result;

        // The curve is made of one piece for each interval between the knots, which are at 0 and then at the whole
        // numbers up to the number of points less the degree, and are repeated at both ends to make the curve start at
        // the first point and end at the last. The piece of a point in time is found with its position in them.
        const std::size_t numIntervals = mNumPoints - mDegree;
        const float span = static_cast<float>(numIntervals);
        const float u = std::isnan(position) ? 0.f : std::clamp(position, 0.f, 1.f) * span;

        if (u >= span || mDegree == 0)
        {
            const float* last = &mPoints[(mNumPoints - 1) * mComponents];
            std::copy(last, last + mComponents, result.begin());
            return result;
        }

        const auto knot = [&](std::size_t index) -> float {
            if (index <= mDegree)
                return 0.f;
            if (index < mNumPoints)
                return static_cast<float>(index - mDegree);
            return span;
        };

        const std::size_t interval = std::min(static_cast<std::size_t>(u), numIntervals - 1);
        const std::size_t k = mDegree + interval;

        // De Boor's algorithm for the piece, from the points that shape it
        float d[4][sMaxComponents];
        for (std::size_t j = 0; j <= mDegree; ++j)
            for (std::size_t c = 0; c < mComponents; ++c)
                d[j][c] = mPoints[(k - mDegree + j) * mComponents + c];

        for (std::size_t r = 1; r <= mDegree; ++r)
        {
            for (std::size_t j = mDegree; j >= r; --j)
            {
                const float left = knot(j + k - mDegree);
                const float right = knot(j + 1 + k - r);
                const float alpha = right > left ? (u - left) / (right - left) : 0.f;
                for (std::size_t c = 0; c < mComponents; ++c)
                    d[j][c] = (1.f - alpha) * d[j - 1][c] + alpha * d[j][c];
            }
        }

        std::copy(d[mDegree], d[mDegree] + mComponents, result.begin());
        return result;
    }

    BSplineTransform::BSplineTransform(const NiBSplineTransformInterpolator& interpolator)
        : mStartTime(interpolator.mStartTime)
        , mStopTime(interpolator.mStopTime)
    {
        // The number of control points belongs to the basis, which every channel of the interpolator shares
        if (interpolator.mSplineData.empty() || interpolator.mBasisData.empty())
            return;

        const NiBSplineData& data = *interpolator.mSplineData.getPtr();
        const std::size_t numPoints = interpolator.mBasisData->mNumControlPoints;
        const bool compact = interpolator.mRecordType == RC_NiBSplineCompTransformInterpolator;
        const std::size_t available = compact ? data.mCompactControlPoints.size() : data.mFloatControlPoints.size();

        // The handle of a channel is where its control points start in the array of the data, which holds those of
        // many channels, and every control point has the components of the channel
        const auto load = [&](uint32_t handle, std::size_t components, float offset, float halfRange) {
            const std::size_t count = numPoints * components;
            if (handle == invalidHandle || handle == invalidWideHandle || count == 0 || handle > available
                || count > available - handle)
                return BSplineCurve();

            std::vector<float> points(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                if (compact)
                    points[i] = offset + halfRange * compactScale * data.mCompactControlPoints[handle + i];
                else
                    points[i] = data.mFloatControlPoints[handle + i];
            }
            return BSplineCurve(std::move(points), components);
        };

        float translationOffset = 0.f, translationHalfRange = 0.f, rotationOffset = 0.f, rotationHalfRange = 0.f,
              scaleOffset = 0.f, scaleHalfRange = 0.f;
        if (compact)
        {
            const auto& comp = static_cast<const NiBSplineCompTransformInterpolator&>(interpolator);
            translationOffset = comp.mTranslationOffset;
            translationHalfRange = comp.mTranslationHalfRange;
            rotationOffset = comp.mRotationOffset;
            rotationHalfRange = comp.mRotationHalfRange;
            scaleOffset = comp.mScaleOffset;
            scaleHalfRange = comp.mScaleHalfRange;
        }

        mTranslation = load(interpolator.mTranslationHandle, 3, translationOffset, translationHalfRange);
        mRotation = load(interpolator.mRotationHandle, 4, rotationOffset, rotationHalfRange);
        mScale = load(interpolator.mScaleHandle, 1, scaleOffset, scaleHalfRange);
    }

    float BSplineTransform::getPosition(float time) const
    {
        if (!(mStopTime > mStartTime))
            return 0.f;
        return std::clamp((time - mStartTime) / (mStopTime - mStartTime), 0.f, 1.f);
    }

    std::optional<osg::Vec3f> BSplineTransform::getTranslation(float time) const
    {
        if (mTranslation.empty())
            return std::nullopt;
        const auto value = mTranslation.evaluate(getPosition(time));
        return osg::Vec3f(value[0], value[1], value[2]);
    }

    std::optional<osg::Quat> BSplineTransform::getRotation(float time) const
    {
        if (mRotation.empty())
            return std::nullopt;
        // The control points of a rotation are the components of a quaternion, w first. The curve through them is not
        // of unit length.
        const auto value = mRotation.evaluate(getPosition(time));
        osg::Quat rotation(value[1], value[2], value[3], value[0]);
        const double length = rotation.length();
        if (!(length > 1e-6))
            return std::nullopt;
        rotation /= length;
        return rotation;
    }

    std::optional<float> BSplineTransform::getScale(float time) const
    {
        if (mScale.empty())
            return std::nullopt;
        return mScale.evaluate(getPosition(time))[0];
    }
}
