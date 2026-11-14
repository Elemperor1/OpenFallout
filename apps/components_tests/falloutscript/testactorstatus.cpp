#include "scriptbuilder.hpp"

#include <components/falloutscript/actorstatus.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ActorActivity;
    using FalloutScript::ActorOverride;
    using FalloutScript::ActorStatus;
    using FalloutScript::FormId;
    using FalloutScript::SaveReader;
    using FalloutScript::SaveWriter;
    namespace Movement = FalloutScript::Movement;
    namespace AnimAction = FalloutScript::AnimAction;

    constexpr FormId player = FalloutScript::playerReference;
    constexpr FormId raider = 0x01000100;
    constexpr FormId otherRaider = 0x01000101;
    constexpr FormId companion = 0x01000102;
    constexpr FormId doctor = 0x01000103;
    constexpr FormId crate = 0x01000200;
    constexpr FormId idleWave = 0x01002000;
    constexpr FormId raceHuman = 0x01003000;
    constexpr FormId raceCrawler = 0x01003001;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpIsMoving = 0x1019;
    constexpr std::uint16_t OpIsTurning = 0x101A;
    constexpr std::uint16_t OpIsRunning = 0x111F;
    constexpr std::uint16_t OpIsSwimming = 0x10B9;
    constexpr std::uint16_t OpIsWaiting = 0x106F;
    constexpr std::uint16_t OpGetWalkSpeed = 0x108E;
    constexpr std::uint16_t OpIsFacingUp = 0x106A;
    constexpr std::uint16_t OpIsLeftUp = 0x111D;
    constexpr std::uint16_t OpGetAnimAction = 0x10DB;
    constexpr std::uint16_t OpIsIdlePlaying = 0x1070;
    constexpr std::uint16_t OpGetIdleDoneOnce = 0x113E;
    constexpr std::uint16_t OpIsLastIdlePlayed = 0x11C3;
    constexpr std::uint16_t OpGetCurrentAIProcedure = 0x108F;
    constexpr std::uint16_t OpGetPackageTarget = 0x10E9;
    constexpr std::uint16_t OpSetActorsAI = 0x118B;
    constexpr std::uint16_t OpToggleActorsAI = 0x112E;
    constexpr std::uint16_t OpIsActorsAIOff = 0x112F;
    constexpr std::uint16_t OpDisableAllActors = 0x11A9;
    constexpr std::uint16_t OpGetActorsInHigh = 0x122D;
    constexpr std::uint16_t OpSetForceRun = 0x10D1;
    constexpr std::uint16_t OpGetForceRun = 0x10D0;
    constexpr std::uint16_t OpSetForceSneak = 0x10D3;
    constexpr std::uint16_t OpGetForceSneak = 0x10D2;
    constexpr std::uint16_t OpSetNoAvoidance = 0x115B;
    constexpr std::uint16_t OpSetAllReachable = 0x1159;
    constexpr std::uint16_t OpGetGroupMemberCount = 0x11A0;
    constexpr std::uint16_t OpGetGroupTargetCount = 0x11A1;
    constexpr std::uint16_t OpGetPlayerTeammateCount = 0x11C7;
    constexpr std::uint16_t OpGetOwnerLastTarget = 0x124E;
    constexpr std::uint16_t OpIsActor = 0x1161;
    constexpr std::uint16_t OpGetFurnitureMarkerID = 0x10A0;
    constexpr std::uint16_t OpGetFatiguePercentage = 0x1080;
    constexpr std::uint16_t OpStartCannibal = 0x107E;
    constexpr std::uint16_t OpStartMisterSandMan = 0x107C;
    constexpr std::uint16_t OpGetCannibal = 0x10E3;
    constexpr std::uint16_t OpGetSandman = 0x10E2;
    constexpr std::uint16_t OpAdvancePCLevel = 0x10D5;
    constexpr std::uint16_t OpModPCAttribute = 0x1110;
    constexpr std::uint16_t OpModPCSkill = 0x110F;
    constexpr std::uint16_t OpAdvancePCSkill = 0x10D4;
    constexpr std::uint16_t OpGetIsPlayableRace = 0x10FE;
    constexpr std::uint16_t OpIsRidingHorse = 0x1147;
    constexpr std::uint16_t OpIsTorchOut = 0x1066;
    constexpr std::uint16_t OpGetDisease = 0x1027;

    class FakeWorld : public FalloutScript::ActorStatusWorld
    {
    public:
        std::set<FormId> mActors{ player, raider, otherRaider, companion, doctor };
        std::set<FormId> mTeammates;
        std::map<FormId, int> mMarkers;
        std::map<FormId, FormId> mOwners;
        std::map<FormId, std::pair<double, double>> mFatigue;
        std::map<FormId, FormId> mRaces;
        std::set<FormId> mPlayable{ raceHuman };
        int mHigh = 0;
        std::vector<FormId> mDisabledExcept;

        bool isActor(FormId reference) override { return mActors.count(reference) != 0; }
        int teammateCount() override { return static_cast<int>(mTeammates.size()); }
        bool isTeammate(FormId actor) override { return mTeammates.count(actor) != 0; }
        int furnitureMarker(FormId actor) override
        {
            const auto found = mMarkers.find(actor);
            return found == mMarkers.end() ? 0 : found->second;
        }
        FormId ownerOf(FormId reference) override
        {
            const auto found = mOwners.find(reference);
            return found == mOwners.end() ? reference : found->second;
        }
        bool fatigue(FormId actor, double& current, double& permanent) override
        {
            const auto found = mFatigue.find(actor);
            if (found == mFatigue.end())
                return false;
            current = found->second.first;
            permanent = found->second.second;
            return true;
        }
        int actorsInHigh() override { return mHigh; }
        void disableAllActors(FormId except) override { mDisabledExcept.push_back(except); }
        bool isPlayableRace(FormId race) override { return mPlayable.count(race) != 0; }
        FormId raceOf(FormId actor) override
        {
            const auto found = mRaces.find(actor);
            return found == mRaces.end() ? 0 : found->second;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        ActorStatus mStatus;
        FalloutScript::CombatState mCombat;
        FalloutScript::PlayerProgress mProgress;
        FalloutScript::ActorValues mValues;
        FakeWorld mWorld;

        explicit Runner(FalloutScript::Game game = FalloutScript::Game::NewVegas)
        {
            if (game != FalloutScript::Game::NewVegas)
                mCommands = FalloutScript::vanillaCommands(game);
            FalloutScript::addActorStatusCommands(mInterpreter, mStatus, mCombat, mProgress, mValues, mWorld, game);
        }

        double ask(std::uint16_t code, FormId reference, std::vector<double> arguments = {})
        {
            std::vector<FalloutScript::Value> values;
            for (const double number : arguments)
                values.push_back({ number, {}, false });
            return mInterpreter.call(code, reference, reference != 0, std::move(values));
        }
    };

    ActorActivity moving(unsigned flags)
    {
        ActorActivity activity;
        activity.mMovement = flags;
        return activity;
    }

    // The movement word

    TEST(FalloutScriptActorStatusTest, is_moving_tells_the_first_direction_that_is_set)
    {
        EXPECT_EQ(FalloutScript::movingAnswer(0), 0);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::forward), 1);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::backward), 2);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::left), 3);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::right), 4);
        // Ahead comes before the rest, and the bits above the directions are no direction
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::forward | Movement::right), 1);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::backward | Movement::left | Movement::right), 2);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::left | Movement::right), 3);
        EXPECT_EQ(FalloutScript::movingAnswer(Movement::run | Movement::sneak | Movement::turnLeft), 0);
    }

    TEST(FalloutScriptActorStatusTest, is_turning_tells_left_before_right_and_nothing_for_a_walk)
    {
        EXPECT_EQ(FalloutScript::turningAnswer(Movement::turnLeft), 1);
        EXPECT_EQ(FalloutScript::turningAnswer(Movement::turnRight), 2);
        EXPECT_EQ(FalloutScript::turningAnswer(Movement::turnLeft | Movement::turnRight), 1);
        EXPECT_EQ(FalloutScript::turningAnswer(Movement::forward | Movement::walk), 0);
    }

    TEST(FalloutScriptActorStatusTest, is_running_is_the_run_bit_and_no_other)
    {
        EXPECT_TRUE(FalloutScript::runningFlag(Movement::run));
        EXPECT_TRUE(FalloutScript::runningFlag(Movement::run | Movement::forward));
        EXPECT_FALSE(FalloutScript::runningFlag(Movement::walk | Movement::sneak | Movement::forward));
    }

    // The procedure

    TEST(FalloutScriptActorStatusTest, the_procedure_is_the_number_of_the_stage_of_the_package)
    {
        // A travel package: the first stage is procedure 3, the second 11
        EXPECT_EQ(FalloutScript::procedureNumber(6, 0), 3);
        EXPECT_EQ(FalloutScript::procedureNumber(6, 1), 11);
        // A sandbox package has one stage
        EXPECT_EQ(FalloutScript::procedureStageCount(12), 1);
        EXPECT_EQ(FalloutScript::procedureNumber(12, 0), 13);
        // A package with four stages
        EXPECT_EQ(FalloutScript::procedureStageCount(8), 4);
        EXPECT_EQ(FalloutScript::procedureNumber(8, 0), 20);
        EXPECT_EQ(FalloutScript::procedureNumber(8, 3), 12);
        // Eating, then the first stage that is none
        EXPECT_EQ(FalloutScript::procedureNumber(3, 0), 0);
        EXPECT_EQ(FalloutScript::procedureNumber(3, 1), 2);
    }

    TEST(FalloutScriptActorStatusTest, a_stage_past_the_list_is_the_end_of_the_package)
    {
        EXPECT_EQ(FalloutScript::procedureNumber(6, 2), FalloutScript::Procedure::done);
        EXPECT_EQ(FalloutScript::procedureNumber(6, 40), 17);
        EXPECT_EQ(FalloutScript::procedureNumber(0, 1), 17);
        // A stage before the first is the first
        EXPECT_EQ(FalloutScript::procedureNumber(6, -3), 3);
    }

    TEST(FalloutScriptActorStatusTest, a_type_the_table_does_not_have_has_no_procedure)
    {
        EXPECT_EQ(FalloutScript::procedureNumber(-1, 0), FalloutScript::Procedure::unknown);
        EXPECT_EQ(FalloutScript::procedureNumber(48, 0), -1);
        EXPECT_EQ(FalloutScript::procedureStageCount(-1), 0);
        EXPECT_EQ(FalloutScript::procedureStageCount(200), 0);
        // An entry of the executable's list that its switch has no number for
        EXPECT_EQ(FalloutScript::procedureNumber(16, 0), -1);
        EXPECT_EQ(FalloutScript::procedureNumber(33, 0), -1);
    }

    TEST(FalloutScriptActorStatusTest, every_type_in_the_table_has_a_list_that_ends_in_the_end_of_the_package)
    {
        for (int type = 0; type < 48; ++type)
        {
            const int count = FalloutScript::procedureStageCount(type);
            EXPECT_GE(count, 1) << type;
            EXPECT_LE(count, 4) << type;
            EXPECT_EQ(FalloutScript::procedureNumber(type, count), 17) << type;
        }
    }

    // The overrides and the activity

    TEST(FalloutScriptActorStatusTest, an_actor_has_no_override_until_a_script_sets_one)
    {
        ActorStatus status;
        EXPECT_TRUE(status.overrideOf(raider).empty());
        EXPECT_FALSE(status.aiOff(raider));
        EXPECT_TRUE(status.overrides().empty());
    }

    TEST(FalloutScriptActorStatusTest, each_override_is_kept_by_itself_and_the_last_one_off_forgets_the_actor)
    {
        ActorStatus status;
        status.setForceSneak(raider, true);
        status.setAiOff(raider, true);
        status.setForceRun(otherRaider, true);
        EXPECT_THAT(status.overrides(), SizeIs(2));
        EXPECT_TRUE(status.overrideOf(raider).mForceSneak);
        EXPECT_TRUE(status.overrideOf(raider).mAiOff);
        EXPECT_FALSE(status.overrideOf(raider).mForceRun);
        status.setForceSneak(raider, false);
        EXPECT_THAT(status.overrides(), SizeIs(2));
        status.setAiOff(raider, false);
        EXPECT_THAT(status.overrides(), SizeIs(1));
        // No reference, no override
        status.setAiOff(0, true);
        EXPECT_THAT(status.overrides(), SizeIs(1));
    }

    TEST(FalloutScriptActorStatusTest, the_activity_stays_until_it_is_told_again_or_forgotten)
    {
        ActorStatus status;
        EXPECT_FALSE(status.activity(raider).has_value());
        status.publish(raider, moving(Movement::forward | Movement::run));
        ASSERT_TRUE(status.activity(raider).has_value());
        EXPECT_EQ(status.activity(raider)->mMovement, Movement::forward | Movement::run);
        // The animation and the mind each change their own fields
        status.update(raider, [](ActorActivity& activity) { activity.mPackageType = 6; });
        status.update(raider, [](ActorActivity& activity) { activity.mMovement = 0; });
        EXPECT_EQ(status.activity(raider)->mPackageType, 6);
        EXPECT_EQ(status.activity(raider)->mMovement, 0u);
        status.publish(otherRaider, ActorActivity());
        EXPECT_THAT(status.active(), ElementsAre(raider, otherRaider));
        EXPECT_EQ(status.activeCount(), 2u);
        status.forget(raider);
        EXPECT_THAT(status.active(), ElementsAre(otherRaider));
        status.publish(0, ActorActivity());
        status.update(0, [](ActorActivity&) {});
        EXPECT_EQ(status.activeCount(), 1u);
    }

    TEST(FalloutScriptActorStatusTest, the_animation_and_the_mind_each_take_back_only_what_they_told)
    {
        ActorStatus status;
        status.update(raider, [](ActorActivity& activity) {
            activity.mMovement = Movement::forward;
            activity.mAnimAction = AnimAction::attack;
            activity.mWalkSpeed = 90.f;
        });
        status.update(raider, [](ActorActivity& activity) {
            activity.mPackageType = 6;
            activity.mStage = 1;
        });
        // The animation is made again: the package stays (a round put a character in another place and its package was lost)
        status.forgetBody(raider);
        ASSERT_TRUE(status.activity(raider).has_value());
        EXPECT_EQ(status.activity(raider)->mPackageType, 6);
        EXPECT_EQ(status.activity(raider)->mStage, 1);
        EXPECT_EQ(status.activity(raider)->mMovement, 0u);
        EXPECT_EQ(status.activity(raider)->mAnimAction, AnimAction::none);
        EXPECT_EQ(status.activity(raider)->mWalkSpeed, 0.f);
        // The mind goes too: nothing is left and the actor is forgotten
        status.forgetMind(raider);
        EXPECT_FALSE(status.activity(raider).has_value());
        // The other way round: the movement stays when the mind goes
        status.update(raider, [](ActorActivity& activity) { activity.mMovement = Movement::run; });
        status.update(raider, [](ActorActivity& activity) { activity.mPackageType = 12; });
        status.forgetMind(raider);
        ASSERT_TRUE(status.activity(raider).has_value());
        EXPECT_EQ(status.activity(raider)->mMovement, Movement::run);
        EXPECT_EQ(status.activity(raider)->mPackageType, -1);
        status.forgetBody(raider);
        EXPECT_FALSE(status.activity(raider).has_value());
        // Nothing to take back from an actor that is not known
        status.forgetBody(otherRaider);
        status.forgetMind(otherRaider);
        EXPECT_EQ(status.activeCount(), 0u);
        // The pose of the spine goes with the body: a body that is gone is not lying down
        status.update(raider, [](ActorActivity& activity) {
            activity.mSpineYUp = 0.5f;
            activity.mSpineZUp = -0.5f;
        });
        status.forgetBody(raider);
        EXPECT_FALSE(status.activity(raider).has_value());
    }

    TEST(FalloutScriptActorStatusTest, clearing_the_status_forgets_everything_a_new_game_starts_without)
    {
        ActorStatus status;
        status.setAiOff(raider, true);
        status.publish(raider, moving(Movement::left));
        status.setNoAvoidance(true);
        status.setFeeding(ActorStatus::Feeding::Cannibal, player, true);
        status.clear();
        EXPECT_TRUE(status.overrides().empty());
        EXPECT_EQ(status.activeCount(), 0u);
        EXPECT_FALSE(status.noAvoidance());
        EXPECT_FALSE(status.feeding(ActorStatus::Feeding::Cannibal, player));
    }

    TEST(FalloutScriptActorStatusTest, a_saved_game_brings_the_overrides_back)
    {
        ActorStatus status;
        status.setAiOff(raider, true);
        status.setForceRun(raider, true);
        status.setForceSneak(doctor, true);
        status.publish(raider, moving(Movement::forward));
        SaveWriter out;
        FalloutScript::writeActorOverrides(status, out);

        ActorStatus loaded;
        loaded.setForceSneak(otherRaider, true);
        const std::string bytes = out.take();
        SaveReader in(bytes);
        FalloutScript::readActorOverrides(loaded, in, FalloutScript::actorOverridesSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(loaded.overrideOf(raider), (ActorOverride{ true, true, false }));
        EXPECT_EQ(loaded.overrideOf(doctor), (ActorOverride{ false, false, true }));
        // What was there stays, and what the actors were doing is not kept
        EXPECT_TRUE(loaded.overrideOf(otherRaider).mForceSneak);
        EXPECT_EQ(loaded.activeCount(), 0u);
    }

    TEST(FalloutScriptActorStatusTest, the_same_overrides_are_always_the_same_bytes)
    {
        ActorStatus first;
        first.setAiOff(doctor, true);
        first.setForceRun(raider, true);
        ActorStatus second;
        second.setForceRun(raider, true);
        second.setAiOff(doctor, true);
        SaveWriter one;
        SaveWriter two;
        FalloutScript::writeActorOverrides(first, one);
        FalloutScript::writeActorOverrides(second, two);
        EXPECT_EQ(one.bytes(), two.bytes());
    }

    TEST(FalloutScriptActorStatusTest, an_actor_whose_plugin_is_gone_is_left_out_of_a_loaded_game)
    {
        ActorStatus status;
        status.setAiOff(raider, true);
        status.setAiOff(doctor, true);
        SaveWriter out;
        FalloutScript::writeActorOverrides(status, out);
        const std::string bytes = out.take();
        SaveReader in(bytes, [](FormId form) -> std::optional<FormId> {
            if (form == doctor)
                return std::nullopt;
            return form;
        });
        ActorStatus loaded;
        FalloutScript::readActorOverrides(loaded, in, 1);
        EXPECT_TRUE(loaded.aiOff(raider));
        EXPECT_FALSE(loaded.aiOff(doctor));
    }

    TEST(FalloutScriptActorStatusTest, a_cut_short_save_is_an_error_and_not_a_guess)
    {
        ActorStatus status;
        status.setAiOff(raider, true);
        SaveWriter out;
        FalloutScript::writeActorOverrides(status, out);
        const std::string bytes = out.take().substr(0, 9);
        SaveReader in(bytes);
        ActorStatus loaded;
        EXPECT_THROW(FalloutScript::readActorOverrides(loaded, in, 1), FalloutScript::SaveError);
    }

    // The combat groups

    using Fights = std::vector<std::pair<FormId, FormId>>;
    const auto noParty = [](FormId) { return false; };

    TEST(FalloutScriptActorStatusTest, an_actor_in_no_fight_has_no_group)
    {
        const FalloutScript::CombatGroup group = FalloutScript::combatGroupOf(doctor, Fights{ { raider, player } }, noParty);
        EXPECT_TRUE(group.mMembers.empty());
        EXPECT_TRUE(group.mTargets.empty());
    }

    TEST(FalloutScriptActorStatusTest, everyone_who_fights_the_same_target_is_in_one_group)
    {
        const Fights fights{ { raider, doctor }, { otherRaider, doctor }, { companion, raider } };
        const FalloutScript::CombatGroup group = FalloutScript::combatGroupOf(raider, fights, noParty);
        EXPECT_THAT(group.mMembers, ElementsAre(raider, otherRaider));
        EXPECT_THAT(group.mTargets, ElementsAre(doctor));
        // The one that fights the raider is on the other side of it
        const FalloutScript::CombatGroup other = FalloutScript::combatGroupOf(companion, fights, noParty);
        EXPECT_THAT(other.mMembers, ElementsAre(companion));
        EXPECT_THAT(other.mTargets, ElementsAre(raider));
    }

    TEST(FalloutScriptActorStatusTest, two_that_fight_each_other_are_a_group_each)
    {
        const Fights fights{ { raider, doctor }, { doctor, raider } };
        EXPECT_THAT(FalloutScript::combatGroupOf(raider, fights, noParty).mMembers, ElementsAre(raider));
        EXPECT_THAT(FalloutScript::combatGroupOf(raider, fights, noParty).mTargets, ElementsAre(doctor));
        EXPECT_THAT(FalloutScript::combatGroupOf(doctor, fights, noParty).mMembers, ElementsAre(doctor));
        EXPECT_THAT(FalloutScript::combatGroupOf(doctor, fights, noParty).mTargets, ElementsAre(raider));
    }

    TEST(FalloutScriptActorStatusTest, those_who_fight_the_players_side_are_one_group_with_all_its_targets)
    {
        const Fights fights{ { raider, player }, { otherRaider, companion }, { companion, otherRaider } };
        const auto party = [](FormId form) { return form == player || form == companion; };
        const FalloutScript::CombatGroup group = FalloutScript::combatGroupOf(raider, fights, party);
        EXPECT_THAT(group.mMembers, ElementsAre(raider, otherRaider));
        EXPECT_THAT(group.mTargets, ElementsAre(player, companion));
        // The teammate fights a raider that is not on its side
        const FalloutScript::CombatGroup mate = FalloutScript::combatGroupOf(companion, fights, party);
        EXPECT_THAT(mate.mMembers, ElementsAre(companion));
        EXPECT_THAT(mate.mTargets, ElementsAre(otherRaider));
    }

    // The commands

    TEST(FalloutScriptActorStatusTest, the_movement_commands_read_what_the_animation_told)
    {
        Runner run;
        run.mStatus.publish(raider, moving(Movement::backward | Movement::run | Movement::turnRight));
        EXPECT_EQ(run.ask(OpIsMoving, raider), 2.0);
        EXPECT_EQ(run.ask(OpIsTurning, raider), 2.0);
        EXPECT_EQ(run.ask(OpIsRunning, raider), 1.0);
        EXPECT_EQ(run.ask(OpIsSwimming, raider), 0.0);
        run.mStatus.update(raider, [](ActorActivity& activity) {
            activity.mMovement = 0;
            activity.mSwimming = true;
            activity.mWalkSpeed = 130.f;
        });
        EXPECT_EQ(run.ask(OpIsMoving, raider), 0.0);
        EXPECT_EQ(run.ask(OpIsSwimming, raider), 1.0);
        EXPECT_EQ(run.ask(OpGetWalkSpeed, raider), 130.0);
        EXPECT_THAT(run.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorStatusTest, the_spine_bone_says_whether_a_character_faces_up_and_whether_its_left_is_up)
    {
        Runner run;
        // No bone yet (or no activity at all): both are 0
        EXPECT_EQ(run.ask(OpIsFacingUp, raider), 0.0);
        EXPECT_EQ(run.ask(OpIsLeftUp, raider), 0.0);
        run.mStatus.publish(raider, ActorActivity{});
        run.mStatus.update(raider, [](ActorActivity& activity) {
            activity.mSpineYUp = 0.4f;
            activity.mSpineZUp = -0.9f;
        });
        EXPECT_EQ(run.ask(OpIsFacingUp, raider), 1.0);
        EXPECT_EQ(run.ask(OpIsLeftUp, raider), 0.0);
        run.mStatus.update(raider, [](ActorActivity& activity) {
            activity.mSpineYUp = 0.f;
            activity.mSpineZUp = 0.1f;
        });
        // The executable asks for more than 0: an axis that lies flat is not up
        EXPECT_EQ(run.ask(OpIsFacingUp, raider), 0.0);
        EXPECT_EQ(run.ask(OpIsLeftUp, raider), 1.0);
    }

    TEST(FalloutScriptActorStatusTest, an_actor_the_game_does_not_look_after_stands_still)
    {
        Runner run;
        EXPECT_EQ(run.ask(OpIsMoving, raider), 0.0);
        EXPECT_EQ(run.ask(OpIsRunning, raider), 0.0);
        EXPECT_EQ(run.ask(OpGetAnimAction, raider), -1.0);
        EXPECT_EQ(run.ask(OpGetCurrentAIProcedure, raider), 0.0);
        EXPECT_EQ(run.ask(OpIsIdlePlaying, raider), 0.0);
        // Something that is no actor is not asked
        run.mStatus.publish(crate, moving(Movement::forward));
        EXPECT_EQ(run.ask(OpIsMoving, crate), 0.0);
        EXPECT_EQ(run.ask(OpGetAnimAction, crate), -1.0);
    }

    TEST(FalloutScriptActorStatusTest, get_anim_action_tells_the_action_or_minus_one)
    {
        Runner run;
        run.mStatus.publish(raider, ActorActivity());
        EXPECT_EQ(run.ask(OpGetAnimAction, raider), -1.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mAnimAction = AnimAction::attack; });
        EXPECT_EQ(run.ask(OpGetAnimAction, raider), 2.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mAnimAction = AnimAction::recoil; });
        EXPECT_EQ(run.ask(OpGetAnimAction, raider), 8.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mAnimAction = AnimAction::forcedByScript; });
        EXPECT_EQ(run.ask(OpGetAnimAction, raider), 14.0);
    }

    TEST(FalloutScriptActorStatusTest, the_idle_commands_tell_whether_one_plays_and_which_was_last)
    {
        Runner run;
        run.mStatus.update(doctor, [](ActorActivity& activity) {
            activity.mIdlePlaying = true;
            activity.mLastIdle = idleWave;
        });
        EXPECT_EQ(run.ask(OpIsIdlePlaying, doctor), 1.0);
        EXPECT_EQ(run.ask(OpGetIdleDoneOnce, doctor), 0.0);
        EXPECT_EQ(run.ask(OpIsLastIdlePlayed, doctor, { static_cast<double>(idleWave) }), 1.0);
        EXPECT_EQ(run.ask(OpIsLastIdlePlayed, doctor, { static_cast<double>(idleWave + 1) }), 0.0);
        // No idle named is none that played
        EXPECT_EQ(run.ask(OpIsLastIdlePlayed, doctor, { 0.0 }), 0.0);
        run.mStatus.update(doctor, [](ActorActivity& activity) {
            activity.mIdlePlaying = false;
            activity.mIdleDoneOnce = true;
        });
        EXPECT_EQ(run.ask(OpIsIdlePlaying, doctor), 0.0);
        EXPECT_EQ(run.ask(OpGetIdleDoneOnce, doctor), 1.0);
    }

    TEST(FalloutScriptActorStatusTest, the_procedure_follows_the_package_and_its_stage)
    {
        Runner run;
        run.mStatus.update(raider, [](ActorActivity& activity) {
            activity.mPackageType = 6;
            activity.mStage = 0;
        });
        EXPECT_EQ(run.ask(OpGetCurrentAIProcedure, raider), 3.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mStage = 1; });
        EXPECT_EQ(run.ask(OpGetCurrentAIProcedure, raider), 11.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mStage = 2; });
        EXPECT_EQ(run.ask(OpGetCurrentAIProcedure, raider), 17.0);
        // No package, no procedure
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mPackageType = -1; });
        EXPECT_EQ(run.ask(OpGetCurrentAIProcedure, raider), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, waiting_is_the_procedure_of_waiting_or_the_flag_of_the_package)
    {
        Runner run;
        run.mStatus.update(raider, [](ActorActivity& activity) {
            activity.mPackageType = 6;
            activity.mStage = 0; // procedure 3
        });
        EXPECT_EQ(run.ask(OpIsWaiting, raider), 1.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mStage = 1; });
        EXPECT_EQ(run.ask(OpIsWaiting, raider), 0.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mPackageFlags = 0x10000; });
        EXPECT_EQ(run.ask(OpIsWaiting, raider), 1.0);
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mPackageType = -1; });
        EXPECT_EQ(run.ask(OpIsWaiting, raider), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, set_actors_ai_switches_the_processing_of_the_actor)
    {
        Runner run;
        EXPECT_EQ(run.ask(OpIsActorsAIOff, raider), 0.0);
        run.ask(OpSetActorsAI, raider, { 0 });
        EXPECT_EQ(run.ask(OpIsActorsAIOff, raider), 1.0);
        EXPECT_TRUE(run.mStatus.aiOff(raider));
        // Any other number than 0 puts it on
        run.ask(OpSetActorsAI, raider, { 7 });
        EXPECT_EQ(run.ask(OpIsActorsAIOff, raider), 0.0);
        EXPECT_FALSE(run.mStatus.aiOff(raider));
    }

    TEST(FalloutScriptActorStatusTest, toggle_actors_ai_turns_it_over)
    {
        Runner run;
        run.ask(OpToggleActorsAI, doctor);
        EXPECT_EQ(run.ask(OpIsActorsAIOff, doctor), 1.0);
        run.ask(OpToggleActorsAI, doctor);
        EXPECT_EQ(run.ask(OpIsActorsAIOff, doctor), 0.0);
        EXPECT_EQ(run.ask(OpIsActorsAIOff, otherRaider), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, the_ai_of_something_that_is_no_actor_is_not_set)
    {
        Runner run;
        run.ask(OpSetActorsAI, crate, { 0 });
        run.ask(OpSetForceRun, crate, { 1 });
        EXPECT_TRUE(run.mStatus.overrides().empty());
        EXPECT_EQ(run.ask(OpIsActorsAIOff, crate), 0.0);
        EXPECT_EQ(run.ask(OpGetForceRun, crate), 0.0);
        // A script on no object has no actor to act on
        run.ask(OpSetActorsAI, 0, { 0 });
        EXPECT_TRUE(run.mStatus.overrides().empty());
    }

    TEST(FalloutScriptActorStatusTest, force_run_and_force_sneak_are_kept_apart_and_read_back)
    {
        Runner run;
        run.ask(OpSetForceSneak, raider, { 1 });
        EXPECT_EQ(run.ask(OpGetForceSneak, raider), 1.0);
        EXPECT_EQ(run.ask(OpGetForceRun, raider), 0.0);
        run.ask(OpSetForceRun, raider, { 1 });
        EXPECT_EQ(run.ask(OpGetForceRun, raider), 1.0);
        EXPECT_EQ(run.ask(OpGetForceSneak, raider), 1.0);
        run.ask(OpSetForceSneak, raider, { 0 });
        EXPECT_EQ(run.ask(OpGetForceSneak, raider), 0.0);
        EXPECT_EQ(run.ask(OpGetForceRun, raider), 1.0);
        EXPECT_EQ(run.ask(OpGetForceRun, otherRaider), 0.0);
        EXPECT_THAT(run.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorStatusTest, the_player_can_be_made_to_sneak_too)
    {
        Runner run;
        run.ask(OpSetForceSneak, player, { 1 });
        EXPECT_EQ(run.ask(OpGetForceSneak, player), 1.0);
        EXPECT_TRUE(run.mStatus.overrideOf(player).mForceSneak);
    }

    TEST(FalloutScriptActorStatusTest, no_avoidance_is_one_switch_for_all)
    {
        Runner run;
        EXPECT_FALSE(run.mStatus.noAvoidance());
        run.ask(OpSetNoAvoidance, 0, { 1 });
        EXPECT_TRUE(run.mStatus.noAvoidance());
        run.ask(OpSetNoAvoidance, 0, { 0 });
        EXPECT_FALSE(run.mStatus.noAvoidance());
        // SetAllReachable changes nothing (it is the empty command of the executables)
        run.ask(OpSetAllReachable, 0, { 1 });
        EXPECT_THAT(run.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorStatusTest, the_group_counts_follow_who_fights_whom)
    {
        Runner run;
        run.mCombat.start(raider, doctor);
        run.mCombat.start(otherRaider, doctor);
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, raider), 2.0);
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, otherRaider), 2.0);
        EXPECT_EQ(run.ask(OpGetGroupTargetCount, raider), 1.0);
        // The one that is fought is in no group of its own
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, doctor), 0.0);
        run.mCombat.stop(otherRaider);
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, raider), 1.0);
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, otherRaider), 0.0);
        run.mCombat.stop(raider);
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, raider), 0.0);
        EXPECT_EQ(run.ask(OpGetGroupTargetCount, raider), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, the_group_that_fights_the_players_side_counts_the_teammates_it_fights)
    {
        Runner run;
        run.mWorld.mTeammates.insert(companion);
        run.mCombat.start(raider, player);
        run.mCombat.start(otherRaider, companion);
        EXPECT_EQ(run.ask(OpGetGroupMemberCount, raider), 2.0);
        EXPECT_EQ(run.ask(OpGetGroupTargetCount, raider), 2.0);
        EXPECT_EQ(run.ask(OpGetGroupTargetCount, otherRaider), 2.0);
        EXPECT_EQ(run.ask(OpGetPlayerTeammateCount, 0), 1.0);
        run.mWorld.mTeammates.insert(doctor);
        EXPECT_EQ(run.ask(OpGetPlayerTeammateCount, 0), 2.0);
    }

    TEST(FalloutScriptActorStatusTest, get_owner_last_target_outlives_the_fight)
    {
        Runner run;
        EXPECT_EQ(run.ask(OpGetOwnerLastTarget, raider), 0.0);
        run.mCombat.start(raider, doctor);
        EXPECT_EQ(run.ask(OpGetOwnerLastTarget, raider), static_cast<double>(doctor));
        run.mCombat.stop(raider);
        EXPECT_EQ(run.ask(OpGetOwnerLastTarget, raider), static_cast<double>(doctor));
        run.mCombat.start(raider, player);
        EXPECT_EQ(run.ask(OpGetOwnerLastTarget, raider), static_cast<double>(player));
        // An item that the actor carries tells the actor's last target
        run.mWorld.mOwners[crate] = raider;
        EXPECT_EQ(run.ask(OpGetOwnerLastTarget, crate), static_cast<double>(player));
        // Something that is held by nobody has none
        EXPECT_EQ(run.ask(OpGetOwnerLastTarget, 0), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, fallout_3_has_no_get_owner_last_target)
    {
        Runner run(FalloutScript::Game::Fallout3);
        run.mCombat.start(raider, doctor);
        EXPECT_FALSE(run.mInterpreter.hasHandler(OpGetOwnerLastTarget));
        EXPECT_TRUE(run.mInterpreter.hasHandler(OpGetGroupMemberCount));
    }

    TEST(FalloutScriptActorStatusTest, is_actor_asks_the_form)
    {
        Runner run;
        EXPECT_EQ(run.ask(OpIsActor, raider), 1.0);
        EXPECT_EQ(run.ask(OpIsActor, player), 1.0);
        EXPECT_EQ(run.ask(OpIsActor, crate), 0.0);
        EXPECT_EQ(run.ask(OpIsActor, 0), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, the_furniture_marker_comes_from_the_world)
    {
        Runner run;
        run.mWorld.mMarkers[doctor] = 26;
        EXPECT_EQ(run.ask(OpGetFurnitureMarkerID, doctor), 26.0);
        EXPECT_EQ(run.ask(OpGetFurnitureMarkerID, raider), 0.0);
        EXPECT_EQ(run.ask(OpGetFurnitureMarkerID, crate), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, get_package_target_tells_the_reference_the_package_names)
    {
        Runner run;
        run.mStatus.update(raider, [](ActorActivity& activity) { activity.mPackageTarget = player; });
        EXPECT_EQ(run.ask(OpGetPackageTarget, raider), static_cast<double>(player));
        EXPECT_EQ(run.ask(OpGetPackageTarget, doctor), 0.0);
        EXPECT_EQ(run.ask(OpGetPackageTarget, crate), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, the_fatigue_percentage_is_the_current_over_the_permanent_value)
    {
        Runner run;
        run.mWorld.mFatigue[raider] = { 50.0, 200.0 };
        run.mWorld.mFatigue[doctor] = { 10.0, 0.0 };
        EXPECT_EQ(run.ask(OpGetFatiguePercentage, raider), 0.25);
        // No permanent value is full
        EXPECT_EQ(run.ask(OpGetFatiguePercentage, doctor), 1.0);
        // An actor the world has no values for, and what is no actor
        EXPECT_EQ(run.ask(OpGetFatiguePercentage, otherRaider), 0.0);
        EXPECT_EQ(run.ask(OpGetFatiguePercentage, crate), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, the_actors_in_high_are_counted_by_the_world_and_disable_all_actors_asks_it)
    {
        Runner run;
        run.mWorld.mHigh = 12;
        EXPECT_EQ(run.ask(OpGetActorsInHigh, 0), 12.0);
        run.mWorld.mHigh = -4;
        EXPECT_EQ(run.ask(OpGetActorsInHigh, 0), 0.0);
        run.ask(OpDisableAllActors, doctor);
        run.ask(OpDisableAllActors, 0);
        EXPECT_THAT(run.mWorld.mDisabledExcept, ElementsAre(doctor, 0u));
    }

    TEST(FalloutScriptActorStatusTest, the_player_is_told_of_the_corpse_it_feeds_on)
    {
        Runner run;
        EXPECT_EQ(run.ask(OpGetCannibal, player), 0.0);
        // The corpse is the object of the command and the player is its argument
        run.ask(OpStartCannibal, raider, { static_cast<double>(player) });
        EXPECT_EQ(run.ask(OpGetCannibal, player), 1.0);
        EXPECT_EQ(run.ask(OpGetSandman, player), 0.0);
        run.ask(OpStartMisterSandMan, doctor, { static_cast<double>(player) });
        EXPECT_EQ(run.ask(OpGetSandman, player), 1.0);
        // Only the player is asked
        EXPECT_EQ(run.ask(OpGetCannibal, raider), 0.0);
        // Neither of them is an actor that does it, or one that is done to
        run.ask(OpStartCannibal, crate, { static_cast<double>(otherRaider) });
        run.ask(OpStartCannibal, raider, { static_cast<double>(crate) });
        EXPECT_FALSE(run.mStatus.feeding(ActorStatus::Feeding::Cannibal, otherRaider));
        EXPECT_FALSE(run.mStatus.feeding(ActorStatus::Feeding::Cannibal, crate));
    }

    TEST(FalloutScriptActorStatusTest, advance_pc_level_gives_the_player_the_next_level)
    {
        Runner run;
        EXPECT_EQ(run.mProgress.level(), 1);
        run.ask(OpAdvancePCLevel, player);
        EXPECT_EQ(run.mProgress.level(), 2);
        run.ask(OpAdvancePCLevel, player);
        EXPECT_EQ(run.mProgress.level(), 3);
    }

    TEST(FalloutScriptActorStatusTest, mod_pc_attribute_changes_only_the_special_values_of_the_player)
    {
        namespace AV = FalloutScript::ActorValue;
        Runner run;
        const double strength = run.mValues.base(player, AV::strength);
        run.ask(OpModPCAttribute, player, { static_cast<double>(AV::strength), 2 });
        EXPECT_EQ(run.mValues.base(player, AV::strength), strength + 2);
        run.ask(OpModPCAttribute, player, { static_cast<double>(AV::luck), -1 });
        EXPECT_EQ(run.mValues.base(player, AV::luck), run.mValues.startingValue(AV::luck) - 1);
        // The values that are not attributes are left alone (the check of the executable takes 5 to 11)
        const double barter = run.mValues.base(player, AV::barter);
        run.ask(OpModPCAttribute, player, { static_cast<double>(AV::barter), 10 });
        run.ask(OpModPCAttribute, player, { static_cast<double>(AV::actionPoints), 10 });
        run.ask(OpModPCAttribute, player, { 4.0, 10 });
        EXPECT_EQ(run.mValues.base(player, AV::barter), barter);
        EXPECT_EQ(run.mValues.base(player, AV::actionPoints), FalloutScript::ActorValues::startingValue(AV::actionPoints));
        // The skills of the player are not changed by the two commands that name them
        run.ask(OpModPCSkill, player, { static_cast<double>(AV::barter), 10 });
        run.ask(OpAdvancePCSkill, player, { static_cast<double>(AV::barter), 10 });
        EXPECT_EQ(run.mValues.base(player, AV::barter), barter);
    }

    TEST(FalloutScriptActorStatusTest, the_race_of_a_character_is_playable_when_its_record_says_so)
    {
        Runner run;
        run.mWorld.mRaces[doctor] = raceHuman;
        run.mWorld.mRaces[raider] = raceCrawler;
        EXPECT_EQ(run.ask(OpGetIsPlayableRace, doctor), 1.0);
        EXPECT_EQ(run.ask(OpGetIsPlayableRace, raider), 0.0);
        // A creature has no race to ask about
        EXPECT_EQ(run.ask(OpGetIsPlayableRace, otherRaider), 0.0);
        EXPECT_EQ(run.ask(OpGetIsPlayableRace, 0), 0.0);
    }

    TEST(FalloutScriptActorStatusTest, the_horse_the_torch_and_the_disease_answer_what_the_executable_does)
    {
        Runner run;
        run.mStatus.publish(raider, moving(Movement::forward));
        EXPECT_EQ(run.ask(OpIsRidingHorse, raider), 0.0);
        EXPECT_EQ(run.ask(OpIsTorchOut, raider), 0.0);
        EXPECT_EQ(run.ask(OpGetDisease, raider), 0.0);
        EXPECT_THAT(run.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptActorStatusTest, every_command_of_the_table_that_is_built_has_a_handler)
    {
        Runner run;
        for (const std::uint16_t code : { OpIsMoving, OpIsTurning, OpIsRunning, OpIsSwimming, OpIsWaiting,
                 OpGetWalkSpeed, OpIsFacingUp, OpIsLeftUp, OpGetAnimAction, OpIsIdlePlaying, OpGetIdleDoneOnce, OpIsLastIdlePlayed,
                 OpGetCurrentAIProcedure, OpGetPackageTarget, OpSetActorsAI, OpToggleActorsAI, OpIsActorsAIOff,
                 OpDisableAllActors, OpGetActorsInHigh, OpSetForceRun, OpGetForceRun, OpSetForceSneak, OpGetForceSneak,
                 OpSetNoAvoidance, OpSetAllReachable, OpGetGroupMemberCount, OpGetGroupTargetCount,
                 OpGetPlayerTeammateCount, OpGetOwnerLastTarget, OpIsActor, OpGetFurnitureMarkerID,
                 OpGetFatiguePercentage, OpStartCannibal, OpStartMisterSandMan, OpGetCannibal, OpGetSandman,
                 OpAdvancePCLevel, OpModPCAttribute, OpModPCSkill, OpAdvancePCSkill, OpGetIsPlayableRace,
                 OpIsRidingHorse, OpIsTorchOut, OpGetDisease })
            EXPECT_TRUE(run.mInterpreter.hasHandler(code)) << std::hex << code;
    }
}
