#include "saveloadharness.hpp"

#include <components/esm3/dialoguecondition.hpp>
#include <components/esm3/filter.hpp>
#include <components/esm3/infoorder.hpp>
#include <components/esm3/loaddial.hpp>
#include <components/esm3/loadinfo.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// The topics of the dialogue and the lines of the answers that the characters say. Which of the answers is said
// depends on the conditions that are read here, so a condition that is read in the wrong way changes what a
// character says; the files that somebody has edited must not do worse than leaving the condition out.
namespace ESM
{
    namespace
    {
        using namespace ::testing;
        using namespace ::ESM::SaveLoadTest;

        RefId id(std::string_view value)
        {
            return RefId::stringRefId(value);
        }

        // Reads the condition that is written by the function, the way that the INFO record does.
        template <class Write>
        std::optional<DialogueCondition> loadCondition(Write&& write)
        {
            std::optional<DialogueCondition> result;
            roundTripWith(CurrentContentFormatVersion, write, [&](ESMReader& reader) {
                reader.getSubName();
                result = DialogueCondition::load(reader, id("info"));
            });
            return result;
        }

        // The condition as an edited file may have it: the rule is written as it is, followed by its value.
        std::optional<DialogueCondition> loadRule(std::string_view rule, std::int32_t value = 1)
        {
            return loadCondition([&](ESMWriter& writer) {
                writer.writeHNString("SCVR", rule);
                writer.writeHNT("INTV", value);
            });
        }

        std::optional<DialogueCondition> saveAndLoadCondition(const DialogueCondition& condition)
        {
            return loadCondition([&](ESMWriter& writer) { condition.save(writer); });
        }

        DialogueCondition makeCondition(DialogueCondition::Function function, std::variant<int32_t, float> value,
            DialogueCondition::Comparison comparison = DialogueCondition::Comp_Ge, std::string variable = {},
            std::uint8_t index = 2)
        {
            DialogueCondition condition;
            condition.mFunction = function;
            condition.mValue = value;
            condition.mComparison = comparison;
            condition.mVariable = std::move(variable);
            condition.mIndex = index;
            return condition;
        }

        struct ConditionCase
        {
            DialogueCondition::Function mFunction;
            std::string mVariable;
        };

        struct Esm3DialogueConditionTest : public TestWithParam<ConditionCase>
        {
        };

        TEST_P(Esm3DialogueConditionTest, conditionKeepsItsFunctionItsComparisonItsVariableAndItsValue)
        {
            const ConditionCase& param = GetParam();
            for (const DialogueCondition::Comparison comparison :
                { DialogueCondition::Comp_Eq, DialogueCondition::Comp_Ne, DialogueCondition::Comp_Gt,
                    DialogueCondition::Comp_Ge, DialogueCondition::Comp_Ls, DialogueCondition::Comp_Le })
            {
                const auto loaded
                    = saveAndLoadCondition(makeCondition(param.mFunction, int32_t{ 7 }, comparison, param.mVariable));
                ASSERT_TRUE(loaded.has_value());
                EXPECT_EQ(loaded->mFunction, param.mFunction);
                EXPECT_EQ(loaded->mComparison, comparison);
                EXPECT_EQ(loaded->mVariable, param.mVariable);
                EXPECT_EQ(loaded->mIndex, 2);
                EXPECT_EQ(std::get<int32_t>(loaded->mValue), 7);
            }
        }

        TEST_P(Esm3DialogueConditionTest, conditionKeepsAValueThatDoesNotFitAShort)
        {
            const auto loaded = saveAndLoadCondition(makeCondition(
                GetParam().mFunction, int32_t{ 70000 }, DialogueCondition::Comp_Ls, GetParam().mVariable));
            ASSERT_TRUE(loaded.has_value());
            EXPECT_EQ(std::get<int32_t>(loaded->mValue), 70000);
        }

