#include "scriptbuilder.hpp"

#include <components/falloutscript/formlists.hpp>
#include <components/falloutscript/objects.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::FormId;
    using FalloutScript::FormLists;
    using FalloutScript::ObjectScripts;
    namespace BlockType = FalloutScript::BlockType;

    constexpr FormId guard = 0x01000100;
    constexpr FormId crate = 0x01000101;
    constexpr FormId guardBase = 0x01000200;
    constexpr FormId crateBase = 0x01000201;
    constexpr FormId list = 0x01000300;
    constexpr FormId otherList = 0x01000301;
    constexpr FormId scriptOfGuards = 0x01002000;

    constexpr std::uint32_t integerVariable = 1;

    // The codes of the commands in the table of the games
    constexpr std::uint16_t OpIsInList = 0x1174;
    constexpr std::uint16_t OpAddFormToFormList = 0x1233;

    class FakeRecords : public FalloutScript::FormListRecords
    {
    public:
        std::map<FormId, std::vector<FormId>> mLists;
        std::map<FormId, FormId> mBases;

        bool formListEntries(FormId id, std::vector<FormId>& entries) override
        {
            const auto found = mLists.find(id);
            if (found == mLists.end())
                return false;
            entries = found->second;
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
        FakeRecords mRecords;
        FormLists mLists{ mRecords };

        Runner() { FalloutScript::addFormListCommands(mInterpreter, mLists); }

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

    TEST(FalloutScriptFormListsTest, a_list_holds_what_its_record_holds_and_what_a_script_adds)
    {
        FakeRecords records;
        records.mLists[list] = { guardBase };
        FormLists lists(records);

        EXPECT_TRUE(lists.contains(list, guardBase));
        EXPECT_FALSE(lists.contains(list, crateBase));
        lists.add(list, crateBase);
        EXPECT_TRUE(lists.contains(list, guardBase));
        EXPECT_TRUE(lists.contains(list, crateBase));
        // Another list is not changed
        records.mLists[otherList] = {};
        EXPECT_FALSE(lists.contains(otherList, crateBase));
    }

    TEST(FalloutScriptFormListsTest, nothing_is_added_to_a_list_that_no_record_has)
    {
        FakeRecords records;
        FormLists lists(records);

        lists.add(list, guardBase);

        EXPECT_FALSE(lists.contains(list, guardBase));
    }

    TEST(FalloutScriptFormListsTest, a_reference_is_in_a_list_by_the_form_it_stands_for)
    {
        FakeRecords records;
        records.mLists[list] = { guardBase };
        records.mBases[guard] = guardBase;
        records.mBases[crate] = crateBase;
        FormLists lists(records);

        EXPECT_TRUE(lists.containsBaseOf(list, guard));
        EXPECT_FALSE(lists.containsBaseOf(list, crate));
        // A reference the host does not know is in no list
        EXPECT_FALSE(lists.containsBaseOf(list, 0x01000999));
    }

    TEST(FalloutScriptFormListsTest, clearing_forgets_what_scripts_added)
    {
        FakeRecords records;
        records.mLists[list] = {};
        FormLists lists(records);
        lists.add(list, guardBase);

        lists.clear();

        EXPECT_FALSE(lists.contains(list, guardBase));
    }

    TEST(FalloutScriptFormListCommandsTest, a_script_adds_a_form_to_a_list_and_asks_whether_an_object_is_in_it)
    {
        Runner setup;
        setup.mRecords.mLists[list] = { crateBase };
        setup.mRecords.mBases[guard] = guardBase;
        ScriptBuilder builder;
        const std::uint16_t listIndex = builder.form(list);
        const std::uint16_t baseIndex = builder.form(guardBase);
        builder.code(scriptName() + begin(BlockType::OnActivate)
            + setTo(var('s', 1), callToken(OpIsInList, arguments(1).add(refArg(listIndex))))
            + command(OpAddFormToFormList, arguments(2).add(refArg(listIndex)).add(refArg(baseIndex)))
            + setTo(var('s', 2), callToken(OpIsInList, arguments(1).add(refArg(listIndex)))) + end());
        builder.variable(1, integerVariable, "before").variable(2, integerVariable, "after");

        setup.run(builder);

        EXPECT_EQ(setup.variable(1), 0);
        EXPECT_EQ(setup.variable(2), 1);
        EXPECT_THAT(setup.mHost.mLog, IsEmpty());
    }
}
