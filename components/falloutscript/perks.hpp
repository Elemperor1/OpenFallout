#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PERKS_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PERKS_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <components/esm4/script.hpp>

#include "conditions.hpp"
#include "script.hpp"

namespace FalloutScript
{
    class ActorValues;
    class PlayerProgress;

    /// The places where a perk changes a number that the game works out (the entry points of the format, by their
    /// numbers in the executables); New Vegas has more than are named here
    namespace EntryPoint
    {
        constexpr int weaponDamage = 0;
        constexpr int criticalChance = 1;
        constexpr int criticalDamage = 2;
        constexpr int limbDamage = 6;
        constexpr int hitChance = 8;
        constexpr int experience = 9;
        constexpr int gainedSkillPoints = 10;
        constexpr int bookSkillPoints = 11;
        constexpr int recoveredHealth = 12;
        constexpr int buyPrice = 17;
        constexpr int addictionChance = 23;
        constexpr int chemDuration = 25;
        constexpr int drinkingRadiation = 26;
        constexpr int enemyCriticalChance = 36;
        constexpr int actionPointRegen = 39;
        constexpr int actionPointCost = 40;
        constexpr int runSpeed = 42;
        constexpr int attackSpeed = 43;
        constexpr int thresholdDefender = 56;
        constexpr int thresholdAttacker = 58;
    }

    /// The conditions of a group of an entry. The run on says who they are about: see PerkEngine::contextFor.
    struct PerkConditionGroup
    {
        bool mHasRunOn = false;
        int mRunOn = 0;
        std::vector<ESM4::TargetCondition> mConditions;
    };

    /// One thing a perk does at one of its ranks
    struct PerkEntry
    {
        enum Type : std::uint8_t
        {
            QuestStage = 0,
            Ability = 1,
            EntryPoint = 2,
        };
        std::uint8_t mType = EntryPoint;
        /// The rank (counted from 0) that the entry belongs to
        int mRank = 0;
        int mPriority = 0;
        /// A quest stage: the quest and the stage; an ability: the spell
        FormId mQuest = 0;
        int mStage = 0;
        FormId mAbility = 0;
        /// An entry point: which, the function (1 set value, 2 add value, 3 multiply value, 8 add leveled list, 9
        /// add activate choice) and the number the function uses
        int mEntryPoint = 0;
        int mFunction = 0;
        double mValue = 0;
        std::vector<PerkConditionGroup> mGroups;
    };

    struct PerkInfo
    {
        FormId mId = 0;
        std::string mName;
        std::string mDescription;
        int mMinimumLevel = 0;
        int mRanks = 1;
        bool mPlayable = false;
        bool mTrait = false;
        bool mHidden = false;
        /// What the character needs to be offered the perk
        std::vector<ESM4::TargetCondition> mRequirements;
        std::vector<PerkEntry> mEntries;
    };

    /// A constant effect of an ability that adds to an actor value as long as the character has it
    struct AbilityEffect
    {
        int mActorValue = 0;
        double mMagnitude = 0;
    };

    /// Where the game finds the perks
    class PerkRecords
    {
    public:
        virtual ~PerkRecords() = default;

        /// What the record of the perk says; false for a form that is no perk
        virtual bool perk(FormId perk, PerkInfo& info) = 0;
        /// Every perk of the loaded files
        virtual std::vector<FormId> perks() = 0;
        /// The effects that change actor values for as long as the ability is there; false for a form that is no
        /// ability
        virtual bool abilityEffects(FormId ability, std::vector<AbilityEffect>& effects) = 0;
    };

    /// What perks do. A character who has a perk at a rank gets the entries of that rank: an entry point changes a
    /// number that the game works out (apply), an ability adds to actor values and a quest stage is set once, when
    /// the character first has the rank (refresh). It can be used from any thread.
    class PerkEngine
    {
    public:
        PerkEngine(PerkRecords& records, PlayerProgress& progress, ActorValues& values);

        /// Whether the conditions hold for the context
        using ConditionCheck = std::function<bool(const std::vector<ESM4::TargetCondition>&, const ConditionContext&)>;
        void setConditions(ConditionCheck check);
        /// Sets a quest to a stage (for the entries of the type quest stage)
        void setQuestSetter(std::function<void(FormId quest, int stage)> setter);

        /// The other reference of an exchange that a number is worked out for: the character that is hit when the
        /// owner strikes, the attacker when the owner is the one that is hit; the weapon of the exchange (the item
        /// for a price)
        struct Context
        {
            FormId mOther = 0;
            FormId mWeapon = 0;
        };

        /// What the number becomes in the hands of the character, as the games work it out: the entries of its perks
        /// for the entry point that have their conditions true, one after the other in the order of the games'
        /// lists (the perk gained last first, and of one perk the entry that comes last in the record first; a
        /// higher priority before a lower one), each setting the number, adding to it or multiplying it
        double apply(int entryPoint, FormId owner, double value, const Context& context);
        double apply(int entryPoint, FormId owner, double value);

        /// Brings the abilities and quest stages of the perks of the character up to date: those of a rank it has
        /// gained are applied and those of a rank it lost are taken back
        void refresh(FormId owner);

        /// A row of the list that the game shows when the player gains a perk
        struct Offer
        {
            FormId mPerk = 0;
            /// False for a perk that is shown but cannot be chosen yet (dimmed in the game)
            bool mAvailable = false;
        };

        /// The list of the level-up screen as the executables build it: the perks that are playable, not a trait,
        /// have a level to be offered at (a record with level 0 is never listed), are not at the highest rank
        /// already and, when the last condition of the record asks for the sex, pass that one. Hidden perks are not
        /// tested. A perk is available when it is of that level or below and all its conditions hold; the game
        /// lists the ones that are not too (its setting to hide them is off by default). Available perks first,
        /// then by level and by name.
        std::vector<Offer> levelUpList(FormId owner, int level);

        /// The perks the character can choose at a level: the available rows of the level-up list
        std::vector<FormId> offered(FormId owner, int level);

        /// The record of a perk (kept once read)
        bool info(FormId perk, PerkInfo& info);

        /// Forgets what was applied, as for a new game
        void clear();

    private:
        /// Who the conditions of a group are about. The number of a group is the tab of the entry point it belongs to:
        /// 0 for the owner of the perk, 1 and 2 for the arguments the entry point has after it (the executables'
        /// tables say which: a weapon, the other character of the exchange, or an item).
        ConditionContext contextFor(
            const PerkConditionGroup& group, int entryPoint, FormId owner, const Context& context) const;
        bool holds(const PerkEntry& entry, FormId owner, const Context& context) const;

        PerkRecords& mRecords;
        PlayerProgress& mProgress;
        ActorValues& mValues;
        ConditionCheck mConditions;
        std::function<void(FormId, int)> mQuestSetter;

        std::mutex mMutex;
        std::map<FormId, PerkInfo> mKnown;
        std::map<FormId, bool> mUnknown;
        /// What is applied to a character: by the perk, the rank it was applied for and the effects
        struct Applied
        {
            int mRank = 0;
            std::vector<AbilityEffect> mEffects;
        };
        std::map<std::pair<FormId, FormId>, Applied> mApplied;
    };
}

#endif
