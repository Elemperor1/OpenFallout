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
        /// The activator activates the target: the object does what it does by default, or, when `runOnActivate` is
        /// set, its script runs its OnActivate block in place of that. Nothing happens for an object or an activator
        /// the host does not know.
        virtual void activate(FormId target, FormId activator, bool runOnActivate) = 0;
        /// The form that the reference stands for (the character, creature or object it is of). False when the host
        /// can not tell, which is what a host that has no records says.
        virtual bool baseFormOf(FormId, FormId&) { return false; }
        /// The worldspace of the cell the reference is in; 0 for a reference inside a building or one the host does not
        /// know
        virtual FormId worldspaceOf(FormId) { return 0; }
        /// The reference that the record of the reference links to (0 for none). A link with a keyword is only found
        /// by the same keyword; the one with none is the one that is asked for with 0.
        virtual FormId linkedRef(FormId, FormId /*keyword*/) { return 0; }
        /// The hour of the game's clock, from 0 to 24 (what GetCurrentTime tells)
        virtual double gameHour() { return 0; }

        /// The days that have passed since the game began (the global GameDaysPassed, which counts a part of a day too)
        virtual double gameDaysPassed() { return 0; }
        /// The reference of the furniture that the actor sits on, lies on or leans against; 0 when it uses none (or the
        /// host does not know it)
        virtual FormId currentFurniture(FormId /*actor*/) { return 0; }
        /// Makes `count` objects of the base form next to the reference, `distance` units from it in the direction (0
        /// in front of it, 1 behind, 2 to its left, 3 to its right), and tells the reference of the first. 0 when
        /// nothing is made.
        virtual FormId placeAtMe(
            FormId /*reference*/, FormId /*base*/, int /*count*/, double /*distance*/, int /*direction*/)
        {
            return 0;
        }
    };

    /// Gives the commands of objects to the interpreter: Activate, Enable, Disable, MarkForDelete, GetDisabled,
    /// GetDistance, GetInCell, GetInSameCell, GetInWorldspace, GetLinkedRef, PlaceAtMe, GetSelf, GetActionRef,
    /// IsActionRef, GetIsReference, GetIsID, IsCurrentFurnitureRef, IsCurrentFurnitureObj, GetGlobalValue,
    /// GetCurrentTime, GetDayOfWeek, GetSecondsPassed and GetRandomPercent. Those that the command table does not have
    /// are left out. `random` gives the whole numbers from 0 to 99 for GetRandomPercent.
    void addObjectCommands(
        Interpreter& interpreter, ObjectScripts& objects, ObjectWorld& world, std::function<int()> random);
}

#endif
