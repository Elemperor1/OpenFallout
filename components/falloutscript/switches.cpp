#include "switches.hpp"

#include <iterator>
#include <utility>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        // The commands whose effect is on something this engine does not have. They are accepted so that a script goes
        // on as it does in the games, and the log says once for each what is not done.
        constexpr IgnoredCommand ignored[] = {
            { "TriggerScreenBlood", "the blood on the screen is not drawn" },
            { "ClearScreenSplatter", "the blood on the screen is not drawn" },
            { "ResetPipboyManager", "the Pip-Boy keeps nothing that could be reset" },
            { "ForceRadioStationUpdate", "there is no radio" },
            { "PipBoyRadioOff", "there is no radio" },
            { "StartRadioConversation", "there is no radio" },
            { "SetCellImageSpace", "image spaces are not drawn" },
            { "MatchRace", "the race of a character does not change" },
            { "MatchFaceGeometry", "the face of a character does not change" },
            { "AgeRace", "the race of a character does not change" },
            { "ResetInventory", "an inventory is not rebuilt from the form of the character" },
            { "UnlockChallenge", "the challenges are not kept" },
            { "IncrementScriptedChallenge", "the challenges are not kept" },
            { "SwapTextureOnRef", "the textures of a model are not swapped" },
            { "PlayBink", "videos are not played" },
            { "SetNoActivationSound", "an activation that does nothing makes no sound" },
            { "ClearNoActivationSound", "an activation that does nothing makes no sound" },
            { "SetActorAlpha", "the transparency of a character is not drawn" },
            { "PlayMagicShaderVisuals", "the shaders of effects are not drawn" },
            { "StopMagicShaderVisuals", "the shaders of effects are not drawn" },
            { "SendAssaultAlarm", "there is no crime that could be reported" },
            // The leveled lists that scripts add to are those of vendors and loot, which nothing here rolls (the only
            // lists that are rolled are those of the armor that characters wear)
            { "AddItemToLeveledList", "the lists that scripts add to (vendors, loot) are not rolled" },
            // Tale of Two Wastelands' own commands
            { "TTW_EnableRadioFix", "there is no radio to fix" },
            { "InitExtraMiscStat", "the extra statistics of Tale of Two Wastelands are not kept" },
            { "InitRockItLauncher", "the Rock-It Launcher of Tale of Two Wastelands is not set up" },
            { "SetOnProjectileImpactEventHandler", "the handlers for the impact of a projectile are never called" },
            { "SetOnUseAidItemEventHandler", "the handlers for the use of an aid item are never called" },
        };

        struct SwitchCommand
        {
            const char* mName;
            const char* mReason;
            /// The argument is a form, kept by its id
            bool mForm;
        };

        // The commands that set a switch that is kept; the reason says what the switch does not do. The value is the
        // first argument, 1 for a command that has none.
        constexpr SwitchCommand switchCommands[] = {
            { "SetHardcore", "the needs of the hardcore mode are not done", false },
            { "SetPCYoung", "the body of the player does not change with its age", false },
            { "SetPCToddler", "the body of the player does not change with its age", false },
            { "SetLocationSpecificLoadScreensOnly", "load screens are not chosen by the place", false },
            { "SetBroadcastState", "there is no radio", false },
            { "AutoDisplayObjectives", "objectives are not shown on the screen by themselves", false },
            { "KillQuestUpdates", "the messages of quest updates are not held back", false },
            { "ForceActiveQuest", "there is no active quest to show", true },
        };

        void tell(CallContext& call, Switches& switches, const char* name, const char* reason)
        {
            if (switches.firstTime(name))
                call.mHost.log(std::string(name) + " is accepted and does nothing: " + reason);
        }
    }

    void Switches::clear()
    {
        const std::lock_guard lock(mMutex);
        mValues.clear();
        mTold.clear();
    }

    void Switches::set(std::string_view name, std::int32_t value)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mValues.find(name);
        if (value == 0)
        {
            if (found != mValues.end())
                mValues.erase(found);
        }
        else if (found != mValues.end())
            found->second = value;
        else
            mValues.emplace(std::string(name), value);
    }

    std::int32_t Switches::get(std::string_view name) const
    {
        const std::lock_guard lock(mMutex);
        const auto found = mValues.find(name);
        return found == mValues.end() ? 0 : found->second;
    }

    std::map<std::string, std::int32_t> Switches::all() const
    {
        const std::lock_guard lock(mMutex);
        return std::map<std::string, std::int32_t>(mValues.begin(), mValues.end());
    }

    bool Switches::firstTime(std::string_view name)
    {
        const std::lock_guard lock(mMutex);
        if (mTold.find(name) != mTold.end())
            return false;
        mTold.emplace(name);
        return true;
    }

    const IgnoredCommand* ignoredCommands(std::size_t& count)
    {
        count = std::size(ignored);
        return ignored;
    }

    void addSwitchCommands(Interpreter& interpreter, Switches& switches)
    {
        for (const IgnoredCommand& command : ignored)
            interpreter.setHandler(command.mName, [&switches, &command](CallContext& call) {
                tell(call, switches, command.mName, command.mReason);
                return 0.0;
            });
        for (const SwitchCommand& command : switchCommands)
            interpreter.setHandler(command.mName, [&switches, &command](CallContext& call) {
                tell(call, switches, command.mName, command.mReason);
                std::int32_t value = 1;
                if (!call.mArguments.empty())
                    value = command.mForm ? static_cast<std::int32_t>(formArgument(call, 0)) : intArgument(call, 0);
                switches.set(command.mName, value);
                return 0.0;
            });

        // The questions the games answer the same for every character: this is not a console, and nobody here is a
        // vampire (the games keep that for the vampires of Fallout 3, which no record of the intros makes)
        interpreter.setHandler("IsXBox", [](CallContext&) { return 0.0; });
        interpreter.setHandler("IsPS3", [](CallContext&) { return 0.0; });
        interpreter.setHandler("GetVampire", [](CallContext&) { return 0.0; });
        // The games read the bits of the limbs that were torn off (dismemberment), which nothing here sets
        interpreter.setHandler("IsLimbGone", [](CallContext&) { return 0.0; });
        // The games work the disposition out of the character's records and skills (the formula was not read from the
        // executables), which this does not: the answer is 0
        interpreter.setHandler("GetDisposition", [&switches](CallContext& call) {
            if (switches.firstTime("GetDisposition"))
                call.mHost.log(
                    "GetDisposition answers 0: the disposition of a character towards another is not worked out");
            return 0.0;
        });
        interpreter.setHandler(
            "IsHardcore", [&switches](CallContext&) { return switches.get("SetHardcore") > 0 ? 1.0 : 0.0; });
    }

    void writeSwitches(const Switches& switches, SaveWriter& out)
    {
        const std::map<std::string, std::int32_t> values = switches.all();
        out.count(values.size());
        for (const auto& [name, value] : values)
        {
            out.string(name);
            out.i32(value);
        }
    }

    void readSwitches(Switches& switches, SaveReader& in, std::uint32_t)
    {
        const std::size_t count = in.count(6);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::string name = in.string();
            const std::int32_t value = in.i32();
            switches.set(name, value);
        }
    }
}
