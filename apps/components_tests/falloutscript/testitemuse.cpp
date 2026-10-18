#include <components/falloutscript/actors.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/itemuse.hpp>
#include <components/falloutscript/perks.hpp>
#include <components/falloutscript/playerprogress.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <map>
#include <thread>
#include <vector>

namespace
{
    using namespace testing;
    using FalloutScript::ActorState;
    using FalloutScript::ActorValues;
    using FalloutScript::Consumable;
    using FalloutScript::ConsumableEffect;
    using FalloutScript::FormId;
    using FalloutScript::ItemUse;
    using FalloutScript::PlayerProgress;
    namespace ActorValue = FalloutScript::ActorValue;

    constexpr FormId thePlayer = 0x14;
    constexpr FormId stimpak = 0x01000001;
    constexpr FormId radAway = 0x01000002;
    constexpr FormId radX = 0x01000003;
    constexpr FormId mentats = 0x01000004;
    constexpr FormId buffout = 0x01000005;
    constexpr FormId nukaCola = 0x01000006;
    constexpr FormId jet = 0x01000007;
    constexpr FormId guns = 0x01000008;
    constexpr FormId novel = 0x01000009;
    constexpr FormId superStimpak = 0x0100000A;
    constexpr FormId poison = 0x0100000B;
    constexpr FormId victim = 0x01000500;
    constexpr FormId withdrawal = 0x01000100;

    constexpr int radiationLevel = 54;
    constexpr int radiationResistance = 20;
    constexpr int smallGuns = 33;

    ConsumableEffect effect(int actorValue, double magnitude, double duration = 0, bool detrimental = false,
        bool recover = false, int archetype = 0)
    {
        ConsumableEffect result;
        result.mArchetype = archetype;
        result.mActorValue = actorValue;
        result.mMagnitude = magnitude;
        result.mDuration = duration;
        result.mDetrimental = detrimental;
        result.mRecover = recover;
        return result;
    }

    class NoRecords : public FalloutScript::ActorRecords
    {
    public:
        bool initialItems(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
        bool initialFactions(FormId, std::vector<std::pair<FormId, int>>&) override { return true; }
    };

    class TestRecords final : public FalloutScript::UseRecords
    {
    public:
        std::map<FormId, Consumable> mConsumables;
        std::map<FormId, FalloutScript::BookInfo> mBooks;
        /// Runs when the use of an item looks its record up, which is after the character was found to have the item:
        /// where a use on another thread can get in
        std::function<void()> mWhileUsing;

        bool consumable(FormId item, Consumable& result) override
        {
            const auto found = mConsumables.find(item);
            if (found == mConsumables.end())
                return false;
            result = found->second;
            if (mWhileUsing)
                mWhileUsing();
            return true;
        }
        bool book(FormId item, FalloutScript::BookInfo& result) override
        {
            const auto found = mBooks.find(item);
            if (found == mBooks.end())
                return false;
            result = found->second;
            if (mWhileUsing)
                mWhileUsing();
            return true;
        }
    };

    struct Fixture
    {
        NoRecords mActorRecords;
        TestRecords mRecords;
        ActorState mState{ mActorRecords };
        ActorValues mValues;
        PlayerProgress mProgress;
        ItemUse mUse{ mRecords, mState, mValues, mProgress };

