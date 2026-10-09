#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_COMMANDTABLE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_COMMANDTABLE_H

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <components/esm4/scriptargs.hpp>

namespace FalloutScript
{
    /// A command of the script language of Fallout 3 and New Vegas: the code a compiled script calls it by, its name
    /// and the parameters it takes, whose types say how the arguments of a call are written
    /// (ESM4::ScriptCode::decodeArguments).
    struct CommandInfo
    {
        std::uint16_t mOpcode = 0;
        std::string mName;
        std::string mShortName;
        std::vector<ESM4::ScriptCode::Parameter> mParameters;
        /// The command has to be called on an object (ref.Command)
        bool mNeedsReference = false;
        /// The flags and the parse function that the executable's table gives the command (an address, 0 when the table
        /// was read without it). Most commands share one parse function; the few that read their arguments in another
        /// way are told apart by it.
        std::uint32_t mFlags = 0;
        std::uint32_t mParse = 0;
    };

    /// The commands a script can call, by code and by name (any case, long or short name).
    class CommandTable
    {
    public:
        /// Adds a command, or replaces the one with the same code.
        void add(CommandInfo info);

        const CommandInfo* find(std::uint16_t opcode) const;
        const CommandInfo* find(std::string_view name) const;

        std::size_t size() const { return mCommands.size(); }

        auto begin() const { return mCommands.begin(); }
        auto end() const { return mCommands.end(); }

    private:
        std::map<std::uint16_t, CommandInfo> mCommands;
        std::map<std::string, std::uint16_t> mNames; // in lower case
    };

    std::string lowerCase(std::string_view text);

    /// What reading a table of commands from a CSV file found
    struct CommandCsvResult
    {
        std::size_t mRows = 0; // lines that held a row
        std::size_t mCommands = 0; // added to the table
        std::size_t mOther = 0; // block types and console commands (codes below 0x1000), which a script cannot call
        std::vector<std::string> mProblems; // lines that could not be read, with their number
    };

    /// Reads the CSV that scripts/openfallout/dump_command_table.py writes from the executable of a game, one command
    /// to a line: table, code, name, short name, needs a reference, parameters, flags and (optionally) the parse
    /// function. The parameters are the type numbers of the parameters, separated by spaces, with a ? after the type of
    /// an optional one. Only the commands a script can call (code 0x1000 or more) are added to the table.
    CommandCsvResult readCommandTableCsv(std::istream& stream, CommandTable& table);
}

#endif
