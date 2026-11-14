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
            { "ResetPipboyManager", "the Pip-Boy keeps nothing that could be reset" },
            { "StartRadioConversation", "there is no radio" },
            { "SetCellImageSpace", "image spaces are not drawn" },
            { "MatchRace", "the race of a character does not change" },
            { "MatchFaceGeometry", "the face of a character does not change" },
            { "AgeRace", "the race of a character does not change" },
            { "SetNoActivationSound", "an activation that does nothing makes no sound" },
            { "ClearNoActivationSound", "an activation that does nothing makes no sound" },
            { "SetActorAlpha", "the transparency of a character is not drawn" },
            { "PlayMagicShaderVisuals", "the shaders of effects are not drawn" },
            { "StopMagicShaderVisuals", "the shaders of effects are not drawn" },
            { "PlayMagicEffectVisuals", "the visuals of effects are not drawn" },
            { "StopMagicEffectVisuals", "the visuals of effects are not drawn" },
            { "SetActorRefraction", "the refraction of a character is not drawn" },
            { "PreloadMagicEffect", "there is nothing to load ahead for an effect" },
            { "ShowQuestStages", "the stages of a quest are written to the console of the games, which this engine does not feed" },
            // What a character's idle is picked or looped from is the idle tree and the animation groups of the world
            { "PickIdle", "the idle that the conditions of the idle tree pick is not played" },
            { "LoopGroup", "an animation group is not looped a number of times" },
            { "SkipAnim", "the animation that plays is not skipped to its end" },
            { "SetClass", "the class of a character is the one its record gives" },
            { "GetActorAggroRadiusViolated", "the aggression radius of a character is not watched" },
            { "IsContinuingPackagePCNear", "the package that goes on while the player is near is not told apart" },
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
            { "SetPCYoung", "the body of the player does not change with its age", false },
            { "SetPCToddler", "the body of the player does not change with its age", false },
            { "SetLocationSpecificLoadScreensOnly", "load screens are not chosen by the place", false },
            { "AutoDisplayObjectives", "objectives are not shown on the screen by themselves", false },
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
