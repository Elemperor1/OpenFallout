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
        /// The script that a magic effect runs on its character (an effect of the script archetype), 0 for any other
        virtual FormId effectScript(FormId /*effect*/) { return 0; }
        /// The kind of a magic item that is no spell, as the executables number them (0x824A28): 6 an enchantment, 7 an
        /// aid item, 8 an ingredient; -1 for a form that is none of them. A spell has the type of its record.
        virtual int itemKind(FormId /*item*/) { return -1; }
    };

    /// The spells that characters have and the magic effects that are running on them, which HasMagicEffect asks
    /// about. Adding an ability, a disease or an addiction puts its effects on the character for good (the spell is
    /// listed, with its effects running until it is removed, and an effect that changes an actor value does so for as
    /// long); a spell of another type is only listed. Casting a spell on oneself runs its effects for their duration;
    /// the changes of actor values they make for that time are made by the caster of the host. An effect of the script
    /// archetype runs the script of its magic effect on the character: its start block when the effect begins, its
    /// update block each time update is called while it runs, and its finish block when it runs out or is taken away
    /// (with the host's script runner). It can be used from any thread.
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
        /// What happens to an effect that has a script of its own
        enum class ScriptStage
        {
            /// The effect begins on the character
            Start,
            /// A frame (or an hour of rest) passes while it runs
            Update,
            /// It ends, run out or taken away
            Finish,
        };
        /// Tells one effect that runs from another, though they are of one script on one character (a spell cast twice,
        /// an item used twice): the script of each has variables of its own
        using EffectId = std::uint64_t;
        /// Told that an effect that a spell or an item put on a character ended before its time (dispelled), so that
        /// whoever keeps what it did can undo it: the character, the spell or item and the magic effect
        using Ender = std::function<void(FormId actor, FormId source, FormId effect)>;
        /// Runs the script of an effect for the stage, with the character it works on. For an update the last argument
        /// is the seconds that the update covers (the frame, or the hour of a rest, or the part of the step that the
        /// effect was still running when it ran out), which GetSecondsPassed tells the script; it is 0 for the others.
        using ScriptRunner
            = std::function<void(FormId actor, FormId script, ScriptStage stage, EffectId effect, double seconds)>;

        explicit Magic(MagicRecords& records);

        void setModifier(Modifier modifier);
        void setCaster(Caster caster);
        /// Without it an effect that has conditions is never on a character
        void setConditions(ConditionCheck check);
        /// Without it the scripts of effects do not run
        void setScriptRunner(ScriptRunner runner);
        /// Without it a dispelled effect leaves what the caster did as it is
        void setEnder(Ender ender);
        /// How often the conditions of the effects that an ability or a disease keeps are asked again while they run
        /// (fActiveEffectConditionUpdateInterval, 1 second in both games' files)
        void setConditionInterval(double seconds);

        /// Forgets everything, as for a new game
        void clear();

        /// Gives the spell to the character. False when it has the spell already or the form is no spell.
        bool addSpell(FormId actor, FormId spell);
        /// Takes the spell away and ends the effects it put on. False when the character did not have it.
        bool removeSpell(FormId actor, FormId spell);
        bool hasSpell(FormId actor, FormId spell);
        std::vector<FormId> spells(FormId actor);

        /// The character casts the spell on itself. An ability is no spell to cast and does nothing. The scale multiplies
        /// the magnitude of each effect (the executable's drink of water casts the water's spell with the scale that the
        /// perk entry point 26 gives, 0x5805C0).
        void castOnSelf(FormId actor, FormId spell, double magnitudeScale = 1.0);
        /// An effect of something else (an item that was used) runs on the character for the seconds
        void noteEffect(FormId actor, FormId effect, FormId source, double seconds, double magnitude = 0);

        /// Whether a magic effect is running on the character
        bool hasEffect(FormId actor, FormId effect);
        /// Whether the character is the target of a spell that is running
        bool isSpellTarget(FormId actor, FormId spell);

        /// Lets the effects that have a duration run down by that many seconds, and asks again the conditions of the
        /// effects that keep them once each interval
        void update(double seconds);

        /// Ends the effects that the spell or item has running on the character (and, when it is not 0, only those of
        /// that magic effect), whatever the spell is: what an effect changed is changed back, its script finishes, and
        /// the ender is told. The spell stays on the character's list. Returns how many it ended (Dispel,
        /// FalloutNV.exe 0x824400).
        int dispel(FormId actor, FormId source, FormId effect = 0);
        /// Ends the effects of the character that come from spells, powers, lesser powers, aid items and ingredients:
        /// not those of diseases, abilities, poisons and addictions, nor those of enchantments (DispelAllSpells,
        /// 0x8249D0; what the executable asks of an enchantment, the number at +0x34, is not read, so all of them stay).
        /// Returns how many it ended.
        int dispelAll(FormId actor);
        /// The sum of the magnitudes of the effects the spell or item has running on the character (GetSpellUsageNum
        /// adds the float at +0x1C of each active effect, 0x822EB0, which is set from the number the effect item starts
        /// with, 0x803DCF: its magnitude)
        double usage(FormId actor, FormId source);
        /// Asks again the conditions of all the effects the character's spells keep, at once (EvaluateSpellConditions,
        /// 0x5D7B50 calls the function that updates them all, 0x8255A0)
        void evaluateConditions(FormId actor);

        /// The spell or power the player has chosen (SelectPlayerSpell), 0 for none. Nothing reads it.
        void selectSpell(FormId spell);
        FormId selectedSpell();

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
            /// The script of the magic effect, 0 for one without (not saved: the records tell it)
            FormId mScript = 0;
            /// The magnitude of the effect, which GetSpellUsageNum adds up (saved from version 3)
            double mMagnitude = 0;
            /// The conditions the effect is on the character by, for an ability or a disease (not saved: the records tell
            /// them), and whether they hold: an effect they do not hold for is on the list and does nothing
            std::vector<ESM4::TargetCondition> mConditions;
            bool mEnabled = true;
            /// The seconds the conditions have been counted for since they were asked last
            double mSinceAsked = 0;
            /// Which effect it is among those that run (saved from version 2 of the section, which the saved
            /// variables of its script are held by; a game saved before that numbers them again when it is loaded)
            EffectId mId = 0;
        };

        /// Takes the effects of a spell whose conditions do not hold for the character out of its record
        void dropFailing(FormId actor, SpellInfo& info);
        /// Whether the conditions of the effect hold for the character (they do when it has none)
        bool holds(FormId actor, const SpellEffect& effect);
        /// Ends the entries that are chosen (outside the lock the scripts and the ender run in)
        int endEntries(FormId actor, const std::function<bool(const Running&)>& chosen);
        /// Asks the conditions of the entries again; all of them or those whose interval has passed
        void askAgain(const std::vector<EffectId>& ids);
        /// Whether the effect still runs
        bool still(EffectId id);

        MagicRecords& mRecords;
        std::mutex mMutex;
        Modifier mModifier;
        Caster mCaster;
        ConditionCheck mConditions;
        ScriptRunner mScripts;
        Ender mEnder;
        double mConditionInterval = 1.0;
        FormId mSelected = 0;
        std::map<FormId, std::set<FormId>> mSpells;
        std::vector<Running> mRunning;
        /// The last number given to an effect that runs; the numbers are not used again
        EffectId mLastId = 0;
    };

    /// The section of a saved game that holds the spells and the effects that run (version 2 holds the number of each
    /// effect that runs, version 3 its magnitude, whether it is on the character and the spell chosen with
    /// SelectPlayerSpell)
    constexpr std::uint32_t magicSectionVersion = 3;
    void writeMagic(Magic& magic, SaveWriter& out);
    void readMagic(Magic& magic, SaveReader& in, std::uint32_t version);

    /// Gives the commands AddSpell, RemoveSpell, CastImmediateOnSelf, Cast, HasMagicEffect, IsSpellTarget, Dispel,
    /// DispelAllSpells, EvaluateSpellConditions, GetSpellUsageNum and SelectPlayerSpell to the interpreter. Cast does nothing, as in the executables. Those that the command table does not have are left out.
    void addMagicCommands(Interpreter& interpreter, Magic& magic, MagicRecords& records);
}

#endif
