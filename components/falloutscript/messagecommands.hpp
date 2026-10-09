#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGECOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MESSAGECOMMANDS_H

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What the commands that tell the player something ask of the game
    class MessageWorld
    {
    public:
        virtual ~MessageWorld() = default;

        /// Shows the player the message (a MESG record) that a script names
        virtual void showMessage(FormId message) = 0;
    };

    /// Gives the commands that show messages to the interpreter: ShowMessage. The command table of a game that does not
    /// have it is left as it is.
    void addMessageCommands(Interpreter& interpreter, MessageWorld& world);
}

#endif
