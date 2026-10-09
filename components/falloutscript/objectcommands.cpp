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
        interpreter.setHandler(
            "GetSecondsPassed", [&objects](CallContext&) { return static_cast<double>(objects.secondsPassed()); });
        interpreter.setHandler("GetRandomPercent",
            [random = std::move(random)](CallContext&) { return random ? static_cast<double>(random()) : 0.0; });
    }
}
