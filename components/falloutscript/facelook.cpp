#include "facelook.hpp"

#include <algorithm>
#include <cmath>

namespace FalloutScript
{
    Gauss::Gauss(std::function<double()> uniform)
        : mUniform(std::move(uniform))
    {
    }

    double Gauss::next()
    {
        if (mHasSpare)
        {
            mHasSpare = false;
            return mSpare;
        }
        // The polar method: a point in the unit circle (not the middle), scaled so that both its coordinates are normal
        for (int attempts = 0; attempts < 1000; ++attempts)
        {
            const double x = 2.0 * mUniform() - 1.0;
            const double y = 2.0 * mUniform() - 1.0;
            const double s = x * x + y * y;
            if (s >= 1.0 || s == 0.0)
                continue;
            const double scale = std::sqrt(-2.0 * std::log(s) / s);
            mSpare = y * scale;
            mHasSpare = true;
            return x * scale;
        }
        return 0.0;
    }

    void FaceControls::set(FaceKind kind, std::vector<std::vector<float>> rows)
    {
        mRows[static_cast<std::size_t>(kind)] = std::move(rows);
    }

    int FaceControls::count(FaceKind kind) const
    {
        return static_cast<int>(mRows[static_cast<std::size_t>(kind)].size());
    }

    bool FaceControls::has(FaceKind kind, int control) const
    {
        return control >= 0 && control < count(kind);
    }

    float faceDot(const std::vector<float>& direction, const std::vector<float>& coefficients)
    {
        // The executable keeps the sum as a float after every term (0x6584A3), the product of a term it does not round
        float sum = 0.f;
        const std::size_t length = std::min(direction.size(), coefficients.size());
        for (std::size_t i = 0; i < length; ++i)
            sum = static_cast<float>(static_cast<double>(sum) + static_cast<double>(direction[i]) * coefficients[i]);
        return sum;
    }

    namespace
    {
        /// coefficients += direction * scale, each term rounded to a float and so is the sum (0x658260 and 0x6586C0)
        void addAlong(const std::vector<float>& direction, float scale, std::vector<float>& coefficients)
        {
            if (coefficients.size() < direction.size())
                coefficients.resize(direction.size(), 0.f);
            for (std::size_t i = 0; i < direction.size(); ++i)
            {
                const float term = direction[i] * scale;
                coefficients[i] = coefficients[i] + term;
            }
        }
    }

    float FaceControls::value(FaceKind kind, int control, const std::vector<float>& coefficients) const
    {
        if (!has(kind, control))
            return 0.f;
        return faceDot(mRows[static_cast<std::size_t>(kind)][static_cast<std::size_t>(control)], coefficients);
    }

    bool FaceControls::move(FaceKind kind, int control, float value, std::vector<float>& coefficients) const
    {
        if (!has(kind, control))
            return false;
        const std::vector<float>& row = mRows[static_cast<std::size_t>(kind)][static_cast<std::size_t>(control)];
        const float difference = value - this->value(kind, control, coefficients);
        addAlong(row, difference, coefficients);
        return true;
    }

    void FaceAge::set(FaceKind kind, int group, std::vector<float> direction, float offset)
    {
        if (group < 0 || group >= groups)
            return;
        Fan& fan = mFans[static_cast<std::size_t>(kind)][static_cast<std::size_t>(group)];
        fan.mDirection = std::move(direction);
        fan.mOffset = offset;
    }