        TEST_P(Esm3DialogueConditionTest, conditionKeepsAFloatingPointValue)
        {
            const auto loaded = saveAndLoadCondition(
                makeCondition(GetParam().mFunction, 2.5f, DialogueCondition::Comp_Ne, GetParam().mVariable, 5));
            ASSERT_TRUE(loaded.has_value());
            EXPECT_EQ(loaded->mIndex, 5);
            EXPECT_EQ(std::get<float>(loaded->mValue), 2.5f);
        }

        INSTANTIATE_TEST_SUITE_P(Functions, Esm3DialogueConditionTest,
            Values(ConditionCase{ DialogueCondition::Function_FacReactionLowest, "" },
                ConditionCase{ DialogueCondition::Function_RankRequirement, "" },
                ConditionCase{ DialogueCondition::Function_PcLevel, "" },
                ConditionCase{ DialogueCondition::Function_PcStrength, "" },
                ConditionCase{ DialogueCondition::Function_PcWerewolfKills, "" },
                ConditionCase{ DialogueCondition::Function_Global, "wolf_state" },
                ConditionCase{ DialogueCondition::Function_Local, "state" },
                ConditionCase{ DialogueCondition::Function_Journal, "a1_1_findspymaster" },
                ConditionCase{ DialogueCondition::Function_Item, "gold_001" },
                ConditionCase{ DialogueCondition::Function_Dead, "fargoth" },
                ConditionCase{ DialogueCondition::Function_NotId, "ra'virr" },
                ConditionCase{ DialogueCondition::Function_NotFaction, "Fighters Guild" },
                ConditionCase{ DialogueCondition::Function_NotClass, "Thief" },
                ConditionCase{ DialogueCondition::Function_NotRace, "Khajiit" },
                ConditionCase{ DialogueCondition::Function_NotCell, "Seyda Neen" },
                ConditionCase{ DialogueCondition::Function_NotLocal, "done" }));

        struct InvalidRule
        {
            const char* mName;
            std::string mRule;
        };

        struct Esm3DialogueInvalidConditionTest : public TestWithParam<InvalidRule>
        {
        };

        TEST_P(Esm3DialogueInvalidConditionTest, conditionThatCannotBeUnderstoodIsLeftOut)
        {
            EXPECT_FALSE(loadRule(GetParam().mRule).has_value());
        }

        INSTANTIATE_TEST_SUITE_P(Rules, Esm3DialogueInvalidConditionTest,
            Values(InvalidRule{ "TooShort", "0107" }, InvalidRule{ "EmptyRule", "" },
                InvalidRule{ "ComparisonBelowTheRange", "0107/" }, InvalidRule{ "ComparisonAboveTheRange", "01076" },
                InvalidRule{ "NoSuchFunction", "01990" }, InvalidRule{ "NegativeFunction", "01-10" },
                InvalidRule{ "NoSuchKindOfFunction", "0Z000x" }, InvalidRule{ "KindOfFunctionZero", "00000x" },
                InvalidRule{ "VariableIsMissing", "02fX0" }, InvalidRule{ "VariableIsMissingInTheJournal", "04JX0" }),
            [](const TestParamInfo<InvalidRule>& info) { return std::string(info.param.mName); });

        struct MalformedRule
        {
            const char* mName;
            std::string mRule;
            DialogueCondition::Function mFunction;
        };

        struct Esm3DialogueMalformedConditionTest : public TestWithParam<MalformedRule>
        {
        };

        // The rules that have the wrong letters are still read, since the game does not look at those letters.
        TEST_P(Esm3DialogueMalformedConditionTest, conditionWithTheWrongLettersIsStillRead)
        {
            const auto loaded = loadRule(GetParam().mRule + "name");
            ASSERT_TRUE(loaded.has_value());
            EXPECT_EQ(loaded->mFunction, GetParam().mFunction);
            EXPECT_EQ(loaded->mVariable, "name");
        }

