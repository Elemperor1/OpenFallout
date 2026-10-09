#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SCRIPT_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SCRIPT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <components/esm4/script.hpp>
#include <components/esm4/scriptargs.hpp>
#include <components/esm4/scriptcode.hpp>

#include "commandtable.hpp"

namespace FalloutScript
{
    /// The 32 bit id of a record as the game writes it: the index of the plugin in the load order in the top byte and
    /// the number of the record in the others. Scripts hold forms (references, quests, globals, any record) as
    /// numbers of this kind.
    using FormId = std::uint32_t;

    enum class VariableKind : std::uint8_t
    {
        Float, // a number kept as a 32 bit float
        Integer, // a short or a long, kept as a whole number
        Reference, // a form, kept as its id
    };

    struct Variable
    {
        VariableKind mKind = VariableKind::Float;
        std::string mName;
    };

    /// An entry of the table a script uses to name things outside itself (SCRO and SCRV)
    struct ReferenceSlot
    {
        bool mIsVariable = false; // the form is held by a local variable
        FormId mForm = 0; // otherwise the form
        std::uint32_t mVariable = 0; // the 1-based index of that variable
    };

    /// The numbers of the events a block can be about (the table of events of the games' executables; the last three
    /// are New Vegas only)
    namespace BlockType
    {
        constexpr std::uint16_t GameMode = 0;
        constexpr std::uint16_t MenuMode = 1;
        constexpr std::uint16_t OnActivate = 2;
        constexpr std::uint16_t OnAdd = 3;
        constexpr std::uint16_t OnEquip = 4;
        constexpr std::uint16_t OnUnequip = 5;
        constexpr std::uint16_t OnDrop = 6;
        constexpr std::uint16_t SayToDone = 7;
        constexpr std::uint16_t OnHit = 8;
        constexpr std::uint16_t OnHitWith = 9;
        constexpr std::uint16_t OnDeath = 10;
        constexpr std::uint16_t OnMurder = 11;
        constexpr std::uint16_t OnCombatEnd = 12;
        constexpr std::uint16_t OnPackageStart = 15;
        constexpr std::uint16_t OnPackageDone = 16;
        constexpr std::uint16_t OnPackageChange = 20;
        constexpr std::uint16_t OnLoad = 21;
        constexpr std::uint16_t OnReset = 30;
        constexpr std::uint16_t OnOpen = 31;
        constexpr std::uint16_t OnClose = 32;
    }

    /// A block of statements that runs on an event: Begin <type> ... End
    struct Block
    {
        std::uint16_t mType = 0;
        std::size_t mBegin = 0; // index of the Begin statement
        std::size_t mEnd = 0; // index of the End statement
    };

    /// Where the branches of a condition lead: each If, ElseIf and Else knows the statement that follows its
    /// body (the next ElseIf, the Else or the EndIf) and the EndIf of the whole condition.
    struct Branch
    {
        std::size_t mNext = 0;
        std::size_t mEndIf = 0;
    };

    /// What stands in the way of running a call: the script can still run, and the call is skipped.
    enum class CallProblem : std::uint8_t
    {
        None,
        UnknownCommand, // the table has no command of that code
        BadArguments, // the arguments do not fit the parameters of the command
    };

    struct PreparedCall
    {
        const CommandInfo* mCommand = nullptr;
        ESM4::ScriptCode::Arguments mArguments;
        CallProblem mProblem = CallProblem::None;
    };

    enum class ScriptProblem : std::uint8_t
    {
        None,
        DoesNotDecode, // see mDecodeError
        BadStructure, // blocks or conditions that are not closed or not in order
    };

    /// A script ready to run: the bytecode taken apart (ESM4::ScriptCode::decode), the arguments of its calls read by
    /// the parameters of their commands, the blocks and conditions matched, and the variables and references listed.
    /// It holds nothing that changes while it runs, so any number of instances can share one.
    class Script
    {
    public:
        /// Never throws: a script that can not be taken apart is kept with its problem and does nothing when run.
        static Script prepare(const ESM4::ScriptDefinition& definition, const CommandTable& commands);

        ScriptProblem problem() const { return mProblem; }
        ESM4::ScriptCode::Error decodeError() const { return mProgram.mError; }
        bool usable() const { return mProblem == ScriptProblem::None; }

        /// 0 for a script of an object, 1 for a quest, 0x100 for a magic effect
        std::uint16_t type() const { return mType; }

        const ESM4::ScriptCode::Program& program() const { return mProgram; }
        const std::vector<std::uint8_t>& code() const { return mCode; }
        const std::vector<PreparedCall>& calls() const { return mCalls; }
        const std::vector<Block>& blocks() const { return mBlocks; }
        const std::vector<Branch>& branches() const { return mBranches; }
        const std::vector<ReferenceSlot>& references() const { return mReferences; }

        /// The variables by their 1-based index (entry 0 is not used)
        const std::vector<Variable>& variables() const { return mVariables; }

        /// The script has no Begin and End: it is a list of statements to run from the first to the last (the result
        /// of a line of dialogue or of a quest stage). Its one block then covers all the statements.
        bool isResult() const { return mIsResult; }

    private:
        ESM4::ScriptCode::Program mProgram;
        std::vector<std::uint8_t> mCode;
        std::vector<PreparedCall> mCalls;
        std::vector<Block> mBlocks;
        std::vector<Branch> mBranches; // by the index of the statement
        std::vector<ReferenceSlot> mReferences;
        std::vector<Variable> mVariables;
        std::uint16_t mType = 0;
        bool mIsResult = false;
        ScriptProblem mProblem = ScriptProblem::None;

        bool matchStructure();
    };
}

#endif
