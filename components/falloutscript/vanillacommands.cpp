#include "vanillacommands.hpp"

#include <stdexcept>
#include <string>

namespace FalloutScript
{
    namespace
    {
        // The table is made by scripts/openfallout/generate_command_table.py
        struct Entry
        {
            std::uint16_t mOpcode;
            const char* mName;
            const char* mShortName;
            bool mNeedsReference;
            std::uint32_t mFlags;
            const char* mParameters; // as New Vegas has them
            bool mInFallout3;
            const char* mFallout3Parameters; // null when they are the same
        };

        constexpr Entry entries[] = {
#include "vanillacommands.inc"
        };
    }

    CommandTable vanillaCommands(Game game)
    {
        CommandTable table;
        for (const Entry& entry : entries)
        {
            if (game == Game::Fallout3 && !entry.mInFallout3)
                continue;
            CommandInfo info;
            info.mOpcode = entry.mOpcode;
            info.mName = entry.mName;
            info.mShortName = entry.mShortName;
            info.mNeedsReference = entry.mNeedsReference;
            info.mFlags = entry.mFlags;
            const char* parameters = game == Game::Fallout3 && entry.mFallout3Parameters != nullptr
                ? entry.mFallout3Parameters
                : entry.mParameters;
            if (!parseParameterTypes(parameters, info.mParameters))
                throw std::logic_error(std::string("The table of commands has bad parameters for ") + entry.mName);
            table.add(std::move(info));
        }
        return table;
    }
}