        INSTANTIATE_TEST_SUITE_P(Rules, Esm3DialogueMalformedConditionTest,
            Values(MalformedRule{ "GlobalWithAnotherType", "02qX0", DialogueCondition::Function_Global },
                MalformedRule{ "GlobalWithAnotherEnd", "02fY0", DialogueCondition::Function_Global },
                MalformedRule{ "LocalWithAnotherType", "03qX0", DialogueCondition::Function_Local },
                MalformedRule{ "JournalWithAnotherType", "04qX0", DialogueCondition::Function_Journal },
                MalformedRule{ "ItemWithAnotherType", "05qX0", DialogueCondition::Function_Item },
                MalformedRule{ "DeadWithAnotherType", "06qX0", DialogueCondition::Function_Dead },
                MalformedRule{ "IdWithAnotherType", "07qX0", DialogueCondition::Function_NotId },
                MalformedRule{ "FactionWithAnotherType", "08qX0", DialogueCondition::Function_NotFaction },
                MalformedRule{ "ClassWithAnotherType", "09qX0", DialogueCondition::Function_NotClass },
                MalformedRule{ "RaceWithAnotherType", "0AqX0", DialogueCondition::Function_NotRace },
                MalformedRule{ "CellWithAnotherType", "0BqX0", DialogueCondition::Function_NotCell },
                MalformedRule{ "NotLocalWithAnotherType", "0CqX0", DialogueCondition::Function_NotLocal }),
            [](const TestParamInfo<MalformedRule>& info) { return std::string(info.param.mName); });

        TEST(Esm3DialogueConditionLoadingTest, conditionWithoutADigitAsItsIndexHasTheFirstIndex)
        {
            const auto loaded = loadRule("x1072");
            ASSERT_TRUE(loaded.has_value());
            EXPECT_EQ(loaded->mIndex, 0);
            EXPECT_EQ(loaded->mFunction, DialogueCondition::Function_PcHealthPercent);
            EXPECT_EQ(loaded->mComparison, DialogueCondition::Comp_Gt);
        }

        TEST(Esm3DialogueConditionLoadingTest, theValueOfAConditionMayBeAFloat)
        {
            const auto loaded = loadCondition([](ESMWriter& writer) {
                writer.writeHNString("SCVR", "01072");
                writer.writeHNT("FLTV", 0.75f);
            });
            ASSERT_TRUE(loaded.has_value());
            EXPECT_EQ(std::get<float>(loaded->mValue), 0.75f);
        }

        TEST(Esm3DialogueConditionLoadingTest, theValueOfAConditionHasToBeANumber)
        {
            EXPECT_THROW(loadCondition([](ESMWriter& writer) {
                writer.writeHNString("SCVR", "01072");
                writer.writeHNT("STRV", 1);
            }),
                std::runtime_error);
        }

        // The state of the dialogue that is kept with every line of an answer.
        DialInfo makeInfo(std::string_view name, std::string_view previous = {}, std::string_view next = {})
        {
            DialInfo info;
            info.mId = id(name);
            info.mPrev = id(previous);
            info.mNext = id(next);
            info.mData.mType = Dialogue::Topic;
            info.mData.mDisposition = 30;
            info.mData.mRank = 2;
            info.mData.mGender = DialInfo::Female;
            info.mData.mPCrank = 1;
            info.mActor = id("fargoth");
            info.mRace = id("Dark Elf");
            info.mClass = id("Thief");
            info.mFaction = id("Thieves Guild");
            info.mPcFaction = id("Fighters Guild");
            info.mCell = id("Seyda Neen");
            info.mSound = "Vo\\f\\Hlo_Fargoth.mp3";
            info.mResponse = "Hello, outlander.";
            info.mResultScript = "Journal A1_1 10";
            info.mFactionLess = false;
            info.mQuestStatus = DialInfo::QS_None;
            info.mSelects.push_back(makeCondition(DialogueCondition::Function_PcLevel, int32_t{ 5 }));
            info.mSelects.push_back(makeCondition(DialogueCondition::Function_Journal, int32_t{ 10 },
                DialogueCondition::Comp_Eq, "a1_1_findspymaster", 3));
            return info;
        }

        struct Esm3InfoTest : public TestWithParam<FormatVersion>
        {
        };

