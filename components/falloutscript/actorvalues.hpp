#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORVALUES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ACTORVALUES_H

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "saveaccess.hpp"
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
        constexpr int perception = 6;
        constexpr int endurance = 7;
        constexpr int charisma = 8;
        constexpr int intelligence = 9;
        constexpr int agility = 10;
        constexpr int luck = 11;
        constexpr int actionPoints = 12;
        constexpr int carryWeight = 13;
        constexpr int criticalChance = 14;
        constexpr int health = 16;
        constexpr int meleeDamage = 17;
        constexpr int speedMult = 21;
        constexpr int fatigue = 22;
        constexpr int karma = 23;
        constexpr int experience = 24;
        /// The conditions of the head and the limbs (perception, endurance, both arms, both legs and the brain)
        constexpr int firstCondition = 25;
        constexpr int lastCondition = 31;
        constexpr int firstSkill = 32;
        constexpr int sneak = 42;
        /// Barter first, unarmed last: the 14 skills of the records (the one in the middle of the end is survival in
        /// New Vegas and the unused throwing in Fallout 3)
        constexpr int lastSkill = 45;
        /// What hides a character from V.A.T.S. (it halves the chance to hit it): invisibility and chameleon
        constexpr int invisibility = 48;
        constexpr int chameleon = 49;
        /// The numbers from here are not actor values
        constexpr int count = 128;
    }

    /// What the records of the game say about the actor values of a character or creature before a script changes
    /// anything
    class ActorValueRecords
    {
    public:
        virtual ~ActorValueRecords() = default;

        /// The actor values that the record of the reference gives it, by their numbers. False when the host can not
        /// tell what the reference is: it is asked again by the next command that needs it. True with none when the
        /// reference has no record of an actor.
        virtual bool initialValues(FormId actor, std::vector<std::pair<int, double>>& values) = 0;

        /// The level that the record of the reference gives it for a player of that level (a record can take a share
        /// of it). False when the host can not tell what the reference is or it is not a character or creature.
        virtual bool levelOf(FormId /*actor*/, int /*playerLevel*/, int& /*level*/) { return false; }

        /// The value that the games work out for the actor from its other actor values (the health from its
        /// endurance and level, the action points from its agility, ...), when the actor value is one of those and the
        /// host can tell. The function gives the permanent value that the actor has of another actor value (its
        /// attributes, which a script may have changed).
        virtual std::optional<double> derivedValue(
            FormId /*actor*/, int /*value*/, const std::function<double(int)>& /*permanent*/)
        {
            return std::nullopt;
        }
    };

    /// The actor values of characters and creatures, as far as nothing else in the game keeps them yet. An actor value
    /// of a reference is its base value (what SetActorValue sets), a modifier that stays (ModActorValue) and the damage
    /// that scripts did to it (DamageActorValue, which RestoreActorValue takes back): the value the reference has now
    /// is the base plus the modifier minus the damage. A base that no script has set is the one its record gives,
    /// when the host knows one, and else a value that stands for a fresh character, which is what the conditions of
    /// the games ask for most (nothing crippled, nothing hurt). It can be used from any thread.
    class ActorValues
    {
        template <class T>
        friend struct SaveAccess;

    public:
        explicit ActorValues(ActorValueRecords* records = nullptr)
            : mRecords(records)
        {
        }

        static bool isValid(int value) { return value >= 0 && value < ActorValue::count; }

        /// Whether the games work the actor value out from others (see ActorValueRecords::derivedValue)
        static bool isDerived(int value);

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
        /// Adds to the base (a negative amount takes away), to no less than the minimum; the reading and the writing
        /// are one step, so that changes made at the same time all count
        void addBase(FormId actor, int value, double amount, double minimum);
        void modify(FormId actor, int value, double amount);
        /// Changes the permanent modifier so that the current value is the amount, whatever the base and the damage are
        void setCurrent(FormId actor, int value, double amount);
        /// Hurts the actor value (a negative amount takes damage back, down to none)
        void damage(FormId actor, int value, double amount);
        /// Takes damage back, up to what there is
        void restore(FormId actor, int value, double amount);
        /// Takes all the damage back, and the modifier that left nothing of the value
        void heal(FormId actor, int value);
        /// The level of a character or creature that its record gives for a player of that level, none when the
        /// records do not tell
        std::optional<int> levelOf(FormId actor, int playerLevel) const;

    private:
        struct Entry
        {
            std::optional<double> mBase; // none until a script sets it
            double mModifier = 0;
            double mDamage = 0;
        };

        /// What the actor has of the value until a script sets it: the record's, or the one worked out from other
        /// values, or else the starting value. The caller holds the lock.
        double startOf(FormId actor, int value) const;
        /// The base plus the modifier, for a caller that holds the lock
        double permanentOf(FormId actor, int value) const;
        /// An entry with its base filled in, for the actor value of that number
        double baseOf(FormId actor, const Entry& entry, int value) const;

        ActorValueRecords* mRecords;
        mutable std::mutex mMutex;
        std::map<std::pair<FormId, int>, Entry> mEntries;
        /// What the records said, once they could: by actor, then by actor value
        mutable std::map<FormId, std::map<int, double>> mFromRecords;
    };

    /// Gives the commands about actor values to the interpreter: GetActorValue, GetBaseActorValue,
    /// GetPermanentActorValue, SetActorValue, ModActorValue, ForceActorValue, DamageActorValue and RestoreActorValue,
    /// and those that depend on the health of a character: GetHealthPercentage, ResetHealth and ResurrectActor (a
    /// character that comes back has all its health), GetWeaponHealthPerc, and GetLevel. Karma and experience of the
    /// player are the ones that PlayerProgress keeps, and a character whose health is damaged to nothing is dead for
    /// ActorState. Those that the command table does not have are left out.
    void addActorValueCommands(
        Interpreter& interpreter, ActorValues& values, ActorState& actors, PlayerProgress& progress);
}

#endif
