#include "messagecommands.hpp"

#include "arguments.hpp"

namespace FalloutScript
{
    void addMessageCommands(Interpreter& interpreter, MessageWorld& world)
    {
        interpreter.setHandler("ShowMessage", [&world](CallContext& call) {
            if (const FormId message = formArgument(call, 0); message != 0)
                world.showMessage(message);
            return 0.0;
        });
        // The messages of the game have no buttons yet, so none was ever pressed, and no menu is open while a script
        // runs. GetButtonPressed tells -1 for a button not pressed (a script treats 0 as the first button).
        interpreter.setHandler("GetButtonPressed", [](CallContext&) { return -1.0; });
        interpreter.setHandler("MenuMode", [](CallContext&) { return 0.0; });
    }
}