        TEST_P(Esm3InfoTest, infoKeepsItsConditionsItsLinksAndWhatTheCharacterSays)
        {
            const DialInfo record = makeInfo("1234", "1000", "2000");

            DialInfo result;
            bool deleted = true;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_FALSE(deleted);
            EXPECT_EQ(result.mId, id("1234"));
            EXPECT_EQ(result.mPrev, id("1000"));
            EXPECT_EQ(result.mNext, id("2000"));
            EXPECT_EQ(result.mData.mDisposition, 30);
            EXPECT_EQ(result.mData.mRank, 2);
            EXPECT_EQ(static_cast<int>(result.mData.mGender), static_cast<int>(DialInfo::Female));
            EXPECT_EQ(result.mActor, id("fargoth"));
            EXPECT_EQ(result.mRace, id("Dark Elf"));
            EXPECT_EQ(result.mClass, id("Thief"));
            EXPECT_EQ(result.mFaction, id("Thieves Guild"));
            EXPECT_EQ(result.mPcFaction, id("Fighters Guild"));
            EXPECT_EQ(result.mCell, id("Seyda Neen"));
            EXPECT_EQ(result.mSound, record.mSound);
            EXPECT_EQ(result.mResponse, "Hello, outlander.");
            EXPECT_EQ(result.mResultScript, "Journal A1_1 10");
            EXPECT_FALSE(result.mFactionLess);
            ASSERT_EQ(result.mSelects.size(), 2u);
            EXPECT_EQ(result.mSelects[0].mFunction, DialogueCondition::Function_PcLevel);
            EXPECT_EQ(result.mSelects[1].mVariable, "a1_1_findspymaster");
        }

        TEST_P(Esm3InfoTest, infoKeepsWhichQuestItWillStartFinishOrRestart)
        {
            for (const DialInfo::QuestStatus status :
                { DialInfo::QS_None, DialInfo::QS_Name, DialInfo::QS_Finished, DialInfo::QS_Restart })
            {
                DialInfo record = makeInfo("1234");
                record.mQuestStatus = status;

                DialInfo result;
                bool deleted = false;
                roundTripWith(
                    GetParam(), [&](ESMWriter& writer) { record.save(writer); },
                    [&](ESMReader& reader) { result.load(reader, deleted); });

                EXPECT_EQ(result.mQuestStatus, status);
            }
        }

        TEST_P(Esm3InfoTest, infoOfCharactersWithoutAFactionKeepsThatInTheFlag)
        {
            DialInfo record = makeInfo("1234");
            record.mFaction = id("FFFF");

            DialInfo result;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(result.mFactionLess);
        }

        TEST_P(Esm3InfoTest, deletedInfoKeepsItsLinks)
        {
            const DialInfo record = makeInfo("1234", "1000", "2000");

            DialInfo result;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mId, id("1234"));
            EXPECT_EQ(result.mPrev, id("1000"));
            EXPECT_EQ(result.mNext, id("2000"));
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3InfoTest, ValuesIn(getFormats()));

