#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/limbs.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::ActorValues;
    using FalloutScript::BodyPart;
    using FalloutScript::FormId;
    using FalloutScript::Limbs;
    namespace ActorValue = FalloutScript::ActorValue;

    constexpr FormId raider = 0x01000020;
    constexpr FormId robot = 0x01000021;
    constexpr FormId unknown = 0x01000022;
    constexpr int torso = 0;
    constexpr int head = 1;
    constexpr int leftArm = 3;
    constexpr int headCondition = 25;
    constexpr int armCondition = 27;

    class TestRecords final : public FalloutScript::LimbRecords
    {
    public:
        std::map<FormId, std::vector<BodyPart>> mParts;
        int mAsked = 0;

        bool bodyParts(FormId actor, std::vector<BodyPart>& parts) override
        {
            ++mAsked;
            if (actor == unknown)
                return false;
            const auto found = mParts.find(actor);
            if (found != mParts.end())
                parts = found->second;
            return true;
        }
    };

    BodyPart part(const char* name, int type, int condition, double mult, int toHit, int healthPercent)
    {
        BodyPart result;
        result.mName = name;
        result.mType = type;
        result.mTarget = name;
        result.mCondition = condition;
        result.mDamageMult = mult;
        result.mToHit = toHit;
        result.mHealthPercent = healthPercent;
        return result;
    }

    struct Fixture
    {
        TestRecords mRecords;
        ActorValues mValues;
        Limbs mLimbs{ mRecords, mValues };

        Fixture()
        {
            // In the order of a record, which is not the order of the types
            mRecords.mParts[raider] = { part("Left Arm", leftArm, armCondition, 0.75, 60, 25),
                part("Torso", torso, -1, 1.0, 95, 100), part("Head", head, headCondition, 2.0, 40, 50) };
            mValues.setBase(raider, ActorValue::health, 200);
        }
    };

    TEST(FalloutScriptLimbsTest, tellsThePartsOfTheBodyInTheOrderOfTheRecord)
    {
        Fixture f;
        const std::vector<BodyPart> parts = f.mLimbs.partsOf(raider);
        ASSERT_EQ(parts.size(), 3u);
        EXPECT_EQ(parts[0].mName, "Left Arm");
        EXPECT_EQ(parts[1].mName, "Torso");
        EXPECT_EQ(parts[2].mType, head);
        EXPECT_THAT(f.mLimbs.partsOf(robot), IsEmpty());
    }

    TEST(FalloutScriptLimbsTest, askingTwiceAsksTheRecordsOnce)
    {
        Fixture f;
        f.mLimbs.partsOf(raider);
        f.mLimbs.damageMultiplier(raider, head);
        EXPECT_EQ(f.mRecords.mAsked, 1);
    }

    TEST(FalloutScriptLimbsTest, anActorTheHostCanNotTellAboutIsAskedAgain)
    {
        Fixture f;
        EXPECT_THAT(f.mLimbs.partsOf(unknown), IsEmpty());
        EXPECT_THAT(f.mLimbs.partsOf(unknown), IsEmpty());
        EXPECT_EQ(f.mRecords.mAsked, 2);
    }

    TEST(FalloutScriptLimbsTest, aPartIsFoundByItsTypeNotByItsPlaceInTheRecord)
    {
        Fixture f;
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, torso), 1.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, head), 2.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, leftArm), 0.75);
        // A part the actor does not have, or none, is the body
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, 7), 1.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, -1), 1.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(robot, torso), 1.0);
    }

    TEST(FalloutScriptLimbsTest, limbDamageIsTheShareOfTheHealthOfThePartThatTheBlowIs)
    {
        Fixture f;
        // The head stands for half of 200 health: 20 damage is a fifth of it; the arm for 50, the torso for all of it
        EXPECT_NEAR(f.mLimbs.limbDamage(raider, head, 20), 20.0, 1e-9);
        EXPECT_NEAR(f.mLimbs.limbDamage(raider, leftArm, 20), 40.0, 1e-9);
        EXPECT_NEAR(f.mLimbs.limbDamage(raider, torso, 20), 10.0, 1e-9);
        // Nothing for a part the actor does not have
        EXPECT_DOUBLE_EQ(f.mLimbs.limbDamage(raider, 9, 20), 0.0);
    }

    TEST(FalloutScriptLimbsTest, limbDamageIsTheDamageForAnActorWithoutHealthAndNothingForAPartWithoutAShare)
    {
        Fixture f;
        f.mRecords.mParts[robot] = { part("Head", head, headCondition, 1.0, 40, 50), part("Wheel", 7, -1, 1.0, 40, 0) };
        // No health at all: the damage itself
        f.mValues.setBase(robot, ActorValue::health, 0);
        EXPECT_DOUBLE_EQ(f.mLimbs.limbDamage(robot, head, 17), 17.0);
        f.mValues.setBase(robot, ActorValue::health, 100);
        EXPECT_DOUBLE_EQ(f.mLimbs.limbDamage(robot, 7, 17), 0.0);
        EXPECT_NEAR(f.mLimbs.limbDamage(robot, head, 17), 34.0, 1e-9);
    }

    TEST(FalloutScriptLimbsTest, aPartIsHurtByTheLimbDamageItIsGiven)
    {
        Fixture f;
        const Limbs::Hurt hurt = f.mLimbs.hurt(raider, head, 20);
        EXPECT_NEAR(hurt.mCondition, 80.0, 1e-9);
        EXPECT_FALSE(hurt.mCrippled);
        EXPECT_NEAR(f.mLimbs.condition(raider, head), 80.0, 1e-9);
        EXPECT_NEAR(f.mValues.current(raider, headCondition), 80.0, 1e-9);
    }

    TEST(FalloutScriptLimbsTest, aPartIsCrippledWhenItsConditionFallsToNothing)
    {
        Fixture f;
        EXPECT_FALSE(f.mLimbs.hurt(raider, leftArm, 60).mCrippled);
        const Limbs::Hurt hurt = f.mLimbs.hurt(raider, leftArm, 40);
        EXPECT_TRUE(hurt.mCrippled);
        EXPECT_LE(hurt.mCondition, 0.0);
        // It is told once
        EXPECT_FALSE(f.mLimbs.hurt(raider, leftArm, 20).mCrippled);
    }

    TEST(FalloutScriptLimbsTest, aPartWithoutAConditionIsNeverHurt)
    {
        Fixture f;
        const Limbs::Hurt hurt = f.mLimbs.hurt(raider, torso, 500);
        EXPECT_FALSE(hurt.mCrippled);
        EXPECT_DOUBLE_EQ(hurt.mCondition, 100.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.condition(raider, torso), 100.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.condition(raider, 9), 100.0);
    }

    TEST(FalloutScriptLimbsTest, clearForgetsWhatWasRead)
    {
        Fixture f;
        f.mLimbs.partsOf(raider);
        f.mLimbs.clear();
        f.mLimbs.partsOf(raider);
        EXPECT_EQ(f.mRecords.mAsked, 2);
    }
}
