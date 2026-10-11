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
    }
}
