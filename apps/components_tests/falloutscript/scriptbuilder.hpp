#ifndef OPENFALLOUT_TESTS_FALLOUTSCRIPT_SCRIPTBUILDER_H
#define OPENFALLOUT_TESTS_FALLOUTSCRIPT_SCRIPTBUILDER_H

#include <components/esm4/script.hpp>
#include <components/falloutscript/commandtable.hpp>
#include <components/falloutscript/interpreter.hpp>
#include <components/falloutscript/script.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace FalloutScriptTest
{
    /// Writes the bytes of a compiled script the way the compiler of the games does
    class Bytes
    {
    public:
        Bytes() = default;
        Bytes(std::string_view text)
            : mData(text)
        {
        }

        Bytes& u8(std::uint8_t value)
        {
            mData.push_back(static_cast<char>(value));
            return *this;
        }

        Bytes& u16(std::uint16_t value)
        {
            mData.push_back(static_cast<char>(value & 0xFF));
            mData.push_back(static_cast<char>(value >> 8));
            return *this;
        }

        Bytes& u32(std::uint32_t value)
        {
            u16(static_cast<std::uint16_t>(value & 0xFFFF));
            return u16(static_cast<std::uint16_t>(value >> 16));
        }

        Bytes& add(const Bytes& other)
        {
            mData += other.mData;
            return *this;
        }

        Bytes& text(std::string_view value)
        {
            u16(static_cast<std::uint16_t>(value.size()));
            mData.append(value);
            return *this;
        }

        std::size_t size() const { return mData.size(); }
        const std::string& str() const { return mData; }

    private:
        std::string mData;
    };

    inline Bytes operator+(Bytes left, const Bytes& right)
    {
        return left.add(right);
    }

    // Tokens of an expression, in reverse Polish order
    inline Bytes number(std::string_view text)
    {
        // a number is written as text, and what follows it has to be set apart
        return Bytes(text).u8(' ');
    }
    inline Bytes op(std::string_view text)
    {
        return Bytes(text);
    }
    inline Bytes var(char type, std::uint16_t index)
    {
        return Bytes().u8(static_cast<std::uint8_t>(type)).u16(index);
    }
    inline Bytes remoteVar(std::uint16_t reference, char type, std::uint16_t index)
    {
        return Bytes().u8('r').u16(reference).u8(static_cast<std::uint8_t>(type)).u16(index);
    }
    inline Bytes global(std::uint16_t reference)
    {
        return Bytes().u8('G').u16(reference);
    }
    inline Bytes object(std::uint16_t reference)
    {
        return Bytes().u8('Z').u16(reference);
    }
    inline Bytes callToken(std::uint16_t opcode, const Bytes& arguments, std::uint16_t reference = 0)
    {
        Bytes bytes;
        if (reference != 0)
            bytes.u8('r').u16(reference);
        return bytes.u8('X').u16(opcode).u16(static_cast<std::uint16_t>(arguments.size())).add(arguments);
    }

    // Arguments of a command
    inline Bytes arguments(std::uint16_t count)
    {
        return Bytes().u16(count);
    }
    inline Bytes intArg(std::int32_t value)
    {
        return Bytes().u8('n').u32(static_cast<std::uint32_t>(value));
    }
    inline Bytes refArg(std::uint16_t reference)
    {
        return Bytes().u8('r').u16(reference);
    }

    // Statements
    inline Bytes statement(std::uint16_t code, const Bytes& data = {})
    {
        return Bytes().u16(code).u16(static_cast<std::uint16_t>(data.size())).add(data);
    }
    inline Bytes scriptName()
    {
        return statement(0x1D);
    }
    inline Bytes begin(std::uint16_t blockType)
    {
        return statement(0x10, Bytes().u16(blockType).u32(0));
    }
    inline Bytes end()
    {
        return statement(0x11);
    }
    inline Bytes returnStatement()
    {
        return statement(0x1E);
    }
    inline Bytes setTo(const Bytes& target, const Bytes& expression)
    {
        return statement(0x15, Bytes().add(target).u16(static_cast<std::uint16_t>(expression.size())).add(expression));
    }
    inline Bytes branch(std::uint16_t code, const Bytes& expression)
    {
        return statement(code, Bytes().u16(0).u16(static_cast<std::uint16_t>(expression.size())).add(expression));
    }
    inline Bytes ifStatement(const Bytes& expression)
    {
        return branch(0x16, expression);
    }
    inline Bytes elseIfStatement(const Bytes& expression)
    {
        return branch(0x18, expression);
    }
    inline Bytes elseStatement()
    {
        return statement(0x17, Bytes().u16(0));
    }
    inline Bytes endIf()
    {
        return statement(0x19);
    }
    inline Bytes command(std::uint16_t opcode, const Bytes& args, std::uint16_t reference = 0)
    {
        if (reference == 0)
            return statement(opcode, args);
        return Bytes().u16(0x1C).u16(reference).u16(opcode).u16(static_cast<std::uint16_t>(args.size())).add(args);
    }

    /// A script made from bytes and the tables that go with them
    struct ScriptBuilder
    {
        Bytes mCode;
        std::uint16_t mType = 0;
        std::vector<ESM4::ScriptReference> mReferences;
        std::vector<ESM4::ScriptLocalVariableData> mVariables;

        ScriptBuilder& code(const Bytes& bytes)
        {
            mCode.add(bytes);
            return *this;
        }

        /// SCRO: a form named in the script. Returns the 1-based index the bytecode uses for it.
        std::uint16_t form(std::uint32_t id)
        {
            ESM4::ScriptReference reference;
            reference.formId = ESM::FormId::fromUint32(id);
            mReferences.push_back(reference);
            return static_cast<std::uint16_t>(mReferences.size());
        }

        /// SCRV: a local variable that holds a form
        std::uint16_t formVariable(std::uint32_t variableIndex)
        {
            ESM4::ScriptReference reference;
            reference.isVariable = true;
            reference.variableIndex = variableIndex;
            mReferences.push_back(reference);
            return static_cast<std::uint16_t>(mReferences.size());
        }

        /// SLSD: a variable. Type 1 is a short or long, 0 a float.
        ScriptBuilder& variable(std::uint32_t index, std::uint32_t type, std::string name = {})
        {
            ESM4::ScriptLocalVariableData data{};
            data.index = index;
            data.type = type;
            data.variableName = std::move(name);
            mVariables.push_back(std::move(data));
            return *this;
        }

        ESM4::ScriptDefinition definition() const
        {
            ESM4::ScriptDefinition result;
            result.scriptHeader.type = mType;
            result.scriptHeader.refCount = static_cast<std::uint32_t>(mReferences.size());
            result.scriptHeader.compiledSize = static_cast<std::uint32_t>(mCode.size());
            result.compiledScript.assign(mCode.str().begin(), mCode.str().end());
            result.references = mReferences;
            result.localVarData = mVariables;
            return result;
        }
    };

    /// A host with global variables, instances and a log that the test can look at
    class TestHost : public FalloutScript::Host
    {
    public:
        std::map<FalloutScript::FormId, double> mGlobals;
        std::map<FalloutScript::FormId, FalloutScript::Instance*> mInstances;
        std::vector<std::string> mLog;

        bool getGlobal(FalloutScript::FormId global, double& value) override
        {
            const auto it = mGlobals.find(global);
            if (it == mGlobals.end())
                return false;
            value = it->second;
            return true;
        }

        bool setGlobal(FalloutScript::FormId global, double value) override
        {
            const auto it = mGlobals.find(global);
            if (it == mGlobals.end())
                return false;
            it->second = value;
            return true;
        }

        FalloutScript::Instance* findInstance(FalloutScript::FormId owner) override
        {
            const auto it = mInstances.find(owner);
            return it == mInstances.end() ? nullptr : it->second;
        }

        void log(std::string_view message) override { mLog.emplace_back(message); }
    };

    constexpr std::uint16_t TypeString = 0x00;
    constexpr std::uint16_t TypeInteger = 0x01;
    constexpr std::uint16_t TypeFloat = 0x02;
    constexpr std::uint16_t TypeObjectRef = 0x04;
    constexpr std::uint16_t TypeQuest = 0x0E;

    constexpr std::uint16_t OpLog = 0x1100;
    constexpr std::uint16_t OpTwice = 0x1101;
    constexpr std::uint16_t OpGetStage = 0x103A;
    constexpr std::uint16_t OpSetStage = 0x1104;
    constexpr std::uint16_t OpNothing = 0x1105;
    constexpr std::uint16_t OpTouch = 0x1106;
    constexpr std::uint16_t OpGetStageDone = 0x1107;
    constexpr std::uint16_t OpStartQuest = 0x1108;
    constexpr std::uint16_t OpStopQuest = 0x1109;
    constexpr std::uint16_t OpGetQuestRunning = 0x110A;
    constexpr std::uint16_t OpCompleteQuest = 0x110B;
    constexpr std::uint16_t OpGetQuestCompleted = 0x110C;
    constexpr std::uint16_t OpSetObjectiveDisplayed = 0x110D;
    constexpr std::uint16_t OpSetObjectiveCompleted = 0x110E;
    constexpr std::uint16_t OpGetObjectiveDisplayed = 0x110F;
    constexpr std::uint16_t OpGetObjectiveCompleted = 0x1110;

    /// A small table of commands for the tests: Log "text", Twice number, the commands of quests, Touch (anything),
    /// and Nothing, which no handler runs
    inline FalloutScript::CommandTable testCommands()
    {
        using ESM4::ScriptCode::Parameter;
        FalloutScript::CommandTable table;
        auto add = [&](std::uint16_t opcode, const char* name, std::vector<Parameter> parameters) {
            FalloutScript::CommandInfo info;
            info.mOpcode = opcode;
            info.mName = name;
            info.mParameters = std::move(parameters);
            table.add(std::move(info));
        };
        add(OpLog, "Log", { { TypeString, false } });
        add(OpTwice, "Twice", { { TypeFloat, false } });
        add(OpGetStage, "GetStage", { { TypeQuest, false } });
        add(OpSetStage, "SetStage", { { TypeQuest, false }, { TypeInteger, false } });
        add(OpNothing, "Nothing", {});
        add(OpTouch, "Touch", { { TypeObjectRef, true }, { TypeInteger, true } });
        add(OpGetStageDone, "GetStageDone", { { TypeQuest, false }, { TypeInteger, false } });
        add(OpStartQuest, "StartQuest", { { TypeQuest, false } });
        add(OpStopQuest, "StopQuest", { { TypeQuest, false } });
        add(OpGetQuestRunning, "GetQuestRunning", { { TypeQuest, false } });
        add(OpCompleteQuest, "CompleteQuest", { { TypeQuest, false } });
        add(OpGetQuestCompleted, "GetQuestCompleted", { { TypeQuest, false } });
        add(OpSetObjectiveDisplayed, "SetObjectiveDisplayed",
            { { TypeQuest, false }, { TypeInteger, false }, { TypeInteger, false } });
        add(OpSetObjectiveCompleted, "SetObjectiveCompleted",
            { { TypeQuest, false }, { TypeInteger, false }, { TypeInteger, false } });
        add(OpGetObjectiveDisplayed, "GetObjectiveDisplayed", { { TypeQuest, false }, { TypeInteger, false } });
        add(OpGetObjectiveCompleted, "GetObjectiveCompleted", { { TypeQuest, false }, { TypeInteger, false } });
        return table;
    }

    inline std::shared_ptr<const FalloutScript::Script> prepare(
        const ScriptBuilder& builder, const FalloutScript::CommandTable& commands)
    {
        return std::make_shared<const FalloutScript::Script>(
            FalloutScript::Script::prepare(builder.definition(), commands));
    }
}

#endif
