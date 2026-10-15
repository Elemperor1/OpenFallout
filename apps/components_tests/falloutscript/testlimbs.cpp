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

    BodyPart part(const char* name, int condition, double mult, int toHit, int healthPercent)
    {
        BodyPart result;
        result.mName = name;
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
            mRecords.mParts[raider] = { part("Torso", -1, 1.0, 95, 100), part("Head", headCondition, 2.0, 40, 50),
                part("Left Arm", armCondition, 0.75, 60, 25) };
            mValues.setBase(raider, ActorValue::health, 200);
        }
    };

    TEST(FalloutScriptLimbsTest, tellsThePartsOfTheBodyInTheOrderOfTheRecord)
    {
        Fixture f;
        const std::vector<BodyPart> parts = f.mLimbs.partsOf(raider);
        ASSERT_EQ(parts.size(), 3u);
        EXPECT_EQ(parts[0].mName, "Torso");
        EXPECT_EQ(parts[1].mName, "Head");
        EXPECT_THAT(f.mLimbs.partsOf(robot), IsEmpty());
    }

    TEST(FalloutScriptLimbsTest, askingTwiceAsksTheRecordsOnce)
    {
        Fixture f;
        f.mLimbs.partsOf(raider);
        f.mLimbs.damageMultiplier(raider, 1);
        EXPECT_EQ(f.mRecords.mAsked, 1);
    }

    TEST(FalloutScriptLimbsTest, anActorTheHostCanNotTellAboutIsAskedAgain)
    {
        Fixture f;
        EXPECT_THAT(f.mLimbs.partsOf(unknown), IsEmpty());
        EXPECT_THAT(f.mLimbs.partsOf(unknown), IsEmpty());
        EXPECT_EQ(f.mRecords.mAsked, 2);
    }

    TEST(FalloutScriptLimbsTest, aBlowAtAPartDoesWhatItsMultiplierSays)
    {
        Fixture f;
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, 0), 1.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, 1), 2.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, 2), 0.75);
        // A part the actor does not have, or none, is the body
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, 7), 1.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(raider, -1), 1.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.damageMultiplier(robot, 0), 1.0);
    }

    TEST(FalloutScriptLimbsTest, aPartIsHurtByTheShareOfItsHealthThatTheBlowIs)
    {
        Fixture f;
        // The head stands for half of 200 health: 20 damage is a fifth of it
        const Limbs::Hurt hurt = f.mLimbs.hurt(raider, 1, 20);
        EXPECT_NEAR(hurt.mCondition, 80.0, 1e-9);
        EXPECT_FALSE(hurt.mCrippled);
        EXPECT_NEAR(f.mLimbs.condition(raider, 1), 80.0, 1e-9);
        EXPECT_NEAR(f.mValues.current(raider, headCondition), 80.0, 1e-9);
    }

    TEST(FalloutScriptLimbsTest, aPartIsCrippledWhenNothingIsLeftOfIt)
    {
        Fixture f;
        EXPECT_FALSE(f.mLimbs.hurt(raider, 2, 30).mCrippled);
        // The arm stands for a quarter of 200 health, 50: 30 and 20 are all of it
        const Limbs::Hurt hurt = f.mLimbs.hurt(raider, 2, 20);
        EXPECT_TRUE(hurt.mCrippled);
        EXPECT_LE(hurt.mCondition, 0.0);
        // It is told once
        EXPECT_FALSE(f.mLimbs.hurt(raider, 2, 20).mCrippled);
    }

    TEST(FalloutScriptLimbsTest, aPartWithoutAConditionIsNeverHurt)
    {
        Fixture f;
        const Limbs::Hurt hurt = f.mLimbs.hurt(raider, 0, 500);
        EXPECT_FALSE(hurt.mCrippled);
        EXPECT_DOUBLE_EQ(hurt.mCondition, 100.0);
        EXPECT_DOUBLE_EQ(f.mLimbs.condition(raider, 0), 100.0);
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
