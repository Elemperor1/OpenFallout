#include "inputlog.hpp"

#include <array>

#include <components/debug/debuglog.hpp>

namespace OFInput
{
    namespace
    {
        struct Kind
        {
            const char* mName;
            int mLimit;
            int mWritten;
        };

        std::array<Kind, 3> kinds{ {
            { "keys", 800, 0 },
            { "buttons", 800, 0 },
            { "sticks", 2400, 0 },
        } };
    }

    void logInput(InputLogKind kind, const std::string& text)
    {
        Kind& entry = kinds[static_cast<std::size_t>(kind)];
        if (entry.mWritten >= entry.mLimit)
            return;
        ++entry.mWritten;
        Log(Debug::Info) << "Input: " << text
                         << (entry.mWritten == entry.mLimit ? std::string(" (further ") + entry.mName + " are not logged)"
                                                            : std::string());
    }
}
