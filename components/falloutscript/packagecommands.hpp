#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PACKAGECOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PACKAGECOMMANDS_H

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What the commands about the AI packages of characters ask of the game world. A character is told by the form id
    /// of its reference; the host knows which of them are in loaded cells and keeps what a script tells those that are
    /// not.
    class PackageWorld
    {
    public:
        virtual ~PackageWorld() = default;

        /// The character follows the package before all its own, whatever the time or the conditions say, until a
        /// script removes it
        virtual void addScriptPackage(FormId actor, FormId package) = 0;
        /// Removes the package that a script gave the character last
        virtual void removeScriptPackage(FormId actor) = 0;
        /// The character looks at its packages again, now and not at the next time it does
        virtual void evaluatePackage(FormId actor) = 0;
    };

    /// Gives the commands about packages to the interpreter: AddScriptPackage, RemoveScriptPackage and
    /// EvaluatePackage. Those that the command table does not have are left out.
    void addPackageCommands(Interpreter& interpreter, PackageWorld& world);
}

#endif
