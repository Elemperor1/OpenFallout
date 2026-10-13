#include "objectcommands.hpp"

#include <cctype>
#include <cmath>
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

    bool cellNameBegins(std::string_view name, std::string_view prefix)
    {
        if (prefix.empty() || name.size() < prefix.size())
            return false;
        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            const unsigned char a = static_cast<unsigned char>(name[i]);
            const unsigned char b = static_cast<unsigned char>(prefix[i]);
            if (std::tolower(a) != std::tolower(b))
                return false;
        }
        return true;
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
            return call.mReference != 0 && cell != 0 && world.inCell(call.mReference, cell) ? 1.0 : 0.0;
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
        interpreter.setHandler("IsCurrentFurnitureRef", [&world](CallContext& call) {
            const FormId wanted = formArgument(call, 0);
            return wanted != 0 && call.mReference != 0 && world.currentFurniture(call.mReference) == wanted ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsCurrentFurnitureObj", [&world](CallContext& call) {
            const FormId wanted = formArgument(call, 0);
            const FormId furniture = call.mReference != 0 ? world.currentFurniture(call.mReference) : 0;
            FormId base = 0;
            return wanted != 0 && furniture != 0 && world.baseFormOf(furniture, base) && base == wanted ? 1.0 : 0.0;
        });
        // The games count a character that is getting into the furniture, in it, and getting out of it (1, 2, 3, 4 for
        // sitting, 2 while it sits down, 3 once it has, 4 while it gets up); there is no animation here, so the
        // character is settled (3) all the time it uses a piece of furniture
        interpreter.setHandler("GetSitting", [&world](CallContext& call) {
            const int kind = call.mReference != 0 ? world.currentFurnitureKind(call.mReference) : 0;
            return kind >= 10 && kind <= 20 ? 3.0 : 0.0;
        });
        interpreter.setHandler("GetSleeping", [&world](CallContext& call) {
            const int kind = call.mReference != 0 ? world.currentFurnitureKind(call.mReference) : 0;
            return kind >= 1 && kind <= 9 ? 3.0 : 0.0;
        });
        interpreter.setHandler("GetGlobalValue", [](CallContext& call) {
            double value = 0;
            const FormId global = formArgument(call, 0);
            return global != 0 && call.mHost.getGlobal(global, value) ? value : 0.0;
        });
        interpreter.setHandler("GetCurrentTime", [&world](CallContext&) { return world.gameHour(); });
        // 0 is Sunday. The game begins on a Monday, as the schedules of the packages take it.
        interpreter.setHandler("GetDayOfWeek", [&world](CallContext&) {
            const double days = std::floor(world.gameDaysPassed());
            return std::isfinite(days) ? static_cast<double>((((static_cast<long long>(days) + 1) % 7) + 7) % 7) : 0.0;
        });
        interpreter.setHandler(
            "GetSecondsPassed", [&objects](CallContext&) { return static_cast<double>(objects.secondsPassed()); });
        interpreter.setHandler("GetRandomPercent",
            [random = std::move(random)](CallContext&) { return random ? static_cast<double>(random()) : 0.0; });
    }
}