        TEST(Esm3InfoLoadingTest, conditionsThatCannotBeUnderstoodAreLeftOutOfTheInfo)
        {
            DialInfo result;
            bool deleted = false;
            roundTripWith(
                CurrentContentFormatVersion,
                [&](ESMWriter& writer) {
                    writer.writeHNCRefId("INAM", id("5"));
                    writer.writeHNCRefId("PNAM", RefId());
                    writer.writeHNCRefId("NNAM", RefId());
                    writer.writeHNString("SCVR", "0Z000");
                    writer.writeHNT("INTV", 1);
                    makeCondition(DialogueCondition::Function_PcLevel, int32_t{ 4 }).save(writer);
                },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            ASSERT_EQ(result.mSelects.size(), 1u);
            EXPECT_EQ(result.mSelects[0].mFunction, DialogueCondition::Function_PcLevel);
        }

        TEST(Esm3InfoLoadingTest, infoWithAnUnknownSubrecordFails)
        {
            DialInfo result;
            bool deleted = false;
            try
            {
                roundTripWith(
                    CurrentContentFormatVersion,
                    [&](ESMWriter& writer) {
                        writer.writeHNCRefId("INAM", id("5"));
                        writer.writeHNCRefId("PNAM", RefId());
                        writer.writeHNCRefId("NNAM", RefId());
                        writer.writeHNT("ZZZZ", 1);
                    },
                    [&](ESMReader& reader) { result.load(reader, deleted); });
                FAIL() << "Expected an exception";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord"));
            }
        }

        TEST(Esm3InfoLoadingTest, blankInfoAnswersNobodyInParticular)
        {
            DialInfo info = makeInfo("1234", "1000", "2000");
            info.mQuestStatus = DialInfo::QS_Finished;
            info.mFactionLess = true;
            info.blank();

            EXPECT_TRUE(info.mSelects.empty());
            EXPECT_TRUE(info.mPrev.empty());
            EXPECT_TRUE(info.mNext.empty());
            EXPECT_TRUE(info.mActor.empty());
            EXPECT_TRUE(info.mFaction.empty());
            EXPECT_TRUE(info.mCell.empty());
            EXPECT_TRUE(info.mSound.empty());
            EXPECT_TRUE(info.mResponse.empty());
            EXPECT_TRUE(info.mResultScript.empty());
            EXPECT_FALSE(info.mFactionLess);
            EXPECT_EQ(info.mQuestStatus, DialInfo::QS_None);
            EXPECT_EQ(static_cast<int>(info.mData.mGender), static_cast<int>(DialInfo::NA));
        }

        struct Esm3DialogueTest : public TestWithParam<FormatVersion>
        {
        };

        Dialogue makeDialogue(Dialogue::Type type = Dialogue::Journal)
        {
            Dialogue dialogue;
            dialogue.blank();
            dialogue.mStringId = "a1_1_findspymaster";
            dialogue.mId = id(dialogue.mStringId);
            dialogue.mType = type;
            return dialogue;
        }

        TEST_P(Esm3DialogueTest, topicKeepsItsNameAndItsType)
        {
            const Dialogue record = makeDialogue(Dialogue::Greeting);

            Dialogue result;
            bool deleted = true;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_FALSE(deleted);
            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mStringId, record.mStringId);
            EXPECT_EQ(result.mType, Dialogue::Greeting);
        }

