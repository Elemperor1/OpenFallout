#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_INTERPRETER_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_INTERPRETER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "commandtable.hpp"
#include "script.hpp"

namespace FalloutScript
{
    /// The form id the games give the player's reference (PlayerRef), as it is in the plugin that holds it: a host
    /// whose plugins are not first in the load order tells the real one
    constexpr FormId playerReference = 0x14;

    /// A script at work: its variables, and the object it belongs to (a reference for the script of an object, a quest
    /// for the script of a quest). Several instances share the Script they were made from.
    class Instance
    {
    public:
        /// A script of a quest is run on no object: a command in it that names none has no object to act on.
        Instance(std::shared_ptr<const Script> script, FormId owner, bool forQuest = false);

        const Script& script() const { return *mScript; }
        FormId owner() const { return mOwner; }
        bool forQuest() const { return mForQuest; }

        /// The value of a variable by its 1-based index, 0 for one the script does not have
        double variable(std::uint32_t index) const;
        /// Sets a variable, keeping to the kind it has: an integer drops its fraction, a float is a 32 bit float.
        /// Returns false for a variable the script does not have.
        bool setVariable(std::uint32_t index, double value);

    private:
        std::shared_ptr<const Script> mScript;
        FormId mOwner;
        bool mForQuest;
        std::vector<double> mVariables;
    };

    /// What a script can reach that is not in the script: the global variables, the scripts of other objects (for
    /// Quest.variable) and a place to say what went wrong.
    class Host
    {
    public:
        virtual ~Host() = default;

        virtual bool getGlobal(FormId global, double& value) = 0;
        virtual bool setGlobal(FormId global, double value) = 0;
        /// The instance of the script of a reference or a quest, or null when it has none
        virtual Instance* findInstance(FormId owner) = 0;
        virtual void log(std::string_view message) = 0;
        /// The reference of the player, which a script names with the form PlayerRef and a condition runs on when it
        /// is about no one else
        virtual FormId player() { return playerReference; }
    };

    /// An argument of a call, worked out: a number (a form is its id) or a text
    struct Value
    {
        double mNumber = 0;
        std::string mText;
        bool mIsText = false;
    };

    class Interpreter;

    /// What a command is given when a script calls it
    struct CallContext
    {
        Interpreter& mInterpreter;
        Host& mHost;
        Instance& mInstance; // the script that makes the call
        const CommandInfo& mCommand;
        /// The object the command is called on: the one named before the command (ref.Command), or else the object the
        /// script belongs to, which is 0 for a quest
        FormId mReference = 0;
        bool mOnReference = false; // the script names the object
        std::vector<Value> mArguments; // as many as the call has (the last optional ones can be missing)
    };

    /// A command of the engine. Its result is the value of the command when a script uses it in an expression.
    using Handler = std::function<double(CallContext&)>;

    /// Runs the statements of a script.
    ///
    /// Everything is a number: integers, floats and forms (as their ids). Commands are looked up by their code in the
    /// table and run by the handlers given to setHandler. A command without a handler, with arguments that do not fit
    /// or that is not in the table is reported once and does nothing (its value is 0), so that the rest of the script
    /// still runs.
    class Interpreter
    {
    public:
        enum class Status : std::uint8_t
        {
            Done, // the block or script ran to its end
            Returned, // a Return ended it
            Failed, // it could not run, or ran too long
        };

        struct Result
        {
            Status mStatus = Status::Done;
            std::size_t mStatements = 0;
        };

        /// How many scripts may be running inside each other (a result script that sets a quest stage whose result
        /// script sets another stage ...) and how many statements one run may take.
        static constexpr std::size_t maxDepth = 300;
        static constexpr std::size_t maxStatements = 100000;

        Interpreter(Host& host, const CommandTable& commands);

        /// The command of that name (long or short, any case) is run by the handler. Returns false when the table has
        /// no such command.
        bool setHandler(std::string_view name, Handler handler);
        void setHandler(std::uint16_t opcode, Handler handler);
        bool hasHandler(std::uint16_t opcode) const { return mHandlers.count(opcode) != 0; }

        const CommandTable& commands() const { return mCommands; }
        Host& host() { return mHost; }

        /// Runs the blocks of the instance's script that are of this type, in the order they are written.
        Result run(Instance& instance, std::uint16_t blockType);

        /// Runs all the statements of a script that is a list of statements, such as a result script.
        Result runResult(Instance& instance);

        /// Calls the command of that code outside of a script, the way a condition does: with arguments that are
        /// numbers (a form is its id) and no script of its own. Tells the value of the command, which is 0 for one
        /// that is not in the table or has no handler (that is reported once).
        double call(std::uint16_t opcode, FormId reference, bool onReference, std::vector<Value> arguments);

    private:
        struct Frame;

        Result runBlock(Instance& instance, const Block& block);
        double evaluate(Frame& frame, const ESM4::ScriptCode::Statement& statement);
        double callCommand(Frame& frame, std::size_t callIndex);
        double readVariable(Frame& frame, const ESM4::ScriptCode::VariableRef& ref);
        bool writeVariable(Frame& frame, const ESM4::ScriptCode::VariableRef& ref, double value);
        FormId resolveReference(Frame& frame, std::uint16_t slot);
        void reportOnce(std::uint32_t key, std::string_view message);

        Host& mHost;
        const CommandTable& mCommands;
        std::map<std::uint16_t, Handler> mHandlers;
        std::set<std::uint32_t> mReported;
        std::size_t mDepth = 0;
        std::size_t mBudget = 0;
        /// The script that a call from outside of a script runs as, which has no code and no variables
        std::unique_ptr<Instance> mOutside;
    };
}

#endif
