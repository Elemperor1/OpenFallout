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

    float FaceControls::value(FaceKind kind, int control, const std::vector<float>& coefficients) const
    {
        if (!has(kind, control))
            return 0.f;
        const std::vector<float>& row = mRows[static_cast<std::size_t>(kind)][static_cast<std::size_t>(control)];
        double sum = 0.0;
        const std::size_t length = std::min(row.size(), coefficients.size());
        for (std::size_t i = 0; i < length; ++i)
            sum += static_cast<double>(row[i]) * coefficients[i];
        return static_cast<float>(sum);
    }

    bool FaceControls::move(FaceKind kind, int control, float value, std::vector<float>& coefficients) const
    {
        if (!has(kind, control))
            return false;
        const std::vector<float>& row = mRows[static_cast<std::size_t>(kind)][static_cast<std::size_t>(control)];
        const float difference = value - this->value(kind, control, coefficients);
        if (coefficients.size() < row.size())
            coefficients.resize(row.size(), 0.f);
        for (std::size_t i = 0; i < row.size(); ++i)
            coefficients[i] += difference * row[i];
        return true;
    }

    int faceSliderInteger(float value)
    {
        // (guess) the slider's whole number is the value times ten, rounded to the nearest
        return static_cast<int>(std::lround(static_cast<double>(value) * 10.0));
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
            const auto shape = [&sliders](int list, int control, int option, int minimum = -50, int maximum = 50) {
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
            shape(14, 49, 50, -50, 30);
            shape(14, 53, 54, -50, 30);
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
