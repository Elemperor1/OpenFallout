#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/playerprogress.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <map>
#include <thread>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::ActorValues;
    using FalloutScript::ConditionContext;
    using FalloutScript::FormId;
    using FalloutScript::PerkEngine;
    using FalloutScript::PerkEntry;
    using FalloutScript::PerkInfo;
    using FalloutScript::PlayerProgress;
    namespace EntryPoint = FalloutScript::EntryPoint;
    namespace ActorValue = FalloutScript::ActorValue;

    constexpr FormId thePlayer = 0x14;
    constexpr FormId raider = 0x01000100;
    constexpr FormId rifle = 0x01000200;
    constexpr FormId swiftLearner = 0x0100A001;
    constexpr FormId bloodyMess = 0x0100A002;
    constexpr FormId strongBack = 0x0100A003;
    constexpr FormId hidden = 0x0100A004;
    constexpr FormId trait = 0x0100A005;
    constexpr FormId comprehension = 0x0100A006;
    constexpr FormId quester = 0x0100A007;
    constexpr FormId abilityOfStrongBack = 0x0100B001;

    PerkEntry entryPoint(int point, int function, double value, int rank = 0)
    {
        PerkEntry entry;
        entry.mType = PerkEntry::EntryPoint;
        entry.mEntryPoint = point;
        entry.mFunction = function;
        entry.mValue = value;
        entry.mRank = rank;
        return entry;
    }

    PerkInfo perk(FormId id, const char* name, int minimumLevel, int ranks = 1)
    {
        PerkInfo info;
        info.mId = id;
        info.mName = name;
        info.mMinimumLevel = minimumLevel;
        info.mRanks = ranks;
        info.mPlayable = true;
        return info;
    }

    class TestPerks final : public FalloutScript::PerkRecords
    {
    public:
        std::map<FormId, PerkInfo> mPerks;
        std::map<FormId, std::vector<FalloutScript::AbilityEffect>> mAbilities;

        bool perk(FormId id, PerkInfo& info) override
        {
            const auto found = mPerks.find(id);
            if (found == mPerks.end())
                return false;
            info = found->second;
            return true;
        }
        std::vector<FormId> perks() override
        {
            std::vector<FormId> all;
            for (const auto& [id, info] : mPerks)
                all.push_back(id);
            return all;
        }
        bool abilityEffects(FormId ability, std::vector<FalloutScript::AbilityEffect>& effects) override
        {
            const auto found = mAbilities.find(ability);
            if (found == mAbilities.end())
                return false;
            effects = found->second;
            return true;
        }
    };

    struct Fixture
    {
        TestPerks mRecords;
        PlayerProgress mProgress;
        ActorValues mValues;
        PerkEngine mEngine{ mRecords, mProgress, mValues };
    };

    TEST(FalloutScriptPerksTest, leavesANumberAloneWithoutPerks)
    {
        Fixture f;
        EXPECT_EQ(f.mEngine.apply(EntryPoint::experience, thePlayer, 100), 100);
    }

    TEST(FalloutScriptPerksTest, multipliesWithThePerkAtItsRank)
    {
        Fixture f;
        PerkInfo info = perk(swiftLearner, "Swift Learner", 2, 3);
        info.mEntries = { entryPoint(EntryPoint::experience, 3, 1.1, 0), entryPoint(EntryPoint::experience, 3, 1.2, 1),
            entryPoint(EntryPoint::experience, 3, 1.3, 2) };
        f.mRecords.mPerks[swiftLearner] = info;

        f.mProgress.addPerk(thePlayer, swiftLearner);
        EXPECT_NEAR(f.mEngine.apply(EntryPoint::experience, thePlayer, 100), 110, 1e-9);
        f.mProgress.addPerk(thePlayer, swiftLearner);
        EXPECT_NEAR(f.mEngine.apply(EntryPoint::experience, thePlayer, 100), 120, 1e-9);
        f.mProgress.addPerk(thePlayer, swiftLearner);
        EXPECT_NEAR(f.mEngine.apply(EntryPoint::experience, thePlayer, 100), 130, 1e-9);
        // a point of another kind is not changed
        EXPECT_EQ(f.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 100), 100);
        // another character does not have the perk
        EXPECT_EQ(f.mEngine.apply(EntryPoint::experience, raider, 100), 100);
    }

    TEST(FalloutScriptPerksTest, theEntriesOfAPerkGainedLastComeFirst)
    {
        Fixture f;
        PerkInfo setter = perk(bloodyMess, "Bloody Mess", 6);
        setter.mEntries = { entryPoint(EntryPoint::weaponDamage, 1, 20) };
        PerkInfo multiplier = perk(strongBack, "Strong Back", 8);
        multiplier.mEntries = { entryPoint(EntryPoint::weaponDamage, 3, 2) };
        f.mRecords.mPerks[bloodyMess] = setter;
        f.mRecords.mPerks[strongBack] = multiplier;

        // The multiplier is first in the list, and the set of the older perk overwrites what it made
        f.mProgress.addPerk(thePlayer, bloodyMess);
        f.mProgress.addPerk(thePlayer, strongBack);
        EXPECT_EQ(f.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 10), 20);

        // The other way round, the set comes first and the multiplier doubles it
        Fixture other;
        other.mRecords.mPerks = f.mRecords.mPerks;
        other.mProgress.addPerk(thePlayer, strongBack);
        other.mProgress.addPerk(thePlayer, bloodyMess);
        EXPECT_EQ(other.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 10), 40);
    }

    TEST(FalloutScriptPerksTest, ofOnePerkTheLastEntryComesFirst)
    {
        Fixture f;
        PerkInfo info = perk(bloodyMess, "Bloody Mess", 6);
        info.mEntries = { entryPoint(EntryPoint::weaponDamage, 3, 2), entryPoint(EntryPoint::weaponDamage, 2, 5) };
        f.mRecords.mPerks[bloodyMess] = info;
        f.mProgress.addPerk(thePlayer, bloodyMess);

        // add 5, then multiply by 2
        EXPECT_EQ(f.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 10), 30);
    }

    TEST(FalloutScriptPerksTest, aHigherPriorityComesBeforeALowerOne)
    {
        Fixture f;
        PerkInfo info = perk(bloodyMess, "Bloody Mess", 6);
        info.mEntries = { entryPoint(EntryPoint::weaponDamage, 2, 5), entryPoint(EntryPoint::weaponDamage, 1, 20) };
        info.mEntries[0].mPriority = 3;
        f.mRecords.mPerks[bloodyMess] = info;
        f.mProgress.addPerk(thePlayer, bloodyMess);

        // add 5 (priority 3) and then set to 20, whatever the order in the record
        EXPECT_EQ(f.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 10), 20);
    }

    TEST(FalloutScriptPerksTest, appliesAnEntryOnlyWhenItsConditionsHold)
    {
        Fixture f;
        PerkInfo info = perk(bloodyMess, "Bloody Mess", 6);
        info.mEntries = { entryPoint(EntryPoint::weaponDamage, 3, 1.5) };
        FalloutScript::PerkConditionGroup group;
        group.mConditions.resize(1);
        info.mEntries[0].mGroups = { group };
        f.mRecords.mPerks[bloodyMess] = info;
        f.mProgress.addPerk(thePlayer, bloodyMess);

        std::vector<ConditionContext> asked;
        bool answer = false;
        f.mEngine.setConditions([&](const auto&, const ConditionContext& context) {
            asked.push_back(context);
            return answer;
        });
        PerkEngine::Context context;
        context.mOther = raider;
        EXPECT_EQ(f.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 10, context), 10);
        answer = true;
        EXPECT_EQ(f.mEngine.apply(EntryPoint::weaponDamage, thePlayer, 10, context), 15);
        ASSERT_EQ(asked.size(), 2u);
        EXPECT_EQ(asked[0].mSubject, thePlayer);
        EXPECT_EQ(asked[0].mTarget, raider);
    }

    namespace
    {
        FalloutScript::PerkConditionGroup tab(int number)
        {
            FalloutScript::PerkConditionGroup group;
            group.mHasRunOn = true;
            group.mRunOn = number;
            group.mConditions.resize(1);
            return group;
        }

        std::vector<ConditionContext> askAbout(int point, const PerkEngine::Context& context, FormId owner)
        {
            Fixture f;
            PerkInfo info = perk(bloodyMess, "Bloody Mess", 6);
            info.mEntries = { entryPoint(point, 3, 1.5) };
            info.mEntries[0].mGroups = { tab(0), tab(1), tab(2) };
            f.mRecords.mPerks[bloodyMess] = info;
            f.mProgress.addPerk(owner, bloodyMess);
            std::vector<ConditionContext> asked;
            f.mEngine.setConditions([&](const auto&, const ConditionContext& asking) {
                asked.push_back(asking);
                return true;
            });
            f.mEngine.apply(point, owner, 10, context);
            return asked;
        }
    }

    TEST(FalloutScriptPerksTest, theTabsOfDamageAreTheOwnerTheWeaponAndTheTarget)
    {
        PerkEngine::Context context;
        context.mOther = raider;
        context.mWeapon = rifle;
        const std::vector<ConditionContext> asked = askAbout(EntryPoint::weaponDamage, context, thePlayer);
        ASSERT_EQ(asked.size(), 3u);
        EXPECT_EQ(asked[0].mSubject, thePlayer);
        EXPECT_EQ(asked[1].mSubject, rifle);
        EXPECT_EQ(asked[2].mSubject, raider);
        EXPECT_EQ(asked[2].mTarget, thePlayer);
    }

    TEST(FalloutScriptPerksTest, theTabsOfLimbDamageAreTheVictimTheAttackerAndItsWeapon)
    {
        PerkEngine::Context context;
        context.mOther = raider;
        context.mWeapon = rifle;
        const std::vector<ConditionContext> asked = askAbout(EntryPoint::limbDamage, context, thePlayer);
        ASSERT_EQ(asked.size(), 3u);
        EXPECT_EQ(asked[0].mSubject, thePlayer);
        EXPECT_EQ(asked[1].mSubject, raider);
        EXPECT_EQ(asked[2].mSubject, rifle);
    }

    TEST(FalloutScriptPerksTest, aTabThatTheEntryPointDoesNotHaveIsAboutTheOwner)
    {
        PerkEngine::Context context;
        context.mOther = raider;
        context.mWeapon = rifle;
        const std::vector<ConditionContext> asked = askAbout(EntryPoint::experience, context, thePlayer);
        ASSERT_EQ(asked.size(), 3u);
        EXPECT_EQ(asked[1].mSubject, thePlayer);
        EXPECT_EQ(asked[2].mSubject, thePlayer);
    }

    TEST(FalloutScriptPerksTest, offersThePerksThatAreSelectable)
    {
        Fixture f;
        f.mRecords.mPerks[swiftLearner] = perk(swiftLearner, "Swift Learner", 2, 3);
        f.mRecords.mPerks[bloodyMess] = perk(bloodyMess, "Bloody Mess", 6);
        f.mRecords.mPerks[comprehension] = perk(comprehension, "Comprehension", 4);
        PerkInfo notPlayable = perk(strongBack, "Strong Back", 2);
        notPlayable.mPlayable = false;
        PerkInfo traitPerk = perk(trait, "Trait", 1);
        traitPerk.mTrait = true;
        f.mRecords.mPerks[strongBack] = notPlayable;
        f.mRecords.mPerks[trait] = traitPerk;

        EXPECT_THAT(f.mEngine.offered(thePlayer, 4), ElementsAre(swiftLearner, comprehension));
        EXPECT_THAT(f.mEngine.offered(thePlayer, 6), ElementsAre(swiftLearner, comprehension, bloodyMess));

        // a perk at its highest rank is not offered, one with ranks left is
        f.mProgress.addPerk(thePlayer, comprehension);
        f.mProgress.addPerk(thePlayer, swiftLearner);
        EXPECT_THAT(f.mEngine.offered(thePlayer, 6), ElementsAre(swiftLearner, bloodyMess));
        f.mProgress.addPerk(thePlayer, swiftLearner);
        f.mProgress.addPerk(thePlayer, swiftLearner);
        EXPECT_THAT(f.mEngine.offered(thePlayer, 6), ElementsAre(bloodyMess));
    }

    TEST(FalloutScriptPerksTest, theLevelUpListShowsThePerksThatCannotBeChosenYet)
    {
        Fixture f;
        f.mRecords.mPerks[swiftLearner] = perk(swiftLearner, "Swift Learner", 2);
        f.mRecords.mPerks[comprehension] = perk(comprehension, "Comprehension", 4);
        f.mRecords.mPerks[bloodyMess] = perk(bloodyMess, "Bloody Mess", 6);
        // A perk with no level is never listed, a hidden one is (the list does not test the flag)
        f.mRecords.mPerks[strongBack] = perk(strongBack, "Strong Back", 0);
        PerkInfo hiddenPerk = perk(hidden, "Hidden", 2);
        hiddenPerk.mHidden = true;
        f.mRecords.mPerks[hidden] = hiddenPerk;

        const auto list = f.mEngine.levelUpList(thePlayer, 3);
        ASSERT_EQ(list.size(), 4u);
        EXPECT_EQ(list[0].mPerk, hidden);
        EXPECT_TRUE(list[0].mAvailable);
        EXPECT_EQ(list[1].mPerk, swiftLearner);
        EXPECT_TRUE(list[1].mAvailable);
        // Too high a level: listed after the ones that can be chosen
        EXPECT_EQ(list[2].mPerk, comprehension);
        EXPECT_FALSE(list[2].mAvailable);
        EXPECT_EQ(list[3].mPerk, bloodyMess);
        EXPECT_FALSE(list[3].mAvailable);
        EXPECT_THAT(f.mEngine.offered(thePlayer, 3), ElementsAre(hidden, swiftLearner));
    }

    TEST(FalloutScriptPerksTest, theLevelUpListHidesAPerkOnlyForTheSexOfItsLastCondition)
    {
        Fixture f;
        PerkInfo forWomen = perk(swiftLearner, "Swift Learner", 2);
        forWomen.mRequirements.resize(2);
        forWomen.mRequirements[0].functionIndex = ESM4::FUN_GetLevel;
        forWomen.mRequirements[1].functionIndex = ESM4::FUN_GetIsSex;
        // The sex is not the last condition: it only decides whether the perk is available
        PerkInfo sexFirst = perk(comprehension, "Comprehension", 2);
        sexFirst.mRequirements.resize(2);
        sexFirst.mRequirements[0].functionIndex = ESM4::FUN_GetIsSex;
        sexFirst.mRequirements[1].functionIndex = ESM4::FUN_GetLevel;
        f.mRecords.mPerks[swiftLearner] = forWomen;
        f.mRecords.mPerks[comprehension] = sexFirst;
        bool rightSex = false;
        bool others = true;
        f.mEngine.setConditions([&](const std::vector<ESM4::TargetCondition>& conditions, const ConditionContext&) {
            if (conditions.size() == 1 && conditions[0].functionIndex == ESM4::FUN_GetIsSex)
                return rightSex;
            return others && rightSex;
        });

        // Not the sex: the perk of the last condition is not listed at all
        auto list = f.mEngine.levelUpList(thePlayer, 5);
        ASSERT_EQ(list.size(), 1u);
        EXPECT_EQ(list[0].mPerk, comprehension);
        EXPECT_FALSE(list[0].mAvailable);

        // The right sex, but another condition fails: listed, and not available
        rightSex = true;
        others = false;
        list = f.mEngine.levelUpList(thePlayer, 5);
        ASSERT_EQ(list.size(), 2u);
        EXPECT_FALSE(list[0].mAvailable);
        EXPECT_FALSE(list[1].mAvailable);

        others = true;
        EXPECT_THAT(f.mEngine.offered(thePlayer, 5), UnorderedElementsAre(swiftLearner, comprehension));
    }

    TEST(FalloutScriptPerksTest, offersAPerkOnlyWhenItsRequirementsHold)
    {
        Fixture f;
        PerkInfo info = perk(comprehension, "Comprehension", 4);
        info.mRequirements.resize(1);
        f.mRecords.mPerks[comprehension] = info;

        EXPECT_TRUE(f.mEngine.offered(thePlayer, 10).empty());
        bool answer = true;
        f.mEngine.setConditions([&](const auto&, const ConditionContext& context) {
            EXPECT_EQ(context.mSubject, thePlayer);
            return answer;
        });
        EXPECT_THAT(f.mEngine.offered(thePlayer, 10), ElementsAre(comprehension));
        answer = false;
        EXPECT_TRUE(f.mEngine.offered(thePlayer, 10).empty());
    }

    TEST(FalloutScriptPerksTest, anAbilityAddsToActorValuesForAsLongAsThePerkIsThere)
    {
        Fixture f;
        PerkInfo info = perk(strongBack, "Strong Back", 8);
        PerkEntry ability;
        ability.mType = PerkEntry::Ability;
        ability.mAbility = abilityOfStrongBack;
        info.mEntries = { ability };
        f.mRecords.mPerks[strongBack] = info;
        f.mRecords.mAbilities[abilityOfStrongBack] = { { ActorValue::carryWeight, 50 } };

        const double before = f.mValues.current(thePlayer, ActorValue::carryWeight);
        f.mProgress.addPerk(thePlayer, strongBack);
        f.mEngine.refresh(thePlayer);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before + 50);
        // asking again does not add it twice
        f.mEngine.refresh(thePlayer);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before + 50);

        f.mProgress.removePerk(thePlayer, strongBack);
        f.mEngine.refresh(thePlayer);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before);
    }

    TEST(FalloutScriptPerksTest, refreshesAtTheSameTimeApplyAnAbilityOnce)
    {
        constexpr int threads = 6;
        for (int round = 0; round < 100; ++round)
        {
            Fixture f;
            PerkInfo info = perk(strongBack, "Strong Back", 8);
            PerkEntry ability;
            ability.mType = PerkEntry::Ability;
            ability.mAbility = abilityOfStrongBack;
            info.mEntries = { ability };
            f.mRecords.mPerks[strongBack] = info;
            f.mRecords.mAbilities[abilityOfStrongBack] = { { ActorValue::carryWeight, 50 } };
            f.mProgress.addPerk(thePlayer, strongBack);
            const double before = f.mValues.current(thePlayer, ActorValue::carryWeight);

            // Every one of them waits for the others, so that they all come to the check of what is applied together
            std::atomic<int> ready = 0;
            std::vector<std::thread> refreshing;
            for (int i = 0; i < threads; ++i)
                refreshing.emplace_back([&] {
                    ++ready;
                    while (ready < threads)
                        std::this_thread::yield();
                    f.mEngine.refresh(thePlayer);
                });
            for (std::thread& thread : refreshing)
                thread.join();

            ASSERT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before + 50) << "round " << round;
        }
    }

    TEST(FalloutScriptPerksTest, aQuestStageThatGivesAnotherPerkDoesNotWaitForTheRefreshItCameFrom)
    {
        Fixture f;
        PerkInfo info = perk(quester, "Quester", 1);
        PerkEntry stage;
        stage.mType = PerkEntry::QuestStage;
        stage.mQuest = 0x01002000;
        stage.mStage = 30;
        info.mEntries = { stage };
        f.mRecords.mPerks[quester] = info;
        PerkInfo strong = perk(strongBack, "Strong Back", 8);
        PerkEntry ability;
        ability.mType = PerkEntry::Ability;
        ability.mAbility = abilityOfStrongBack;
        strong.mEntries = { ability };
        f.mRecords.mPerks[strongBack] = strong;
        f.mRecords.mAbilities[abilityOfStrongBack] = { { ActorValue::carryWeight, 50 } };

        // The stage runs a script that gives another perk and refreshes (the stage is set with nothing held)
        f.mEngine.setQuestSetter([&](FormId, int) {
            f.mProgress.addPerk(thePlayer, strongBack);
            f.mEngine.refresh(thePlayer);
        });
        const double before = f.mValues.current(thePlayer, ActorValue::carryWeight);
        f.mProgress.addPerk(thePlayer, quester);
        f.mEngine.refresh(thePlayer);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before + 50);
    }

    TEST(FalloutScriptPerksTest, aHigherRankReplacesTheAbilityOfTheRankBefore)
    {
        Fixture f;
        PerkInfo info = perk(strongBack, "Strong Back", 8, 2);
        PerkEntry first;
        first.mType = PerkEntry::Ability;
        first.mAbility = abilityOfStrongBack;
        first.mRank = 0;
        PerkEntry second = first;
        second.mAbility = abilityOfStrongBack + 1;
        second.mRank = 1;
        info.mEntries = { first, second };
        f.mRecords.mPerks[strongBack] = info;
        f.mRecords.mAbilities[abilityOfStrongBack] = { { ActorValue::carryWeight, 50 } };
        f.mRecords.mAbilities[abilityOfStrongBack + 1] = { { ActorValue::carryWeight, 120 } };

        const double before = f.mValues.current(thePlayer, ActorValue::carryWeight);
        f.mProgress.addPerk(thePlayer, strongBack);
        f.mEngine.refresh(thePlayer);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before + 50);
        f.mProgress.addPerk(thePlayer, strongBack);
        f.mEngine.refresh(thePlayer);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::carryWeight), before + 120);
    }

    TEST(FalloutScriptPerksTest, setsTheQuestStageOfAnEntryOnceWhenTheRankIsGained)
    {
        Fixture f;
        PerkInfo info = perk(quester, "Quester", 1);
        PerkEntry stage;
        stage.mType = PerkEntry::QuestStage;
        stage.mQuest = 0x01002000;
        stage.mStage = 30;
        info.mEntries = { stage };
        f.mRecords.mPerks[quester] = info;
        std::vector<std::pair<FormId, int>> set;
        f.mEngine.setQuestSetter([&set](FormId quest, int number) { set.emplace_back(quest, number); });

        f.mEngine.refresh(thePlayer);
        EXPECT_TRUE(set.empty());
        f.mProgress.addPerk(thePlayer, quester);
        f.mEngine.refresh(thePlayer);
        f.mEngine.refresh(thePlayer);
        ASSERT_EQ(set.size(), 1u);
        const std::pair<FormId, int> expected(0x01002000, 30);
        EXPECT_EQ(set[0], expected);
    }
}
