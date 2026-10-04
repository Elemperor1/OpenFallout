#ifndef GAME_SCRIPT_CONSOLEEXTENSIONS_H
#define GAME_SCRIPT_CONSOLEEXTENSIONS_H

namespace Compiler
{
    class Extensions;
}

namespace Interpreter
{
    class Interpreter;
}

namespace OFScript
{
    /// \brief Script functionality limited to the console
    namespace Console
    {
        void installOpcodes(Interpreter::Interpreter& interpreter) {}
    }
}

#endif
