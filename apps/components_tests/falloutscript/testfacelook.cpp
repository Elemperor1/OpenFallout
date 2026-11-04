#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include <components/falloutscript/facelook.hpp>

namespace
{
    using namespace FalloutScript;

    /// A source of uniform numbers that is always the same
    std::function<double()> sequence()
    {
        return [state = std::uint64_t(12345)]() mutable {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            return static_cast<double>(state >> 11) / 9007199254740992.0;
        };
    }

    TEST(FalloutScriptFaceLookTest, gaussGivesStandardNormalNumbers)
    {
        Gauss gauss(sequence());
        constexpr int count = 20000;
        double sum = 0.0;
        double squares = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const double value = gauss.next();
            sum += value;
            squares += value * value;
        }
        EXPECT_NEAR(sum / count, 0.0, 0.05);
        EXPECT_NEAR(squares / count, 1.0, 0.05);
    }

    TEST(FalloutScriptFaceLookTest, gaussUsesBothCoordinatesOfEachPoint)
    {
        int calls = 0;
        Gauss gauss([&calls]() {
            ++calls;
            return calls % 2 == 0 ? 0.3 : 0.6;
        });
        gauss.next();
        const int afterOne = calls;
        gauss.next();
        EXPECT_EQ(calls, afterOne);
    }

    TEST(FalloutScriptFaceLookTest, controlValueIsTheDotProductAndMovingAddsTheDifferenceAlongTheRow)
    {
        FaceControls controls;
        // The second row is a unit vector, which is what a move along it sets the value exactly for
        controls.set(FaceKind::Shape, { { 1.f, 0.f, 0.f }, { 0.6f, 0.8f, 0.f } });
        EXPECT_EQ(controls.count(FaceKind::Shape), 2);
        EXPECT_EQ(controls.count(FaceKind::Tone), 0);
        EXPECT_TRUE(controls.has(FaceKind::Shape, 1));
        EXPECT_FALSE(controls.has(FaceKind::Shape, 2));
        EXPECT_FALSE(controls.has(FaceKind::Shape, -1));

        std::vector<float> coefficients{ 2.f, 4.f, 6.f };
        EXPECT_FLOAT_EQ(controls.value(FaceKind::Shape, 0, coefficients), 2.f);
        EXPECT_NEAR(controls.value(FaceKind::Shape, 1, coefficients), 4.4f, 1e-5f);

        EXPECT_TRUE(controls.move(FaceKind::Shape, 1, 5.f, coefficients));
        // 5 - 4.4 = 0.6, along (0.6, 0.8, 0)
        EXPECT_NEAR(coefficients[0], 2.36f, 1e-5f);
        EXPECT_NEAR(coefficients[1], 4.48f, 1e-5f);
        EXPECT_FLOAT_EQ(coefficients[2], 6.f);
        EXPECT_NEAR(controls.value(FaceKind::Shape, 1, coefficients), 5.f, 1e-5f);
        // the first control moved with it, since the rows are not orthogonal
        EXPECT_NEAR(controls.value(FaceKind::Shape, 0, coefficients), 2.36f, 1e-5f);
    }

    TEST(FalloutScriptFaceLookTest, controlThatIsNotThereChangesNothing)
    {
        FaceControls controls;
        std::vector<float> coefficients{ 1.f, 2.f };
        EXPECT_FALSE(controls.move(FaceKind::Tone, 0, 3.f, coefficients));
        EXPECT_EQ(coefficients, (std::vector<float>{ 1.f, 2.f }));
        EXPECT_FLOAT_EQ(controls.value(FaceKind::Tone, 0, coefficients), 0.f);
    }

    TEST(FalloutScriptFaceLookTest, sliderIntegersAreTenthsOfTheControlTruncatedTowardsZero)
    {
        // Not rounded and not floored (FalloutNV.exe 0x7B181A, Mac round 12BB B)
        EXPECT_EQ(faceSliderInteger(1.25f), 12);
        EXPECT_EQ(faceSliderInteger(-1.25f), -12);
        EXPECT_EQ(faceSliderInteger(0.f), 0);
        EXPECT_EQ(faceSliderInteger(0.55f), 5);
        EXPECT_EQ(faceSliderInteger(-0.55f), -5);
        EXPECT_EQ(faceSliderInteger(-0.05f), 0);
        EXPECT_FLOAT_EQ(faceSliderValue(-37), -3.7f);
        EXPECT_FLOAT_EQ(faceSliderValue(35), 3.5f);
    }

    TEST(FalloutScriptFaceLookTest, aSliderSetToANumberReadsBackAsThatNumber)
    {
        // 1.3f is 1.29999995: cut towards zero it would read 12, and an arrow of the slider would skip a step (Mac round 12BD)
        for (int number = -50; number <= 50; ++number)
        {
            EXPECT_EQ(faceSliderInteger(faceSliderValue(number)), number) << number;
            // The floats of the sum of a move are off by a little more than the one of the value
            const float off = 3e-6f;
            EXPECT_EQ(faceSliderInteger(faceSliderValue(number) + off), number) << number;
            EXPECT_EQ(faceSliderInteger(faceSliderValue(number) - off), number) << number;
        }
        // A control that is really between two numbers still reads the one towards zero
        EXPECT_EQ(faceSliderInteger(1.2985f), 12);
        EXPECT_EQ(faceSliderInteger(-1.2985f), -12);
    }

    TEST(FalloutScriptFaceLookTest, theAgeSliderStandsForYearsThatAreTruncated)
    {
        const int expected[] = { 15, 20, 26, 31, 37, 42, 48, 53, 59, 64 };
        for (int slider = 1; slider <= 10; ++slider)
            EXPECT_EQ(faceAgeYears(slider), expected[slider - 1]) << slider;
    }

    TEST(FalloutScriptFaceLookTest, theSumOfADotProductIsKeptAsAFloatAfterEveryTerm)
    {
        // 1 + 2^-25 twice: each sum rounds back to 1 as a float, where a sum kept in a double would give 1 + 2^-24
        const std::vector<float> direction{ 1.f, 1.f, 1.f };
        const std::vector<float> coefficients{ 1.f, 0.0000000298023224f, 0.0000000298023224f };
        EXPECT_EQ(faceDot(direction, coefficients), 1.f);
        // The shorter of the two decides how many terms there are
        EXPECT_EQ(faceDot({ 2.f, 3.f }, { 5.f }), 10.f);
        EXPECT_EQ(faceDot({}, { 5.f }), 0.f);
    }

    TEST(FalloutScriptFaceLookTest, slidersAreThirtyOneOfShapeAndTwelveOfTone)
    {
        int shape = 0;
        int tone = 0;
        std::set<std::pair<int, int>> seen;
        for (const FaceSlider& slider : faceSliders())
        {
            (slider.mKind == FaceKind::Shape ? shape : tone)++;
            EXPECT_TRUE(seen.insert({ static_cast<int>(slider.mKind), slider.mControl }).second)
                << "control " << slider.mControl << " twice";
            if (slider.mKind == FaceKind::Shape)
            {
                EXPECT_GE(slider.mList, 10);
                EXPECT_LE(slider.mList, 18);
            }
            else
                EXPECT_EQ(slider.mList, 19);
            EXPECT_LT(slider.mMinimum, slider.mMaximum);
        }
        EXPECT_EQ(shape, 31);
        EXPECT_EQ(tone, 12);
    }

    TEST(FalloutScriptFaceLookTest, slidersHaveTheirOwnRangesWhereTheExecutableGivesOne)
    {
        for (const FaceSlider& slider : faceSliders())
        {
            if (slider.mKind != FaceKind::Shape)
                continue;
            if (slider.mControl == 21 || slider.mControl == 23)
            {
                EXPECT_EQ(slider.mMinimum, -20);
                EXPECT_EQ(slider.mMaximum, 20);
            }
            else if (slider.mControl == 49 || slider.mControl == 53)
            {
                EXPECT_EQ(slider.mMinimum, -35);
                EXPECT_EQ(slider.mMaximum, 30);
            }
            else
            {
                // The usual range is the games' INI value of 3.5 either way, times ten
                EXPECT_EQ(slider.mMinimum, -35);
                EXPECT_EQ(slider.mMaximum, 35);
            }
        }
    }

    TEST(FalloutScriptFaceLookTest, sliderSettingsAreNamedByTheirNumber)
    {
        const std::vector<FaceSlider>& sliders = faceSliders();
        EXPECT_EQ(sliders[0].mSetting, "sRSMShapeOption22");
        EXPECT_EQ(sliders[1].mSetting, "sRSMShapeOption06");
        EXPECT_EQ(sliders.back().mSetting, "sBeard");
    }

    TEST(FalloutScriptFaceLookTest, randomFaceReplacesShapeAndToneAndKeepsTheRest)
    {
        FaceLook look;
        look.mRace = 0x19;
        look.mFemale = true;
        look.mHair = 0x44;
        look.mEyes = 0x55;
        look.mHairColour = { 10, 20, 30 };
        look.mHeadParts = { 7 };
        look.mAsymmetric.assign(FaceLook::asymmetricCount, 0.7f);
        Gauss gauss(sequence());
        const FaceLook random = randomFace(look, 0.5, gauss);
        EXPECT_EQ(random.mRace, 0x19u);
        EXPECT_TRUE(random.mFemale);
        EXPECT_EQ(random.mHair, 0x44u);
        EXPECT_EQ(random.mEyes, 0x55u);
        EXPECT_EQ(random.mHairColour, (std::array<std::uint8_t, 3>{ 10, 20, 30 }));
        EXPECT_EQ(random.mHeadParts, std::vector<FormId>{ 7 });
        EXPECT_EQ(random.mShape.size(), 50u);
        EXPECT_EQ(random.mTone.size(), 50u);
        for (const float value : random.mAsymmetric)
            EXPECT_EQ(value, 0.f);
        EXPECT_NE(random.mShape, look.mShape);
        EXPECT_NE(random.mTone, look.mTone);

        // Standard deviation 0.75 with the default variation of 0.5
        double squares = 0.0;
        for (const float value : random.mShape)
            squares += static_cast<double>(value) * value;
        for (const float value : random.mTone)
            squares += static_cast<double>(value) * value;
        EXPECT_NEAR(std::sqrt(squares / 100.0), 0.75, 0.2);
    }
}