        TEST_P(Esm3DialogueTest, deletedTopicHasNoType)
        {
            const Dialogue record = makeDialogue();

            Dialogue result;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mId, record.mId);
            EXPECT_EQ(result.mType, Dialogue::Unknown);
        }

        // The lines of the answers that are read after the topic, in the order of the links between them.
        std::vector<std::string> readInfos(FormatVersion format, const std::vector<std::pair<DialInfo, bool>>& infos)
        {
            ESMWriter writer;
            auto stream = std::make_unique<std::stringstream>();
            writer.setFormatVersion(format);
            writer.save(*stream);
            for (const auto& [info, deleted] : infos)
            {
                writer.startRecord(fakeRecordId);
                info.save(writer, deleted);
                writer.endRecord(fakeRecordId);
            }

            ESMReader reader;
            reader.open(std::move(stream), "stream");
            Dialogue dialogue = makeDialogue();
            while (reader.hasMoreRecs())
            {
                reader.getRecName();
                reader.getRecHeader();
                dialogue.readInfo(reader);
            }
            dialogue.setUp();

            std::vector<std::string> ids;
            for (const DialInfo& info : dialogue.mInfo)
                ids.push_back(info.mId.getRefIdString());
            return ids;
        }

        TEST_P(Esm3DialogueTest, linesAreOrderedByTheLinksAndTheDeletedOnesAreDropped)
        {
            const auto ids = readInfos(GetParam(),
                { { makeInfo("c", "b", ""), false }, { makeInfo("a", "", "b"), false },
                    { makeInfo("d", "c", ""), true }, { makeInfo("b", "a", "c"), false } });

            EXPECT_THAT(ids, ElementsAre("a", "b", "c"));
        }

        TEST_P(Esm3DialogueTest, lineThatIsLoadedAgainReplacesTheEarlierOne)
        {
            DialInfo replacement = makeInfo("b", "a", "c");
            replacement.mResponse = "A new answer.";

            ESMWriter writer;
            auto stream = std::make_unique<std::stringstream>();
            writer.setFormatVersion(GetParam());
            writer.save(*stream);
            for (const DialInfo& info : { makeInfo("a", "", "b"), makeInfo("b", "a", "c"), replacement })
            {
                writer.startRecord(fakeRecordId);
                info.save(writer);
                writer.endRecord(fakeRecordId);
            }

            ESMReader reader;
            reader.open(std::move(stream), "stream");
            Dialogue dialogue = makeDialogue();
            while (reader.hasMoreRecs())
            {
                reader.getRecName();
                reader.getRecHeader();
                dialogue.readInfo(reader);
            }
            dialogue.setUp();

            ASSERT_EQ(dialogue.mInfo.size(), 2u);
            EXPECT_EQ(dialogue.mInfo.back().mResponse, "A new answer.");
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3DialogueTest, ValuesIn(getFormats()));

        TEST(Esm3DialogueLoadingTest, topicOfAnotherSizeOfTypeHasAnUnknownType)
        {
            Dialogue result;
            bool deleted = false;
            roundTripWith(
                MaxStringRefIdFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNCString("NAME", "topic");
                    writer.writeHNT("DATA", std::int32_t{ 4 });
                },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_EQ(result.mType, Dialogue::Unknown);
            EXPECT_EQ(result.mId, id("topic"));
        }

        TEST(Esm3DialogueLoadingTest, topicWithAnUnknownSubrecordFails)
        {
            Dialogue result;
            bool deleted = false;
            try
            {
                roundTripWith(
                    MaxStringRefIdFormatVersion,
                    [](ESMWriter& writer) {
                        writer.writeHNCString("NAME", "topic");
                        writer.writeHNT("ZZZZ", 1);
                    },
                    [&](ESMReader& reader) { result.load(reader, deleted); });
                FAIL() << "Expected an exception";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord"));
            }
        }

        TEST(Esm3DialogueLoadingTest, topicWhoseNameDiffersFromItsIdCannotBeSavedInAnOldFormat)
        {
            Dialogue record = makeDialogue();
            record.mStringId = "another name";

            ESMWriter writer;
            writer.setFormatVersion(MaxStringRefIdFormatVersion);
            std::stringstream stream;
            writer.save(stream);
            writer.startRecord(fakeRecordId);
            EXPECT_THROW(record.save(writer), std::runtime_error);
        }

        TEST(Esm3DialogueLoadingTest, theNameOfTheTopicTakesTheIdInTheFormatsThatDoNotSaveItTwice)
        {
            Dialogue result;
            bool deleted = false;
            roundTripWith(
                MaxNameIsRefIdOnlyFormatVersion,
                [](ESMWriter& writer) {
                    writer.writeHNCRefId("NAME", id("Journal Topic"));
                    writer.writeHNT("DATA", std::int8_t{ 4 });
                },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_EQ(result.mId, id("Journal Topic"));
            EXPECT_EQ(result.mStringId, "Journal Topic");
            EXPECT_EQ(result.mType, Dialogue::Journal);
        }

        struct Esm3FilterTest : public TestWithParam<FormatVersion>
        {
        };

        TEST_P(Esm3FilterTest, filterKeepsItsConditionAndItsDescription)
        {
            Filter record;
            record.blank();
            record.mId = id("filter_barter");
            record.mFilter = "Items(Weapon)";
            record.mDescription = "Everything that cuts";

            Filter result;
            bool deleted = true;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_FALSE(deleted);
            EXPECT_EQ(result.mId, id("filter_barter"));
            EXPECT_EQ(result.mFilter, "Items(Weapon)");
            EXPECT_EQ(result.mDescription, "Everything that cuts");
        }

        TEST_P(Esm3FilterTest, deletedFilterKeepsItsId)
        {
            Filter record;
            record.blank();
            record.mId = id("filter_barter");

            Filter result;
            bool deleted = false;
            roundTripWith(
                GetParam(), [&](ESMWriter& writer) { record.save(writer, true); },
                [&](ESMReader& reader) { result.load(reader, deleted); });

            EXPECT_TRUE(deleted);
            EXPECT_EQ(result.mId, id("filter_barter"));
        }

        INSTANTIATE_TEST_SUITE_P(FormatVersions, Esm3FilterTest, ValuesIn(getFormats()));

        TEST(Esm3FilterLoadingTest, filterWithAnUnknownSubrecordFails)
        {
            Filter result;
            bool deleted = false;
            try
            {
                roundTripWith(
                    CurrentContentFormatVersion,
                    [](ESMWriter& writer) {
                        writer.writeHNCRefId("NAME", id("filter_barter"));
                        writer.writeHNT("ZZZZ", 1);
                    },
                    [&](ESMReader& reader) { result.load(reader, deleted); });
                FAIL() << "Expected an exception";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("Unknown subrecord"));
            }
        }

        // The small types that the order of the answers is built of.
        struct Link
        {
            RefId mId;
            RefId mPrev;
            int mValue = 0;
        };

        std::vector<std::string> ids(const InfoOrder<Link>& order)
        {
            std::vector<std::string> result;
            for (const Link& link : order.getOrderedInfo())
                result.push_back(link.mId.getRefIdString());
            return result;
        }

        TEST(Esm3InfoOrderTest, infoIsPlacedAfterTheOneItFollows)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("c"), id("b") }, false);
            order.insertInfo(Link{ id("b"), id("a") }, false);

            EXPECT_THAT(ids(order), ElementsAre("a", "b", "c"));
        }

        TEST(Esm3InfoOrderTest, infoThatFollowsAnUnknownOneGoesToTheEnd)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("b"), id("missing") }, false);

            EXPECT_THAT(ids(order), ElementsAre("a", "b"));
        }

        TEST(Esm3InfoOrderTest, infoWithoutAPredecessorGoesToTheStart)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("b"), RefId() }, false);

            EXPECT_THAT(ids(order), ElementsAre("b", "a"));
        }

        TEST(Esm3InfoOrderTest, infoThatIsInsertedAgainWithTheSamePredecessorIsReplacedInPlace)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId(), 1 }, false);
            order.insertInfo(Link{ id("b"), id("a"), 2 }, false);
            order.insertInfo(Link{ id("a"), RefId(), 3 }, false);

            EXPECT_THAT(ids(order), ElementsAre("a", "b"));
            EXPECT_EQ(order.getOrderedInfo().front().mValue, 3);
        }

        TEST(Esm3InfoOrderTest, infoThatIsInsertedAgainWithAnotherPredecessorIsMoved)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("b"), id("a") }, false);
            order.insertInfo(Link{ id("c"), id("b") }, false);
            order.insertInfo(Link{ id("a"), id("c"), 9 }, false);

            EXPECT_THAT(ids(order), ElementsAre("b", "c", "a"));
            EXPECT_EQ(order.getOrderedInfo().back().mValue, 9);
        }

        TEST(Esm3InfoOrderTest, infoThatIsMovedToTheStartHasNoPredecessor)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("b"), id("a") }, false);
            order.insertInfo(Link{ id("b"), RefId() }, false);

            EXPECT_THAT(ids(order), ElementsAre("b", "a"));
        }

        TEST(Esm3InfoOrderTest, infoCanBeRemoved)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("b"), id("a") }, false);

            order.removeInfo(id("a"));
            order.removeInfo(id("unknown"));

            EXPECT_THAT(ids(order), ElementsAre("b"));
        }

        TEST(Esm3InfoOrderTest, deletedInfoIsRemovedAtOnceAndTheRestIsExtracted)
        {
            InfoOrder<Link> order;
            order.insertInfo(Link{ id("a"), RefId() }, false);
            order.insertInfo(Link{ id("b"), id("a") }, true);
            order.insertInfo(Link{ id("c"), id("b") }, false);

            order.removeDeleted();
            std::list<Link> extracted;
            order.extractOrderedInfo(extracted);

            ASSERT_EQ(extracted.size(), 2u);
            EXPECT_EQ(extracted.front().mId, id("a"));
            EXPECT_EQ(extracted.back().mId, id("c"));
        }
    }
}
