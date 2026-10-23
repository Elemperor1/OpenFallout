#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SWITCHES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_SWITCHES_H

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>

#include "interpreter.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// The switches that scripts set for parts of the games that are not in this engine (the hardcore mode, the radio,
    /// the age of the player, the objectives on the screen ...), kept as they were set so that the questions about them
    /// are answered and a saved game holds them. Nothing here changes what is on the screen or in the world. It can be
    /// used from any thread.
    class Switches
    {
    public:
        /// Forgets everything, as for a new game
        void clear();

        void set(std::string_view name, std::int32_t value);
        /// What the switch was set to, 0 for one that was never set
        std::int32_t get(std::string_view name) const;
        std::map<std::string, std::int32_t> all() const;

        /// Whether this is the first time that the command is told about, which the log says once for each
        bool firstTime(std::string_view name);

    private:
        mutable std::mutex mMutex;
        std::map<std::string, std::int32_t, std::less<>> mValues;
        std::set<std::string, std::less<>> mTold;
    };

    /// Gives the interpreter the commands that set a switch (SetHardcore, SetPCYoung, SetPCToddler,
    /// SetLocationSpecificLoadScreensOnly, SetBroadcastState, AutoDisplayObjectives, KillQuestUpdates,
    /// ForceActiveQuest), the questions that are always answered the same (IsXBox, IsPS3, GetVampire, IsHardcore after
    /// SetHardcore) and the commands that do nothing here, each of which says once in the log what it does not do. The
    /// commands that the table does not have are left out.
    void addSwitchCommands(Interpreter& interpreter, Switches& switches);

    /// The commands that do nothing and the reason, for the list of what is not done
    struct IgnoredCommand
    {
        const char* mName;
        const char* mReason;
    };
    const IgnoredCommand* ignoredCommands(std::size_t& count);

    constexpr std::uint32_t switchesSectionVersion = 1;
    void writeSwitches(const Switches& switches, SaveWriter& out);
    void readSwitches(Switches& switches, SaveReader& in, std::uint32_t version);
}

#endif
