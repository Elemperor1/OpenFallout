#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORVALUES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORVALUES_H

#include <map>
#include <mutex>
#include <optional>
#include <utility>

#include "script.hpp"

namespace FalloutScript
{
    class ActorState;
    class Interpreter;
    class PlayerProgress;

    /// The numbers scripts give to the actor values (the first argument of GetActorValue and the other commands about
    /// them), as the games number them: the ones that the rest of this component has to know by name
    namespace ActorValue
    {
        constexpr int strength = 5;
        constexpr int luck = 11;
        constexpr int health = 16;
        constexpr int speedMult = 21;
        constexpr int fatigue = 22;
        constexpr int karma = 23;
        constexpr int experience = 24;
        /// The conditions of the head and the limbs (perception, endurance, both arms, both legs and the brain)
        constexpr int firstCondition = 25;
        constexpr int lastCondition = 31;
        constexpr int firstSkill = 32;
        constexpr int lastSkill = 43;
        /// The numbers from here are not actor values
        constexpr int count = 128;
    }

    /// The actor values of characters and creatures, as far as nothing else in the game keeps them yet. An actor value
    /// of a reference is its base value (what SetActorValue sets), a modifier that stays (ModActorValue) and the damage
    /// that scripts did to it (DamageActorValue, which RestoreActorValue takes back): the value the reference has now
    /// is the base plus the modifier minus the damage. Nothing reads the records of the character yet, so an actor
    /// value that no script has set starts at a value that stands for a fresh character, which is what the conditions
    /// of the games ask for most (nothing crippled, nothing hurt). It can be used from any thread.
    class ActorValues
    {
    public:
        static bool isValid(int value) { return value >= 0 && value < ActorValue::count; }

        /// What a reference has of an actor value until a script sets it: 5 for each of the SPECIAL attributes, a full
        /// health, fatigue, speed and condition of the head and limbs, an average skill and nothing else
        static double startingValue(int value);

        /// Forgets everything, as for a new game
        void clear();

        /// The base value
        double base(FormId actor, int value) const;
        /// The base plus the modifier
        double permanent(FormId actor, int value) const;
        /// The permanent value minus the damage
        double current(FormId actor, int value) const;

        void setBase(FormId actor, int value, double amount);
        void modify(FormId actor, int value, double amount);
        /// Changes the base so that the current value is the amount, whatever the modifier and the damage are
        void setCurrent(FormId actor, int value, double amount);
        /// Hurts the actor value (a negative amount takes damage back)
        void damage(FormId actor, int value, double amount);
        /// Takes damage back, up to what there is
        void restore(FormId actor, int value, double amount);
        /// Takes all the damage back
        void heal(FormId actor, int value);

    private:
        struct Entry
        {
            std::optional<double> mBase; // none until a script sets it
            double mModifier = 0;
            double mDamage = 0;
        };

        /// An entry with its base filled in, for the actor value of that number
        static double baseOf(const Entry& entry, int value);

        mutable std::mutex mMutex;
        std::map<std::pair<FormId, int>, Entry> mEntries;
    };

    /// Gives the commands about actor values to the interpreter: GetActorValue, GetBaseActorValue,
    /// GetPermanentActorValue, SetActorValue, ModActorValue, ForceActorValue, DamageActorValue and RestoreActorValue,
    /// and those that depend on the health of a character: GetHealthPercentage and ResurrectActor (a character that
    /// comes back has all its health), and GetWeaponHealthPerc. Karma and experience of the player are the ones that
    /// PlayerProgress keeps, and a character whose health is damaged to nothing is dead for ActorState. Those that the
    /// command table does not have are left out.
    void addActorValueCommands(
        Interpreter& interpreter, ActorValues& values, ActorState& actors, PlayerProgress& progress);
}

#endif
