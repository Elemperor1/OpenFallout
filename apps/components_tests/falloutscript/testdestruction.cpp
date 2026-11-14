#include "scriptbuilder.hpp"

#include <components/falloutscript/destruction.hpp>
#include <components/falloutscript/referencestate.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::Destruction;
    using FalloutScript::DestructionData;
    using FalloutScript::DestructionOutcome;
    using FalloutScript::DestructionStage;
    using FalloutScript::FormId;
    namespace Rule = FalloutScript::DestructionRule;

    constexpr FormId truck = 0x01000200;
    constexpr FormId shell = 0x01000201;
    constexpr FormId turret = 0x01000202;
    constexpr FormId crate = 0x01000203;
    constexpr FormId plain = 0x01000204;
    constexpr FormId blast = 0x01000300;
    constexpr FormId junk = 0x01000301;

    constexpr std::uint16_t OpDamageObject = 0x116C;
    constexpr std::uint16_t OpGetDestructionStage = 0x11D7;
    constexpr std::uint16_t OpClearDestruction = 0x11D8;
    constexpr std::uint16_t OpGetDestroyed = 0x10CB;

    DestructionStage stage(int percent, int damageStage, int flags, int self = 0, FormId explosion = 0,
        FormId debris = 0, int debrisCount = 0, const char* model = "")
    {
        DestructionStage result;
        result.mHealthPercent = percent;
        result.mDamageStage = damageStage;
        result.mFlags = flags;
        result.mSelfDamage = self;
        result.mExplosion = explosion;
        result.mDebris = debris;
        result.mDebrisCount = debrisCount;
        result.mModel = model;
        return result;
    }

    /// The shape of a truck of New Vegas: six stages, the fifth with a model and the flags of capping and destroying
    DestructionData truckData()
    {
        DestructionData data;
        data.mHealth = 450;
        data.mFlags = 1;
        data.mStages = { stage(93, 0, 0), stage(89, 1, 0, 2), stage(83, 2, 0, 5, blast), stage(75, 3, 0, 5),
            stage(65, 4, DestructionStage::capDamage | DestructionStage::destroy, 2, blast, 0, 0, "hulk.nif"),
            stage(0, 5, 0) };
        return data;
    }

    class FakeRecords final : public FalloutScript::DestructionRecords, public FalloutScript::ReferenceRecords
    {
    public:
        std::map<FormId, DestructionData> mData;
        std::map<FormId, double> mActors; // the base health
        std::map<FormId, double> mActorHealth;

        bool destructionData(FormId reference, DestructionData& data) override
        {
            const auto found = mData.find(reference);
            if (found == mData.end())
                return false;
            data = found->second;
            return true;
        }
        double destructionBaseHealth(FormId reference) override
        {
            const auto found = mActors.find(reference);
            return found == mActors.end() ? -1.0 : found->second;
        }
        std::optional<double> destructionActorHealth(FormId reference) override
        {
            const auto found = mActorHealth.find(reference);
            if (found == mActorHealth.end())
                return std::nullopt;
            return found->second;
        }
        bool initialLock(FormId, bool&, int&) override { return false; }
    };

    struct Fixture
    {
        FakeRecords mRecords;
        FalloutScript::ReferenceState mReferences{ mRecords };
        Destruction mDestruction{ mRecords, mReferences };
        std::vector<std::pair<FormId, DestructionOutcome>> mHeard;
        std::vector<FormId> mRestored;

        Fixture()
        {
            mRecords.mData[truck] = truckData();
            DestructionData small;
            small.mHealth = 15;
            small.mStages = { stage(0, 1, DestructionStage::disable | DestructionStage::destroy, 0, blast) };
            mRecords.mData[shell] = small;
            DestructionData robot;
            robot.mHealth = 40;
            robot.mStages = { stage(0, 0, DestructionStage::disable, 0, blast, junk, 3) };
            mRecords.mData[turret] = robot;
            mRecords.mActors[turret] = 40;
            // Three stages, the first at 100 percent: the least blow crosses it
            DestructionData egg;
            egg.mHealth = 1;
            egg.mStages = { stage(100, 0, 0), stage(99, 1, DestructionStage::destroy | DestructionStage::capDamage, 50),
                stage(0, 2, 0, 0, blast, junk, 5, "broken.nif") };
            mRecords.mData[crate] = egg;
            mDestruction.setListener([this](FormId reference, const DestructionOutcome& outcome) {
                mHeard.emplace_back(reference, outcome);
            });
            mDestruction.setRestoreListener([this](FormId reference) { mRestored.push_back(reference); });
        }
    };

    TEST(FalloutScriptDestructionTest, the_percentage_is_the_share_of_the_health_rounded_up)
    {
        EXPECT_EQ(Rule::percent(450, 450), 100);
        EXPECT_EQ(Rule::percent(225, 450), 50);
        EXPECT_EQ(Rule::percent(449, 450), 100);
        EXPECT_EQ(Rule::percent(1, 450), 1);
        EXPECT_EQ(Rule::percent(0, 450), 0);
        EXPECT_EQ(Rule::percent(292.5, 450), 65);
        EXPECT_EQ(Rule::percent(292.4, 450), 65);
        EXPECT_EQ(Rule::percent(292.6, 450), 66);
    }

    TEST(FalloutScriptDestructionTest, the_percentage_is_a_byte_that_wraps_and_nothing_for_a_thing_with_no_health)
    {
        // The executable keeps the low byte: 300 percent comes out as 44
        EXPECT_EQ(Rule::percent(300, 100), 44);
        EXPECT_EQ(Rule::percent(255, 100), 255);
        EXPECT_EQ(Rule::percent(5, 0), 0);
        EXPECT_EQ(Rule::percent(0, 0), 0);
        // A negative most is widened without a sign: its share is next to nothing, which rounds up to 1
        EXPECT_EQ(Rule::percent(10, -5), 1);
    }

    TEST(FalloutScriptDestructionTest, the_stage_is_the_number_of_the_first_stage_below_the_percentage)
    {
        const DestructionData data = truckData();
        EXPECT_EQ(Rule::stage(data, 450, 450), 0);
        EXPECT_EQ(Rule::stage(data, 418.5, 450), 1); // 93 percent: the stage at 93 is not below it
        EXPECT_EQ(Rule::stage(data, 400, 450), 2); // 89 percent
        EXPECT_EQ(Rule::stage(data, 292.5, 450), 5);
        EXPECT_EQ(Rule::stage(data, 100, 450), 5);
        // No stage is below nothing: the number of the stages
        EXPECT_EQ(Rule::stage(data, 0, 450), 6);
        EXPECT_EQ(Rule::stage(DestructionData(), 10, 10), 0);
    }

    TEST(FalloutScriptDestructionTest, a_blow_that_goes_through_a_stage_crosses_it)
    {
        const DestructionData data = truckData();
        // 10 percent: from 100 to 90, which takes the stage at 93
        const DestructionOutcome outcome = Rule::damage(data, 450, -1, 45);
        EXPECT_TRUE(outcome.mApplied);
        EXPECT_DOUBLE_EQ(outcome.mHealth, 405);
        EXPECT_THAT(outcome.mCrossed, ElementsAre(0));
        EXPECT_EQ(outcome.mStageBefore, -1);
        EXPECT_EQ(outcome.mStageAfter, 1);
        EXPECT_EQ(outcome.mModelStage, -1);
        EXPECT_FALSE(outcome.mDestroyed);
        EXPECT_EQ(outcome.mSelfDamage, 0);
        EXPECT_EQ(outcome.mDamageStage, 0);
    }

    TEST(FalloutScriptDestructionTest, a_stage_with_a_number_and_damage_of_its_own_brings_them)
    {
        const DestructionData data = truckData();
        // From 100 to 88: the stages at 93 and 89 are crossed; the second sets the damage stage and burns
        const DestructionOutcome outcome = Rule::damage(data, 450, -1, 54);
        EXPECT_THAT(outcome.mCrossed, ElementsAre(0, 1));
        EXPECT_EQ(outcome.mDamageStage, 1);
        EXPECT_EQ(outcome.mSelfDamage, 2);
        EXPECT_EQ(outcome.mModelStage, -1);
    }

    TEST(FalloutScriptDestructionTest, the_stage_with_the_cap_stops_the_health_and_the_one_with_destroy_marks_the_thing)
    {
        const DestructionData data = truckData();
        const DestructionOutcome outcome = Rule::damage(data, 450, -1, 400);
        EXPECT_THAT(outcome.mCrossed, ElementsAre(0, 1, 2, 3, 4));
        // The stage at 65 percent holds the health at 65 percent of 450
        EXPECT_DOUBLE_EQ(outcome.mHealth, 292.5);
        EXPECT_TRUE(outcome.mDestroyed);
        // It has a model, which comes with its own damage stage and not as one of the nodes
        EXPECT_EQ(outcome.mModelStage, 4);
        EXPECT_EQ(outcome.mDamageStage, 0);
        EXPECT_EQ(outcome.mSelfDamage, 2);
        EXPECT_EQ(outcome.mStageAfter, 5);
    }

    TEST(FalloutScriptDestructionTest, a_stage_that_was_passed_is_not_crossed_again)
    {
        const DestructionData data = truckData();
        const DestructionOutcome first = Rule::damage(data, 450, -1, 54);
        const DestructionOutcome second = Rule::damage(data, 450, first.mHealth, 4.5);
        // From 88 to 87: no stage lies between
        EXPECT_TRUE(second.mApplied);
        EXPECT_TRUE(second.mCrossed.empty());
        EXPECT_DOUBLE_EQ(second.mHealth, 391.5);
        EXPECT_EQ(second.mStageBefore, 2);
        EXPECT_EQ(second.mSelfDamage, 0);
        // The damage the thing takes from itself is kept by the state, not told again
        const DestructionOutcome third = Rule::damage(data, 450, second.mHealth, 22.5);
        EXPECT_THAT(third.mCrossed, ElementsAre(2));
        EXPECT_EQ(third.mDamageStage, 2);
        EXPECT_EQ(third.mSelfDamage, 5);
    }

    TEST(FalloutScriptDestructionTest, a_thing_with_no_health_left_is_destroyed_and_takes_no_more)
    {
        DestructionData data;
        data.mHealth = 15;
        data.mStages = { stage(0, 1, DestructionStage::disable | DestructionStage::destroy, 0, blast) };
        const DestructionOutcome outcome = Rule::damage(data, 15, -1, 20);
        EXPECT_DOUBLE_EQ(outcome.mHealth, 0);
        EXPECT_TRUE(outcome.mDestroyed);
        EXPECT_TRUE(outcome.mDisable);
        EXPECT_THAT(outcome.mCrossed, ElementsAre(0));
        EXPECT_EQ(outcome.mStageAfter, 1);
        const DestructionOutcome again = Rule::damage(data, 15, 0, 5);
        EXPECT_FALSE(again.mApplied);
    }

    TEST(FalloutScriptDestructionTest, damage_that_is_not_above_nothing_does_nothing)
    {
        const DestructionData data = truckData();
        EXPECT_FALSE(Rule::damage(data, 450, -1, 0).mApplied);
        EXPECT_FALSE(Rule::damage(data, 450, -1, -5).mApplied);
        EXPECT_FALSE(Rule::damage(data, 450, -1, std::nan("")).mApplied);
    }

    TEST(FalloutScriptDestructionTest, what_is_left_of_a_thing_is_never_counted_as_nothing)
    {
        DestructionData data;
        data.mHealth = 1000;
        data.mStages = { stage(0, 0, DestructionStage::destroy) };
        // 0.001 of 1000 is a thousandth of a percent: it rounds up to 1, so the stage at 0 is not crossed
        const DestructionOutcome outcome = Rule::damage(data, 1000, -1, 999.999);
        EXPECT_TRUE(outcome.mCrossed.empty());
        EXPECT_FALSE(outcome.mDestroyed);
    }

    TEST(FalloutScriptDestructionTest, a_character_goes_by_the_health_it_has_left_not_by_the_blow)
    {
        DestructionData data;
        data.mHealth = 40;
        data.mStages = { stage(50, 0, 0, 0, blast), stage(0, 0, DestructionStage::disable, 0, blast, junk, 3) };
        // The blow was 5, the character has 10 of 40 left
        const DestructionOutcome outcome = Rule::damage(data, 40, -1, 5, 10.0);
        EXPECT_DOUBLE_EQ(outcome.mHealth, 10);
        EXPECT_THAT(outcome.mCrossed, ElementsAre(0));
        // And a character at nothing crosses the last stage
        const DestructionOutcome dead = Rule::damage(data, 40, outcome.mHealth, 5, -3.0);
        EXPECT_DOUBLE_EQ(dead.mHealth, 0);
        EXPECT_THAT(dead.mCrossed, ElementsAre(1));
        EXPECT_TRUE(dead.mDisable);
        EXPECT_TRUE(dead.mDestroyed);
    }

    TEST(FalloutScriptDestructionTest, a_thing_takes_what_it_is_given_and_the_listener_hears_of_it)
    {
        Fixture fixture;
        EXPECT_EQ(fixture.mDestruction.stage(truck), -1);
        EXPECT_FALSE(fixture.mDestruction.isDamaged(truck));
        const DestructionOutcome outcome = fixture.mDestruction.damage(truck, 54);
        EXPECT_TRUE(outcome.mApplied);
        EXPECT_EQ(fixture.mDestruction.stage(truck), 2);
        EXPECT_DOUBLE_EQ(fixture.mDestruction.health(truck), 396);
        EXPECT_EQ(fixture.mDestruction.damageStage(truck), 1);
        EXPECT_EQ(fixture.mDestruction.selfDamage(truck), 2);
        EXPECT_TRUE(fixture.mDestruction.isDamaged(truck));
        ASSERT_EQ(fixture.mHeard.size(), 1u);
        EXPECT_EQ(fixture.mHeard[0].first, truck);
        EXPECT_THAT(fixture.mHeard[0].second.mCrossed, ElementsAre(0, 1));
        EXPECT_THAT(fixture.mDestruction.damaged(), ElementsAre(truck));
    }

    TEST(FalloutScriptDestructionTest, a_thing_with_no_data_takes_nothing)
    {
        Fixture fixture;
        EXPECT_FALSE(fixture.mDestruction.damage(plain, 100).mApplied);
        EXPECT_FALSE(fixture.mDestruction.damage(0, 100).mApplied);
        EXPECT_EQ(fixture.mDestruction.stage(plain), -1);
        EXPECT_TRUE(fixture.mHeard.empty());
    }

    TEST(FalloutScriptDestructionTest, a_thing_that_is_destroyed_is_marked_so_for_the_scripts)
    {
        Fixture fixture;
        fixture.mDestruction.damage(shell, 100);
        EXPECT_TRUE(fixture.mReferences.isDestroyed(shell));
        EXPECT_FALSE(fixture.mReferences.isDestroyed(truck));
        // A capped stage marks it too
        fixture.mDestruction.damage(truck, 400);
        EXPECT_TRUE(fixture.mReferences.isDestroyed(truck));
        EXPECT_DOUBLE_EQ(fixture.mDestruction.health(truck), 292.5);
        EXPECT_EQ(fixture.mDestruction.modelStage(truck), 4);
    }

    TEST(FalloutScriptDestructionTest, a_character_with_destruction_data_takes_its_health_from_its_actor_value)
    {
        Fixture fixture;
        fixture.mRecords.mActorHealth[turret] = 0;
        const DestructionOutcome outcome = fixture.mDestruction.damage(turret, 40);
        EXPECT_TRUE(outcome.mDisable);
        EXPECT_THAT(outcome.mCrossed, ElementsAre(0));
        // The host that applied the blow itself says what is left
        Fixture other;
        const DestructionOutcome told = other.mDestruction.damage(turret, 1, 0.0);
        EXPECT_THAT(told.mCrossed, ElementsAre(0));
    }

    TEST(FalloutScriptDestructionTest, the_things_that_burn_take_damage_as_time_passes)
    {
        Fixture fixture;
        fixture.mDestruction.damage(truck, 54);
        ASSERT_EQ(fixture.mDestruction.selfDamage(truck), 2);
        fixture.mHeard.clear();
        fixture.mDestruction.update(10);
        EXPECT_DOUBLE_EQ(fixture.mDestruction.health(truck), 376);
        EXPECT_EQ(fixture.mHeard.size(), 1u);
        fixture.mDestruction.update(0);
        fixture.mDestruction.update(-1);
        EXPECT_DOUBLE_EQ(fixture.mDestruction.health(truck), 376);
        // It burns through the next stage and takes the damage of that one
        fixture.mDestruction.update(30);
        EXPECT_EQ(fixture.mDestruction.selfDamage(truck), 5);
    }

    TEST(FalloutScriptDestructionTest, a_thing_with_nothing_left_stops_burning)
    {
        Fixture fixture;
        DestructionData data;
        data.mHealth = 100;
        data.mStages = { stage(50, 1, 0, 10), stage(0, 2, DestructionStage::destroy) };
        fixture.mRecords.mData[plain] = data;
        fixture.mDestruction.damage(plain, 60);
        EXPECT_EQ(fixture.mDestruction.selfDamage(plain), 10);
        fixture.mDestruction.update(100);
        EXPECT_DOUBLE_EQ(fixture.mDestruction.health(plain), 0);
        EXPECT_EQ(fixture.mDestruction.selfDamage(plain), 0);
        const std::size_t heard = fixture.mHeard.size();
        fixture.mDestruction.update(100);
        EXPECT_EQ(fixture.mHeard.size(), heard);
    }

    TEST(FalloutScriptDestructionTest, clearing_the_destruction_puts_the_thing_back_as_its_record_made_it)
    {
        Fixture fixture;
        EXPECT_FALSE(fixture.mDestruction.restore(truck));
        fixture.mDestruction.damage(truck, 400);
        ASSERT_TRUE(fixture.mReferences.isDestroyed(truck));
        EXPECT_TRUE(fixture.mDestruction.restore(truck));
        EXPECT_FALSE(fixture.mReferences.isDestroyed(truck));
        EXPECT_EQ(fixture.mDestruction.stage(truck), -1);
        EXPECT_EQ(fixture.mDestruction.modelStage(truck), -1);
        EXPECT_EQ(fixture.mDestruction.selfDamage(truck), 0);
        EXPECT_THAT(fixture.mRestored, ElementsAre(truck));
        // And it takes the same blow again
        EXPECT_THAT(fixture.mDestruction.damage(truck, 400).mCrossed, ElementsAre(0, 1, 2, 3, 4));
    }

    TEST(FalloutScriptDestructionTest, a_reference_that_was_deleted_is_forgotten)
    {
        Fixture fixture;
        fixture.mDestruction.damage(truck, 400);
        fixture.mDestruction.forget(truck);
        EXPECT_FALSE(fixture.mDestruction.isDamaged(truck));
    }

    TEST(FalloutScriptDestructionTest, the_damage_of_a_thing_is_saved_and_loaded)
    {
        Fixture fixture;
        fixture.mDestruction.damage(truck, 54);
        fixture.mDestruction.damage(shell, 100);
        fixture.mDestruction.damage(crate, 1);
        FalloutScript::SaveWriter out;
        FalloutScript::writeDestruction(fixture.mDestruction, out);

        Fixture loaded;
        FalloutScript::SaveReader in(out.bytes());
        FalloutScript::readDestruction(loaded.mDestruction, in, FalloutScript::destructionSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_DOUBLE_EQ(loaded.mDestruction.health(truck), 396);
        EXPECT_EQ(loaded.mDestruction.damageStage(truck), 1);
        EXPECT_EQ(loaded.mDestruction.selfDamage(truck), 2);
        EXPECT_EQ(loaded.mDestruction.stage(truck), 2);
        EXPECT_DOUBLE_EQ(loaded.mDestruction.health(shell), 0);
        EXPECT_THAT(loaded.mDestruction.damaged(), UnorderedElementsAre(truck, shell, crate));

        FalloutScript::SaveWriter again;
        FalloutScript::writeDestruction(loaded.mDestruction, again);
        EXPECT_EQ(again.bytes(), out.bytes());
    }

    TEST(FalloutScriptDestructionTest, a_save_that_is_cut_short_is_an_error)
    {
        Fixture fixture;
        fixture.mDestruction.damage(truck, 54);
        FalloutScript::SaveWriter out;
        FalloutScript::writeDestruction(fixture.mDestruction, out);
        const std::string bytes = out.bytes().substr(0, out.bytes().size() - 3);
        Fixture loaded;
        FalloutScript::SaveReader in(bytes);
        EXPECT_THROW(FalloutScript::readDestruction(loaded.mDestruction, in, 1), FalloutScript::SaveError);
    }

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        Fixture mFixture;

        Runner()
        {
            FalloutScript::addDestructionCommands(mInterpreter, mFixture.mDestruction);
            FalloutScript::addReferenceStateCommands(mInterpreter, mFixture.mReferences);
        }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    TEST(FalloutScriptDestructionTest, DamageObject_hurts_the_reference_and_GetDestructionStage_tells_the_stage)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetDestructionStage, truck), -1.0);
        run.call(OpDamageObject, truck, { 54 });
        EXPECT_EQ(run.call(OpGetDestructionStage, truck), 2.0);
        run.call(OpDamageObject, truck, { 400 });
        EXPECT_EQ(run.call(OpGetDestructionStage, truck), 5.0);
        EXPECT_EQ(run.call(OpGetDestroyed, truck), 1.0);
        // A thing with no data has no stage whatever it is given
        run.call(OpDamageObject, plain, { 50 });
        EXPECT_EQ(run.call(OpGetDestructionStage, plain), -1.0);
        // The command is about a reference
        EXPECT_EQ(run.call(OpGetDestructionStage, 0), -1.0);
        run.call(OpDamageObject, 0, { 50 });
    }

    TEST(FalloutScriptDestructionTest, ClearDestruction_gives_the_thing_back_its_health)
    {
        Runner run;
        run.call(OpDamageObject, truck, { 400 });
        run.call(OpClearDestruction, truck);
        EXPECT_EQ(run.call(OpGetDestructionStage, truck), -1.0);
        EXPECT_EQ(run.call(OpGetDestroyed, truck), 0.0);
        run.call(OpClearDestruction, plain);
        run.call(OpClearDestruction, 0);
    }
}