        Fixture()
        {
            mUse.setPlayer(thePlayer);
            mRecords.mConsumables[stimpak].mEffects = { effect(ActorValue::health, 30, 0, false, false, 34) };
            mRecords.mConsumables[radAway].mEffects = { effect(radiationLevel, 50) };
            mRecords.mConsumables[radX].mEffects = { effect(radiationResistance, 25, 240, false, true) };
            mRecords.mConsumables[mentats].mEffects = { effect(ActorValue::intelligence, 5, 240, false, true),
                effect(ActorValue::perception, 5, 240, false, true) };
            mRecords.mConsumables[mentats].mWithdrawal = withdrawal;
            mRecords.mConsumables[mentats].mAddictionChance = 0.5;
            mRecords.mConsumables[nukaCola].mEffects
                = { effect(ActorValue::health, 10), effect(radiationLevel, 3, 0, true) };
            mRecords.mConsumables[superStimpak].mEffects = { effect(ActorValue::health, 20, 3) };
            mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 500, 0, true) };
            mRecords.mBooks[guns].mSkill = smallGuns;
            mRecords.mBooks[novel].mSkill = -1;
            mState.addItem(thePlayer, stimpak, 2);
            mState.addItem(thePlayer, radAway, 1);
            mState.addItem(thePlayer, radX, 1);
            mState.addItem(thePlayer, mentats, 3);
            mState.addItem(thePlayer, nukaCola, 1);
            mState.addItem(thePlayer, superStimpak, 1);
            mState.addItem(thePlayer, guns, 2);
            mState.addItem(thePlayer, novel, 1);
        }
    };

    TEST(FalloutScriptItemUseTest, doesNothingForAnItemTheCharacterDoesNotHave)
    {
        Fixture f;
        EXPECT_EQ(f.mUse.use(thePlayer, buffout).mOutcome, ItemUse::Outcome::Nothing);
        f.mRecords.mConsumables[buffout].mEffects = { effect(ActorValue::strength, 2, 240, false, true) };
        EXPECT_EQ(f.mUse.use(thePlayer, buffout).mOutcome, ItemUse::Outcome::Nothing);
        EXPECT_EQ(f.mState.itemCount(thePlayer, stimpak), 2);
    }

    TEST(FalloutScriptItemUseTest, aStimpakRestoresHealthAndIsUsedUp)
    {
        Fixture f;
        f.mValues.damage(thePlayer, ActorValue::health, 80);
        const double hurt = f.mValues.current(thePlayer, ActorValue::health);
        EXPECT_EQ(f.mUse.use(thePlayer, stimpak).mOutcome, ItemUse::Outcome::Consumed);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), hurt + 30);
        EXPECT_EQ(f.mState.itemCount(thePlayer, stimpak), 1);
        EXPECT_EQ(f.mProgress.miscStat(6), 1);
    }

    TEST(FalloutScriptItemUseTest, theLastCopyIsUsedOnceWhenTwoCallsRaceForIt)
    {
        Fixture f;
        // Another call takes the only copy after this one found it
        f.mRecords.mWhileUsing = [&f] { f.mState.removeItem(thePlayer, radAway, 1); };
        f.mValues.setBase(thePlayer, radiationLevel, 80);

        EXPECT_EQ(f.mUse.use(thePlayer, radAway).mOutcome, ItemUse::Outcome::Nothing);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 80);
        EXPECT_EQ(f.mProgress.miscStat(8), 0);

        // The same for a skill book, which teaches nothing when it is gone
        f.mState.removeItem(thePlayer, guns, 1);
        f.mRecords.mWhileUsing = [&f] { f.mState.removeItem(thePlayer, guns, 1); };
        const double skill = f.mValues.base(thePlayer, smallGuns);
        EXPECT_EQ(f.mUse.use(thePlayer, guns).mOutcome, ItemUse::Outcome::Nothing);
        EXPECT_EQ(f.mValues.base(thePlayer, smallGuns), skill);
    }

    TEST(FalloutScriptItemUseTest, aPoisonThatTakesAllTheHealthKills)
    {
        Fixture f;
        f.mState.addItem(victim, poison, 1);
        EXPECT_EQ(f.mUse.use(victim, poison).mOutcome, ItemUse::Outcome::Consumed);
        EXPECT_LE(f.mValues.current(victim, ActorValue::health), 0);
        EXPECT_TRUE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, aPoisonThatWorksOverTimeKillsWhenTheHealthIsGone)
    {
        Fixture f;
        f.mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 60, 5, true) };
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        f.mUse.update(1);
        EXPECT_FALSE(f.mState.isDead(victim));
        f.mUse.update(2);
        EXPECT_TRUE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, anEssentialCharacterFallsInsteadOfDyingOfPoison)
    {
        Fixture f;
        int falls = 0;
        f.mState.setFallHandler([&falls](FormId) {
            ++falls;
            return true;
        });
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        EXPECT_EQ(falls, 1);
        EXPECT_FALSE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, aPoisonThatLeavesHealthKillsNoOne)
    {
        Fixture f;
        f.mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 5, 0, true) };
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        EXPECT_FALSE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, aDropOfTheHealthThatTakesAllOfItKills)
    {
        Fixture f;
        f.mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 500, 60, true, true) };
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        EXPECT_TRUE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, anEssentialCharacterFallsInsteadOfDyingOfADropOfTheHealth)
    {
        Fixture f;
        int falls = 0;
        f.mState.setFallHandler([&falls](FormId) {
            ++falls;
            return true;
        });
        f.mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 500, 60, true, true) };
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        EXPECT_EQ(falls, 1);
        EXPECT_FALSE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, aDropOfTheHealthThatLeavesSomeKillsNoOneAndEndsWithTheEffect)
    {
        Fixture f;
        const double full = f.mValues.current(victim, ActorValue::health);
        f.mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 5, 10, true, true) };
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        EXPECT_FALSE(f.mState.isDead(victim));
        EXPECT_EQ(f.mValues.current(victim, ActorValue::health), full - 5);
        f.mUse.update(11);
        EXPECT_EQ(f.mValues.current(victim, ActorValue::health), full);
    }

    /// A perk with an entry for the health a character recovers, which adds two points a second
    class RecoveringPerk final : public FalloutScript::PerkRecords
    {
    public:
        static constexpr FormId id = 0x0100A0A0;

        bool perk(FormId perk, FalloutScript::PerkInfo& info) override
        {
            if (perk != id)
                return false;
            info.mId = id;
            info.mRanks = 1;
            info.mPlayable = true;
            FalloutScript::PerkEntry entry;
            entry.mType = FalloutScript::PerkEntry::EntryPoint;
            entry.mEntryPoint = FalloutScript::EntryPoint::recoveredHealth;
            entry.mFunction = 2;
            entry.mValue = 2;
            info.mEntries = { entry };
            return true;
        }
        std::vector<FormId> perks() override { return { id }; }
        bool abilityEffects(FormId, std::vector<FalloutScript::AbilityEffect>&) override { return false; }
    };

    TEST(FalloutScriptItemUseTest, aPerkThatRecoversHealthGivesThePlayerItsPointsEachSecond)
    {
        Fixture f;
        RecoveringPerk records;
        FalloutScript::PerkEngine perks(records, f.mProgress, f.mValues);
        f.mUse.setPerks(&perks);
        f.mValues.damage(thePlayer, ActorValue::health, 50);
        f.mValues.damage(victim, ActorValue::health, 50);
        const double hurt = f.mValues.current(thePlayer, ActorValue::health);

        f.mUse.update(2);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), hurt);

        f.mProgress.addPerk(thePlayer, RecoveringPerk::id);
        f.mUse.update(2);
        EXPECT_NEAR(f.mValues.current(thePlayer, ActorValue::health), hurt + 4, 1e-9);
        // the others do not have the perk
        EXPECT_EQ(f.mValues.current(victim, ActorValue::health), hurt);
        // it stops at the most
        f.mUse.update(1000);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), f.mValues.permanent(thePlayer, ActorValue::health));
    }

    TEST(FalloutScriptItemUseTest, aPerkThatRecoversHealthDoesNotReviveTheDead)
    {
        Fixture f;
        RecoveringPerk records;
        FalloutScript::PerkEngine perks(records, f.mProgress, f.mValues);
        f.mUse.setPerks(&perks);
        f.mProgress.addPerk(thePlayer, RecoveringPerk::id);
        f.mValues.damage(thePlayer, ActorValue::health, 100000);
        f.mState.setDead(thePlayer, true);
        const double dead = f.mValues.current(thePlayer, ActorValue::health);
        f.mUse.update(5);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), dead);
    }

    TEST(FalloutScriptItemUseTest, aCharacterThatLivedOnATemporaryHealthBoostDiesWhenItEnds)
    {
        Fixture f;
        const double full = f.mValues.current(victim, ActorValue::health);
        f.mRecords.mConsumables[poison].mEffects = { effect(ActorValue::health, 50, 10, false, true) };
        f.mState.addItem(victim, poison, 1);
        f.mUse.use(victim, poison);
        EXPECT_EQ(f.mValues.current(victim, ActorValue::health), full + 50);
        // the health left is only what the boost gave
        f.mValues.damage(victim, ActorValue::health, full + 25);
        EXPECT_FALSE(f.mState.isDead(victim));
        f.mUse.update(5);
        EXPECT_FALSE(f.mState.isDead(victim));
        f.mUse.update(6);
        EXPECT_TRUE(f.mState.isDead(victim));
    }

    TEST(FalloutScriptItemUseTest, aTemporaryChangeOfALevelFollowsTheRuleOfLevels)
    {
        Fixture f;
        f.mValues.setBase(thePlayer, radiationLevel, 50);
        // a beneficial effect lowers the level while it lasts, a detrimental one raises it
        f.mRecords.mConsumables[nukaCola].mEffects = { effect(radiationLevel, 20, 10, false, true) };
        f.mUse.use(thePlayer, nukaCola);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 30);
        f.mUse.update(11);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 50);

        f.mState.addItem(thePlayer, nukaCola, 1);
        f.mRecords.mConsumables[nukaCola].mEffects = { effect(radiationLevel, 20, 10, true, true) };
        f.mUse.use(thePlayer, nukaCola);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 70);
        f.mUse.update(11);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 50);
    }

    TEST(FalloutScriptItemUseTest, aChangeOfALevelKeepsTheModifiersOfTheLevel)
    {
        Fixture f;
        f.mValues.setBase(thePlayer, radiationLevel, 80);
        f.mValues.modify(thePlayer, radiationLevel, -20);
        f.mRecords.mConsumables[radAway].mEffects = { effect(radiationLevel, 10) };
        f.mUse.use(thePlayer, radAway);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 50);
    }

    TEST(FalloutScriptItemUseTest, itemsUsedAtTheSameTimeAllChangeTheLevel)
    {
        for (int round = 0; round < 200; ++round)
        {
            Fixture f;
            f.mValues.setBase(thePlayer, radiationLevel, 80);
            f.mState.addItem(thePlayer, radAway, 1);
            std::atomic<int> ready = 0;
            std::vector<std::thread> users;
            for (int i = 0; i < 2; ++i)
                users.emplace_back([&] {
                    ++ready;
                    while (ready < 2)
                        std::this_thread::yield();
                    f.mUse.use(thePlayer, radAway);
                });
            for (std::thread& user : users)
                user.join();

            ASSERT_EQ(f.mState.itemCount(thePlayer, radAway), 0) << "round " << round;
            ASSERT_EQ(f.mValues.current(thePlayer, radiationLevel), 0) << "round " << round;
        }
    }

    TEST(FalloutScriptItemUseTest, checkingTheHealthKillsOrDropsWhoHasNoneLeft)
    {
        Fixture f;
        f.mValues.damage(victim, ActorValue::health, 100000);
        f.mUse.checkHealth(victim);
        EXPECT_TRUE(f.mState.isDead(victim));

        int falls = 0;
        f.mState.setFallHandler([&falls](FormId) {
            ++falls;
            return true;
        });
        constexpr FormId other = 0x01000501;
        f.mValues.damage(other, ActorValue::health, 100000);
        f.mUse.checkHealth(other);
        EXPECT_EQ(falls, 1);
        EXPECT_FALSE(f.mState.isDead(other));

        // someone with health left is left alone
        f.mUse.checkHealth(thePlayer);
        EXPECT_FALSE(f.mState.isDead(thePlayer));
    }

    TEST(FalloutScriptItemUseTest, healingStopsAtFullHealth)
    {
        Fixture f;
        const double full = f.mValues.current(thePlayer, ActorValue::health);
        f.mUse.use(thePlayer, stimpak);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), full);
    }

    TEST(FalloutScriptItemUseTest, radiationIsALevelThatRadAwayLowers)
    {
        Fixture f;
        f.mUse.use(thePlayer, nukaCola);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 3);
        f.mState.addItem(thePlayer, radAway, 1);
        f.mUse.use(thePlayer, radAway);
        // it does not go below nothing
        EXPECT_EQ(f.mValues.current(thePlayer, radiationLevel), 0);
        EXPECT_EQ(f.mProgress.miscStat(8), 1);
    }

    TEST(FalloutScriptItemUseTest, aBoostLastsForItsDurationAndThenIsTakenBack)
    {
        Fixture f;
        const double intelligence = f.mValues.current(thePlayer, ActorValue::intelligence);
        f.mUse.use(thePlayer, radX);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationResistance), 25);
        EXPECT_EQ(f.mProgress.miscStat(7), 1);
        f.mUse.update(100);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationResistance), 25);
        EXPECT_EQ(f.mUse.active(thePlayer).size(), 1u);
        f.mUse.update(141);
        EXPECT_EQ(f.mValues.current(thePlayer, radiationResistance), 0);
        EXPECT_TRUE(f.mUse.active(thePlayer).empty());
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::intelligence), intelligence);
    }

    TEST(FalloutScriptItemUseTest, aRestoringEffectWithADurationHealsEachSecond)
    {
        Fixture f;
        f.mValues.damage(thePlayer, ActorValue::health, 100);
        const double hurt = f.mValues.current(thePlayer, ActorValue::health);
        f.mUse.use(thePlayer, superStimpak);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), hurt);
        f.mUse.update(1);
        EXPECT_NEAR(f.mValues.current(thePlayer, ActorValue::health), hurt + 20, 1e-9);
        f.mUse.update(10);
        // three seconds in all
        EXPECT_NEAR(f.mValues.current(thePlayer, ActorValue::health), hurt + 60, 1e-9);
        EXPECT_TRUE(f.mUse.active(thePlayer).empty());
    }

    TEST(FalloutScriptItemUseTest, aConditionDecidesWhetherAnEffectApplies)
    {
        Fixture f;
        f.mValues.damage(thePlayer, ActorValue::health, 80);
        const double hurt = f.mValues.current(thePlayer, ActorValue::health);
        f.mRecords.mConsumables[stimpak].mEffects[0].mConditions.resize(1);
        f.mUse.use(thePlayer, stimpak);
        // there is no way to tell the condition, so it does not hold
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), hurt);
        f.mUse.setConditions([](const auto&, const FalloutScript::ConditionContext&) { return true; });
        f.mUse.use(thePlayer, stimpak);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::health), hurt + 30);
    }

    TEST(FalloutScriptItemUseTest, aChemMayMakeTheCharacterAddicted)
    {
        Fixture f;
        f.mUse.setRandom([] { return 0.9; });
        EXPECT_FALSE(f.mUse.use(thePlayer, mentats).mAddicted);
        EXPECT_TRUE(f.mUse.addictions(thePlayer).empty());
        f.mUse.setRandom([] { return 0.1; });
        EXPECT_TRUE(f.mUse.use(thePlayer, mentats).mAddicted);
        EXPECT_THAT(f.mUse.addictions(thePlayer), ElementsAre(mentats));
        EXPECT_EQ(f.mProgress.miscStat(10), 1);
        EXPECT_EQ(f.mProgress.miscStat(9), 2);
        // a second dose does not list it again, and the one who is addicted already does not become addicted again
        EXPECT_FALSE(f.mUse.use(thePlayer, mentats).mAddicted);
        EXPECT_THAT(f.mUse.addictions(thePlayer), ElementsAre(mentats));
        EXPECT_EQ(f.mProgress.miscStat(10), 1);
        EXPECT_EQ(f.mProgress.miscStat(9), 3);
        f.mUse.cure(thePlayer, mentats);
        EXPECT_TRUE(f.mUse.addictions(thePlayer).empty());
        // ... and does become addicted when the addiction was cured
        f.mState.addItem(thePlayer, mentats, 1);
        EXPECT_TRUE(f.mUse.use(thePlayer, mentats).mAddicted);
        EXPECT_EQ(f.mProgress.miscStat(10), 2);
    }

    TEST(FalloutScriptItemUseTest, aSkillBookRaisesTheSkillAndIsUsedUp)
    {
        Fixture f;
        f.mValues.setBase(thePlayer, smallGuns, 40);
        const ItemUse::Result result = f.mUse.use(thePlayer, guns);
        EXPECT_EQ(result.mOutcome, ItemUse::Outcome::Read);
        EXPECT_EQ(result.mSkill, smallGuns);
        EXPECT_EQ(result.mGained, 1);
        EXPECT_EQ(f.mValues.base(thePlayer, smallGuns), 41);
        EXPECT_EQ(f.mState.itemCount(thePlayer, guns), 1);
        EXPECT_EQ(f.mProgress.miscStat(15), 1);
    }

    TEST(FalloutScriptItemUseTest, aSkillBookGivesWhatTheRulesSayAndStopsAtTheCap)
    {
        Fixture f;
        f.mUse.setBookRules(100, 3);
        f.mValues.setBase(thePlayer, smallGuns, 98);
        const ItemUse::Result nearCap = f.mUse.use(thePlayer, guns);
        EXPECT_EQ(nearCap.mGained, 2);
        EXPECT_EQ(f.mValues.base(thePlayer, smallGuns), 100);
        const ItemUse::Result atCap = f.mUse.use(thePlayer, guns);
        EXPECT_EQ(atCap.mOutcome, ItemUse::Outcome::NothingToLearn);
        EXPECT_EQ(f.mState.itemCount(thePlayer, guns), 1);
    }

    TEST(FalloutScriptItemUseTest, booksReadAtTheSameTimeAllRaiseTheSkill)
    {
        for (int round = 0; round < 200; ++round)
        {
            Fixture f;
            f.mValues.setBase(thePlayer, smallGuns, 40);
            // Both of them wait for the other, so that they come to the reading of the skill together
            std::atomic<int> ready = 0;
            std::vector<std::thread> readers;
            for (int i = 0; i < 2; ++i)
                readers.emplace_back([&] {
                    ++ready;
                    while (ready < 2)
                        std::this_thread::yield();
                    f.mUse.use(thePlayer, guns);
                });
            for (std::thread& reader : readers)
                reader.join();

            ASSERT_EQ(f.mState.itemCount(thePlayer, guns), 0) << "round " << round;
            ASSERT_EQ(f.mValues.base(thePlayer, smallGuns), 42) << "round " << round;
        }
    }

    TEST(FalloutScriptItemUseTest, aBookToReadStaysWithItsOwner)
    {
        Fixture f;
        EXPECT_EQ(f.mUse.use(thePlayer, novel).mOutcome, ItemUse::Outcome::Read);
        EXPECT_EQ(f.mState.itemCount(thePlayer, novel), 1);
    }

    TEST(FalloutScriptItemUseTest, restoresTheLimbsOfAnArchetypeThatDoes)
    {
        Fixture f;
        constexpr FormId doctorBag = 0x0100000B;
        f.mRecords.mConsumables[doctorBag].mEffects = { effect(-1, 0, 0, false, false, 35) };
        f.mState.addItem(thePlayer, doctorBag, 1);
        f.mValues.damage(thePlayer, ActorValue::firstCondition + 2, 60);
        f.mUse.use(thePlayer, doctorBag);
        EXPECT_EQ(f.mValues.current(thePlayer, ActorValue::firstCondition + 2),
            f.mValues.permanent(thePlayer, ActorValue::firstCondition + 2));
    }
}
