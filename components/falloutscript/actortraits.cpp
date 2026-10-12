#include "actortraits.hpp"

#include "arguments.hpp"
#include "interpreter.hpp"

namespace FalloutScript
{
    void addActorTraitCommands(Interpreter& interpreter, ActorTraitRecords& records)
    {
        const auto ask = [&records](FormId reference, ActorTraits& traits) {
            return reference != 0 && records.traitsOf(reference, traits);
        };

        // The sex is a choice of two: 0 for male and 1 for female
        interpreter.setHandler("GetIsSex", [ask](CallContext& call) {
            ActorTraits traits;
            if (!ask(call.mReference, traits))
                return 0.0;
            return traits.mFemale == (argument(call, 0) >= 0.5) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetPCIsSex", [ask](CallContext& call) {
            ActorTraits traits;
            if (!ask(call.mHost.player(), traits))
                return 0.0;
            return traits.mFemale == (argument(call, 0) >= 0.5) ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsVoiceType", [ask](CallContext& call) {
            ActorTraits traits;
            const FormId wanted = formArgument(call, 0);
            return wanted != 0 && ask(call.mReference, traits) && traits.mVoice == wanted ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsRace", [ask](CallContext& call) {
            ActorTraits traits;
            const FormId wanted = formArgument(call, 0);
            return wanted != 0 && ask(call.mReference, traits) && traits.mRace == wanted ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsClass", [ask](CallContext& call) {
            ActorTraits traits;
            const FormId wanted = formArgument(call, 0);
            return wanted != 0 && ask(call.mReference, traits) && traits.mClass == wanted ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsChild", [ask](CallContext& call) {
            ActorTraits traits;
            return ask(call.mReference, traits) && traits.mChild ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsCreature", [ask](CallContext& call) {
            ActorTraits traits;
            return ask(call.mReference, traits) && traits.mCreature ? 1.0 : 0.0;
        });
        interpreter.setHandler("GetIsCreatureType", [ask](CallContext& call) {
            ActorTraits traits;
            return ask(call.mReference, traits) && traits.mCreature && traits.mCreatureType == intArgument(call, 0)
                ? 1.0
                : 0.0;
        });
    }
}
