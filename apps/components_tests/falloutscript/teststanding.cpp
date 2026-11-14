#include "scriptbuilder.hpp"

#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actortraits.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/standing.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <set>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::DispositionInput;
    using FalloutScript::FormId;
    using FalloutScript::Standing;
    using FalloutScript::StandingSettings;

    constexpr FormId player = 0x14;
    constexpr FormId doc = 0x00100010;
    constexpr FormId sheriff = 0x00100011;
    constexpr FormId raider = 0x00100012;
    constexpr FormId town = 0x00100001;
    constexpr FormId rangers = 0x00100002;
    constexpr FormId gang = 0x00100003;
    constexpr FormId human = 0x00100050;
    constexpr FormId ghoul = 0x00100051;

    class FakeStandingRecords : public FalloutScript::StandingRecords
    {
    public:
        StandingSettings mSettings;
        std::map<std::pair<FormId, FormId>, int> mReactions;

        StandingSettings standingSettings() override { return mSettings; }
        std::optional<int> recordedReaction(FormId faction, FormId other) override
        {
            const auto found = mReactions.find({ faction, other });
            if (found == mReactions.end())
                return std::nullopt;
            return found->second;
        }
    };

    class FakeActorRecords : public FalloutScript::ActorRecords
    {
    public:
        std::map<FormId, std::vector<std::pair<FormId, int>>> mFactions{ { ::player, { { town, 0 } } },
            { doc, { { town, 2 } } }, { sheriff, { { town, 5 }, { rangers, 1 } } }, { raider, { { gang, 0 } } } };

        bool initialItems(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool initialFactions(FormId actor, std::vector<std::pair<FormId, int>>& factions) override
        {
            const auto found = mFactions.find(actor);
            if (found == mFactions.end())
                return false;
            factions = found->second;
            return true;
        }
    };

    class FakeTraits : public FalloutScript::ActorTraitRecords
    {
    public:
        bool traitsOf(FormId reference, FalloutScript::ActorTraits& traits) override
        {
            switch (reference)
            {
                case ::player:
                case doc:
                    traits.mRace = human;
                    traits.mFemale = reference == doc;
                    return true;
                case sheriff:
                    traits.mRace = human;
                    traits.mFemale = false;
                    return true;
                case raider:
                    traits.mRace = ghoul;
                    traits.mFemale = true;
                    return true;
                default:
                    return false;
            }
        }
    };

    class FakeWorld : public FalloutScript::StandingWorld
    {
    public:
        std::map<FormId, double> mKarma;
        std::set<FormId> mEvil;
        std::map<FormId, DispositionInput> mInputs;
        std::map<std::pair<FormId, FormId>, int> mRelations;
        double mAdjust = 0;

        FormId player() override { return ::player; }
        bool dispositionInput(FormId self, FormId, DispositionInput& input) override
        {
            const auto found = mInputs.find(self);
            if (found == mInputs.end())
                return false;
            input = found->second;
            return true;
        }
        double adjustDisposition(FormId, FormId, double value) override { return value + mAdjust; }
        int relation(FormId self, FormId other) override
        {
            const auto found = mRelations.find({ self, other });
            return found == mRelations.end() ? 0 : found->second;
        }
        double karmaOf(FormId actor) override
        {
            const auto found = mKarma.find(actor);
            return found == mKarma.end() ? 0 : found->second;
        }
        bool isEvil(FormId actor) override { return mEvil.count(actor) != 0; }
    };

    TEST(FalloutScriptStandingTest, the_formula_starts_at_the_base_and_adds_the_charisma_term)
    {
        const StandingSettings settings;
        DispositionInput input;
        input.mBase = 40;
        input.mCharisma = 5;
        // Charisma at the offset adds nothing; each point above adds the multiplier
        EXPECT_EQ(Standing::formula(settings, input, 0), 40);
        input.mCharisma = 9;
        EXPECT_EQ(Standing::formula(settings, input, 0), 60);
        input.mCharisma = 1;
        EXPECT_EQ(Standing::formula(settings, input, 0), 20);
    }

    TEST(FalloutScriptStandingTest, the_term_of_the_karma_goes_by_the_responsibility_of_the_one_judging)
    {
        const StandingSettings settings;
        DispositionInput input;
        input.mCharisma = 5;
        input.mKarma = 500;
        // 0.002 * 500 * (-50 + 1 * 100) = 50
        input.mResponsibility = 100;
        EXPECT_EQ(Standing::formula(settings, input, 0), 50);
        // A saint is liked by the careful (responsibility above 50) and disliked by the careless
        input.mResponsibility = 0;
        EXPECT_EQ(Standing::formula(settings, input, 0), -50);
        input.mKarma = -500;
        EXPECT_EQ(Standing::formula(settings, input, 0), 50);
        // A creature does not judge by karma
        input.mCreature = true;
        EXPECT_EQ(Standing::formula(settings, input, 0), 0);
    }

    TEST(FalloutScriptStandingTest, the_races_the_factions_a_drawn_weapon_and_the_pair_modifier_add_up)
    {
        const StandingSettings settings;
        DispositionInput input;
        input.mCharisma = 5;
        input.mRace = 7;
        EXPECT_EQ(Standing::formula(settings, input, 0), 7);
        // A faction pair that was found counts the reaction twice over (1 * r * (1 + 1)); without one, nothing
        input.mFactionPair = true;
        input.mFactionReaction = 10;
        EXPECT_EQ(Standing::formula(settings, input, 0), 7 + 20);
        input.mFactionPair = false;
        input.mFactionReaction = 10;
        EXPECT_EQ(Standing::formula(settings, input, 0), 7);
        input.mWeaponDrawn = true;
        EXPECT_EQ(Standing::formula(settings, input, 0), 7 - 10);
        input.mCreature = true;
        EXPECT_EQ(Standing::formula(settings, input, 0), 7);
        EXPECT_EQ(Standing::formula(settings, input, 13), 20);
    }

    TEST(FalloutScriptStandingTest, the_terms_are_cut_to_whole_numbers_one_by_one)
    {
        StandingSettings settings;
        settings.mCharMult = 2.5;
        DispositionInput input;
        input.mCharisma = 6;
        // (6 - 5) * 2.5 = 2.5 is cut to 2
        EXPECT_EQ(Standing::formula(settings, input, 0), 2);
        input.mCharisma = 4;
        // -2.5 is cut toward nothing
        EXPECT_EQ(Standing::formula(settings, input, 0), -2);
    }

    TEST(FalloutScriptStandingTest, the_disposition_is_kept_between_0_and_100_and_is_100_for_the_same_character)
    {
        FakeStandingRecords records;
        Standing standing(records);
        DispositionInput input;
        input.mBase = 90;
        input.mCharisma = 10;
        EXPECT_EQ(standing.disposition(doc, player, input), 100);
        input.mBase = -90;
        input.mCharisma = 0;
        EXPECT_EQ(standing.disposition(doc, player, input), 0);
        input.mBase = 30;
        input.mCharisma = 5;
        EXPECT_EQ(standing.disposition(doc, player, input), 30);
        EXPECT_EQ(standing.disposition(doc, player, input, [](double value) { return value + 12.9; }), 42);
        EXPECT_EQ(standing.disposition(doc, 0, input), 0);
        input.mSame = true;
        EXPECT_EQ(standing.disposition(doc, doc, input), 100);
    }

    TEST(FalloutScriptStandingTest, what_has_happened_between_two_is_added_and_changes_stay_within_the_ends)
    {
        FakeStandingRecords records;
        Standing standing(records);
        DispositionInput input;
        input.mBase = 50;
        input.mCharisma = 5;
        EXPECT_EQ(standing.pairModifier(doc, player), 0);
        // A pair that has no entry takes the change as it is
        standing.modifyDisposition(doc, player, -10.7, 50);
        EXPECT_EQ(standing.pairModifier(doc, player), -10);
        EXPECT_EQ(standing.disposition(doc, player, input), 40);
        // One that has an entry is not taken below 0 ...
        standing.modifyDisposition(doc, player, -100, 40);
        EXPECT_EQ(standing.pairModifier(doc, player), -50);
        EXPECT_EQ(standing.disposition(doc, player, input), 0);
        // ... or above 100
        standing.modifyDisposition(doc, player, 500, 0);
        EXPECT_EQ(standing.pairModifier(doc, player), 50);
        EXPECT_EQ(standing.disposition(doc, player, input), 100);
        standing.modifyDisposition(doc, player, 30, 20);
        EXPECT_EQ(standing.pairModifier(doc, player), 80);
        // What one thinks of the other is not what the other thinks of it
        EXPECT_EQ(standing.pairModifier(player, doc), 0);
        standing.clear();
        EXPECT_EQ(standing.pairModifier(doc, player), 0);
    }

    TEST(FalloutScriptStandingTest, a_faction_thinks_of_another_what_a_script_made_of_it_or_else_the_record_says)
    {
        FakeStandingRecords records;
        records.mReactions[{ town, rangers }] = 25;
        Standing standing(records);
        EXPECT_EQ(standing.factionReaction(town, rangers), 25);
        EXPECT_EQ(standing.factionReaction(rangers, town), 0);
        standing.modFactionReaction(town, rangers, -30);
        EXPECT_EQ(standing.factionReaction(town, rangers), -5);
        standing.setFactionReaction(town, rangers, 7);
        EXPECT_EQ(standing.factionReaction(town, rangers), 7);
        standing.setFactionReaction(gang, gang, -40);
        EXPECT_EQ(standing.factionReaction(gang, gang), -40);
    }

    TEST(FalloutScriptStandingTest, a_faction_the_two_share_counts_over_the_others_and_the_lowest_cross_reaction_wins)
    {
        FakeStandingRecords records;
        Standing standing(records);
        standing.setFactionReaction(town, town, 30);
        standing.setFactionReaction(rangers, rangers, 55);
        standing.setFactionReaction(town, gang, -20);
        standing.setFactionReaction(rangers, gang, -60);
        // The highest of the factions they share
        auto result = standing.pairReaction({ town, rangers }, { rangers, town });
        EXPECT_TRUE(result.mFound);
        EXPECT_EQ(result.mReaction, 55);
        // Nothing shared: the lowest of the pairs
        result = standing.pairReaction({ town, rangers }, { gang });
        EXPECT_TRUE(result.mFound);
        EXPECT_EQ(result.mReaction, -60);
        result = standing.pairReaction({}, { gang });
        EXPECT_FALSE(result.mFound);
        EXPECT_EQ(result.mReaction, 0);
        result = standing.pairReaction({ 0 }, { 0 });
        EXPECT_FALSE(result.mFound);
    }

    TEST(FalloutScriptStandingTest, the_class_of_a_karma_goes_by_the_limits_of_the_settings)
    {
        const StandingSettings settings;
        EXPECT_EQ(Standing::karmaClass(settings, 0), 1);
        EXPECT_EQ(Standing::karmaClass(settings, 249), 1);
        EXPECT_EQ(Standing::karmaClass(settings, -249), 1);
        EXPECT_EQ(Standing::karmaClass(settings, 250), 0);
        EXPECT_EQ(Standing::karmaClass(settings, 749), 0);
        EXPECT_EQ(Standing::karmaClass(settings, 750), 3);
        EXPECT_EQ(Standing::karmaClass(settings, -250), 2);
        EXPECT_EQ(Standing::karmaClass(settings, -749), 2);
        EXPECT_EQ(Standing::karmaClass(settings, -750), 4);
        EXPECT_EQ(Standing::karmaClass(settings, -1000), 4);
    }

    TEST(FalloutScriptStandingTest, a_character_is_evil_when_it_has_factions_and_all_of_them_are)
    {
        const auto evil = [](FormId faction) { return faction == gang; };
        EXPECT_TRUE(Standing::actorEvil({ gang }, evil));
        EXPECT_TRUE(Standing::actorEvil({ gang, gang }, evil));
        EXPECT_FALSE(Standing::actorEvil({ gang, town }, evil));
        EXPECT_FALSE(Standing::actorEvil({}, evil));
    }

    TEST(FalloutScriptStandingTest, the_settings_start_as_the_executables_have_them_and_the_records_change_them)
    {
        const StandingSettings defaults;
        EXPECT_DOUBLE_EQ(defaults.mCharMult, 5.0);
        EXPECT_DOUBLE_EQ(defaults.mKarmaMult, 0.002);
        EXPECT_DOUBLE_EQ(defaults.mResponsibilityBase, -50.0);
        EXPECT_EQ(defaults.mFriendValue, 50);
        const std::map<std::string, double> values{ { "fDispTargetCharMult", 10 }, { "iDialogueDispositionFriendValue", 60 },
            { "fAlignEvilMaxKarma", -300 }, { "fDispTargetWeaponBase", -20 }, { "fDispTargetFactionRankBase", 2 },
            { "fDispTargetCharBase", 3 }, { "fDispTargetCharOffset", 4 }, { "fDispTargetKarmaMult", 0.5 },
            { "fDispActorRespBase", -10 }, { "fDispActorRespMult", 2 }, { "fDispTargetRaceMult", 3 },
            { "fDispTargetFactionMult", 2 }, { "fDispTargetDiseaseBase", -1 }, { "fAlignVeryEvilMaxKarma", -800 },
            { "fAlignGoodMinKarma", 300 }, { "fAlignVeryGoodMinKarma", 800 } };
        const StandingSettings changed
            = StandingSettings::fromGameSettings([&values](std::string_view name) -> std::optional<double> {
                  const auto found = values.find(std::string(name));
                  if (found == values.end())
                      return std::nullopt;
                  return found->second;
              });
        EXPECT_DOUBLE_EQ(changed.mCharMult, 10.0);
        EXPECT_EQ(changed.mFriendValue, 60);
        EXPECT_DOUBLE_EQ(changed.mEvilMax, -300.0);
        EXPECT_DOUBLE_EQ(changed.mWeaponBase, -20.0);
        EXPECT_DOUBLE_EQ(changed.mFactionRankBase, 2.0);
        EXPECT_DOUBLE_EQ(changed.mCharBase, 3.0);
        EXPECT_DOUBLE_EQ(changed.mCharOffset, 4.0);
        EXPECT_DOUBLE_EQ(changed.mKarmaMult, 0.5);
        EXPECT_DOUBLE_EQ(changed.mResponsibilityBase, -10.0);
        EXPECT_DOUBLE_EQ(changed.mResponsibilityMult, 2.0);
        EXPECT_DOUBLE_EQ(changed.mRaceMult, 3.0);
        EXPECT_DOUBLE_EQ(changed.mFactionMult, 2.0);
        EXPECT_DOUBLE_EQ(changed.mDiseaseBase, -1.0);
        EXPECT_DOUBLE_EQ(changed.mVeryEvilMax, -800.0);
        EXPECT_DOUBLE_EQ(changed.mGoodMin, 300.0);
        EXPECT_DOUBLE_EQ(changed.mVeryGoodMin, 800.0);
        EXPECT_DOUBLE_EQ(StandingSettings::fromGameSettings({}).mCharMult, 5.0);
    }

    TEST(FalloutScriptStandingTest, the_standing_survives_a_save)
    {
        FakeStandingRecords records;
        Standing standing(records);
        standing.modifyDisposition(doc, player, 12, 40);
        standing.setFactionReaction(town, gang, -8);
        FalloutScript::SaveWriter out;
        FalloutScript::writeStanding(standing, out);
        const std::string bytes = out.take();

        Standing again(records);
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readStanding(again, in, FalloutScript::standingSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(again.pairModifier(doc, player), 12);
        EXPECT_EQ(again.factionReaction(town, gang), -8);

        Standing missing(records);
        FalloutScript::SaveReader gone(bytes, [](FormId form) -> std::optional<FormId> {
            return form == doc ? std::nullopt : std::optional<FormId>(form);
        });
        FalloutScript::readStanding(missing, gone, FalloutScript::standingSectionVersion);
        EXPECT_EQ(missing.pairModifier(doc, player), 0);
        EXPECT_EQ(missing.factionReaction(town, gang), -8);
        FalloutScript::SaveReader cut(std::string_view(bytes).substr(0, 6));
        EXPECT_THROW(FalloutScript::readStanding(missing, cut, FalloutScript::standingSectionVersion), FalloutScript::SaveError);
    }

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FakeStandingRecords mRecords;
        Standing mStanding{ mRecords };
        FakeActorRecords mActorRecords;
        FalloutScript::ActorState mActors{ mActorRecords };
        FakeTraits mTraits;
        FakeWorld mWorld;

        Runner()
        {
            DispositionInput input;
            input.mBase = 40;
            input.mCharisma = 5;
            mWorld.mInputs[doc] = input;
            FalloutScript::addStandingCommands(mInterpreter, mStanding, mRecords, mWorld, mActors, mTraits);
        }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    constexpr std::uint16_t OpGetDisposition = 0x104C;
    constexpr std::uint16_t OpModDisposition = 0x1053;
    constexpr std::uint16_t OpSetDisposition = 0x1184;
    constexpr std::uint16_t OpHasFriendDisposition = 0x1193;
    constexpr std::uint16_t OpFriendDispositionBoost = 0x1195;
    constexpr std::uint16_t OpGetPersuasionNumber = 0x10E1;
    constexpr std::uint16_t OpGetTotalPersuasionNumber = 0x113B;
    constexpr std::uint16_t OpGetFactionRelation = 0x11C2;
    constexpr std::uint16_t OpGetFactionReaction = 0x10A7;
    constexpr std::uint16_t OpSetFactionReaction = 0x10A8;
    constexpr std::uint16_t OpModFactionReaction = 0x10A9;
    constexpr std::uint16_t OpGetFactionRankDifference = 0x103C;
    constexpr std::uint16_t OpModFactionRank = 0x108A;
    constexpr std::uint16_t OpRemoveFromAllFactions = 0x1214;
    constexpr std::uint16_t OpSameFaction = 0x102A;
    constexpr std::uint16_t OpSameFactionAsPC = 0x1085;
    constexpr std::uint16_t OpGetPCInFaction = 0x1084;
    constexpr std::uint16_t OpSameRace = 0x102B;
    constexpr std::uint16_t OpSameSex = 0x102C;
    constexpr std::uint16_t OpSameRaceAsPC = 0x1086;
    constexpr std::uint16_t OpSameSexAsPC = 0x1087;
    constexpr std::uint16_t OpGetIsAlignment = 0x11DA;
    constexpr std::uint16_t OpIsActorEvil = 0x1139;

    TEST(FalloutScriptStandingCommandsTest, the_disposition_is_asked_changed_and_set)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 40.0);
        EXPECT_EQ(run.call(OpGetDisposition, sheriff, { player }), 0.0);
        EXPECT_EQ(run.call(OpGetDisposition, doc, {}), 0.0);
        run.call(OpModDisposition, doc, { player, 15 });
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 55.0);
        run.call(OpModDisposition, doc, { player, 100 });
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 100.0);
        run.call(OpSetDisposition, doc, { player, 20 });
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 20.0);
        run.call(OpSetDisposition, doc, { player, 20 });
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 20.0);
        // Nobody to think of
        run.call(OpModDisposition, doc, { 0, 20 });
        run.call(OpModDisposition, 0, { player, 20 });
        run.call(OpSetDisposition, 0, { player, 20 });
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 20.0);
    }

    TEST(FalloutScriptStandingCommandsTest, the_perks_adjust_the_disposition_that_is_asked)
    {
        Runner run;
        run.mWorld.mAdjust = 25;
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 65.0);
    }

    TEST(FalloutScriptStandingCommandsTest, a_friend_has_the_disposition_of_the_setting_and_the_boost_takes_it_there)
    {
        Runner run;
        EXPECT_EQ(run.call(OpHasFriendDisposition, doc), 0.0);
        run.call(OpFriendDispositionBoost, doc);
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 50.0);
        EXPECT_EQ(run.call(OpHasFriendDisposition, doc), 1.0);
        // A friend is not boosted any higher
        run.call(OpFriendDispositionBoost, doc);
        EXPECT_EQ(run.call(OpGetDisposition, doc, { player }), 50.0);
        EXPECT_EQ(run.call(OpHasFriendDisposition, 0), 0.0);
        run.call(OpFriendDispositionBoost, 0);
        EXPECT_EQ(run.call(OpGetPersuasionNumber, doc), 0.0);
        EXPECT_EQ(run.call(OpGetTotalPersuasionNumber, doc), 0.0);
    }

    TEST(FalloutScriptStandingCommandsTest, the_reactions_of_the_factions_are_asked_set_and_changed)
    {
        Runner run;
        run.mRecords.mReactions[{ town, gang }] = -30;
        EXPECT_EQ(run.call(OpGetFactionReaction, 0, { town, gang }), -30.0);
        run.call(OpModFactionReaction, 0, { town, gang, 10 });
        EXPECT_EQ(run.call(OpGetFactionReaction, 0, { town, gang }), -20.0);
        run.call(OpSetFactionReaction, 0, { town, gang, 5 });
        EXPECT_EQ(run.call(OpGetFactionReaction, 0, { town, gang }), 5.0);
        EXPECT_EQ(run.call(OpGetFactionReaction, 0, { 0, gang }), 0.0);
        run.call(OpSetFactionReaction, 0, { 0, gang, 5 });
        run.call(OpModFactionReaction, 0, { town, 0, 5 });
        run.mWorld.mRelations[{ doc, raider }] = 1;
        EXPECT_EQ(run.call(OpGetFactionRelation, doc, { raider }), 1.0);
        EXPECT_EQ(run.call(OpGetFactionRelation, doc, { sheriff }), 0.0);
        EXPECT_EQ(run.call(OpGetFactionRelation, doc, {}), 0.0);
    }

    TEST(FalloutScriptStandingCommandsTest, ranks_are_compared_changed_and_cleared)
    {
        Runner run;
        // The sheriff is rank 5 in the town and the doctor 2
        EXPECT_EQ(run.call(OpGetFactionRankDifference, sheriff, { town, doc }), 3.0);
        EXPECT_EQ(run.call(OpGetFactionRankDifference, doc, { town, sheriff }), -3.0);
        // The one that is not in the faction makes the difference 0
        EXPECT_EQ(run.call(OpGetFactionRankDifference, sheriff, { town, raider }), 0.0);
        EXPECT_EQ(run.call(OpGetFactionRankDifference, sheriff, { gang, doc }), 0.0);
        EXPECT_EQ(run.call(OpGetFactionRankDifference, 0, { town, doc }), 0.0);
        run.call(OpModFactionRank, doc, { town, 3 });
        EXPECT_EQ(run.call(OpGetFactionRankDifference, sheriff, { town, doc }), 0.0);
        // The rank does not go below 0, and a faction the character is not in does not change
        run.call(OpModFactionRank, doc, { town, -50 });
        EXPECT_EQ(run.call(OpGetFactionRankDifference, sheriff, { town, doc }), 5.0);
        run.call(OpModFactionRank, doc, { gang, 4 });
        EXPECT_EQ(run.call(OpSameFaction, doc, { raider }), 0.0);
        run.call(OpModFactionRank, 0, { town, 4 });
        run.call(OpRemoveFromAllFactions, sheriff);
        EXPECT_EQ(run.call(OpSameFaction, sheriff, { doc }), 0.0);
        run.call(OpRemoveFromAllFactions, 0);
    }

    TEST(FalloutScriptStandingCommandsTest, characters_that_share_a_faction_race_or_sex_are_the_same_in_it)
    {
        Runner run;
        EXPECT_EQ(run.call(OpSameFaction, doc, { sheriff }), 1.0);
        EXPECT_EQ(run.call(OpSameFaction, doc, { raider }), 0.0);
        EXPECT_EQ(run.call(OpSameFaction, doc, { 0 }), 0.0);
        EXPECT_EQ(run.call(OpSameFactionAsPC, doc), 1.0);
        EXPECT_EQ(run.call(OpSameFactionAsPC, raider), 0.0);
        EXPECT_EQ(run.call(OpGetPCInFaction, 0, { town }), 1.0);
        EXPECT_EQ(run.call(OpGetPCInFaction, 0, { gang }), 0.0);
        EXPECT_EQ(run.call(OpGetPCInFaction, 0, { 0 }), 0.0);
        EXPECT_EQ(run.call(OpSameRace, doc, { sheriff }), 1.0);
        EXPECT_EQ(run.call(OpSameRace, doc, { raider }), 0.0);
        EXPECT_EQ(run.call(OpSameRaceAsPC, sheriff), 1.0);
        EXPECT_EQ(run.call(OpSameRaceAsPC, raider), 0.0);
        EXPECT_EQ(run.call(OpSameSex, doc, { raider }), 1.0);
        EXPECT_EQ(run.call(OpSameSex, doc, { sheriff }), 0.0);
        EXPECT_EQ(run.call(OpSameSexAsPC, sheriff), 1.0);
        EXPECT_EQ(run.call(OpSameSexAsPC, doc), 0.0);
        // A character the records do not know is the same as nothing
        EXPECT_EQ(run.call(OpSameRace, doc, { 0x00200000 }), 0.0);
        EXPECT_EQ(run.call(OpSameSex, 0, { doc }), 0.0);
    }

    TEST(FalloutScriptStandingCommandsTest, the_alignment_and_the_evil_of_a_character_come_from_its_karma_and_factions)
    {
        Runner run;
        run.mWorld.mKarma[doc] = 300;
        run.mWorld.mKarma[raider] = -800;
        run.mWorld.mEvil.insert(raider);
        EXPECT_EQ(run.call(OpGetIsAlignment, doc, { 0 }), 1.0);
        EXPECT_EQ(run.call(OpGetIsAlignment, doc, { 1 }), 0.0);
        EXPECT_EQ(run.call(OpGetIsAlignment, raider, { 4 }), 1.0);
        EXPECT_EQ(run.call(OpGetIsAlignment, sheriff, { 1 }), 1.0);
        EXPECT_EQ(run.call(OpGetIsAlignment, 0, { 1 }), 0.0);
        EXPECT_EQ(run.call(OpIsActorEvil, raider), 1.0);
        EXPECT_EQ(run.call(OpIsActorEvil, doc), 0.0);
        EXPECT_EQ(run.call(OpIsActorEvil, 0), 0.0);
    }
}
