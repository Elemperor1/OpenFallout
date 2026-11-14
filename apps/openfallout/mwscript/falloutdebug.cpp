#include "falloutscripts.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/falloutscript/arguments.hpp>

namespace OFScript
{
    double FalloutScripts::debugCommand(
        std::string_view name, FalloutScript::FormId reference, const std::vector<double>& arguments)
    {
        const FalloutScript::CommandInfo* command = mInterpreter.commands().find(name);
        if (command == nullptr)
        {
            Log(Debug::Warning) << "Command: \"" << name << "\" is not a command of the script language";
            return 0.0;
        }
        std::vector<FalloutScript::Value> values;
        std::ostringstream shown;
        for (const double number : arguments)
        {
            values.push_back({ number, {}, false });
            shown << (shown.tellp() > 0 ? ", " : "") << number;
        }
        const double result = mInterpreter.call(command->mOpcode, reference, reference != 0, std::move(values));
        Log(Debug::Info) << "Command: " << command->mName << " on " << (reference != 0 ? FalloutScript::hex(reference) : "-")
                         << " (" << shown.str() << ") = " << result;
        return result;
    }
}
