#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/combatcommands.hpp>
#include <components/falloutscript/combatstate.hpp>
#include <components/falloutscript/objects.hpp>
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
    using FalloutScript::ActorState;
    using FalloutScript::CombatState;
    using FalloutScript::FormId;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId raider = 0x01000200;
    constexpr FormId ghoul = 0x01000300;
    constexpr FormId vault = 0x01001000;
    constexpr FormId gang = 0x01001001;
    constexpr FormId mutants = 0x01001002;
    constexpr FormId scriptOfGuards = 0x01002000;
    constexpr FormId guardBase = 0x01003000;

    constexpr std::uint32_t integerVariable = 1;

    constexpr std::uint16_t OpSetEssential = 0x1003;
    constexpr std::uint16_t OpStartCombat = 0x1016;
    constexpr std::uint16_t OpStopCombat = 0x1017;
    constexpr std::uint16_t OpGetDetected = 0x102D;
    constexpr std::uint16_t OpGetAttacked = 0x103F;
    constexpr std::uint16_t OpGetShouldAttack = 0x1042;
    constexpr std::uint16_t OpSetAlert = 0x105A;
    constexpr std::uint16_t OpGetIsAlerted = 0x105B;
    constexpr std::uint16_t OpSetEnemy = 0x1078;
    constexpr std::uint16_t OpSetAlly = 0x1079;
    constexpr std::uint16_t OpGetCombatTarget = 0x10E8;
    constexpr std::uint16_t OpSetUnconscious = 0x10F1;
    constexpr std::uint16_t OpGetUnconscious = 0x10F2;
    constexpr std::uint16_t OpSetRestrained = 0x10F3;
    constexpr std::uint16_t OpGetRestrained = 0x10F4;
    constexpr std::uint16_t OpStopCombatAlarmOnActor = 0x1101;
    constexpr std::uint16_t OpGetFriendHit = 0x1120;
    constexpr std::uint16_t OpIsInCombat = 0x1121;
    constexpr std::uint16_t OpIsEssential = 0x1162;
    constexpr std::uint16_t OpGetHitLocation = 0x1187;
    constexpr std::uint16_t OpIsKiller = 0x1199;
    constexpr std::uint16_t OpGetFactionCombatReaction = 0x119B;
    constexpr std::uint16_t OpGetKillingBlowLimb = 0x11F0;
    constexpr std::uint16_t OpGetLastHitCritical = 0x11FE;
    constexpr std::uint16_t OpIsCombatTarget = 0x1203;
    constexpr std::uint16_t OpSetActorRefEssential = 0x1273;

    class FakeWorld : public FalloutScript::ActorRecords, public FalloutScript::CombatWorld
    {
    public:
        std::map<FormId, std::vector<std::pair<FormId, int>>> mFactions;
        std::map<FormId, FormId> mBases;
        std::set<FormId> mEssential;
        std::set<std::pair<FormId, FormId>> mSees;
        std::map<std::pair<FormId, FormId>, CombatState::Relation> mRecorded;

        bool initialItems(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool initialFactions(FormId actor, std::vector<std::pair<FormId, int>>& factions) override
        {
            factions = mFactions[actor];
            return true;
        }
        bool baseFormOf(FormId reference, FormId& base) override
        {
            const auto found = mBases.find(reference);
            if (found == mBases.end())
                return false;
            base = found->second;
            return true;
        }

        bool detects(FormId viewer, FormId target) override { return mSees.count({ viewer, target }) != 0; }
        bool essentialByRecord(FormId actor) override { return mEssential.count(actor) != 0; }
        std::optional<CombatState::Relation> recordedRelation(FormId first, FormId second) override
        {
            const auto found = mRecorded.find({ first, second });
            if (found != mRecorded.end())
                return found->second;
            const auto reverse = mRecorded.find({ second, first });
            if (reverse != mRecorded.end())
                return reverse->second;
            return std::nullopt;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        std::map<FormId, ESM4::Script> mScripts;
        ObjectScripts mObjects{ mInterpreter, [this](FormId id) -> const ESM4::Script* {
                                   const auto found = mScripts.find(id);
                                   return found == mScripts.end() ? nullptr : &found->second;
                               } };
        FakeWorld mWorld;
        ActorState mActors{ mWorld };
        CombatState mCombat;

        Runner()
        {
            mWorld.mFactions[guard] = { { vault, 0 } };
            mWorld.mFactions[raider] = { { gang, 0 } };
            mWorld.mFactions[ghoul] = { { mutants, 1 } };
            FalloutScript::addActorCommands(mInterpreter, mActors);
            FalloutScript::addCombatCommands(mInterpreter, mCombat, mActors, mWorld);
        }

        void run(const ScriptBuilder& builder)
        {
            ESM4::Script& record = mScripts[scriptOfGuards];
            record.mId = ESM::FormId::fromUint32(scriptOfGuards);
            record.mScript = builder.definition();
            mObjects.add(guard, scriptOfGuards);
            mObjects.trigger(guard, BlockType::OnActivate);
        }

        double variable(std::uint32_t index)
        {
            FalloutScript::Instance* instance = mObjects.instance(guard);
            return instance != nullptr ? instance->variable(index) : -1;
        }
    };

    Bytes oneArg(std::uint16_t form)
    {
        return arguments(1).add(refArg(form));
    }

    Bytes twoArgs(std::uint16_t first, std::uint16_t second)
    {
        return arguments(2).add(refArg(first)).add(refArg(second));
    }

    Bytes withFlags(std::uint16_t first, std::uint16_t second, std::int32_t one, std::int32_t other)
    {
        return arguments(4).add(refArg(first)).add(refArg(second)).add(intArg(one)).add(intArg(other));
    }

    TEST(FalloutScriptCombatCommandsTest, a_script_starts_and_stops_a_fight)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t enemy = builder.form(raider);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsInCombat, arguments(0))) + command(OpStartCombat, oneArg(enemy))
            + setTo(var('s', 2), callToken(OpIsInCombat, arguments(0)))
            + setTo(var('s', 3), callToken(OpGetCombatTarget, arguments(0))) + command(OpStopCombat, arguments(0))
            + setTo(var('s', 4), callToken(OpIsInCombat, arguments(0))) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "during")
            .variable(3, integerVariable, "target")
            .variable(4, integerVariable, "after");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), static_cast<double>(raider));
        EXPECT_EQ(setup.variable(4), 0);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }

    TEST(FalloutScriptCombatCommandsTest, a_script_tells_who_fights_the_character)
    {
        Runner setup;
        setup.mCombat.start(raider, guard);
        ScriptBuilder builder;
        const std::uint16_t enemy = builder.form(raider);
        const std::uint16_t stranger = builder.form(ghoul);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsCombatTarget, oneArg(enemy)))
            + setTo(var('s', 2), callToken(OpIsCombatTarget, oneArg(stranger))) + end());
        builder.variable(1, integerVariable, "fought").variable(2, integerVariable, "not");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
    }

    TEST(FalloutScriptCombatCommandsTest, the_alarm_ends_the_fight_for_everyone_who_went_for_the_character)
    {
        Runner setup;
        setup.mCombat.start(raider, guard);
        setup.mCombat.start(ghoul, guard);
        setup.mCombat.start(guard, raider);
        ScriptBuilder builder;
        builder.code(
            scriptName() + begin(BlockType::OnActivate) + command(OpStopCombatAlarmOnActor, arguments(0)) + end());

        setup.run(builder);

        EXPECT_FALSE(setup.mCombat.inCombat(raider));
        EXPECT_FALSE(setup.mCombat.inCombat(ghoul));
        EXPECT_FALSE(setup.mCombat.inCombat(guard));
    }

    TEST(FalloutScriptCombatCommandsTest, a_script_tells_what_the_character_notices)
    {
        Runner setup;
        setup.mWorld.mSees.insert({ guard, raider });
        ScriptBuilder builder;
        const std::uint16_t seen = builder.form(raider);
        const std::uint16_t hidden = builder.form(ghoul);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetDetected, oneArg(seen)))
            + setTo(var('s', 2), callToken(OpGetDetected, oneArg(hidden))) + end());
        builder.variable(1, integerVariable, "seen").variable(2, integerVariable, "hidden");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
    }

    TEST(FalloutScriptCombatCommandsTest, alert_unconscious_and_restrained_can_be_set_and_asked)
    {
        Runner setup;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetIsAlerted, arguments(0)))
            + command(OpSetAlert, arguments(1).add(intArg(1)))
            + setTo(var('s', 2), callToken(OpGetIsAlerted, arguments(0)))
            + command(OpSetUnconscious, arguments(1).add(intArg(1)))
            + setTo(var('s', 3), callToken(OpGetUnconscious, arguments(0)))
            + command(OpSetUnconscious, arguments(1).add(intArg(0)))
            + setTo(var('s', 4), callToken(OpGetUnconscious, arguments(0)))
            + command(OpSetRestrained, arguments(1).add(intArg(1)))
            + setTo(var('s', 5), callToken(OpGetRestrained, arguments(0))) + end());
        builder.variable(1, integerVariable, "alertBefore")
            .variable(2, integerVariable, "alert")
            .variable(3, integerVariable, "out")
            .variable(4, integerVariable, "up")
            .variable(5, integerVariable, "restrained");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), 1);
        EXPECT_EQ(setup.variable(4), 0);
        EXPECT_EQ(setup.variable(5), 1);
        EXPECT_TRUE(setup.mCombat.alert(guard));
        EXPECT_TRUE(setup.mCombat.restrained(guard));
    }

    TEST(FalloutScriptCombatCommandsTest, a_character_attacks_the_enemies_of_its_factions)
    {
        Runner setup;
        ScriptBuilder builder;
        const std::uint16_t enemy = builder.form(raider);
        const std::uint16_t first = builder.form(vault);
        const std::uint16_t second = builder.form(gang);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetShouldAttack, oneArg(enemy)))
            + setTo(var('s', 2), callToken(OpGetFactionCombatReaction, twoArgs(first, second)))
            + command(OpSetEnemy, twoArgs(first, second))
            + setTo(var('s', 3), callToken(OpGetShouldAttack, oneArg(enemy)))
            + setTo(var('s', 4), callToken(OpGetFactionCombatReaction, twoArgs(first, second)))
            + command(OpSetAlly, twoArgs(first, second))
            + setTo(var('s', 5), callToken(OpGetShouldAttack, oneArg(enemy)))
            + setTo(var('s', 6), callToken(OpGetFactionCombatReaction, twoArgs(second, first))) + end());
        builder.variable(1, integerVariable, "attackBefore")
            .variable(2, integerVariable, "reactionBefore")
            .variable(3, integerVariable, "attackEnemy")
            .variable(4, integerVariable, "reactionEnemy")
            .variable(5, integerVariable, "attackAlly")
            .variable(6, integerVariable, "reactionAlly");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 0);
        EXPECT_EQ(setup.variable(3), 1);
        EXPECT_EQ(setup.variable(4), 1);
        EXPECT_EQ(setup.variable(5), 0);
        EXPECT_EQ(setup.variable(6), 2);
    }

    TEST(FalloutScriptCombatCommandsTest, the_flags_of_set_enemy_and_set_ally_say_how_each_faction_stands_to_the_other)
    {
        Runner setup;
        setup.mWorld.mRecorded[{ vault, mutants }] = CombatState::Relation::Friend;
        ScriptBuilder builder;
        const std::uint16_t first = builder.form(vault);
        const std::uint16_t second = builder.form(gang);
        const std::uint16_t third = builder.form(mutants);
        builder.code(scriptName()
            + begin(BlockType::OnActivate)
            // The first faction is neutral to the second, which stays an enemy of the first
            + command(OpSetEnemy, withFlags(first, second, 1, 0))
            + setTo(var('s', 1), callToken(OpGetFactionCombatReaction, twoArgs(first, second)))
            + setTo(var('s', 2), callToken(OpGetFactionCombatReaction, twoArgs(second, first)))
            // The first faction is an ally of the second, which is a friend of the first
            + command(OpSetAlly, withFlags(first, second, 0, 1))
            + setTo(var('s', 3), callToken(OpGetFactionCombatReaction, twoArgs(first, second)))
            + setTo(var('s', 4), callToken(OpGetFactionCombatReaction, twoArgs(second, first)))
            // A friend in the records is not an ally
            + setTo(var('s', 5), callToken(OpGetFactionCombatReaction, twoArgs(first, third))) + end());
        builder.variable(1, integerVariable, "neutral")
            .variable(2, integerVariable, "enemy")
            .variable(3, integerVariable, "ally")
            .variable(4, integerVariable, "friend")
            .variable(5, integerVariable, "recorded");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), 2);
        EXPECT_EQ(setup.variable(4), 3);
        EXPECT_EQ(setup.variable(5), 3);
    }

    TEST(FalloutScriptCombatCommandsTest, the_records_say_how_factions_stand_when_no_script_did)
    {
        Runner setup;
        setup.mWorld.mRecorded[{ vault, gang }] = CombatState::Relation::Enemy;
        setup.mWorld.mRecorded[{ vault, mutants }] = CombatState::Relation::Ally;
        ScriptBuilder builder;
        const std::uint16_t enemy = builder.form(raider);
        const std::uint16_t friendly = builder.form(ghoul);
        const std::uint16_t first = builder.form(vault);
        const std::uint16_t second = builder.form(gang);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetShouldAttack, oneArg(enemy)))
            + setTo(var('s', 2), callToken(OpGetShouldAttack, oneArg(friendly)))
            + setTo(var('s', 3), callToken(OpGetFactionCombatReaction, twoArgs(first, second)))
            // What a script says is stronger than the records
            + command(OpSetAlly, twoArgs(first, second))
            + setTo(var('s', 4), callToken(OpGetShouldAttack, oneArg(enemy))) + end());
        builder.variable(1, integerVariable, "enemy")
            .variable(2, integerVariable, "friend")
            .variable(3, integerVariable, "reaction")
            .variable(4, integerVariable, "afterScript");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
        EXPECT_EQ(setup.variable(3), 1);
        EXPECT_EQ(setup.variable(4), 0);
    }

    TEST(FalloutScriptCombatCommandsTest, a_character_does_not_attack_a_dead_one_or_itself)
    {
        Runner setup;
        setup.mWorld.mRecorded[{ vault, gang }] = CombatState::Relation::Enemy;
        setup.mActors.setDead(raider, true);
        ScriptBuilder builder;
        const std::uint16_t enemy = builder.form(raider);
        const std::uint16_t itself = builder.form(guard);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetShouldAttack, oneArg(enemy)))
            + setTo(var('s', 2), callToken(OpGetShouldAttack, oneArg(itself))) + end());
        builder.variable(1, integerVariable, "dead").variable(2, integerVariable, "itself");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 0);
    }

    TEST(FalloutScriptCombatCommandsTest, essential_is_what_a_script_said_for_the_character_or_its_form_or_the_record)
    {
        Runner setup;
        setup.mWorld.mBases[guard] = guardBase;
        ScriptBuilder builder;
        const std::uint16_t base = builder.form(guardBase);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsEssential, arguments(0)))
            + command(OpSetEssential, arguments(1).add(refArg(base)))
            + setTo(var('s', 2), callToken(OpIsEssential, arguments(0)))
            + command(OpSetEssential, arguments(2).add(refArg(base)).add(intArg(0)))
            + setTo(var('s', 3), callToken(OpIsEssential, arguments(0)))
            + command(OpSetActorRefEssential, arguments(1).add(intArg(1)))
            + setTo(var('s', 4), callToken(OpIsEssential, arguments(0))) + end());
        builder.variable(1, integerVariable, "before")
            .variable(2, integerVariable, "formEssential")
            .variable(3, integerVariable, "formNot")
            .variable(4, integerVariable, "referenceEssential");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), 0);
        EXPECT_EQ(setup.variable(4), 1);
    }

    TEST(FalloutScriptCombatCommandsTest, a_character_the_record_makes_essential_is_so_until_a_script_says_otherwise)
    {
        Runner setup;
        setup.mWorld.mEssential.insert(guard);
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsEssential, arguments(0)))
            + command(OpSetActorRefEssential, arguments(1).add(intArg(0)))
            + setTo(var('s', 2), callToken(OpIsEssential, arguments(0))) + end());
        builder.variable(1, integerVariable, "record").variable(2, integerVariable, "script");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
    }

    TEST(FalloutScriptCombatCommandsTest, a_script_asks_what_the_last_blow_was_like)
    {
        Runner setup;
        CombatState::Hit hit;
        hit.mAttacker = raider;
        hit.mCritical = true;
        hit.mLimb = 2;
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetHitLocation, arguments(0)))
            + setTo(var('s', 2), callToken(OpGetLastHitCritical, arguments(0)))
            + setTo(var('s', 3), callToken(OpGetKillingBlowLimb, arguments(0)))
            + setTo(var('s', 4), callToken(OpGetAttacked, arguments(0))) + end());
        builder.variable(1, integerVariable, "limb")
            .variable(2, integerVariable, "critical")
            .variable(3, integerVariable, "killing")
            .variable(4, integerVariable, "attacked");

        setup.run(builder);
        // Nothing hit it yet
        EXPECT_EQ(setup.variable(1), -1);
        EXPECT_EQ(setup.variable(2), 0);
        EXPECT_EQ(setup.variable(3), -1);
        EXPECT_EQ(setup.variable(4), 0);

        setup.mCombat.noteHit(guard, hit);
        setup.mObjects.trigger(guard, BlockType::OnActivate);
        EXPECT_EQ(setup.variable(1), 2);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_EQ(setup.variable(3), -1); // it lives
        EXPECT_EQ(setup.variable(4), 1);

        setup.mActors.setDead(guard, true, raider);
        setup.mObjects.trigger(guard, BlockType::OnActivate);
        EXPECT_EQ(setup.variable(3), 2);

        setup.mCombat.advance(5.f);
        setup.mObjects.trigger(guard, BlockType::OnActivate);
        EXPECT_EQ(setup.variable(4), 0);
        EXPECT_EQ(setup.variable(1), 2);
    }

    TEST(FalloutScriptCombatCommandsTest, a_script_asks_who_killed_the_character)
    {
        Runner setup;
        setup.mActors.setDead(guard, true, raider);
        ScriptBuilder builder;
        const std::uint16_t killer = builder.form(raider);
        const std::uint16_t other = builder.form(ghoul);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsKiller, oneArg(killer)))
            + setTo(var('s', 2), callToken(OpIsKiller, oneArg(other))) + end());
        builder.variable(1, integerVariable, "killer").variable(2, integerVariable, "other");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 1);
        EXPECT_EQ(setup.variable(2), 0);
    }

    TEST(FalloutScriptCombatCommandsTest, a_blow_from_a_friend_is_told)
    {
        Runner setup;
        setup.mWorld.mFactions[ghoul] = { { vault, 2 } };
        CombatState::Hit byFriend;
        byFriend.mAttacker = ghoul;
        setup.mCombat.noteHit(guard, byFriend);
        ScriptBuilder builder;
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpGetFriendHit, arguments(0))) + end());
        builder.variable(1, integerVariable, "friend");

        setup.run(builder);
        EXPECT_EQ(setup.variable(1), 1);

        CombatState::Hit byRaider;
        byRaider.mAttacker = raider;
        setup.mCombat.noteHit(guard, byRaider);
        setup.mObjects.trigger(guard, BlockType::OnActivate);
        EXPECT_EQ(setup.variable(1), 0);
    }
}
