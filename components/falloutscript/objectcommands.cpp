#include "objectcommands.hpp"

#include <limits>

#include "arguments.hpp"

namespace FalloutScript
{
    namespace
    {
        /// What GetDistance tells for two objects that can not be measured, so that a script that asks whether
        /// something is near does not find it near
        constexpr double unmeasurable = std::numeric_limits<float>::max();
    }

    void addObjectCommands(
        Interpreter& interpreter, ObjectScripts& objects, ObjectWorld& world, std::function<int()> random)
    {
        // The activator is the one the call names, else the one that caused the event that runs now: the OnActivate
        // block of an object calls Activate with none to let the object do what it would have done without the block
        interpreter.setHandler("Activate", [&objects, &world](CallContext& call) {
            const FormId named = formArgument(call, 0);
            const FormId activator = named != 0 ? named : objects.actionReference();
            if (call.mReference != 0 && activator != 0)
                world.activate(call.mReference, activator, intArgument(call, 1) != 0);
            return 0.0;
        });
        interpreter.setHandler("Enable", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.setDisabled(call.mReference, false);
            return 0.0;
        });
        interpreter.setHandler("Disable", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.setDisabled(call.mReference, true);
            return 0.0;
        });
        // A reference that a script marks for deletion is gone for the game: nothing here deletes a reference, so it
        // is disabled
        interpreter.setHandler("MarkForDelete", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.setDisabled(call.mReference, true);
            return 0.0;
        });
        interpreter.setHandler("GetDisabled", [&world](CallContext& call) {
            return call.mReference != 0 && world.isDisabled(call.mReference) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetDistance", [&world](CallContext& call) {
            const FormId other = formArgument(call, 0);
            double result = 0;
            if (call.mReference == 0 || other == 0 || !world.distance(call.mReference, other, result))
                return unmeasurable;
            return result;
        });
        interpreter.setHandler("GetInCell", [&world](CallContext& call) {
            const FormId cell = formArgument(call, 0);
            return call.mReference != 0 && cell != 0 && world.cellOf(call.mReference) == cell ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetInSameCell", [&world](CallContext& call) {
            const FormId other = formArgument(call, 0);
            if (call.mReference == 0 || other == 0)
                return 0.0;
            const FormId cell = world.cellOf(call.mReference);
            return cell != 0 && world.cellOf(other) == cell ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetInWorldspace", [&world](CallContext& call) {
            const FormId worldspace = formArgument(call, 0);
            return call.mReference != 0 && worldspace != 0 && world.worldspaceOf(call.mReference) == worldspace ? 1.0
                                                                                                                : 0.0;
        });
        // The links of the games name a reference alone; Skyrim's keywords are what the argument would be, which the
        // command of the games does not have
        interpreter.setHandler("GetLinkedRef", [&world](CallContext& call) {
            return call.mReference != 0 ? static_cast<double>(world.linkedRef(call.mReference, formArgument(call, 0)))
                                        : 0.0;
        });
        // The games make the objects at once; a script can not ask for more than this many in a call
        interpreter.setHandler("PlaceAtMe", [&world](CallContext& call) {
            constexpr int mostAtOnce = 100;
            const FormId base = formArgument(call, 0);
            if (call.mReference == 0 || base == 0)
                return 0.0;
            const int count = std::clamp(call.mArguments.size() > 1 ? intArgument(call, 1) : 1, 1, mostAtOnce);
            const double asked = argument(call, 2);
            const double distance = std::isnan(asked) ? 0.0 : std::clamp(asked, -1.0e6, 1.0e6);
            const int direction = intArgument(call, 3);
            return static_cast<double>(world.placeAtMe(call.mReference, base, count, distance, direction));
        });
        interpreter.setHandler("GetSelf", [&objects](CallContext& call) {
            const FormId owner = call.mInstance.owner();
            return objects.has(owner) ? static_cast<double>(owner) : 0.0;
        });
        interpreter.setHandler(
            "GetActionRef", [&objects](CallContext&) { return static_cast<double>(objects.actionReference()); });
        interpreter.setHandler("IsActionRef", [&objects](CallContext& call) {
            const FormId other = formArgument(call, 0);
            return other != 0 && objects.actionReference() == other ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsReference", [](CallContext& call) {
            const FormId other = formArgument(call, 0);
            return other != 0 && call.mReference == other ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsID", [&world](CallContext& call) {
            const FormId wanted = formArgument(call, 0);
            FormId base = 0;
            return wanted != 0 && call.mReference != 0 && world.baseFormOf(call.mReference, base) && base == wanted
                ? 1.0
                : 0.0;
        });
        interpreter.setHandler(
            "GetSecondsPassed", [&objects](CallContext&) { return static_cast<double>(objects.secondsPassed()); });
        interpreter.setHandler("GetRandomPercent",
            [random = std::move(random)](CallContext&) { return random ? static_cast<double>(random()) : 0.0; });
    }
}
