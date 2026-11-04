#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include <components/falloutscript/facecontrolfile.hpp>

namespace
{
    using namespace FalloutScript;

    void putU32(std::string& bytes, std::uint32_t value)
    {
        bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    void putFloat(std::string& bytes, float value)
    {
        bytes.append(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    /// A row of fifty with the one number in its place and the rest 0
    void putRow(std::string& bytes, std::size_t length, std::size_t place, float value)
    {
        for (std::size_t i = 0; i < length; ++i)
            putFloat(bytes, i == place ? value : 0.f);
    }

    void putControls(std::string& bytes, std::size_t count, std::size_t length, float first)
    {
        putU32(bytes, static_cast<std::uint32_t>(count));
        for (std::size_t i = 0; i < count; ++i)
        {
            putRow(bytes, length, i % length, i == 0 ? first : 1.f);
            const std::string name = "Control " + std::to_string(i);
            putU32(bytes, static_cast<std::uint32_t>(name.size()));
            bytes += name;
        }
    }

    /// A file of the layout of the games' with two shape controls, one asymmetric, three tone and the fan controls:
    /// the age direction of the shape is the first coefficient (the offset 30), of the tone the second (the offset 33),
    /// and the second group is the third coefficient
    std::string makeFile()
    {
        std::string bytes = "FRCTL001";
        putU32(bytes, 0x7745C425);
        putU32(bytes, 0x51);
        for (const std::uint32_t length : { 50u, 30u, 50u, 0u })
            putU32(bytes, length);
        putControls(bytes, 2, 50, 0.5f);
        putControls(bytes, 1, 30, 1.f);
        putControls(bytes, 3, 50, 1.f);
        putU32(bytes, 0);
        for (int type = 0; type < 5; ++type)
        {
            for (int group = 0; group < 2; ++group)
            {
                for (int kind = 0; kind < 2; ++kind)
                {
                    putRow(bytes, 50, group == 0 ? static_cast<std::size_t>(kind) : 2u, 1.f);
                    putFloat(bytes, group == 0 ? (kind == 0 ? 30.f : 33.f) : 0.f);
                }
            }
        }
        bytes.append(20 * 101 * 4, '\0');
        bytes.append(5 * (100 + 10000 + 2500 + 2500) * 4, '\0');
        return bytes;
    }

    TEST(FalloutScriptFaceControlFileTest, readsTheControlsAndTheAgeOfTheLayout)
    {
        const std::string bytes = makeFile();
        const std::optional<FaceControlFile> file = readFaceControlFile(bytes);
        ASSERT_TRUE(file.has_value());
        EXPECT_EQ(file->mControls.count(FaceKind::Shape), 2);
        EXPECT_EQ(file->mControls.count(FaceKind::Tone), 3);
        EXPECT_EQ(file->mAsymmetricCount, 1);
        EXPECT_TRUE(file->mAge.loaded());

        std::vector<float> coefficients(50, 0.f);
        coefficients[0] = 2.f;
        EXPECT_FLOAT_EQ(file->mControls.value(FaceKind::Shape, 0, coefficients), 1.f);
        EXPECT_FLOAT_EQ(file->mControls.value(FaceKind::Shape, 1, coefficients), 0.f);
        // The age of the shape is its first coefficient and the offset
        EXPECT_FLOAT_EQ(file->mAge.value(0, FaceKind::Shape, coefficients), 32.f);
        EXPECT_FLOAT_EQ(file->mAge.value(1, FaceKind::Shape, coefficients), 0.f);
    }

    TEST(FalloutScriptFaceControlFileTest, aFileThatIsNotTheLayoutIsNothing)
    {
        const std::string good = makeFile();
        EXPECT_FALSE(readFaceControlFile("").has_value());
        EXPECT_FALSE(readFaceControlFile("FRCTL001").has_value());
        // A byte more or a byte less than the layout
        EXPECT_FALSE(readFaceControlFile(good + '\0').has_value());
        EXPECT_FALSE(readFaceControlFile(std::string_view(good).substr(0, good.size() - 1)).has_value());
        std::string wrong = good;
        wrong[0] = 'X';
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
        // The rows are 50, 30, 50 and 0 long
        wrong = good;
        wrong[16] = 40;
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
    }

    TEST(FalloutScriptFaceControlFileTest, aNumberThatIsNotFiniteIsRefused)
    {
        const std::string good = makeFile();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        // The first number of the first control, and the first of the age directions (after the fan controls' start)
        std::string wrong = good;
        std::memcpy(&wrong[36], &nan, sizeof(nan));
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
        const std::size_t ageStart = good.size() - (20 * 101 * 4 + 5 * (100 + 10000 + 2500 + 2500) * 4)
            - 5 * 2 * 2 * (50 + 1) * 4;
        wrong = good;
        std::memcpy(&wrong[ageStart], &infinity, sizeof(infinity));
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
        // The offset after a row
        wrong = good;
        std::memcpy(&wrong[ageStart + 50 * 4], &nan, sizeof(nan));
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
    }

    TEST(FalloutScriptFaceControlFileTest, aCountThatTheFileCannotHoldIsRefusedWithoutReadingPastTheEnd)
    {
        std::string wrong = makeFile();
        // The count of the first list: 56 controls become four billion
        const std::uint32_t huge = 0xFFFFFFF0u;
        std::memcpy(&wrong[32], &huge, sizeof(huge));
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
        // A name that is longer than what is left
        wrong = makeFile();
        const std::uint32_t nameLength = 0xFFFFFFFFu;
        std::memcpy(&wrong[36 + 200], &nameLength, sizeof(nameLength));
        EXPECT_FALSE(readFaceControlFile(wrong).has_value());
    }

    TEST(FalloutScriptFaceControlFileTest, theAgeHoldsTheOtherGroupAndStaysInItsRange)
    {
        const std::optional<FaceControlFile> file = readFaceControlFile(makeFile());
        ASSERT_TRUE(file.has_value());
        std::vector<float> shape(50, 0.f);
        shape[2] = 1.5f; // the second group: 1.5
        file->mAge.move(0, FaceKind::Shape, 40.f, shape);
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 40.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(1, FaceKind::Shape, shape), 1.5f, 1e-4f);
        // Out of the range of the group: 15 to 65 for the first, -4 to 4 for the second
        file->mAge.move(0, FaceKind::Shape, 90.f, shape);
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 65.f, 1e-4f);
        file->mAge.move(1, FaceKind::Shape, 9.f, shape);
        EXPECT_NEAR(file->mAge.value(1, FaceKind::Shape, shape), 4.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 65.f, 1e-4f);
    }

    TEST(FalloutScriptFaceControlFileTest, theAgeSliderMovesTheToneByWhatItIsOlderThanTheShape)
    {
        const std::optional<FaceControlFile> file = readFaceControlFile(makeFile());
        ASSERT_TRUE(file.has_value());
        std::vector<float> shape(50, 0.f);
        std::vector<float> tone(50, 0.f);
        // The shape is 30 and the tone 33 with the coefficients 0: the tone is 3 older
        EXPECT_TRUE(file->mAge.setAge(40.f, shape, tone));
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 40.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Tone, tone), 43.f, 1e-4f);
        // Kept to the range
        EXPECT_TRUE(file->mAge.setAge(64.f, shape, tone));
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 64.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Tone, tone), 65.f, 1e-4f);
    }

    TEST(FalloutScriptFaceControlFileTest, randomizeKeepsTheDrawnAgeAndDrawsTheSecondNumberAgain)
    {
        const std::optional<FaceControlFile> file = readFaceControlFile(makeFile());
        ASSERT_TRUE(file.has_value());
        // The face from before: the shape 35 and the tone 43 (8 older), the second numbers 1 and 1.5
        std::vector<float> oldShape(50, 0.f);
        std::vector<float> oldTone(50, 0.f);
        oldShape[0] = 5.f;
        oldTone[1] = 10.f;
        oldShape[2] = 1.f;
        oldTone[2] = 1.5f;
        // The draw: the shape is 30 (kept), the tone is anywhere (it follows the shape)
        std::vector<float> shape(50, 0.f);
        std::vector<float> tone(50, 0.f);
        tone[1] = -4.f;
        EXPECT_TRUE(file->mAge.settleRandom(oldShape, oldTone, shape, tone, 0.75));
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 30.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Tone, tone), 38.f, 1e-4f);
        // 0.75 * 4 - 2 = 1 for the shape, and the tone is 0.5 above it
        EXPECT_NEAR(file->mAge.value(1, FaceKind::Shape, shape), 1.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(1, FaceKind::Tone, tone), 1.5f, 1e-4f);
        // A tone that would be out of -2 to 2 is kept to it, and an age out of 15 to 65 too
        oldTone[2] = 3.9f;
        oldShape[2] = 0.f;
        shape.assign(50, 0.f);
        shape[0] = 50.f;
        tone.assign(50, 0.f);
        EXPECT_TRUE(file->mAge.settleRandom(oldShape, oldTone, shape, tone, 0.875));
        EXPECT_NEAR(file->mAge.value(0, FaceKind::Shape, shape), 65.f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(1, FaceKind::Shape, shape), 1.5f, 1e-4f);
        EXPECT_NEAR(file->mAge.value(1, FaceKind::Tone, tone), 2.f, 1e-4f);
    }

    TEST(FalloutScriptFaceControlFileTest, directionsSoLargeThatTheirProductsOverflowGiveNoAge)
    {
        for (const FaceKind kind : { FaceKind::Shape, FaceKind::Tone })
        {
            // Finite numbers, but 1e30 squared is no float
            std::vector<float> huge(50, 0.f);
            huge[0] = 1e30f;
            std::vector<float> other(50, 0.f);
            other[1] = 1.f;
            FaceAge age;
            for (const FaceKind each : { FaceKind::Shape, FaceKind::Tone })
            {
                age.set(each, 0, each == kind ? huge : other, 0.f);
                age.set(each, 1, other, 0.f);
            }
            age.finish();
            EXPECT_FALSE(age.loaded());
        }
        // The same directions at a size the matrix can hold give an age
        std::vector<float> first(50, 0.f);
        first[0] = 1.f;
        std::vector<float> second(50, 0.f);
        second[1] = 1.f;
        FaceAge fine;
        for (const FaceKind each : { FaceKind::Shape, FaceKind::Tone })
        {
            fine.set(each, 0, first, 0.f);
            fine.set(each, 1, second, 0.f);
        }
        fine.finish();
        EXPECT_TRUE(fine.loaded());
    }

    TEST(FalloutScriptFaceControlFileTest, noAgeWithoutTheDirections)
    {
        FaceAge age;
        std::vector<float> shape(50, 0.f);
        std::vector<float> tone(50, 0.f);
        EXPECT_FALSE(age.loaded());
        EXPECT_FALSE(age.setAge(40.f, shape, tone));
        EXPECT_FALSE(age.settleRandom(shape, tone, shape, tone, 0.5));
        EXPECT_FLOAT_EQ(age.value(0, FaceKind::Shape, shape), 0.f);
    }
}
