#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORTRAITS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORTRAITS_H

#include "script.hpp"

namespace FalloutScript
{
    class Interpreter;

    /// What makes a character or creature the kind it is, as the records of the game say (the base form, with what it
    /// takes from its templates)
    struct ActorTraits
    {
        FormId mRace = 0;
        FormId mClass = 0;
        /// The voice type (VTYP): the character's own or, when it has none, that of its race for its sex
        FormId mVoice = 0;
        bool mCreature = false;
        bool mFemale = false;
        bool mChild = false;
        /// The kind of creature (animal, mutated animal, mutated insect, abomination, crawler, ...) as the record has
        /// it
        int mCreatureType = 0;
    };

    class ActorTraitRecords
    {
    public:
        virtual ~ActorTraitRecords() = default;

        /// The traits of the character or creature (the player included). False when the host does not know the
        /// reference, which counts as none of the questions below holding.
        virtual bool traitsOf(FormId reference, ActorTraits& traits) = 0;
    };

    /// Gives the interpreter the commands that ask what a character is, which conditions of dialogue use most of all:
    /// GetIsSex, GetPCIsSex (the sex of the player), GetIsVoiceType, GetIsRace, GetIsClass, IsChild, GetIsCreature and
    /// GetIsCreatureType. Those that the command table does not have are left out.
    void addActorTraitCommands(Interpreter& interpreter, ActorTraitRecords& records);
}

#endif
