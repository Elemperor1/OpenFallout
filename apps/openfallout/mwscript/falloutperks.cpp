#include "falloutscripts.hpp"

#include <string_view>

#include <components/debug/debuglog.hpp>
#include <components/falloutscript/arguments.hpp>

namespace OFScript
{
    double FalloutScripts::perkNumber(
        int entryPoint, FalloutScript::FormId owner, double value, std::string_view what)
    {
        const double result = mPerks.apply(entryPoint, owner, value);
        // What a perk changed is told once for each change, with the number before and after
        if (result != value)
            Log(Debug::Info) << "Perk: entry point " << entryPoint << " (" << what << ") of " << FalloutScript::hex(owner)
                             << " changes " << value << " to " << result;
        return result;
    }

    double FalloutScripts::maxCarryWeight(FalloutScript::FormId actor)
    {
        // The value of Carry Weight, then the entry point 22 (FalloutNV.exe 0x8A0C20)
        return perkNumber(FalloutScript::EntryPoint::maxCarryWeight, actor,
            mActorValues.permanent(actor, FalloutScript::ActorValue::carryWeight), "carry weight");
    }
}
