#include "scriptbuilder.hpp"

#include <components/falloutscript/actortraits.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ActorTraits;
    using FalloutScript::FormId;

    constexpr FormId clerk = 0x01000100;
    constexpr FormId wolf = 0x01000101;
    constexpr FormId unknown = 0x01000102;
    constexpr FormId raceHuman = 0x01000200;
    constexpr FormId raceGhoul = 0x01000201;
    constexpr FormId classMerchant = 0x01000300;
    constexpr FormId voiceClerk = 0x01000400;
    constexpr FormId voiceOther = 0x01000401;

    constexpr std::uint16_t OpGetIsCreature = 0x1040;
    constexpr std::uint16_t OpGetIsClass = 0x1044;
    constexpr std::uint16_t OpGetIsRace = 0x1045;
    constexpr std::uint16_t OpGetIsSex = 0x1046;
    constexpr std::uint16_t OpGetPCIsSex = 0x1083;
    constexpr std::uint16_t OpIsChild = 0x116D;
    constexpr std::uint16_t OpGetIsVoiceType = 0x11AB;
    constexpr std::uint16_t OpGetIsCreatureType = 0x11B6;

    class FakeTraits : public FalloutScript::ActorTraitRecords
    {
    public:
        std::map<FormId, ActorTraits> mTraits;

        bool traitsOf(FormId reference, ActorTraits& traits) override
        {
            const auto found = mTraits.find(reference);
            if (found == mTraits.end())
                return false;
            traits = found->second;
            return true;
        }
    };

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeTraits mRecords;

        Runner()
        {
            ActorTraits person;
            person.mRace = raceHuman;
            person.mClass = classMerchant;
            person.mVoice = voiceClerk;
            person.mFemale = true;
            mRecords.mTraits[clerk] = person;
            ActorTraits beast;
            beast.mCreature = true;
            beast.mCreatureType = 1;
            mRecords.mTraits[wolf] = beast;
            ActorTraits player;
            player.mRace = raceGhoul;
            player.mChild = true;
            mRecords.mTraits[FalloutScript::playerReference] = player;
            FalloutScript::addActorTraitCommands(mInterpreter, mRecords);
        }

        double ask(std::uint16_t code, FormId reference, double argument)
        {
            return mInterpreter.call(code, reference, true, { { argument, {}, false } });
        }

        double ask(std::uint16_t code, FormId reference) { return mInterpreter.call(code, reference, true, {}); }
    };

    TEST(FalloutScriptActorTraitsTest, theSexIsAskedAsMaleOrFemale)
    {
        Runner run;

        EXPECT_EQ(run.ask(OpGetIsSex, clerk, 1), 1.0);
        EXPECT_EQ(run.ask(OpGetIsSex, clerk, 0), 0.0);
        EXPECT_EQ(run.ask(OpGetIsSex, wolf, 0), 1.0);
        EXPECT_EQ(run.ask(OpGetIsSex, wolf, 1), 0.0);
    }

    TEST(FalloutScriptActorTraitsTest, theSexOfThePlayerIsAskedWhoeverTheCommandRunsOn)
    {
        Runner run;

        // The player here is male
        EXPECT_EQ(run.mInterpreter.call(OpGetPCIsSex, clerk, true, { { 0, {}, false } }), 1.0);
        EXPECT_EQ(run.mInterpreter.call(OpGetPCIsSex, clerk, true, { { 1, {}, false } }), 0.0);
        run.mRecords.mTraits[FalloutScript::playerReference].mFemale = true;
        EXPECT_EQ(run.mInterpreter.call(OpGetPCIsSex, 0, false, { { 1, {}, false } }), 1.0);
    }

    TEST(FalloutScriptActorTraitsTest, theVoiceTypeRaceAndClassAreThoseOfTheRecord)
    {
        Runner run;

        EXPECT_EQ(run.ask(OpGetIsVoiceType, clerk, voiceClerk), 1.0);
        EXPECT_EQ(run.ask(OpGetIsVoiceType, clerk, voiceOther), 0.0);
        EXPECT_EQ(run.ask(OpGetIsRace, clerk, raceHuman), 1.0);
        EXPECT_EQ(run.ask(OpGetIsRace, clerk, raceGhoul), 0.0);
        EXPECT_EQ(run.ask(OpGetIsClass, clerk, classMerchant), 1.0);
        EXPECT_EQ(run.ask(OpGetIsClass, wolf, classMerchant), 0.0);
        EXPECT_EQ(run.ask(OpGetIsRace, FalloutScript::playerReference, raceGhoul), 1.0);
    }

    TEST(FalloutScriptActorTraitsTest, aFormThatIsNotGivenNeverMatches)
    {
        Runner run;
        run.mRecords.mTraits[wolf].mVoice = 0;

        // A character with no voice type is not of the voice type that the call leaves out
        EXPECT_EQ(run.mInterpreter.call(OpGetIsVoiceType, wolf, true, {}), 0.0);
        EXPECT_EQ(run.mInterpreter.call(OpGetIsRace, wolf, true, {}), 0.0);
        EXPECT_EQ(run.mInterpreter.call(OpGetIsClass, wolf, true, {}), 0.0);
    }

    TEST(FalloutScriptActorTraitsTest, creaturesAndChildrenAreToldApart)
    {
        Runner run;

        EXPECT_EQ(run.ask(OpGetIsCreature, wolf), 1.0);
        EXPECT_EQ(run.ask(OpGetIsCreature, clerk), 0.0);
        EXPECT_EQ(run.ask(OpGetIsCreatureType, wolf, 1), 1.0);
        EXPECT_EQ(run.ask(OpGetIsCreatureType, wolf, 2), 0.0);
        // A character that is not a creature is of no kind of creature, whatever number its record has
        EXPECT_EQ(run.ask(OpGetIsCreatureType, clerk, 0), 0.0);
        EXPECT_EQ(run.ask(OpIsChild, FalloutScript::playerReference), 1.0);
        EXPECT_EQ(run.ask(OpIsChild, clerk), 0.0);
    }

    TEST(FalloutScriptActorTraitsTest, whatTheHostDoesNotKnowIsNone)
    {
        Runner run;

        EXPECT_EQ(run.ask(OpGetIsSex, unknown, 0), 0.0);
        EXPECT_EQ(run.ask(OpGetIsSex, unknown, 1), 0.0);
        EXPECT_EQ(run.ask(OpIsChild, unknown), 0.0);
        EXPECT_EQ(run.ask(OpGetIsCreature, unknown), 0.0);
        // No object at all
        EXPECT_EQ(run.mInterpreter.call(OpGetIsSex, 0, false, { { 0, {}, false } }), 0.0);
    }
}
