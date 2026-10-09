#include "interpreter.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace FalloutScript
{
    using ESM4::ScriptCode::Operator;
    using ESM4::ScriptCode::Statement;
    using ESM4::ScriptCode::Token;
    using ESM4::ScriptCode::VariableRef;

    namespace
    {
        double coerce(VariableKind kind, double value)
        {
            if (std::isnan(value))
                return 0;
            switch (kind)
            {
                case VariableKind::Integer:
                    return std::clamp(std::trunc(value), -2147483648.0, 2147483647.0);
                case VariableKind::Reference:
                    return std::clamp(std::trunc(value), 0.0, 4294967295.0);
                case VariableKind::Float:
                    break;
            }
            const double limit = std::numeric_limits<float>::max();
            return static_cast<double>(static_cast<float>(std::clamp(value, -limit, limit)));
        }

        double apply(Operator op, double left, double right)
        {
            switch (op)
            {
                case Operator::Add:
                    return left + right;
                case Operator::Subtract:
                    return left - right;
                case Operator::Multiply:
                    return left * right;
                case Operator::Divide:
                    return right == 0 ? 0 : left / right;
                case Operator::Modulo:
                {
                    const double divisor = std::trunc(right);
                    return divisor == 0 ? 0 : std::fmod(std::trunc(left), divisor);
                }
                case Operator::Equal:
                    return left == right;
                case Operator::NotEqual:
                    return left != right;
                case Operator::Less:
                    return left < right;
                case Operator::LessOrEqual:
                    return left <= right;
                case Operator::Greater:
                    return left > right;
                case Operator::GreaterOrEqual:
                    return left >= right;
                case Operator::And:
                    return left != 0 && right != 0;
                case Operator::Or:
                    return left != 0 || right != 0;
                case Operator::Negate:
                    break;
            }
            return 0;
        }

        std::string hex(std::uint32_t value)
        {
            std::ostringstream stream;
            stream << "0x" << std::hex << value;
            return stream.str();
        }

        // Which problem a message about a command is about, in the bits above the code of the command
        enum ReportKind : std::uint32_t
        {
            Report_UnknownCommand = 0x10000,
            Report_NoHandler = 0x20000,
            Report_BadArguments = 0x30000,
            Report_NoGlobal = 0x40000,
            Report_NoInstance = 0x50000,
            Report_BadScript = 0x60000,
            Report_TooDeep = 0x70000,
            Report_TooLong = 0x80000,
        };
    }

    Instance::Instance(std::shared_ptr<const Script> script, FormId owner, bool forQuest)
        : mScript(std::move(script))
        , mOwner(owner)
        , mForQuest(forQuest)
        , mVariables(mScript->variables().size(), 0.0)
    {
    }

    double Instance::variable(std::uint32_t index) const
    {
        return index >= 1 && index < mVariables.size() ? mVariables[index] : 0.0;
    }

    bool Instance::setVariable(std::uint32_t index, double value)
    {
        if (index < 1 || index >= mVariables.size())
            return false;
        mVariables[index] = coerce(mScript->variables()[index].mKind, value);
        return true;
    }

    struct Interpreter::Frame
    {
        Instance& mInstance;
        const Script& mScript;
        std::vector<double> mStack;
    };

    Interpreter::Interpreter(Host& host, const CommandTable& commands)
        : mHost(host)
        , mCommands(commands)
    {
    }

    bool Interpreter::setHandler(std::string_view name, Handler handler)
    {
        const CommandInfo* command = mCommands.find(name);
        if (command == nullptr)
            return false;
        mHandlers[command->mOpcode] = std::move(handler);
        return true;
    }

    void Interpreter::setHandler(std::uint16_t opcode, Handler handler)
    {
        mHandlers[opcode] = std::move(handler);
    }

    void Interpreter::reportOnce(std::uint32_t key, std::string_view message)
    {
        if (mReported.insert(key).second)
            mHost.log(message);
    }

    Interpreter::Result Interpreter::run(Instance& instance, std::uint16_t blockType)
    {
        if (mDepth == 0)
            mBudget = maxStatements;
        Result total;
        const Script& script = instance.script();
        if (!script.usable() || script.isResult())
        {
            if (!script.usable())
                total.mStatus = Status::Failed;
            return total;
        }
        for (const Block& block : script.blocks())
        {
            if (block.mType != blockType)
                continue;
            const Result result = runBlock(instance, block);
            total.mStatements += result.mStatements;
            // A Return ends the script, not only its block
            if (result.mStatus != Status::Done)
            {
                total.mStatus = result.mStatus;
                break;
            }
        }
        return total;
    }

    Interpreter::Result Interpreter::runResult(Instance& instance)
    {
        if (mDepth == 0)
            mBudget = maxStatements;
        const Script& script = instance.script();
        if (!script.usable())
        {
            reportOnce(Report_BadScript | static_cast<std::uint32_t>(instance.owner() & 0xffff),
                "A script that can not be run was given to the interpreter");
            return Result{ Status::Failed, 0 };
        }
        if (!script.isResult())
            return Result{ Status::Done, 0 };
        return runBlock(instance, script.blocks().front());
    }

    Interpreter::Result Interpreter::runBlock(Instance& instance, const Block& block)
    {
        const Script& script = instance.script();
        const auto& statements = script.program().mStatements;
        const auto& branches = script.branches();

        Result result;
        if (mDepth >= maxDepth)
        {
            reportOnce(Report_TooDeep, "Scripts are running inside each other too deeply, so one was not run");
            result.mStatus = Status::Failed;
            return result;
        }
        ++mDepth;
        struct Leave
        {
            std::size_t& mDepth;
            ~Leave() { --mDepth; }
        } leave{ mDepth };

        Frame frame{ instance, script, {} };
        std::size_t pc = script.isResult() ? block.mBegin : block.mBegin + 1;
        const std::size_t end = block.mEnd;
        while (pc < end)
        {
            if (mBudget == 0)
            {
                reportOnce(Report_TooLong, "A script ran for too many statements, so it was stopped");
                result.mStatus = Status::Failed;
                return result;
            }
            --mBudget;
            ++result.mStatements;
            const Statement& statement = statements[pc];
            switch (statement.mKind)
            {
                case Statement::Kind::SetTo:
                {
                    const double value = evaluate(frame, statement);
                    if (statement.mGlobal != 0)
                    {
                        const ReferenceSlot& slot = script.references()[statement.mGlobal - 1];
                        const FormId global
                            = slot.mIsVariable ? static_cast<FormId>(instance.variable(slot.mVariable)) : slot.mForm;
                        if (!mHost.setGlobal(global, value))
                            reportOnce(Report_NoGlobal | (global & 0xffff),
                                "A script sets a global variable that does not exist");
                    }
                    else
                        writeVariable(frame, statement.mTarget, value);
                    ++pc;
                    break;
                }
                case Statement::Kind::Call:
                    callCommand(frame, statement.mCall);
                    ++pc;
                    break;
                case Statement::Kind::Return:
                    result.mStatus = Status::Returned;
                    return result;
                case Statement::Kind::If:
                {
                    if (evaluate(frame, statement) != 0)
                    {
                        ++pc;
                        break;
                    }
                    // Look for the first branch that holds: an ElseIf whose condition is true, the Else, or the EndIf
                    std::size_t target = branches[pc].mNext;
                    while (true)
                    {
                        const Statement& next = statements[target];
                        if (next.mKind == Statement::Kind::ElseIf)
                        {
                            ++result.mStatements;
                            if (evaluate(frame, next) != 0)
                                break;
                            target = branches[target].mNext;
                            continue;
                        }
                        break;
                    }
                    pc = target + 1;
                    break;
                }
                case Statement::Kind::ElseIf:
                case Statement::Kind::Else:
                    // The body of a branch that was taken has ended: leave the condition
                    pc = branches[pc].mEndIf + 1;
                    break;
                default:
                    ++pc;
                    break;
            }
        }
        return result;
    }

    double Interpreter::evaluate(Frame& frame, const Statement& statement)
    {
        const auto& tokens = frame.mScript.program().mTokens;
        std::vector<double> stack;
        stack.reserve(8);
        const std::size_t last = static_cast<std::size_t>(statement.mFirstToken) + statement.mTokenCount;
        for (std::size_t i = statement.mFirstToken; i < last; ++i)
        {
            const Token& token = tokens[i];
            switch (token.mKind)
            {
                case Token::Kind::Number:
                    stack.push_back(token.mNumber);
                    break;
                case Token::Kind::Variable:
                    stack.push_back(readVariable(frame, token.mVariable));
                    break;
                case Token::Kind::Global:
                {
                    const FormId global = resolveReference(frame, token.mIndex);
                    double value = 0;
                    if (!mHost.getGlobal(global, value))
                        reportOnce(Report_NoGlobal | (global & 0xffff),
                            "A script reads a global variable that does not exist");
                    stack.push_back(value);
                    break;
                }
                case Token::Kind::Reference:
                    stack.push_back(resolveReference(frame, token.mIndex));
                    break;
                case Token::Kind::Call:
                    stack.push_back(callCommand(frame, token.mCall));
                    break;
                case Token::Kind::String:
                    stack.push_back(0);
                    break;
                case Token::Kind::Operator:
                {
                    const std::size_t operands = token.mOperator == Operator::Negate ? 1 : 2;
                    if (stack.size() < operands)
                        return 0;
                    if (operands == 1)
                        stack.back() = -stack.back();
                    else
                    {
                        const double right = stack.back();
                        stack.pop_back();
                        stack.back() = apply(token.mOperator, stack.back(), right);
                    }
                    break;
                }
            }
        }
        return stack.empty() ? 0 : stack.back();
    }

    FormId Interpreter::resolveReference(Frame& frame, std::uint16_t slot)
    {
        const auto& references = frame.mScript.references();
        if (slot < 1 || slot > references.size())
            return 0;
        const ReferenceSlot& entry = references[slot - 1];
        if (entry.mIsVariable)
            return static_cast<FormId>(frame.mInstance.variable(entry.mVariable));
        return entry.mForm;
    }

    double Interpreter::readVariable(Frame& frame, const VariableRef& ref)
    {
        if (ref.mRemote == 0)
            return frame.mInstance.variable(ref.mIndex);
        const FormId owner = resolveReference(frame, ref.mRemote);
        Instance* target = mHost.findInstance(owner);
        if (target == nullptr)
        {
            reportOnce(Report_NoInstance | (owner & 0xffff), "A script reads a variable of a script that is not there");
            return 0;
        }
        return target->variable(ref.mIndex);
    }

    bool Interpreter::writeVariable(Frame& frame, const VariableRef& ref, double value)
    {
        if (ref.mRemote == 0)
            return frame.mInstance.setVariable(ref.mIndex, value);
        const FormId owner = resolveReference(frame, ref.mRemote);
        Instance* target = mHost.findInstance(owner);
        if (target == nullptr)
        {
            reportOnce(Report_NoInstance | (owner & 0xffff), "A script sets a variable of a script that is not there");
            return false;
        }
        return target->setVariable(ref.mIndex, value);
    }

    double Interpreter::call(std::uint16_t opcode, FormId reference, bool onReference, std::vector<Value> arguments)
    {
        const CommandInfo* command = mCommands.find(opcode);
        if (command == nullptr)
        {
            reportOnce(Report_UnknownCommand | opcode,
                "A condition names the function " + hex(opcode) + ", which is not in the table");
            return 0;
        }
        const auto handler = mHandlers.find(opcode);
        if (handler == mHandlers.end())
        {
            reportOnce(Report_NoHandler | opcode,
                "The script command " + command->mName + " (" + hex(opcode) + ") is not implemented");
            return 0;
        }
        if (!mOutside)
            mOutside = std::make_unique<Instance>(std::make_shared<const Script>(), 0);
        CallContext context{ *this, mHost, *mOutside, *command, reference, onReference, std::move(arguments) };
        return handler->second(context);
    }

    double Interpreter::callCommand(Frame& frame, std::size_t callIndex)
    {
        const Script& script = frame.mScript;
        const PreparedCall& prepared = script.calls()[callIndex];
        const ESM4::ScriptCode::Call& call = script.program().mCalls[callIndex];

        if (prepared.mProblem == CallProblem::UnknownCommand)
        {
            reportOnce(Report_UnknownCommand | call.mOpcode,
                "A script calls the command " + hex(call.mOpcode) + ", which is not in the table");
            return 0;
        }
        if (prepared.mProblem == CallProblem::BadArguments)
        {
            reportOnce(Report_BadArguments | call.mOpcode,
                "A script calls " + prepared.mCommand->mName + " with arguments that do not fit it");
            return 0;
        }
        const auto handler = mHandlers.find(call.mOpcode);
        if (handler == mHandlers.end())
        {
            reportOnce(Report_NoHandler | call.mOpcode,
                "The script command " + prepared.mCommand->mName + " (" + hex(call.mOpcode) + ") is not implemented");
            return 0;
        }

        CallContext context{ *this, mHost, frame.mInstance, *prepared.mCommand, 0, false, {} };
        context.mOnReference = call.mReference != 0;
        if (context.mOnReference)
            context.mReference = resolveReference(frame, call.mReference);
        else
            context.mReference = frame.mInstance.forQuest() ? 0 : frame.mInstance.owner();
        context.mArguments.reserve(prepared.mArguments.mValues.size());
        for (const ESM4::ScriptCode::Argument& argument : prepared.mArguments.mValues)
        {
            Value value;
            switch (argument.mKind)
            {
                case ESM4::ScriptCode::Argument::Kind::Number:
                    value.mNumber = argument.mNumber;
                    break;
                case ESM4::ScriptCode::Argument::Kind::String:
                    value.mText = argument.mText;
                    value.mIsText = true;
                    break;
                case ESM4::ScriptCode::Argument::Kind::Variable:
                    value.mNumber = readVariable(frame, argument.mVariable);
                    break;
                case ESM4::ScriptCode::Argument::Kind::Global:
                {
                    const FormId global = resolveReference(frame, argument.mIndex);
                    if (!mHost.getGlobal(global, value.mNumber))
                    {
                        value.mNumber = 0;
                        reportOnce(Report_NoGlobal | (global & 0xffff),
                            "A script reads a global variable that does not exist");
                    }
                    break;
                }
                case ESM4::ScriptCode::Argument::Kind::Reference:
                    value.mNumber = resolveReference(frame, argument.mIndex);
                    break;
            }
            context.mArguments.push_back(std::move(value));
        }
        return handler->second(context);
    }
}
