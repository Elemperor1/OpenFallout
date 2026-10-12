#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLAYERCONTROLS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PLAYERCONTROLS_H

#include <cstddef>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What the player does with the game, as EnablePlayerControls and DisablePlayerControls name it: the order here is
    /// the order of the flags of the two commands
    enum class PlayerControl : std::size_t
    {
        Movement,
        PipBoy,
        Fighting,
        PointOfView,
        Looking,
        Rollover,
        Sneaking,
    };

    constexpr std::size_t playerControlCount = 7;

    /// What the commands that give the controls of the player or take them away ask of the game
    class ControlsWorld
    {
    public:
        virtual ~ControlsWorld() = default;

        /// The player can (or can not) use the control from now on
        virtual void setPlayerControl(PlayerControl control, bool enabled) = 0;
    };

    /// Gives EnablePlayerControls and DisablePlayerControls to the interpreter. Both take up to seven flags, one for
    /// each control in the order of PlayerControl: 1 acts on the control and 0 leaves it as it is. A flag the call
    /// leaves out is 1 for EnablePlayerControls, and for DisablePlayerControls 1 for movement, the Pip-Boy and
    /// fighting and 0 for the other four (the defaults the executables read; a call with no flags at all takes only
    /// those three). The command table of a game that does not have a command is left as it is.
    void addPlayerControlCommands(Interpreter& interpreter, ControlsWorld& world);
}

#endif
