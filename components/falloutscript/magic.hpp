#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MAGIC_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_MAGIC_H

#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <vector>

#include <components/esm4/script.hpp>

#include "conditions.hpp"
#include "interpreter.hpp"
#include "saveaccess.hpp"
#include "savestate.hpp"

namespace FalloutScript
{
    /// One effect of a spell, with what its magic effect says about it
    struct SpellEffect
    {
        /// The magic effect (MGEF)
        FormId mEffect = 0;
        /// The archetype of the magic effect: 0 modifies an actor value
        int mArchetype = 0;
        int mActorValue = -1;
        double mMagnitude = 0;
        /// In seconds, 0 for an effect that happens at once
        double mDuration = 0;
        /// Hurts the value (the flag of the magic effect)
        bool mDetrimental = false;
        /// The magic effect is hostile (its first flag)
        bool mHostile = false;
        /// A change that is undone when the effect ends (the flag of the magic effect)
        bool mRecover = false;
        /// The effect is on the character only when these hold for it (none: always)
        std::vector<ESM4::TargetCondition> mConditions;
    };

    struct SpellInfo
    {
        /// The type of the spell: 0 a spell that is cast, 1 a disease, 2 and 3 powers, 4 an ability, 5 a poison, 10 an
        /// addiction
        int mType = 0;
        std::vector<SpellEffect> mEffects;
    };

    /// What the records of the game say about spells
    class MagicRecords
    {
    public:
        virtual ~MagicRecords() = default;

        /// False for a form that is no spell
        virtual bool spell(FormId spell, SpellInfo& info) = 0;
        /// Whether the reference is a character or a creature
        virtual bool isActor(FormId reference) = 0;
    };

    /// The spells that characters have and the magic effects that are running on them, which HasMagicEffect asks
    /// about. Adding an ability, a disease or an addiction puts its effects on the character for good (the spell is
    /// listed, with its effects running until it is removed, and an effect that changes an actor value does so for as
    /// long); a spell of another type is only listed. Casting a spell on oneself runs its effects for their duration;
    /// the changes of actor values they make for that time are made by the caster of the host. It can be used from any
    /// thread.
    class Magic
    {
        template <class T>
        friend struct SaveAccess;

    public:
        /// Changes an actor value of a character by an amount
        using Modifier = std::function<void(FormId actor, int actorValue, double amount)>;
        /// Makes the changes that the effects of a cast spell have on the actor values of the character
        /// The effects are the ones whose conditions held when the spell was cast, and are not asked again
        using Caster = std::function<void(FormId actor, FormId spell, const std::vector<SpellEffect>& effects)>;
        /// Whether the conditions of an effect hold for the character who has it
        using ConditionCheck = std::function<bool(const std::vector<ESM4::TargetCondition>&, const ConditionContext&)>;

        explicit Magic(MagicRecords& records);

        void setModifier(Modifier modifier);
        void setCaster(Caster caster);
        /// Without it an effect that has conditions is never on a character
        void setConditions(ConditionCheck check);

        /// Forgets everything, as for a new game
        void clear();

        /// Gives the spell to the character. False when it has the spell already or the form is no spell.
        bool addSpell(FormId actor, FormId spell);
        /// Takes the spell away and ends the effects it put on. False when the character did not have it.
        bool removeSpell(FormId actor, FormId spell);
        bool hasSpell(FormId actor, FormId spell);
        std::vector<FormId> spells(FormId actor);

        /// The character casts the spell on itself. An ability is no spell to cast and does nothing.
        void castOnSelf(FormId actor, FormId spell);
        /// An effect of something else (an item that was used) runs on the character for the seconds
        void noteEffect(FormId actor, FormId effect, FormId source, double seconds);

        /// Whether a magic effect is running on the character
        bool hasEffect(FormId actor, FormId effect);
        /// Whether the character is the target of a spell that is running
        bool isSpellTarget(FormId actor, FormId spell);

        /// Lets the effects that have a duration run down by that many seconds
        void update(double seconds);

    private:
        struct Running
        {
            FormId mActor = 0;
            FormId mEffect = 0;
            /// The spell or the item it comes from
            FormId mSource = 0;
            /// The seconds left, 0 or less for one that lasts until its spell is removed
            double mRemaining = 0;
            /// What it changed an actor value by, which the end of the effect takes back
            int mActorValue = -1;
            double mDelta = 0;
        };

        /// Takes the effects of a spell whose conditions do not hold for the character out of its record
        void dropFailing(FormId actor, SpellInfo& info);

        MagicRecords& mRecords;
        std::mutex mMutex;
        Modifier mModifier;
        Caster mCaster;
        ConditionCheck mConditions;
        std::map<FormId, std::set<FormId>> mSpells;
        std::vector<Running> mRunning;
    };

    /// The section of a saved game that holds the spells and the effects that run
    constexpr std::uint32_t magicSectionVersion = 1;
    void writeMagic(Magic& magic, SaveWriter& out);
    void readMagic(Magic& magic, SaveReader& in, std::uint32_t version);

    /// Gives the commands AddSpell, RemoveSpell, CastImmediateOnSelf, Cast, HasMagicEffect and IsSpellTarget to the
    /// interpreter. Cast does nothing, as in the executables. Those that the command table does not have are left out.
    void addMagicCommands(Interpreter& interpreter, Magic& magic, MagicRecords& records);
}

#endif