    void FaceAge::finish()
    {
        mLoaded = true;
        for (std::size_t kind = 0; kind < mFans.size(); ++kind)
        {
            const std::vector<float>& first = mFans[kind][0].mDirection;
            const std::vector<float>& second = mFans[kind][1].mDirection;
            if (first.empty() || second.empty())
            {
                mLoaded = false;
                continue;
            }
            // The matrix of the dot products of the two directions, and its inverse (0xAADD20: the determinant is a
            // float, and each entry is divided by it)
            const float a = faceDot(first, first);
            const float b = faceDot(first, second);
            const float c = faceDot(second, first);
            const float d = faceDot(second, second);
            const float determinant = static_cast<float>(static_cast<double>(a) * d - static_cast<double>(b) * c);
            // Directions that are finite but so large that a product overflows give no matrix to turn around
            if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) || !std::isfinite(d)
                || !std::isfinite(determinant) || determinant == 0.f)
            {
                mLoaded = false;
                continue;
            }
            const std::array<float, 4> inverse = { d / determinant, -b / determinant, -c / determinant, a / determinant };
            if (!std::all_of(inverse.begin(), inverse.end(), [](float value) { return std::isfinite(value); }))
            {
                mLoaded = false;
                continue;
            }
            mInverse[kind] = inverse;
        }
    }

    float FaceAge::value(int group, FaceKind kind, const std::vector<float>& coefficients) const
    {
        if (!mLoaded || group < 0 || group >= groups)
            return 0.f;
        const Fan& fan = mFans[static_cast<std::size_t>(kind)][static_cast<std::size_t>(group)];
        return static_cast<float>(static_cast<double>(faceDot(fan.mDirection, coefficients)) + fan.mOffset);
    }

    void FaceAge::move(int group, FaceKind kind, float value, std::vector<float>& coefficients) const
    {
        if (!mLoaded || group < 0 || group >= groups)
            return;
        const std::size_t index = static_cast<std::size_t>(kind);
        // Both groups are asked for: the one that is set takes the value, the other the value it has now (0x652470)
        const std::array<float, groups> wanted = { group == 0 ? value : this->value(0, kind, coefficients),
            group == 1 ? value : this->value(1, kind, coefficients) };
        const std::array<float, groups> low = { minimumYears, minimumSecond };
        const std::array<float, groups> high = { maximumYears, maximumSecond };
        std::array<float, groups> difference{};
        for (int g = 0; g < groups; ++g)
        {
            const float kept = std::min(std::max(wanted[g], low[g]), high[g]);
            difference[g] = kept - this->value(g, kind, coefficients);
        }
        const std::array<float, 4>& inverse = mInverse[index];
        const std::array<float, groups> along = {
            static_cast<float>(
                static_cast<double>(inverse[0]) * difference[0] + static_cast<double>(inverse[1]) * difference[1]),
            static_cast<float>(
                static_cast<double>(inverse[2]) * difference[0] + static_cast<double>(inverse[3]) * difference[1]),
        };
        for (int g = 0; g < groups; ++g)
            addAlong(mFans[index][static_cast<std::size_t>(g)].mDirection, along[g], coefficients);
    }

    bool FaceAge::setAge(float years, std::vector<float>& shape, std::vector<float>& tone) const
    {
        if (!mLoaded)
            return false;
        const float shapeNow = value(0, FaceKind::Shape, shape);
        const float toneNow = value(0, FaceKind::Tone, tone);
        // The tone moves with the shape: it stays as much older or younger than the shape as it was (0x7B4ED2)
        const float toneYears = toneNow - shapeNow + years;
        const auto kept = [](float age) { return std::min(std::max(age, minimumYears), maximumYears); };
        move(0, FaceKind::Shape, kept(years), shape);
        move(0, FaceKind::Tone, kept(toneYears), tone);
        return true;
    }

    bool FaceAge::settleRandom(const std::vector<float>& oldShape, const std::vector<float>& oldTone,
        std::vector<float>& shape, std::vector<float>& tone, double uniform) const
    {
        if (!mLoaded)
            return false;
        const auto kept = [](float number, float low, float high) { return std::min(std::max(number, low), high); };
        // The age pair: the shape keeps the age the draw gave it, the tone keeps its distance from the shape
        const float shapeYears = value(0, FaceKind::Shape, shape);
        const float toneYears = value(0, FaceKind::Tone, oldTone) - value(0, FaceKind::Shape, oldShape) + shapeYears;
        move(0, FaceKind::Shape, kept(shapeYears, minimumYears, maximumYears), shape);
        move(0, FaceKind::Tone, kept(toneYears, minimumYears, maximumYears), tone);
        // The second pair is drawn again in -2 to 2 for the shape
        const float second = static_cast<float>(uniform * 4.0 - 2.0);
        const float toneSecond = value(1, FaceKind::Tone, oldTone) - value(1, FaceKind::Shape, oldShape) + second;
        move(1, FaceKind::Shape, kept(second, -2.f, 2.f), shape);
        move(1, FaceKind::Tone, kept(toneSecond, -2.f, 2.f), tone);
        return true;
    }

    int faceAgeYears(int slider)
    {
        // ftol(slider * 5.55f + 9.45f) with the two floats widened: 1 to 10 gives 15, 20, 26, 31, 37, 42, 48, 53, 59,
        // 64
        return static_cast<int>(slider * static_cast<double>(5.55f) + static_cast<double>(9.45f));
    }

    int faceSliderInteger(float value)
    {
        return static_cast<int>(static_cast<double>(value) * 10.0);
    }

    float faceSliderValue(int integer)
    {
        return static_cast<float>(static_cast<double>(integer) / 10.0);
    }

    namespace
    {
        std::string optionName(int number)
        {
            std::string text = "sRSMShapeOption";
            if (number < 10)
                text += '0';
            return text + std::to_string(number);
        }

        std::vector<FaceSlider> makeSliders()
        {
            std::vector<FaceSlider> sliders;
            const auto shape = [&sliders](int list, int control, int option, int minimum = faceSliderDefaultMinimum,
                                   int maximum = faceSliderDefaultMaximum) {
                FaceSlider slider;
                slider.mKind = FaceKind::Shape;
                slider.mControl = control;
                slider.mList = list;
                slider.mSetting = optionName(option);
                slider.mMinimum = minimum;
                slider.mMaximum = maximum;
                sliders.push_back(std::move(slider));
            };
            // General
            shape(10, 21, 22, -20, 20);
            shape(10, 23, 6, -20, 20);
            // Forehead
            shape(11, 24, 13);
            shape(11, 25, 14);
            shape(11, 26, 27);
            // Brow
            shape(12, 0, 1);
            shape(12, 1, 2);
            shape(12, 2, 3);
            // Eyes
            shape(13, 15, 16);
            shape(13, 16, 13);
            shape(13, 18, 19);
            // Nose (the last two have 3.0 as their highest value; the second of them is the control the file's 56th
            // control row makes (12AY R2: derived))
            shape(14, 41, 42);
            shape(14, 43, 16);
            shape(14, 44, 45);
            shape(14, 45, 46);
            shape(14, 47, 48);
            shape(14, 49, 50, faceSliderDefaultMinimum, 30);
            shape(14, 53, 54, faceSliderDefaultMinimum, 30);
            // Mouth
            shape(15, 35, 36);
            shape(15, 39, 40);
            // Cheeks
            shape(16, 3, 4);
            shape(16, 4, 5);
            shape(16, 6, 7);
            // Jaw
            shape(17, 27, 11);
            shape(17, 28, 15);
            shape(17, 29, 30);
            shape(17, 30, 7);
            // Chin
            shape(18, 8, 9);
            shape(18, 9, 10);
            shape(18, 11, 12);
            shape(18, 13, 14);

            const auto tone = [&sliders](const char* setting, int control) {
                FaceSlider slider;
                slider.mKind = FaceKind::Tone;
                slider.mControl = control;
                slider.mList = 19;
                slider.mSetting = setting;
                sliders.push_back(std::move(slider));
            };
            tone("sRSMToneOption24", 29);
            tone("sRSMToneOption01", 28);
            tone("sRSMToneOption25", 30);
            tone("sRSMToneOption26", 31);
            tone("sRSMEyeSockets", 8);
            tone("sEyebrows", 9);
            tone("sEyeliner", 18);
            tone("sNose", 27);
            tone("sLips", 22);
            tone("sRSMToneOption06", 5);
            tone("sCheeks", 6);
            tone("sBeard", 3);
            return sliders;
        }
    }

    const std::vector<FaceSlider>& faceSliders()
    {
        static const std::vector<FaceSlider> sliders = makeSliders();
        return sliders;
    }

    FaceLook randomFace(const FaceLook& current, double variation, Gauss& gauss)
    {
        FaceLook look = current;
        const double sigma = variation * 1.5;
        // One new normal number for each coefficient, the shape ones first and then the tone ones, as the executable's
        // loops go (0xAAD6B0)
        for (float& coefficient : look.mShape)
            coefficient = static_cast<float>(gauss.next() * sigma);
        std::fill(look.mAsymmetric.begin(), look.mAsymmetric.end(), 0.f);
        for (float& coefficient : look.mTone)
            coefficient = static_cast<float>(gauss.next() * sigma);
        return look;
    }
}
