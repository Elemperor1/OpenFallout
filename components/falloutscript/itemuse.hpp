#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ITEMUSE_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_ITEMUSE_H

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <components/esm4/script.hpp>

#include "conditions.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class ActorState;
    class ActorValues;
    class PerkEngine;
    class PlayerProgress;

    /// The actor values that are levels, which a detrimental effect raises and a restoring one lowers (radiation;
    /// dehydration, starvation and sleep deprivation in New Vegas)
    bool isLevelValue(int actorValue);

    /// One effect of an aid item
    struct ConsumableEffect
    {
        /// Of the magic effect: 0 modifies an actor value, 34 restores health like a stimpak, 35 restores the limbs
        int mArchetype = 0;
        int mActorValue = -1;
        double mMagnitude = 0;
        /// In seconds, 0 for an effect that happens at once
        double mDuration = 0;
        /// Hurts the value (the flag of the magic effect)
        bool mDetrimental = false;
        /// A change that is undone when the effect ends (the flag of the magic effect); otherwise it lasts
        bool mRecover = false;
        std::vector<ESM4::TargetCondition> mConditions;
    };

    /// What the records say about an item that is used up
    struct Consumable
    {
        std::vector<ConsumableEffect> mEffects;
        /// The chance in 0 to 1 that using it makes the character addicted, and what the addiction is
        double mAddictionChance = 0;
        FormId mWithdrawal = 0;
        bool mFood = false;
    };

    /// What the records say about a book
    struct BookInfo
    {
        /// The actor value of the skill it teaches, -1 for a book that is only to read
        int mSkill = -1;
    };

    class UseRecords
    {
    public:
        virtual ~UseRecords() = default;

        /// False for a form that is no aid item
        virtual bool consumable(FormId item, Consumable& result) = 0;
        /// False for a form that is no book
        virtual bool book(FormId item, BookInfo& result) = 0;
    };

    /// Using the items that are used up: aid items give their effects, skill books teach. Effects with a duration
    /// keep on in update. It can be used from any thread.
    class ItemUse
    {
    public:
        ItemUse(UseRecords& records, ActorState& state, ActorValues& values, PlayerProgress& progress);

        enum class Outcome
        {
            /// The form is no item that can be used up, or the character has none
            Nothing,
            /// An aid item was used up
            Consumed,
            /// A book was read: a skill book is used up, another book stays
            Read,
            /// A skill book that teaches nothing, the skill being at its highest
            NothingToLearn,
        };

        struct Result
        {
            Outcome mOutcome = Outcome::Nothing;
            /// The character became addicted to the item
            bool mAddicted = false;
            /// A skill book: which skill and by how much
            int mSkill = -1;
            double mGained = 0;
        };

        /// The character uses the item (an aid item or a book)
        Result use(FormId actor, FormId item);

        /// Lets the effects that have a duration go on by that many seconds, and gives the player the health that its
        /// perks recover in that time
        void update(double seconds);

        /// Perks change what an item does (the chance of addiction, how long a chem lasts, the skill points of a
        /// book)
        void setPerks(PerkEngine* perks);
        /// Whether the conditions of an effect hold for the character who uses the item
        void setConditions(
            std::function<bool(const std::vector<ESM4::TargetCondition>&, const ConditionContext&)> check);
        /// A number from 0 to 1 that decides whether an addiction happens (a random one by default)
        void setRandom(std::function<double()> random);
        /// The player, whose counters of the Pip-Boy count what is used
        void setPlayer(FormId player);
        /// The skill a book can raise to and the points a book gives when no perk changes them
        void setBookRules(double skillCap, double points);

        /// The items the character is addicted to (the form of the item), in the order they were taken
        std::vector<FormId> addictions(FormId actor) const;
        /// The character is free of the addiction
        void cure(FormId actor, FormId item);

        /// A character's effects that are running: the value they change, by how much and how long they last
        struct Active
        {
            FormId mActor = 0;
            int mActorValue = 0;
            double mMagnitude = 0;
            double mRemaining = 0;
            /// Whether it is a change that is undone (mMagnitude is added once and taken back at the end) or a number
            /// of points each second
            bool mTemporary = false;
        };
        std::vector<Active> active(FormId actor) const;

        /// Forgets everything, as for a new game
        void clear();

    private:
        void applyEffect(FormId actor, const ConsumableEffect& effect, double durationScale);
        void instant(FormId actor, int actorValue, double amount, bool detrimental);
        /// Kills a character with no health left, or makes it fall if it is one that falls (to be called with the
        /// transitions held)
        void dieIfSpent(FormId actor);

        UseRecords& mRecords;
        ActorState& mState;
        ActorValues& mValues;
        PlayerProgress& mProgress;
        PerkEngine* mPerks = nullptr;
        std::function<bool(const std::vector<ESM4::TargetCondition>&, const ConditionContext&)> mConditions;
        std::function<double()> mRandom;
        double mSkillCap = 100;
        double mBookPoints = 1;
        FormId mPlayer = 0;

        mutable std::mutex mMutex;
        std::vector<Active> mActive;
        std::vector<std::pair<FormId, FormId>> mAddictions;
        /// Held while a book reads the skill, takes the copy and raises the skill (taken before any other lock)
        std::mutex mBookMutex;
    };
}

#endif
