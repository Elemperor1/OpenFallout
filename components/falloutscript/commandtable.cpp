#include "commandtable.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <istream>

namespace FalloutScript
{
    std::string lowerCase(std::string_view text)
    {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    namespace
    {
        // The distinct names of a command in lower case, long name first, without the one it does not have
        std::vector<std::string> namesOf(const CommandInfo& info)
        {
            std::vector<std::string> names;
            for (const std::string& name : { info.mName, info.mShortName })
            {
                std::string lower = lowerCase(name);
                if (!lower.empty() && std::find(names.begin(), names.end(), lower) == names.end())
                    names.push_back(std::move(lower));
            }
            return names;
        }
    }

    void CommandTable::add(CommandInfo info)
    {
        const std::uint16_t opcode = info.mOpcode;
        const std::vector<std::string> names = namesOf(info);

        // A command that is replaced gives up the names it does not keep, and each goes to the command that claimed it
        // next, in the order the commands were added
        if (const auto old = mCommands.find(opcode); old != mCommands.end())
        {
            for (const std::string& name : namesOf(old->second))
            {
                if (std::find(names.begin(), names.end(), name) != names.end())
                    continue;
                std::vector<std::uint16_t>& claims = mClaims[name];
                claims.erase(std::remove(claims.begin(), claims.end(), opcode), claims.end());
                const auto owner = mNames.find(name);
                if (owner == mNames.end() || owner->second != opcode)
                    continue;
                if (claims.empty())
                    mNames.erase(owner);
                else
                    owner->second = claims.front();
            }
        }

        // The first command to have a name keeps it: the short name of a command can be the long name of another
        for (const std::string& name : names)
        {
            std::vector<std::uint16_t>& claims = mClaims[name];
            if (std::find(claims.begin(), claims.end(), opcode) == claims.end())
                claims.push_back(opcode);
            mNames.emplace(name, opcode);
        }
        mCommands[opcode] = std::move(info);
    }

    const CommandInfo* CommandTable::find(std::uint16_t opcode) const
    {
        const auto it = mCommands.find(opcode);
        return it == mCommands.end() ? nullptr : &it->second;
    }

    const CommandInfo* CommandTable::find(std::string_view name) const
    {
        const auto it = mNames.find(lowerCase(name));
        return it == mNames.end() ? nullptr : find(it->second);
    }

    namespace
    {
        // The fields of a line, split at the commas that are not inside quotes
        std::vector<std::string> splitFields(const std::string& line)
        {
            std::vector<std::string> fields(1);
            bool quoted = false;
            for (std::size_t i = 0; i < line.size(); ++i)
            {
                const char c = line[i];
                if (quoted)
                {
                    if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
                    {
                        fields.back() += '"';
                        ++i;
                    }
                    else if (c == '"')
                        quoted = false;
                    else
                        fields.back() += c;
                }
                else if (c == '"')
                    quoted = true;
                else if (c == ',')
                    fields.emplace_back();
                else if (c != '\r')
                    fields.back() += c;
            }
            return fields;
        }

        // A whole number, written in decimal or as 0x and hexadecimal digits
        bool parseNumber(std::string_view text, std::uint32_t& value)
        {
            int base = 10;
            if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            {
                text.remove_prefix(2);
                base = 16;
            }
            if (text.empty())
                return false;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            return error == std::errc() && end == text.data() + text.size();
        }
    }

    bool parseParameterTypes(std::string_view text, std::vector<ESM4::ScriptCode::Parameter>& parameters)
    {
        std::size_t at = 0;
        while (at < text.size())
        {
            if (text[at] == ' ')
            {
                ++at;
                continue;
            }
            const std::size_t end = std::min(text.find(' ', at), text.size());
            std::string_view token = text.substr(at, end - at);
            ESM4::ScriptCode::Parameter parameter;
            if (token.back() == '?')
            {
                parameter.mOptional = true;
                token.remove_suffix(1);
            }
            if (!parseNumber(token, parameter.mType))
                return false;
            parameters.push_back(parameter);
            at = end;
        }
        return true;
    }

    CommandCsvResult readCommandTableCsv(std::istream& stream, CommandTable& table)
    {
        CommandCsvResult result;
        std::string line;
        std::size_t number = 0;
        while (std::getline(stream, line))
        {
            ++number;
            if (line.empty() || line == "\r")
                continue;
            ++result.mRows;
            const std::vector<std::string> fields = splitFields(line);
            CommandInfo info;
            std::uint32_t tableNumber = 0;
            std::uint32_t opcode = 0;
            std::uint32_t needsParent = 0;
            const bool readable = fields.size() >= 7 && parseNumber(fields[0], tableNumber)
                && parseNumber(fields[1], opcode) && opcode <= 0xFFFF && !fields[2].empty()
                && parseNumber(fields[4], needsParent) && parseParameterTypes(fields[5], info.mParameters)
                && parseNumber(fields[6], info.mFlags) && (fields.size() == 7 || parseNumber(fields[7], info.mParse));
            if (!readable)
            {
                result.mProblems.push_back("line " + std::to_string(number) + " is not a command");
                continue;
            }
            if (opcode < ESM4::ScriptCode::firstCommand)
            {
                ++result.mOther;
                continue;
            }
            info.mOpcode = static_cast<std::uint16_t>(opcode);
            info.mName = fields[2];
            info.mShortName = fields[3];
            info.mNeedsReference = needsParent != 0;
            table.add(std::move(info));
            ++result.mCommands;
        }
        return result;
    }
}
