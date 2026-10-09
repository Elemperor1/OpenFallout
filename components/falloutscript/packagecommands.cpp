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
    }
}
