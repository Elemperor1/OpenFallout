#ifndef COMPONENTS_NIF_BSPLINE_H
#define COMPONENTS_NIF_BSPLINE_H

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include <osg/Quat>
#include <osg/Vec3f>

#include "controller.hpp"

namespace Nif
{
    /// A uniform B-spline of up to the third degree whose first and last control point are the ends of the curve (a
    /// clamped, or open, knot vector), as the animation files of Gamebryo store them. The curve has no more than four
    /// components in a point.
    class BSplineCurve
    {
    public:
        static constexpr std::size_t sMaxComponents = 4;

        BSplineCurve() = default;

        /// @param controlPoints the components of the points, one point after the other
        /// @param components the number of components in a point, at most sMaxComponents
        BSplineCurve(std::vector<float> controlPoints, std::size_t components);

        bool empty() const { return mPoints.empty(); }

        std::size_t size() const { return mNumPoints; }

        /// The control point at the index, which must be less than the size
        std::array<float, sMaxComponents> getPoint(std::size_t index) const;

        /// @param position from 0, the start of the curve, to 1, its end; values outside are clamped
        std::array<float, sMaxComponents> evaluate(float position) const;

    private:
        std::vector<float> mPoints;
        std::size_t mComponents = 0;
        std::size_t mNumPoints = 0;
        std::size_t mDegree = 0;
    };

    /// The curves of a B-spline transform interpolator of a sequence: translation, rotation and scale, each when the
    /// interpolator has control points for it
    class BSplineTransform
    {
    public:
        explicit BSplineTransform(const Nif::NiBSplineTransformInterpolator& interpolator);

        bool empty() const { return mTranslation.empty() && mRotation.empty() && mScale.empty(); }

        const BSplineCurve& getTranslationCurve() const { return mTranslation; }
        const BSplineCurve& getRotationCurve() const { return mRotation; }
        const BSplineCurve& getScaleCurve() const { return mScale; }

        std::optional<osg::Vec3f> getTranslation(float time) const;
        std::optional<osg::Quat> getRotation(float time) const;
        std::optional<float> getScale(float time) const;

    private:
        float getPosition(float time) const;

        float mStartTime;
        float mStopTime;
        BSplineCurve mTranslation;
        BSplineCurve mRotation;
        BSplineCurve mScale;
    };
}

#endif
