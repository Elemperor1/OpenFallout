#include <components/esm4/census.hpp>
#include <components/esm4/common.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>

#include "syntheticplugin.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    using namespace testing;
    using namespace ESM4Test;

    /// Build an enabled SCHR subrecord with the supplied reference, bytecode and variable counts.
    std::string scriptHeader(
        std::uint32_t refCount, std::uint32_t compiledSize, std::uint32_t variableCount, std::uint16_t type = 0)
    {
        std::string data;
        append<std::uint32_t>(data, 0);
        append(data, refCount);
        append(data, compiledSize);
        append(data, variableCount);
        append(data, type);
        append<std::uint16_t>(data, 1); // enabled
        return subRecord("SCHR", data);
    }

    /// Build an SLSD local variable followed by the SCVR subrecord containing its name.
    std::string localVariable(std::uint32_t index, std::string_view name)
    {
        std::string data;
        append(data, index);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 1); // type
        append<std::uint32_t>(data, 0);
        return subRecord("SLSD", data) + zString("SCVR", name);
    }

    /// Build a FO3/FONV CTDA condition comparing the given function on the subject to 1.
    std::string condition(std::uint32_t functionIndex)
    {
        std::string data;
        append<std::uint32_t>(data, 0); // equal to
        append<float>(data, 1.f);
        append(data, functionIndex);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0);
        append<std::uint32_t>(data, 0); // run on subject
        append<std::uint32_t>(data, 0); // no reference
        return subRecord("CTDA", data);
    }

    // Bytes with a zero, the top bit set and a newline: nothing that would survive being read as text.
    const std::string bytecode("\x1d\x00\x06\x00\x72\x01\x00\xff\n\x80", 10);

    /// Verify SCPT preserves binary bytecode, source, locals and the mixed reference order.
    TEST(ESM4ScriptTest, keepsEverythingTheBytecodeNames)
    {
        const std::string data = zString("EDID", "TestScript")
            + scriptHeader(3, static_cast<std::uint32_t>(bytecode.size()), 2) + subRecord("SCDA", bytecode)
            + subRecord("SCTX", "ScriptName TestScript") + localVariable(1, "iCount") + localVariable(2, "rTarget")
            + valueSubRecord<std::uint32_t>("SCRO", 0x000a0001) + valueSubRecord<std::uint32_t>("SCRV", 2)
            + valueSubRecord<std::uint32_t>("SCRO", 0x000a0002);

        const std::vector<ESM4::Script> scripts = loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, data));

        ASSERT_EQ(scripts.size(), 1u);
        const ESM4::Script& script = scripts.front();
        EXPECT_EQ(script.mEditorId, "TestScript");
        const ESM4::ScriptDefinition& definition = script.mScript;
        EXPECT_EQ(definition.scriptHeader.compiledSize, bytecode.size());
        EXPECT_EQ(std::string(definition.compiledScript.begin(), definition.compiledScript.end()), bytecode);
        EXPECT_EQ(definition.scriptSource, "ScriptName TestScript");

        ASSERT_EQ(definition.localVarData.size(), 2u);
        EXPECT_EQ(definition.localVarData[0].index, 1u);
        EXPECT_EQ(definition.localVarData[0].variableName, "iCount");
        EXPECT_EQ(definition.localVarData[1].index, 2u);
        EXPECT_EQ(definition.localVarData[1].variableName, "rTarget");

        // The bytecode counts the two kinds of reference together, so their order has to survive.
        ASSERT_EQ(definition.references.size(), 3u);
        EXPECT_FALSE(definition.references[0].isVariable);
        EXPECT_EQ(definition.references[0].formId.mIndex, 0x0a0001u);
        EXPECT_TRUE(definition.references[1].isVariable);
        EXPECT_EQ(definition.references[1].variableIndex, 2u);
        EXPECT_FALSE(definition.references[2].isVariable);
        EXPECT_EQ(definition.references[2].formId.mIndex, 0x0a0002u);

        EXPECT_TRUE(definition.isConsistent());
    }

    /// Verify a source-only script loads with empty bytecode and references and consistent counts.
    TEST(ESM4ScriptTest, readsAScriptWithoutBytecode)
    {
        const std::string data
            = zString("EDID", "Empty") + scriptHeader(0, 0, 0) + subRecord("SCTX", "ScriptName Empty");

        const std::vector<ESM4::Script> scripts = loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, data));

        ASSERT_EQ(scripts.size(), 1u);
        EXPECT_TRUE(scripts.front().mScript.compiledScript.empty());
        EXPECT_TRUE(scripts.front().mScript.references.empty());
        EXPECT_TRUE(scripts.front().mScript.isConsistent());
    }

    /// Verify size, reference and variable inconsistencies are detected independently.
    TEST(ESM4ScriptTest, noticesWhenTheHeaderDisagreesWithTheContent)
    {
        const std::string wrongSize = zString("EDID", "A") + scriptHeader(0, 4, 0) + subRecord("SCDA", bytecode);
        const std::string wrongRefCount
            = zString("EDID", "B") + scriptHeader(2, 0, 0) + valueSubRecord<std::uint32_t>("SCRO", 1);
        const std::string wrongVariableCount
            = zString("EDID", "C") + scriptHeader(0, 0, 1) + localVariable(1, "iOne") + localVariable(2, "iTwo");

        const std::string records
            = record("SCPT", 1, wrongSize) + record("SCPT", 2, wrongRefCount) + record("SCPT", 3, wrongVariableCount);
        const std::vector<ESM4::Script> scripts = loadRecords<ESM4::Script>("SCPT", records);

        ASSERT_EQ(scripts.size(), 3u);
        EXPECT_FALSE(scripts[0].mScript.hasConsistentSize());
        EXPECT_TRUE(scripts[0].mScript.hasConsistentReferences());
        EXPECT_TRUE(scripts[0].mScript.hasConsistentVariables());
        EXPECT_FALSE(scripts[0].mScript.isConsistent());

        EXPECT_TRUE(scripts[1].mScript.hasConsistentSize());
        EXPECT_FALSE(scripts[1].mScript.hasConsistentReferences());
        EXPECT_TRUE(scripts[1].mScript.hasConsistentVariables());
        EXPECT_FALSE(scripts[1].mScript.isConsistent());

        EXPECT_TRUE(scripts[2].mScript.hasConsistentSize());
        EXPECT_TRUE(scripts[2].mScript.hasConsistentReferences());
        EXPECT_FALSE(scripts[2].mScript.hasConsistentVariables());
        EXPECT_FALSE(scripts[2].mScript.isConsistent());
    }

    /// Verify gaps and removed locals are allowed when the header covers the highest variable index.
    TEST(ESM4ScriptTest, acceptsAVariableCountAboveTheNumberOfVariables)
    {
        // The game files have scripts whose header counts more variables than the record lists: the ones deleted
        // from the middle or the end of the list leave their index behind.
        const std::string gaps
            = zString("EDID", "Gaps") + scriptHeader(0, 0, 5) + localVariable(2, "iTwo") + localVariable(4, "iFour");
        const std::string none = zString("EDID", "None") + scriptHeader(0, 0, 3);

        const std::vector<ESM4::Script> scripts
            = loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, gaps) + record("SCPT", 2, none));

        ASSERT_EQ(scripts.size(), 2u);
        EXPECT_EQ(scripts[0].mScript.localVarData.size(), 2u);
        EXPECT_EQ(scripts[0].mScript.highestVariableIndex(), 4u);
        EXPECT_TRUE(scripts[0].mScript.isConsistent());
        EXPECT_EQ(scripts[1].mScript.highestVariableIndex(), 0u);
        EXPECT_TRUE(scripts[1].mScript.isConsistent());
    }

    /// Verify an unexpected SCHR size is skipped without losing later subrecords or records.
    TEST(ESM4ScriptTest, skipsAScriptHeaderOfAnotherSizeAndKeepsReading)
    {
        const std::string odd = zString("EDID", "Odd") + subRecord("SCHR", std::string(12, '\x07'))
            + subRecord("SCDA", bytecode) + subRecord("SCTX", "ScriptName Odd");
        const std::string next = zString("EDID", "Next") + scriptHeader(0, 0, 0);

        const std::vector<ESM4::Script> scripts
            = loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, odd) + record("SCPT", 2, next));

        ASSERT_EQ(scripts.size(), 2u);
        EXPECT_EQ(scripts[0].mScript.scriptHeader.compiledSize, 0u);
        EXPECT_EQ(scripts[0].mScript.compiledScript.size(), bytecode.size());
        EXPECT_EQ(scripts[0].mScript.scriptSource, "ScriptName Odd");
        EXPECT_EQ(scripts[1].mEditorId, "Next");
    }

    /// Verify a file that ends inside the SCDA payload raises a loading error.
    TEST(ESM4ScriptTest, rejectsBytecodeThatIsCutShort)
    {
        // The record and the sub-record promise ten bytes of bytecode and the file ends after four.
        const std::string data = zString("EDID", "Cut") + subRecord("SCDA", "0123456789");

        try
        {
            loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, data), 6);
            FAIL() << "the cut-short SCDA was accepted";
        }
        catch (const std::exception& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("SCDA is shorter than its size"));
        }
    }

    /// Verify an SCDA that promises more bytes than its record holds is rejected, not read from the next record.
    TEST(ESM4ScriptTest, rejectsBytecodeThatCrossesItsRecord)
    {
        // The sub-record header promises ten bytes, the record holds four, and another record follows.
        std::string crossing = "SCDA";
        append<std::uint16_t>(crossing, 10);
        crossing.append("\x01\x02\x03\x04", 4);
        const std::string first = zString("EDID", "Cross") + crossing;
        const std::string second = zString("EDID", "Next") + subRecord("SCDA", "ab");

        try
        {
            loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, first) + record("SCPT", 2, second));
            FAIL() << "the SCDA was read across the record boundary";
        }
        catch (const std::exception& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("longer than its record"));
        }
    }

    /// Verify locals and references that are not the size of their fields are rejected, not read past their end.
    TEST(ESM4ScriptTest, rejectsLocalsAndReferencesOfTheWrongSize)
    {
        for (const std::string_view type : { "SLSD", "SCRO", "SCRV" })
        {
            for (const std::string_view payload : { "", "ab", "abcdefghijklmnopqrstuvwxyz" })
            {
                const std::string first = zString("EDID", "Odd") + scriptHeader(1, 2, 1) + subRecord("SCDA", "ab")
                    + subRecord(type, payload);
                const std::string second = zString("EDID", "Next") + subRecord("SCDA", "cd");

                try
                {
                    loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, first) + record("SCPT", 2, second));
                    ADD_FAILURE() << type << " of " << payload.size() << " bytes was accepted";
                }
                catch (const std::exception& e)
                {
                    EXPECT_THAT(e.what(), HasSubstr("unexpected size")) << type << " of " << payload.size() << " bytes";
                }
            }
        }
    }

    /// Verify a file that ends inside a script header, local variable or reference is rejected, not read as zeros.
    TEST(ESM4ScriptTest, rejectsFieldsThatTheFileEndsInside)
    {
        std::string local;
        for (int i = 0; i < 6; ++i)
            append<std::uint32_t>(local, 0x01010101);

        const std::string before = zString("EDID", "Cut");
        const std::string counted = scriptHeader(1, 2, 1) + subRecord("SCDA", "ab");
        const std::vector<std::pair<std::string_view, std::string>> cases = {
            { "SCHR", before + scriptHeader(0, 0, 0) },
            { "SLSD", before + counted + subRecord("SLSD", local) },
            { "SCRO", before + counted + valueSubRecord<std::uint32_t>("SCRO", 0x000a0001) },
            { "SCRV", before + counted + valueSubRecord<std::uint32_t>("SCRV", 2) },
        };

        for (const auto& [type, data] : cases)
        {
            // Two bytes of the last field are missing, as in a file that was cut off.
            try
            {
                loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, data), 2);
                ADD_FAILURE() << type << " cut by two bytes was accepted";
            }
            catch (const std::exception& e)
            {
                EXPECT_THAT(e.what(), HasSubstr("shorter than its size")) << type;
            }
        }
    }

    /// Verify bytecode that ends a compressed record is still read in full.
    TEST(ESM4ScriptTest, readsBytecodeThatEndsACompressedRecord)
    {
        const std::string data = zString("EDID", "Packed")
            + scriptHeader(0, static_cast<std::uint32_t>(bytecode.size()), 0) + subRecord("SCDA", bytecode);

        const std::vector<ESM4::Script> scripts = loadRecords<ESM4::Script>("SCPT", compressedRecord("SCPT", 1, data));

        ASSERT_EQ(scripts.size(), 1u);
        const ESM4::ScriptDefinition& definition = scripts.front().mScript;
        EXPECT_EQ(std::string(definition.compiledScript.begin(), definition.compiledScript.end()), bytecode);
        EXPECT_TRUE(definition.hasConsistentSize());
    }

    /// Verify delegating script subrecords still rejects an unknown SCPT subrecord.
    TEST(ESM4ScriptTest, stillRejectsUnknownSubrecords)
    {
        const std::string data = zString("EDID", "Bad") + subRecord("ZZZZ", "1234");

        EXPECT_ANY_THROW(loadRecords<ESM4::Script>("SCPT", record("SCPT", 1, data)));
    }

    /// Verify NEXT separates INFO begin and end bytecode, source, locals and references.
    TEST(ESM4DialogInfoTest, keepsTheFirstAndSecondScriptApart)
    {
        const std::string firstCode("\x01\x02\x03", 3);
        const std::string secondCode("\x0a\x0b\x0c\x0d\x0e", 5);
        const std::string data = valueSubRecord<std::uint32_t>("QSTI", 0x000b0001)
            + scriptHeader(1, static_cast<std::uint32_t>(firstCode.size()), 1) + subRecord("SCDA", firstCode)
            + subRecord("SCTX", "begin") + localVariable(1, "iFirst")
            + valueSubRecord<std::uint32_t>("SCRO", 0x000a0001) + subRecord("NEXT", "")
            + scriptHeader(2, static_cast<std::uint32_t>(secondCode.size()), 0) + subRecord("SCDA", secondCode)
            + subRecord("SCTX", "end") + valueSubRecord<std::uint32_t>("SCRO", 0x000a0002)
            + valueSubRecord<std::uint32_t>("SCRV", 3);

        const std::vector<ESM4::DialogInfo> infos = loadRecords<ESM4::DialogInfo>("INFO", record("INFO", 1, data));

        ASSERT_EQ(infos.size(), 1u);
        const ESM4::DialogInfo& info = infos.front();
        EXPECT_EQ(info.mQuest.mIndex, 0x0b0001u);

        EXPECT_EQ(std::string(info.mScript.compiledScript.begin(), info.mScript.compiledScript.end()), firstCode);
        EXPECT_EQ(info.mScript.scriptSource, "begin");
        ASSERT_EQ(info.mScript.localVarData.size(), 1u);
        ASSERT_EQ(info.mScript.references.size(), 1u);
        EXPECT_EQ(info.mScript.references[0].formId.mIndex, 0x0a0001u);
        EXPECT_TRUE(info.mScript.isConsistent());

        EXPECT_EQ(
            std::string(info.mEndScript.compiledScript.begin(), info.mEndScript.compiledScript.end()), secondCode);
        EXPECT_EQ(info.mEndScript.scriptSource, "end");
        EXPECT_TRUE(info.mEndScript.localVarData.empty());
        ASSERT_EQ(info.mEndScript.references.size(), 2u);
        EXPECT_EQ(info.mEndScript.references[0].formId.mIndex, 0x0a0002u);
        EXPECT_TRUE(info.mEndScript.references[1].isVariable);
        EXPECT_TRUE(info.mEndScript.isConsistent());
    }

    /// Verify dialogue text loads when neither response script is present.
    TEST(ESM4DialogInfoTest, readsAResponseWithNoScript)
    {
        const std::string data = valueSubRecord<std::uint32_t>("QSTI", 0x000b0001) + zString("NAM1", "Hello");

        const std::vector<ESM4::DialogInfo> infos = loadRecords<ESM4::DialogInfo>("INFO", record("INFO", 1, data));

        ASSERT_EQ(infos.size(), 1u);
        EXPECT_EQ(infos.front().mResponse, "Hello");
        EXPECT_TRUE(infos.front().mScript.compiledScript.empty());
        EXPECT_TRUE(infos.front().mEndScript.compiledScript.empty());
    }

    /// Build quest DATA with flags 1, priority 50 and a five-second delay.
    std::string questData()
    {
        std::string data;
        append<std::uint8_t>(data, 1); // flags
        append<std::uint8_t>(data, 50); // priority
        append<std::uint16_t>(data, 0);
        append<float>(data, 5.f); // quest delay
        return subRecord("DATA", data);
    }

    /// Verify quest stages retain separate log entries, scripts, text, conditions and next quests.
    /// Quest conditions stay separate and objective conditions are skipped.
    TEST(ESM4QuestTest, putsConditionsTextAndScriptsInTheLogEntryTheyBelongTo)
    {
        const std::string firstCode("\x01\x02\x03", 3);
        const std::string thirdCode("\x04\x05", 2);
        std::string objective;
        append<std::int32_t>(objective, 5);
        std::string target;
        append<std::uint32_t>(target, 0x000a0009);
        append<std::uint32_t>(target, 0); // flags and padding

        const std::string data = zString("EDID", "TestQuest") + valueSubRecord<std::uint32_t>("SCRI", 0x000c0001)
            + zString("FULL", "A Test Quest") + questData()
            + condition(100)
            // stage 10, two log entries
            + valueSubRecord<std::int16_t>("INDX", 10) + valueSubRecord<std::uint8_t>("QSDT", 0x01) + condition(200)
            + zString("CNAM", "First entry") + scriptHeader(1, static_cast<std::uint32_t>(firstCode.size()), 0, 1)
            + subRecord("SCDA", firstCode) + valueSubRecord<std::uint32_t>("SCRO", 0x000a0001)
            + valueSubRecord<std::uint32_t>("NAM0", 0x000c0002) + valueSubRecord<std::uint8_t>("QSDT", 0x02)
            + zString("CNAM", "Second entry")
            // stage 20
            + valueSubRecord<std::int16_t>("INDX", 20) + valueSubRecord<std::uint8_t>("QSDT", 0)
            + zString("CNAM", "Third entry") + scriptHeader(1, static_cast<std::uint32_t>(thirdCode.size()), 0, 1)
            + subRecord("SCDA", thirdCode)
            + valueSubRecord<std::uint32_t>("SCRV", 4)
            // objectives
            + subRecord("QOBJ", objective) + zString("NNAM", "Go there") + subRecord("QSTA", target) + condition(300);

        const std::vector<ESM4::Quest> quests = loadRecords<ESM4::Quest>("QUST", record("QUST", 1, data));

        ASSERT_EQ(quests.size(), 1u);
        const ESM4::Quest& quest = quests.front();
        EXPECT_EQ(quest.mEditorId, "TestQuest");
        EXPECT_EQ(quest.mQuestName, "A Test Quest");
        EXPECT_EQ(quest.mQuestScript.mIndex, 0x0c0001u);
        EXPECT_EQ(quest.mData.priority, std::uint8_t(50));

        // Only the condition before the first stage belongs to the quest.
        ASSERT_EQ(quest.mTargetConditions.size(), 1u);
        EXPECT_EQ(quest.mTargetConditions[0].functionIndex, 100u);

        ASSERT_EQ(quest.mStages.size(), 2u);
        const ESM4::QuestStage& first = quest.mStages[0];
        EXPECT_EQ(first.mIndex, 10);
        ASSERT_EQ(first.mLogEntries.size(), 2u);

        const ESM4::QuestLogEntry& entry = first.mLogEntries[0];
        EXPECT_EQ(entry.mFlags, static_cast<std::uint8_t>(ESM4::QuestLogEntry::Flag_CompleteQuest));
        ASSERT_EQ(entry.mTargetConditions.size(), 1u);
        EXPECT_EQ(entry.mTargetConditions[0].functionIndex, 200u);
        EXPECT_EQ(entry.mText, "First entry");
        EXPECT_EQ(std::string(entry.mScript.compiledScript.begin(), entry.mScript.compiledScript.end()), firstCode);
        ASSERT_EQ(entry.mScript.references.size(), 1u);
        EXPECT_EQ(entry.mScript.references[0].formId.mIndex, 0x0a0001u);
        EXPECT_TRUE(entry.mScript.isConsistent());
        EXPECT_EQ(entry.mNextQuest.mIndex, 0x0c0002u);

        const ESM4::QuestLogEntry& second = first.mLogEntries[1];
        EXPECT_EQ(second.mFlags, static_cast<std::uint8_t>(ESM4::QuestLogEntry::Flag_FailQuest));
        EXPECT_EQ(second.mText, "Second entry");
        EXPECT_TRUE(second.mTargetConditions.empty());
        EXPECT_TRUE(second.mScript.compiledScript.empty());
        EXPECT_EQ(second.mNextQuest.mIndex, 0u);

        const ESM4::QuestStage& other = quest.mStages[1];
        EXPECT_EQ(other.mIndex, 20);
        ASSERT_EQ(other.mLogEntries.size(), 1u);
        EXPECT_EQ(other.mLogEntries[0].mText, "Third entry");
        EXPECT_EQ(std::string(other.mLogEntries[0].mScript.compiledScript.begin(),
                      other.mLogEntries[0].mScript.compiledScript.end()),
            thirdCode);
        ASSERT_EQ(other.mLogEntries[0].mScript.references.size(), 1u);
        EXPECT_TRUE(other.mLogEntries[0].mScript.references[0].isVariable);
        EXPECT_TRUE(other.mLogEntries[0].mScript.isConsistent());
    }

    /// Verify a stage index above 32767 is kept as it is, not read as a negative number.
    TEST(ESM4QuestTest, keepsStageIndicesAboveTheSignedRange)
    {
        const std::string data = zString("EDID", "Big") + valueSubRecord<std::uint16_t>("INDX", 40000)
            + valueSubRecord<std::uint8_t>("QSDT", 0) + zString("CNAM", "Late stage");

        const std::vector<ESM4::Quest> quests = loadRecords<ESM4::Quest>("QUST", record("QUST", 1, data));

        ASSERT_EQ(quests.size(), 1u);
        ASSERT_EQ(quests[0].mStages.size(), 1u);
        EXPECT_EQ(quests[0].mStages[0].mIndex, 40000u);
    }

    /// Verify scripts outside stage log entries are skipped without disrupting later quest records.
    TEST(ESM4QuestTest, skipsScriptDataThatHasNoLogEntryAndKeepsReading)
    {
        // A script before any stage, and one after the objectives start.
        std::string objective;
        append<std::int32_t>(objective, 1);
        const std::string data = zString("EDID", "Odd") + scriptHeader(0, 2, 0) + subRecord("SCDA", "ab")
            + valueSubRecord<std::int16_t>("INDX", 5) + valueSubRecord<std::uint8_t>("QSDT", 0)
            + zString("CNAM", "Only entry") + subRecord("QOBJ", objective) + scriptHeader(0, 2, 0)
            + subRecord("SCDA", "cd") + zString("NNAM", "Objective");

        const std::vector<ESM4::Quest> quests
            = loadRecords<ESM4::Quest>("QUST", record("QUST", 1, data) + record("QUST", 2, zString("EDID", "Next")));

        ASSERT_EQ(quests.size(), 2u);
        ASSERT_EQ(quests[0].mStages.size(), 1u);
        ASSERT_EQ(quests[0].mStages[0].mLogEntries.size(), 1u);
        EXPECT_EQ(quests[0].mStages[0].mLogEntries[0].mText, "Only entry");
        EXPECT_TRUE(quests[0].mStages[0].mLogEntries[0].mScript.compiledScript.empty());
        EXPECT_EQ(quests[1].mEditorId, "Next");
    }

    /// Verify a reference cut short at the end of a log entry is rejected, not read from the next record.
    TEST(ESM4QuestTest, rejectsAReferenceThatIsShorterThanItsFieldAtTheEndOfAnEntry)
    {
        const std::string first = zString("EDID", "Short") + valueSubRecord<std::int16_t>("INDX", 10)
            + valueSubRecord<std::uint8_t>("QSDT", 0) + zString("CNAM", "Entry") + scriptHeader(1, 2, 0)
            + subRecord("SCDA", "ab") + subRecord("SCRV", "");
        const std::string second = zString("EDID", "Next") + zString("FULL", "The next quest");

        try
        {
            loadRecords<ESM4::Quest>("QUST", record("QUST", 1, first) + record("QUST", 2, second));
            FAIL() << "an empty SCRV was accepted";
        }
        catch (const std::exception& e)
        {
            EXPECT_THAT(e.what(), HasSubstr("unexpected size"));
        }
    }

    /// Verify the sub-records after a malformed QSDT are not added to the entry before it.
    TEST(ESM4QuestTest, keepsTheSubrecordsOfAMalformedLogEntryOffTheEntryBeforeIt)
    {
        const std::string data = zString("EDID", "Odd") + valueSubRecord<std::int16_t>("INDX", 10)
            + valueSubRecord<std::uint8_t>("QSDT", 0) + zString("CNAM", "Good entry")
            + valueSubRecord<std::uint16_t>("QSDT", 7) + zString("CNAM", "Stray text")
            + valueSubRecord<std::uint32_t>("NAM0", 0x000c0002) + condition(200);

        const std::vector<ESM4::Quest> quests = loadRecords<ESM4::Quest>("QUST", record("QUST", 1, data));

        ASSERT_EQ(quests.size(), 1u);
        ASSERT_EQ(quests[0].mStages.size(), 1u);
        ASSERT_EQ(quests[0].mStages[0].mLogEntries.size(), 1u);
        const ESM4::QuestLogEntry& entry = quests[0].mStages[0].mLogEntries[0];
        EXPECT_EQ(entry.mText, "Good entry");
        EXPECT_EQ(entry.mNextQuest.mIndex, 0u);
        EXPECT_TRUE(entry.mTargetConditions.empty());
    }

    /// Verify quest-level conditions load when the quest has no stages.
    TEST(ESM4QuestTest, readsAQuestWithoutStages)
    {
        const std::string data = zString("EDID", "NoStages") + questData() + condition(100);

        const std::vector<ESM4::Quest> quests = loadRecords<ESM4::Quest>("QUST", record("QUST", 1, data));

        ASSERT_EQ(quests.size(), 1u);
        EXPECT_TRUE(quests.front().mStages.empty());
        EXPECT_EQ(quests.front().mTargetConditions.size(), 1u);
    }

    /// Verify SCPT, INFO and QUST script totals, bytecode sizes and header mismatch counts.
    /// Empty blocks are excluded, while headers declaring missing references are counted.
    TEST(ESM4CensusTest, countsTheScriptsRecordsHoldInLine)
    {
        const std::string good = zString("EDID", "Good")
            + scriptHeader(1, static_cast<std::uint32_t>(bytecode.size()), 0) + subRecord("SCDA", bytecode)
            + valueSubRecord<std::uint32_t>("SCRO", 0x000a0001);
        const std::string wrong = zString("EDID", "Wrong") + scriptHeader(0, 99, 0) + subRecord("SCDA", bytecode);
        const std::string empty = zString("EDID", "Empty") + scriptHeader(0, 0, 0);
        const std::string missingReferences = zString("EDID", "Refs") + scriptHeader(2, 0, 0);
        const std::string scripts = record("SCPT", 1, good) + record("SCPT", 2, wrong) + record("SCPT", 3, empty)
            + record("SCPT", 7, missingReferences);

        const std::string firstCode("\x01\x02\x03", 3);
        const std::string info
            = scriptHeader(0, 3, 0) + subRecord("SCDA", firstCode) + subRecord("NEXT", "") + scriptHeader(0, 0, 0);
        const std::string noScripts = valueSubRecord<std::uint32_t>("QSTI", 0x000b0001);
        const std::string infos = record("INFO", 4, info) + record("INFO", 5, noScripts);

        const std::string quest = zString("EDID", "Q") + valueSubRecord<std::int16_t>("INDX", 1)
            + valueSubRecord<std::uint8_t>("QSDT", 0) + scriptHeader(0, 3, 0) + subRecord("SCDA", firstCode)
            + valueSubRecord<std::int16_t>("INDX", 2) + valueSubRecord<std::uint8_t>("QSDT", 0) + scriptHeader(0, 3, 0)
            + subRecord("SCDA", firstCode);
        const std::string quests = record("QUST", 6, quest);

        const std::string plugin
            = header() + topGroup("SCPT", scripts) + topGroup("INFO", infos) + topGroup("QUST", quests);
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "scripts.esp", nullptr, nullptr);
        ESM4::Census census;
        census.collect(reader, [&](ESM4::Reader& r) {
            r.getRecordData();
            switch (r.hdr().record.typeId)
            {
                case ESM4::REC_SCPT:
                {
                    ESM4::Script value;
                    value.load(r);
                    census.addScripts(value);
                    return true;
                }
                case ESM4::REC_INFO:
                {
                    ESM4::DialogInfo value;
                    value.load(r);
                    census.addScripts(value);
                    return true;
                }
                case ESM4::REC_QUST:
                {
                    ESM4::Quest value;
                    value.load(r);
                    census.addScripts(value);
                    return true;
                }
            }
            return false;
        });

        EXPECT_EQ(census.getFatalError(), "");
        ASSERT_EQ(census.getScripts().size(), 3u);

        // The empty script block is not counted, but a header that declares references is.
        const ESM4::CensusScripts& scpt = census.getScripts().at("SCPT");
        EXPECT_EQ(scpt.mCount, 3u);
        EXPECT_EQ(scpt.mBytecode, 2 * bytecode.size());
        EXPECT_EQ(scpt.mWrongSize, 1u);
        EXPECT_EQ(scpt.mWrongReferences, 1u);
        EXPECT_EQ(scpt.mWrongVariables, 0u);

        // Only the first script of the INFO record holds anything.
        const ESM4::CensusScripts& info4 = census.getScripts().at("INFO");
        EXPECT_EQ(info4.mCount, 1u);
        EXPECT_EQ(info4.mBytecode, firstCode.size());
        EXPECT_EQ(info4.mWrongSize + info4.mWrongReferences + info4.mWrongVariables, 0u);

        // One script in each of the two stages.
        const ESM4::CensusScripts& qust = census.getScripts().at("QUST");
        EXPECT_EQ(qust.mCount, 2u);
        EXPECT_EQ(qust.mBytecode, 2 * firstCode.size());
        EXPECT_EQ(qust.mWrongSize + qust.mWrongReferences + qust.mWrongVariables, 0u);

        std::ostringstream out;
        census.write(out);
        EXPECT_THAT(out.str(), HasSubstr("Scripts held in line by records:"));
        EXPECT_THAT(out.str(), Not(HasSubstr("TestScript")));
    }
}
