#include "playercontrols.hpp"

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        // The flags a call gives, with the default for each control that it leaves out
        void applyFlags(
            CallContext& call, ControlsWorld& world, bool enable, const bool (&defaults)[playerControlCount])
        {
            for (std::size_t i = 0; i < playerControlCount; ++i)
            {
                const bool act = i < call.mArguments.size() ? intArgument(call, i) != 0 : defaults[i];
                if (act)
                    world.setPlayerControl(static_cast<PlayerControl>(i), enable);
            }
        }
    }

    void addPlayerControlCommands(Interpreter& interpreter, ControlsWorld& world)
    {
        static constexpr bool enableDefaults[playerControlCount] = { true, true, true, true, true, true, true };
        static constexpr bool disableDefaults[playerControlCount] = { true, true, true, false, false, false, false };
        interpreter.setHandler("EnablePlayerControls", [&world](CallContext& call) {
            applyFlags(call, world, true, enableDefaults);
            return 1.0;
        });
        interpreter.setHandler("DisablePlayerControls", [&world](CallContext& call) {
            applyFlags(call, world, false, disableDefaults);
            return 1.0;
        });
    }
}
