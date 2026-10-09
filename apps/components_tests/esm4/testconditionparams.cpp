#include <components/esm4/conditionparams.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/reader.hpp>
#include <components/esm4/readerutils.hpp>
#include <components/esm4/script.hpp>

#include "syntheticplugin.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    using namespace ESM4Test;

    constexpr std::uint32_t getStage = 58;
    constexpr std::uint32_t getStageDone = 59;
    constexpr std::uint32_t notInTheTable = 500;

    TEST(ESM4ConditionParamsTest, knowsTheTypesOfTheParametersOfAFunction)
    {
        const ESM4::ConditionParameterTypes stage = ESM4::conditionParameterTypes(getStage);
        EXPECT_EQ(stage.mFirst, 14u); // a quest
        EXPECT_EQ(stage.mSecond, ESM4::ConditionParameterTypes::none);

        const ESM4::ConditionParameterTypes stageDone = ESM4::conditionParameterTypes(getStageDone);
        EXPECT_EQ(stageDone.mFirst, 14u);
        EXPECT_EQ(stageDone.mSecond, 23u); // a quest stage, which is a number

        // a function that only a condition can name, which the table of commands has no entry for
        const ESM4::ConditionParameterTypes variable = ESM4::conditionParameterTypes(79);
        EXPECT_EQ(variable.mFirst, 14u);
        EXPECT_EQ(variable.mSecond, 22u); // the index of a variable

        const ESM4::ConditionParameterTypes none = ESM4::conditionParameterTypes(notInTheTable);
        EXPECT_EQ(none.mFirst, ESM4::ConditionParameterTypes::none);
        EXPECT_EQ(none.mSecond, ESM4::ConditionParameterTypes::none);
    }

    // A condition of 28 bytes with the given parameters; `type` carries the flags (4 is the global flag)
    std::string condition(std::uint32_t type, std::uint32_t comparisonBits, std::uint32_t function,
        std::uint32_t param1, std::uint32_t param2, std::uint32_t reference)
    {
        std::string data;
        append(data, type);
        append(data, comparisonBits);
        append(data, function);
        append(data, param1);
        append(data, param2);
        append<std::uint32_t>(data, reference != 0 ? 2 : 0);
        append(data, reference);
        return subRecord("CTDA", data);
    }

    // A group in the layout of Fallout 3 and the games after it, which has a header of 24 bytes
    std::string versionedGroup(std::string_view label, std::string_view children)
    {
        std::string result("GRUP");
        append<std::uint32_t>(result, static_cast<std::uint32_t>(24 + children.size()));
        result.append(label);
        append<std::int32_t>(result, 0); // top level group
        append<std::uint16_t>(result, 0); // stamp
        append<std::uint16_t>(result, 0);
        append<std::uint32_t>(result, 0);
        result.append(children);
        return result;
    }

    // The TES4 record of a plugin of New Vegas that has one master
    std::string header()
    {
        std::string hedr;
        append<float>(hedr, 1.34f);
        append<std::int32_t>(hedr, 1);
        append<std::uint32_t>(hedr, 0x800);
        return versionedRecord("TES4", 0,
            subRecord("HEDR", hedr) + zString("MAST", "base.esm") + valueSubRecord<std::uint64_t>("DATA", 0), 15);
    }

    std::vector<ESM4::TargetCondition> conditionsOf(const std::string& conditions)
    {
        const std::string plugin = header()
            + versionedGroup("QUST", versionedRecord("QUST", 0x801, zString("EDID", "OFTestQuest") + conditions, 15));
        // the plugin has the load order index 5 and its master, base.esm, has the index 2
        ESM4::Reader reader(std::make_unique<std::istringstream>(plugin), "patch.esp", nullptr, nullptr);
        reader.setModIndex(5);
        reader.updateModIndices({ { "base.esm", 2 } });
        std::vector<ESM4::Quest> quests;
        ESM4::ReaderUtils::readAll(
            reader,
            [&](ESM4::Reader& r) {
                r.getRecordData();
                quests.emplace_back().load(r);
                return true;
            },
            [](ESM4::Reader&) {});
        EXPECT_EQ(quests.size(), 1u);
        return quests.empty() ? std::vector<ESM4::TargetCondition>() : quests.front().mTargetConditions;
    }

    TEST(ESM4ConditionParamsTest, givesTheFormsOfAConditionTheIndexOfTheLoadOrderAndLeavesTheNumbers)
    {
        const std::vector<ESM4::TargetCondition> conditions = conditionsOf(
            // a quest of the master and a stage
            condition(0, 0, getStageDone, 0x00001234, 10, 0)
            // a quest of the plugin itself (the index after the masters)
            + condition(0, 0, getStage, 0x01000100, 0, 0)
            // no form at all
            + condition(0, 0, getStage, 0, 0, 0)
            // a function the table does not have: both parameters stay
            + condition(0, 0, notInTheTable, 0x00001234, 0x01000100, 0));

        ASSERT_EQ(conditions.size(), 4u);
        EXPECT_EQ(conditions[0].param1, 0x02001234u);
        EXPECT_EQ(conditions[0].param2, 10u);
        EXPECT_EQ(conditions[1].param1, 0x05000100u);
        EXPECT_EQ(conditions[2].param1, 0u);
        EXPECT_EQ(conditions[3].param1, 0x00001234u);
        EXPECT_EQ(conditions[3].param2, 0x01000100u);
    }

    TEST(ESM4ConditionParamsTest, givesTheGlobalAndTheReferenceOfAConditionTheIndexToo)
    {
        // the global flag makes the comparison value the form id of a global, in the bytes of the float
        const std::vector<ESM4::TargetCondition> conditions
            = conditionsOf(condition(ESM4::CTF_UseGlobal, 0x00000555, getStage, 0x00000010, 0, 0x00000777)
                + condition(0, 0x00000555, getStage, 0x00000010, 0, 0));

        ASSERT_EQ(conditions.size(), 2u);
        std::uint32_t comparison = 0;
        std::memcpy(&comparison, &conditions[0].comparison, sizeof(comparison));
        EXPECT_EQ(comparison, 0x02000555u);
        EXPECT_EQ(conditions[0].reference, 0x02000777u);
        // without the flag the value is a number
        std::memcpy(&comparison, &conditions[1].comparison, sizeof(comparison));
        EXPECT_EQ(comparison, 0x00000555u);
    }
}
