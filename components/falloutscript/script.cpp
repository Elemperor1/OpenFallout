#include "script.hpp"

#include <algorithm>

namespace FalloutScript
{
    namespace
    {
        FormId toFormId(const ESM::FormId& id)
        {
            if (!id.hasContentFile())
                return 0;
            return (id.mIndex & 0xffffff) | (static_cast<std::uint32_t>(id.mContentFile & 0xff) << 24);
        }
    }

    Script Script::prepare(const ESM4::ScriptDefinition& definition, const CommandTable& commands)
    {
        Script script;
        script.mType = definition.scriptHeader.type;
        script.mCode = definition.compiledScript;

        // The references keep the order they have in the record, which the bytecode counts from 1
        std::uint32_t highestVariable = definition.highestVariableIndex();
        for (const ESM4::ScriptReference& reference : definition.references)
        {
            ReferenceSlot& slot = script.mReferences.emplace_back();
            slot.mIsVariable = reference.isVariable;
            if (reference.isVariable)
            {
                slot.mVariable = reference.variableIndex;
                highestVariable = std::max(highestVariable, reference.variableIndex);
            }
            else
                slot.mForm = toFormId(reference.formId);
        }

        // A script can name a variable of its own that no declaration lists, so there are as many as the highest
        // index a declaration or a reference gives. The bytecode names a variable by 16 bits, so a plugin that gives
        // more (it is damaged) does not make the script take the memory for them.
        constexpr std::uint32_t maxVariable = 0xffff;
        highestVariable = std::min(highestVariable, maxVariable);
        script.mVariables.resize(static_cast<std::size_t>(highestVariable) + 1);
        for (const ESM4::ScriptLocalVariableData& data : definition.localVarData)
        {
            if (data.index >= script.mVariables.size())
                continue;
            Variable& variable = script.mVariables[data.index];
            variable.mKind = data.type == 1 ? VariableKind::Integer : VariableKind::Float;
            variable.mName = data.variableName;
        }
        for (const ReferenceSlot& slot : script.mReferences)
            if (slot.mIsVariable && slot.mVariable < script.mVariables.size())
                script.mVariables[slot.mVariable].mKind = VariableKind::Reference;

        ESM4::ScriptCode::Limits limits;
        limits.mReferences = script.mReferences.size();
        limits.mVariables = highestVariable;
        script.mProgram = ESM4::ScriptCode::decode(script.mCode, limits);
        if (script.mProgram.mError != ESM4::ScriptCode::Error::None)
        {
            script.mProblem = ScriptProblem::DoesNotDecode;
            return script;
        }

        script.mCalls.reserve(script.mProgram.mCalls.size());
        for (const ESM4::ScriptCode::Call& call : script.mProgram.mCalls)
        {
            PreparedCall& prepared = script.mCalls.emplace_back();
            prepared.mCommand = commands.find(call.mOpcode);
            if (prepared.mCommand == nullptr)
            {
                prepared.mProblem = CallProblem::UnknownCommand;
                continue;
            }
            prepared.mArguments
                = ESM4::ScriptCode::decodeArguments(script.mCode, call, prepared.mCommand->mParameters, limits);
            if (prepared.mArguments.mError != ESM4::ScriptCode::ArgumentError::None)
                prepared.mProblem = CallProblem::BadArguments;
        }

        if (!script.matchStructure())
            script.mProblem = ScriptProblem::BadStructure;
        return script;
    }

    bool Script::matchStructure()
    {
        using Kind = ESM4::ScriptCode::Statement::Kind;
        const auto& statements = mProgram.mStatements;
        mBranches.assign(statements.size(), Branch{});

        struct Chain
        {
            std::vector<std::size_t> mBranches; // the If, the ElseIfs and the Else
            bool mHasElse = false;
        };
        std::vector<Chain> chains;
        bool inBlock = false;
        bool hasBlocks = false;
        const auto fail = [this](std::string reason, std::size_t statement) {
            mStructureProblem = std::move(reason) + " (statement " + std::to_string(statement) + ")";
            return false;
        };

        for (std::size_t i = 0; i < statements.size(); ++i)
        {
            switch (statements[i].mKind)
            {
                case Kind::Begin:
                    if (inBlock)
                        return fail("a Begin inside a block", i);
                    if (!chains.empty())
                        return fail("a Begin inside an If", i);
                    inBlock = true;
                    hasBlocks = true;
                    mBlocks.push_back(Block{ statements[i].mBlockType, i, i });
                    break;
                case Kind::End:
                    if (!inBlock)
                        return fail("an End with no Begin", i);
                    if (!chains.empty())
                        return fail("an End inside an If", i);
                    inBlock = false;
                    mBlocks.back().mEnd = i;
                    break;
                case Kind::If:
                    chains.emplace_back().mBranches.push_back(i);
                    break;
                case Kind::ElseIf:
                case Kind::Else:
                {
                    // The compiler of the games lets through an ElseIf or Else that has no If to belong to (it comes
                    // after the EndIf that closed the If) and one that comes after an Else, and so do the scripts of
                    // the games. Such a branch is taken as part of the condition it follows, or, when there is none,
                    // as the first branch of one of its own that the next EndIf closes. The interpreter ends a
                    // condition when it comes to a branch in turn, so the body of such a branch is never run, which
                    // is what the games' own interpreter does with the jump of a statement that is reached in turn.
                    if (chains.empty())
                        chains.emplace_back();
                    chains.back().mHasElse = chains.back().mHasElse || statements[i].mKind == Kind::Else;
                    chains.back().mBranches.push_back(i);
                    break;
                }
                case Kind::EndIf:
                {
                    // An EndIf with no If to close does nothing: it is only the place a jump can land on
                    if (chains.empty())
                        break;
                    const Chain& chain = chains.back();
                    for (std::size_t k = 0; k < chain.mBranches.size(); ++k)
                    {
                        Branch& branch = mBranches[chain.mBranches[k]];
                        branch.mNext = k + 1 < chain.mBranches.size() ? chain.mBranches[k + 1] : i;
                        branch.mEndIf = i;
                    }
                    chains.pop_back();
                    break;
                }
                default:
                    break;
            }
        }
        if (inBlock)
            return fail("a Begin with no End", statements.size());
        if (!chains.empty())
            return fail("an If with no EndIf", chains.back().mBranches.front());

        // A script with no blocks is a list of statements to run, and the one block of it is all of them
        mIsResult = !hasBlocks;
        if (mIsResult)
            mBlocks.push_back(Block{ 0, 0, statements.size() });
        return true;
    }

    std::string Script::problemText() const
    {
        switch (mProblem)
        {
            case ScriptProblem::None:
                return {};
            case ScriptProblem::DoesNotDecode:
                return "its code does not decode: " + std::string(ESM4::ScriptCode::errorText(mProgram.mError))
                    + " at byte " + std::to_string(mProgram.mErrorOffset);
            case ScriptProblem::BadStructure:
                return "its blocks or conditions are not in order: " + mStructureProblem;
        }
        return {};
    }
}
