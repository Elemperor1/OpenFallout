#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_OBJECTCOMMANDS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_OBJECTCOMMANDS_H

#include <functional>

#include "interpreter.hpp"
#include "objects.hpp"

namespace FalloutScript
{
    /// What the commands of objects ask of the game world. A reference is told by its form id; the host knows which of
    /// them are in loaded cells.
    class ObjectWorld
    {
    public:
        virtual ~ObjectWorld() = default;

        /// Whether the reference is disabled. A reference the host does not know is not.
        virtual bool isDisabled(FormId reference) = 0;
        /// Enables or disables the reference. A reference that is not in a loaded cell keeps the state until it is.
        virtual void setDisabled(FormId reference, bool disabled) = 0;
        /// The distance in game units between two references, false when it can not be told (one of them is not in a
        /// loaded cell, or they are in different spaces)
        virtual bool distance(FormId first, FormId second, double& result) = 0;
        /// The form id of the cell the reference is in, 0 for a reference the host does not know
        virtual FormId cellOf(FormId reference) = 0;
    };

    /// Gives the commands of objects to the interpreter: Enable, Disable, GetDisabled, GetDistance, GetInCell, GetSelf,
    /// GetActionRef, IsActionRef, GetIsReference, GetSecondsPassed and GetRandomPercent. Those that the command table
    /// does not have are left out. `random` gives the whole numbers from 0 to 99 for GetRandomPercent.
    void addObjectCommands(
        Interpreter& interpreter, ObjectScripts& objects, ObjectWorld& world, std::function<int()> random);
}

#endif
