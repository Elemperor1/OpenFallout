#include "packagecommands.hpp"

#include "arguments.hpp"

namespace FalloutScript
{
    void addPackageCommands(Interpreter& interpreter, PackageWorld& world)
    {
        interpreter.setHandler("AddScriptPackage", [&world](CallContext& call) {
            const FormId package = formArgument(call, 0);
            if (call.mReference != 0 && package != 0)
                world.addScriptPackage(call.mReference, package);
            return 0.0;
        });
        interpreter.setHandler("RemoveScriptPackage", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.removeScriptPackage(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("EvaluatePackage", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.evaluatePackage(call.mReference);
            return 0.0;
        });
        // Whether the package is the one the character follows now (it follows none while it is not in a loaded cell)
        interpreter.setHandler("GetIsCurrentPackage", [&world](CallContext& call) {
            const FormId package = formArgument(call, 0);
            return package != 0 && call.mReference != 0 && world.currentPackage(call.mReference) == package ? 1.0 : 0.0;
        });
        // ResetAI also drops what the character was doing: it gets up from the furniture it uses (the scripts of the
        // New Vegas intro use it to get Doc out of his chair). Combat and conversations are not kept here, so it comes
        // to the character looking at its packages again.
        interpreter.setHandler("ResetAI", [&world](CallContext& call) {
            if (call.mReference != 0)
                world.resetAI(call.mReference);
            return 0.0;
        });
    }
}
