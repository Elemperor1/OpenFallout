#include "scriptbuilder.hpp"

#include <components/falloutscript/crime.hpp>
#include <components/falloutscript/objectstate.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::CrimeKind;
    using FalloutScript::CrimeLine;
    using FalloutScript::FactionFlag;
    using FalloutScript::FormId;
    using FalloutScript::Law;
    using FalloutScript::LawSettings;
    using FalloutScript::Outcome;
    using FalloutScript::Reaction;
    using FalloutScript::Witness;

    constexpr FormId player = 0x14;
    constexpr FormId playerBase = 0x7;
    constexpr FormId town = 0x00100001;
    constexpr FormId gang = 0x00100002;
    constexpr FormId rangers = 0x00100003;
    constexpr FormId doc = 0x00100010;
    constexpr FormId sheriff = 0x00100011;
    constexpr FormId trudy = 0x00100012;
    constexpr FormId raider = 0x00100013;
    constexpr FormId repTown = 0x00100020;
    constexpr FormId cell = 0x00100030;
    constexpr FormId crate = 0x00100040;

    class FakeObjectRecords : public FalloutScript::ObjectStateRecords
    {
    public:
        bool isActor(FormId reference) override { return reference >= 0x00100010 && reference < 0x00100020; }
        FormId player() override { return ::player; }
        FormId playerBase() override { return ::playerBase; }
        FormId ownerForm(FormId form) override { return form; }
        FormId recordOwner(FormId) override { return 0; }
        FormId recordCellOwner(FormId) override { return 0; }
        bool recordCellPublic(FormId) override { return false; }
    };

    class FakeRecords : public FalloutScript::LawRecords
    {
    public:
        LawSettings mSettings = LawSettings::forGame(FalloutScript::Game::NewVegas);
        std::set<FormId> mTracking{ town, rangers };
        std::set<FormId> mEvil{ gang };
        std::map<FormId, FormId> mReputations{ { town, repTown } };

        FormId player() override { return ::player; }
        LawSettings lawSettings() override { return mSettings; }
        bool tracksCrime(FormId faction) override { return mTracking.count(faction) != 0; }
        bool evil(FormId faction) override { return mEvil.count(faction) != 0; }
        FormId reputationOf(FormId faction) override
        {
            const auto found = mReputations.find(faction);
            return found == mReputations.end() ? 0 : found->second;
        }
    };

    class FakeWorld : public FalloutScript::LawWorld
    {
    public:
        std::map<FormId, std::vector<FormId>> mFactions{ { doc, { town } }, { sheriff, { town, rangers } },
            { trudy, { town } }, { raider, { gang } } };
        std::set<FormId> mGuards{ sheriff };
        std::set<FormId> mAlarmed;
        std::set<FormId> mTrespassing;
        std::set<FormId> mDetected;
        std::map<FormId, FormId> mCells{ { doc, cell }, { raider, cell } };
        std::vector<Witness> mResponders;
        std::vector<Witness> mWatchers;
        std::vector<Outcome> mApplied;
        FormId mAppliedVictim = 0;

        std::vector<FormId> factionsOf(FormId actor) override
        {
            const auto found = mFactions.find(actor);
            return found == mFactions.end() ? std::vector<FormId>() : found->second;
        }
        FormId ownerForm(FormId form) override { return form; }
        FormId cellOf(FormId reference) override
        {
            const auto found = mCells.find(reference);
            return found == mCells.end() ? 0 : found->second;
        }
        bool isGuard(FormId actor) override { return mGuards.count(actor) != 0; }
        int detectionLevel(FormId watcher, FormId target) override { return watcher == doc && target == player ? 3 : 0; }
        bool alarmed(FormId actor) override { return mAlarmed.count(actor) != 0; }
        bool detected(FormId actor) override { return mDetected.count(actor) != 0; }
        bool trespassingNow(FormId actor) override { return mTrespassing.count(actor) != 0; }
        std::vector<Witness> responders(FormId, FormId) override { return mResponders; }
        std::vector<Witness> watchers(FormId) override { return mWatchers; }
        void carryOut(const Outcome& outcome, FormId victim) override
        {
            mApplied.push_back(outcome);
            mAppliedVictim = victim;
        }
    };

    Witness watcher(FormId actor, std::vector<FormId> factions, bool friendOfVictim = false, bool watching = true)
    {
        Witness witness;
        witness.mActor = actor;
        witness.mWatching = watching;
        witness.mFriendOfVictim = friendOfVictim;
        witness.mFactions = std::move(factions);
        return witness;
    }

    struct Fixture
    {
        FakeObjectRecords mObjectRecords;
        FalloutScript::ObjectState mObjects{ mObjectRecords };
        FakeRecords mRecords;
        FakeWorld mWorld;
        Law mLaw{ mRecords, mObjects };
    };

    bool said(const Outcome& outcome, FormId actor, Reaction::Kind kind)
    {
        return std::any_of(outcome.mReactions.begin(), outcome.mReactions.end(),
            [&](const Reaction& reaction) { return reaction.mActor == actor && reaction.mKind == kind; });
    }

    TEST(FalloutScriptCrimeTest, a_blow_at_a_citizen_in_front_of_the_town_is_a_major_crime_and_the_town_turns)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        const Outcome outcome
            = f.mLaw.attack(blow, { watcher(doc, { town }), watcher(trudy, { town }), watcher(raider, { gang }) });
        EXPECT_GT(outcome.mCrime, 0);
        EXPECT_THAT(outcome.mEnemies, ElementsAre(town));
        EXPECT_TRUE(f.mObjects.enemyOfFaction(town));
        EXPECT_TRUE(f.mLaw.flag(town, FactionFlag::PlayerEnemy));
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 1);
        EXPECT_EQ(f.mLaw.playerMinorCrimes(), 0);
        EXPECT_EQ(f.mLaw.majorCrimes({ town }), 1);
        // The people that care say the line of the assault, the stranger says the one of a crime that is not its concern
        EXPECT_TRUE(said(outcome, doc, Reaction::Kind::Say));
        EXPECT_TRUE(said(outcome, trudy, Reaction::Kind::Say));
        EXPECT_TRUE(said(outcome, raider, Reaction::Kind::SayNoCrime));
        // An attack puts no infamy on the reputation
        EXPECT_TRUE(outcome.mInfamy.empty());
        EXPECT_EQ(outcome.mKarma, 0);
        EXPECT_EQ(f.mLaw.valueOf(CrimeKind::Attack), 1);
    }

    TEST(FalloutScriptCrimeTest, a_friend_of_the_victim_that_does_not_care_fights_for_it)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = raider;
        blow.mVictimFactions = { gang };
        // The gang does not track crime: nobody cares, and the one that is a friend of the victim attacks
        const Outcome outcome
            = f.mLaw.attack(blow, { watcher(raider, { gang }), watcher(trudy, { town }, true), watcher(doc, { town }) });
        EXPECT_TRUE(outcome.mEnemies.empty());
        EXPECT_FALSE(f.mObjects.enemyOfFaction(gang));
        EXPECT_TRUE(said(outcome, trudy, Reaction::Kind::Attack));
        EXPECT_TRUE(said(outcome, doc, Reaction::Kind::SayNoCrime));
        EXPECT_TRUE(said(outcome, raider, Reaction::Kind::SayNoCrime));
        // The victim is always a witness of the blow, so the major crime counts for the player, but a faction that does
        // not track crime counts none
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 1);
        EXPECT_EQ(f.mLaw.majorCrimes({ gang }), 0);
    }

    TEST(FalloutScriptCrimeTest, a_watcher_that_cannot_see_says_nothing_and_flags_nothing)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        const Outcome outcome = f.mLaw.attack(blow, { watcher(trudy, { town }, false, false) });
        EXPECT_TRUE(outcome.mReactions.empty());
        EXPECT_TRUE(outcome.mEnemies.empty());
        EXPECT_FALSE(f.mObjects.enemyOfFaction(town));
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 1);
    }

    TEST(FalloutScriptCrimeTest, a_blow_of_a_weapon_of_the_minor_kind_concerns_only_the_victim)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        blow.mMinor = true;
        const Outcome outcome = f.mLaw.attack(blow, { watcher(doc, { town }), watcher(trudy, { town }) });
        EXPECT_TRUE(said(outcome, doc, Reaction::Kind::Say));
        EXPECT_TRUE(said(outcome, trudy, Reaction::Kind::SayNoCrime));
        EXPECT_FALSE(f.mObjects.enemyOfFaction(town));
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 0);
        EXPECT_EQ(f.mLaw.minorCrimes({ town }), 1);
        EXPECT_EQ(f.mLaw.majorCrimes({ town }), 0);
        EXPECT_TRUE(outcome.mInfamy.empty());
    }

    TEST(FalloutScriptCrimeTest, a_blow_at_a_victim_that_fights_is_no_crime_and_costs_the_disposition)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        blow.mFighting = true;
        const Outcome outcome = f.mLaw.attack(blow, { watcher(doc, { town }) });
        EXPECT_EQ(outcome.mCrime, 0);
        EXPECT_DOUBLE_EQ(outcome.mVictimDisposition, -10.0);
        EXPECT_TRUE(outcome.mReactions.empty());
        EXPECT_EQ(f.mLaw.crimeCount(), 0u);
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 0);
        EXPECT_TRUE(f.mLaw.attack({}, {}).mReactions.empty());
    }

    TEST(FalloutScriptCrimeTest, a_blow_at_a_creature_is_no_attack_crime_unless_it_belongs_to_someone)
    {
        // 0x898D6A to 0x898E5B: fighting victims first, then the creature test (the virtual at +0x21C), then the character
        EXPECT_EQ(Law::judgeBlow(false, false, false), Law::BlowKind::Attack);
        EXPECT_EQ(Law::judgeBlow(false, false, true), Law::BlowKind::Attack);
        EXPECT_EQ(Law::judgeBlow(false, true, false), Law::BlowKind::Nothing);
        EXPECT_EQ(Law::judgeBlow(false, true, true), Law::BlowKind::Property);
        EXPECT_EQ(Law::judgeBlow(true, false, false), Law::BlowKind::Nothing);
        EXPECT_EQ(Law::judgeBlow(true, true, true), Law::BlowKind::Nothing);
    }

    TEST(FalloutScriptCrimeTest, a_blow_of_a_minor_weapon_is_the_business_of_the_victim_alone)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = trudy;
        blow.mVictimFactions = { town };
        blow.mMinor = true;
        const Outcome outcome = f.mLaw.attack(blow, { watcher(doc, { town }), watcher(trudy, { town }) });
        // Trudy, who was hit, is told of the crime; Doc, who only watched, is not (he has the no-crime line, 0x8C0742)
        const auto kindOf = [&](FormId actor) {
            for (const Reaction& reaction : outcome.mReactions)
                if (reaction.mActor == actor)
                    return static_cast<int>(reaction.mKind);
            return -1;
        };
        EXPECT_EQ(kindOf(trudy), static_cast<int>(Reaction::Kind::Say));
        EXPECT_EQ(kindOf(doc), static_cast<int>(Reaction::Kind::SayNoCrime));
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 0);
    }

    TEST(FalloutScriptCrimeTest, the_disposition_cost_of_a_blow_follows_the_setting)
    {
        Fixture f;
        f.mRecords.mSettings.mCrimeDispAttack = -25.0;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mFighting = true;
        EXPECT_DOUBLE_EQ(f.mLaw.attack(blow, {}).mVictimDisposition, -25.0);
    }

    TEST(FalloutScriptCrimeTest, a_murder_seen_by_the_town_is_a_major_crime_with_infamy_and_makes_a_murderer)
    {
        Fixture f;
        Law::Killing killing;
        killing.mVictim = doc;
        killing.mVictimFactions = { town };
        const Outcome outcome = f.mLaw.murder(killing, { watcher(trudy, { town }), watcher(raider, { gang }) });
        EXPECT_TRUE(outcome.mMurderer);
        EXPECT_TRUE(f.mLaw.murderer());
        EXPECT_TRUE(said(outcome, trudy, Reaction::Kind::Say));
        EXPECT_TRUE(said(outcome, raider, Reaction::Kind::SayNoCrime));
        EXPECT_TRUE(f.mObjects.enemyOfFaction(town));
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 1);
        EXPECT_EQ(f.mLaw.majorCrimes({ town }), 1);
        ASSERT_EQ(outcome.mInfamy.size(), 1u);
        EXPECT_EQ(outcome.mInfamy[0].mReputation, repTown);
        EXPECT_DOUBLE_EQ(outcome.mInfamy[0].mAmount, 30.0);
        EXPECT_EQ(f.mLaw.valueOf(CrimeKind::Murder), 1);
    }

    TEST(FalloutScriptCrimeTest, a_murder_nobody_that_cares_saw_counts_for_nothing_but_the_flag_and_the_karma)
    {
        Fixture f;
        Law::Killing killing;
        killing.mVictim = doc;
        killing.mVictimFactions = { town };
        const Outcome outcome = f.mLaw.murder(killing, { watcher(raider, { gang }) });
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 0);
        EXPECT_TRUE(outcome.mInfamy.empty());
        EXPECT_FALSE(f.mObjects.enemyOfFaction(town));
        EXPECT_TRUE(f.mLaw.murderer());
        EXPECT_TRUE(f.mLaw.hasCrime(doc, player, static_cast<int>(CrimeKind::Murder)));
        EXPECT_FALSE(f.mLaw.hasCrime(doc, player, static_cast<int>(CrimeKind::Murder), raider));
    }

    TEST(FalloutScriptCrimeTest, a_friend_of_the_murdered_one_that_does_not_care_attacks)
    {
        Fixture f;
        Law::Killing killing;
        killing.mVictim = raider;
        killing.mVictimFactions = { gang };
        const Outcome outcome = f.mLaw.murder(killing, { watcher(raider, { gang }), watcher(trudy, { town }, true) });
        EXPECT_TRUE(said(outcome, trudy, Reaction::Kind::Attack));
        // The victim is not a witness of its own death
        EXPECT_FALSE(said(outcome, raider, Reaction::Kind::SayNoCrime));
    }

    TEST(FalloutScriptCrimeTest, the_karma_of_a_murder_goes_by_the_class_of_the_victim_in_new_vegas)
    {
        Fixture f;
        const auto karma = [&f](int karmaClass, bool creature, bool evil) {
            Law::Killing killing;
            killing.mVictim = doc;
            killing.mVictimFactions = { town };
            killing.mKarmaClass = karmaClass;
            killing.mCharacter = !creature;
            killing.mEvil = evil;
            return f.mLaw.murder(killing, {}).mKarma;
        };
        EXPECT_EQ(karma(1, false, false), 0);
        EXPECT_EQ(karma(0, false, false), -50);
        EXPECT_EQ(karma(3, false, false), -100);
        EXPECT_EQ(karma(2, false, true), 1);
        EXPECT_EQ(karma(4, false, true), 2);
        EXPECT_EQ(karma(1, true, false), 0);
    }

    TEST(FalloutScriptCrimeTest, the_karma_of_a_murder_in_fallout_3_has_one_price_for_everyone_who_is_not_evil)
    {
        Fixture f;
        f.mRecords.mSettings = LawSettings::forGame(FalloutScript::Game::Fallout3);
        const auto karma = [&f](int karmaClass, bool creature) {
            Law::Killing killing;
            killing.mVictim = doc;
            killing.mVictimFactions = { town };
            killing.mKarmaClass = karmaClass;
            killing.mCharacter = !creature;
            return f.mLaw.murder(killing, {}).mKarma;
        };
        EXPECT_EQ(karma(1, false), -100);
        EXPECT_EQ(karma(0, false), -100);
        EXPECT_EQ(karma(3, false), -100);
        EXPECT_EQ(karma(1, true), -25);
        // 0x71B1F8: the evil pay nothing, only the very evil pay the one setting for the killing of the evil
        EXPECT_EQ(karma(2, false), 0);
        EXPECT_EQ(karma(4, false), 1);
        // And no infamy: the game has no reputations
        Law::Killing killing;
        killing.mVictim = doc;
        killing.mVictimFactions = { town };
        EXPECT_TRUE(f.mLaw.murder(killing, { watcher(trudy, { town }) }).mInfamy.empty());
    }

    TEST(FalloutScriptCrimeTest, in_fallout_3_the_very_evil_pay_whatever_their_faction_and_a_kill_that_is_no_crime_costs_nothing)
    {
        Fixture f;
        f.mRecords.mSettings = LawSettings::forGame(FalloutScript::Game::Fallout3);
        Law::Killing killing;
        killing.mVictim = raider;
        killing.mVictimFactions = { gang };
        killing.mKarmaClass = 4;
        // (0x71B238: the very evil pay on every path)
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 1);
        killing.mKarmaClass = 1;
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 0);
        // a victim of a town that tracks crime costs the price, unless the kill is marked as no crime
        killing.mVictim = doc;
        killing.mVictimFactions = { town };
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, -100);
        killing.mCrimeOnDeath = false;
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 0);
        killing.mKarmaClass = 4;
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 1);
        // a kill that a script asked to count for no karma counts for none
        killing.mKarmaCounts = false;
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 0);
    }

    TEST(FalloutScriptCrimeTest, a_kill_of_someone_outside_a_faction_that_tracks_crime_costs_no_karma)
    {
        Fixture f;
        Law::Killing killing;
        killing.mVictim = raider;
        killing.mVictimFactions = { gang };
        killing.mKarmaClass = 0;
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 0);
    }

    TEST(FalloutScriptCrimeTest, an_evil_victim_does_not_make_a_murderer_and_a_creature_does_not_either)
    {
        Fixture f;
        Law::Killing killing;
        killing.mVictim = raider;
        killing.mVictimFactions = { gang };
        killing.mEvil = true;
        EXPECT_FALSE(f.mLaw.murder(killing, {}).mMurderer);
        EXPECT_FALSE(f.mLaw.murderer());
        killing.mEvil = false;
        killing.mCharacter = false;
        EXPECT_FALSE(f.mLaw.murder(killing, {}).mMurderer);
        EXPECT_FALSE(f.mLaw.murderer());
    }

    TEST(FalloutScriptCrimeTest, a_death_that_counts_for_nothing_is_no_crime_but_the_karma_still_counts)
    {
        Fixture f;
        Law::Killing killing;
        killing.mVictim = doc;
        killing.mVictimFactions = { town };
        killing.mKarmaClass = 0;
        killing.mCrimeOnDeath = false;
        Outcome outcome = f.mLaw.murder(killing, { watcher(trudy, { town }) });
        EXPECT_EQ(outcome.mKarma, -50);
        EXPECT_EQ(outcome.mCrime, 0);
        EXPECT_FALSE(outcome.mMurderer);
        EXPECT_EQ(f.mLaw.crimeCount(), 0u);
        killing.mKarmaCounts = false;
        EXPECT_EQ(f.mLaw.murder(killing, {}).mKarma, 0);
        EXPECT_TRUE(f.mLaw.murder({}, {}).mReactions.empty());
    }

    TEST(FalloutScriptCrimeTest, taking_what_is_owned_costs_karma_seen_or_not_unless_the_owner_is_evil)
    {
        Fixture f;
        Law::Theft theft;
        theft.mValue = 40;
        EXPECT_EQ(f.mLaw.steal(theft).mKarma, -5);
        theft.mOwnerNotEvil = false;
        EXPECT_EQ(f.mLaw.steal(theft).mKarma, 0);
        EXPECT_EQ(f.mLaw.valueOf(CrimeKind::Steal), 80);
        // Nobody saw: no crime
        EXPECT_EQ(f.mLaw.crimeCount(), 0u);
        theft.mVictim = doc;
        theft.mVictimWatching = false;
        EXPECT_EQ(f.mLaw.steal(theft).mCrime, 0);
    }

    TEST(FalloutScriptCrimeTest, a_theft_that_the_owner_sees_is_warned_first_and_fought_on_the_same_day_after)
    {
        Fixture f;
        Law::Theft theft;
        theft.mVictim = doc;
        theft.mVictimFactions = { town };
        theft.mVictimWatching = true;
        theft.mValue = 25;
        theft.mDay = 12;
        const Outcome first = f.mLaw.steal(theft);
        EXPECT_GT(first.mCrime, 0);
        EXPECT_TRUE(said(first, doc, Reaction::Kind::Warn));
        EXPECT_EQ(f.mLaw.actorMinorCrimes(doc), 1);
        EXPECT_EQ(f.mLaw.minorCrimes({ town }), 1);
        // The minor crime puts infamy of the minor kind on the reputation of the town
        ASSERT_EQ(first.mInfamy.size(), 1u);
        EXPECT_DOUBLE_EQ(first.mInfamy[0].mAmount, 2.0);
        // No faction turns on the player for a theft, only the owner is warned
        EXPECT_FALSE(f.mObjects.enemyOfFaction(town));
        const Outcome second = f.mLaw.steal(theft);
        EXPECT_TRUE(said(second, doc, Reaction::Kind::Attack));
        EXPECT_FALSE(said(second, doc, Reaction::Kind::Warn));
        // The next day it is a warning again
        theft.mDay = 13;
        EXPECT_TRUE(said(f.mLaw.steal(theft), doc, Reaction::Kind::Warn));
        EXPECT_EQ(f.mLaw.actorMinorCrimes(doc), 3);
        // A theft is no crime of the player's own count
        EXPECT_EQ(f.mLaw.playerMinorCrimes(), 0);
        EXPECT_TRUE(f.mLaw.hasCrime(doc, player, static_cast<int>(CrimeKind::Steal), doc));
    }

    TEST(FalloutScriptCrimeTest, a_theft_past_the_warnings_makes_the_owner_say_the_line_of_the_crime)
    {
        Fixture f;
        f.mRecords.mSettings.mStealWarnings = 0;
        Law::Theft theft;
        theft.mVictim = doc;
        theft.mVictimWatching = true;
        EXPECT_TRUE(said(f.mLaw.steal(theft), doc, Reaction::Kind::Say));
    }

    TEST(FalloutScriptCrimeTest, a_caught_pickpocket_goes_the_way_of_a_theft_with_its_own_warnings)
    {
        Fixture f;
        Law::Theft theft;
        theft.mVictim = doc;
        theft.mVictimFactions = { town };
        theft.mValue = 9;
        theft.mDay = 3;
        const Outcome first = f.mLaw.pickpocket(theft);
        EXPECT_TRUE(said(first, doc, Reaction::Kind::Warn));
        EXPECT_EQ(first.mReactions[0].mCrime, CrimeKind::Pickpocket);
        EXPECT_EQ(f.mLaw.valueOf(CrimeKind::Pickpocket), 9);
        EXPECT_EQ(f.mLaw.minorCrimes({ town }), 1);
        EXPECT_TRUE(said(f.mLaw.pickpocket(theft), doc, Reaction::Kind::Attack));
        f.mRecords.mSettings.mPickPocketWarnings = 0;
        theft.mDay = 4;
        EXPECT_TRUE(said(f.mLaw.pickpocket(theft), doc, Reaction::Kind::Say));
        EXPECT_EQ(f.mLaw.pickpocket({}).mCrime, 0);
    }

    TEST(FalloutScriptCrimeTest, the_alarm_turns_those_that_answer_and_counts_one_minor_crime)
    {
        Fixture f;
        Law::Alarm alarm;
        alarm.mResponders = { watcher(sheriff, { town, rangers }), watcher(doc, { town }),
            watcher(trudy, { town }, false, false) };
        const Outcome outcome = f.mLaw.alarm(alarm);
        EXPECT_TRUE(said(outcome, sheriff, Reaction::Kind::Attack));
        EXPECT_TRUE(said(outcome, doc, Reaction::Kind::Attack));
        EXPECT_FALSE(said(outcome, trudy, Reaction::Kind::Attack));
        EXPECT_EQ(f.mLaw.playerMinorCrimes(), 1);
        // The first one that answers marks its factions, once
        EXPECT_EQ(f.mLaw.minorCrimes({ town }), 1);
        EXPECT_EQ(f.mLaw.minorCrimes({ rangers }), 1);
        // Nobody answers: nothing counts
        EXPECT_TRUE(f.mLaw.alarm({}).mReactions.empty());
        EXPECT_EQ(f.mLaw.playerMinorCrimes(), 1);
    }

    TEST(FalloutScriptCrimeTest, the_alarm_of_an_assault_is_a_major_attack_and_the_victim_fights)
    {
        Fixture f;
        const Outcome outcome = f.mLaw.assaultAlarm(doc, { town }, { watcher(trudy, { town }) });
        ASSERT_FALSE(outcome.mReactions.empty());
        EXPECT_EQ(outcome.mReactions.front().mActor, doc);
        EXPECT_EQ(outcome.mReactions.front().mKind, Reaction::Kind::Attack);
        EXPECT_TRUE(said(outcome, trudy, Reaction::Kind::Say));
        EXPECT_EQ(f.mLaw.playerMajorCrimes(), 1);
        EXPECT_TRUE(f.mObjects.enemyOfFaction(town));
    }

    TEST(FalloutScriptCrimeTest, a_trespass_gets_three_warnings_ten_seconds_apart_and_then_the_alarm)
    {
        Fixture f;
        using Step = Law::TrespassStep;
        EXPECT_EQ(f.mLaw.trespassLevel(doc), -1);
        EXPECT_FALSE(f.mLaw.trespassing(doc));
        EXPECT_EQ(f.mLaw.trespass(doc, 0.0, false), Step::Warn);
        EXPECT_EQ(f.mLaw.trespassLevel(doc), 1);
        EXPECT_TRUE(f.mLaw.trespassing(doc));
        EXPECT_EQ(f.mLaw.trespass(doc, 4.0, false), Step::Wait);
        EXPECT_EQ(f.mLaw.trespass(doc, 6.0, false), Step::Warn);
        EXPECT_EQ(f.mLaw.trespass(doc, 10.0, false), Step::Warn);
        EXPECT_EQ(f.mLaw.trespassLevel(doc), 3);
        EXPECT_EQ(f.mLaw.trespass(doc, 10.0, false), Step::Alarm);
        // Once the alarm is given the package waits
        EXPECT_EQ(f.mLaw.trespass(doc, 20.0, false), Step::Wait);
        f.mLaw.endTrespass(doc);
        EXPECT_EQ(f.mLaw.trespassLevel(doc), -1);
        EXPECT_EQ(f.mLaw.trespass(0, 1.0, false), Step::Wait);
    }

    TEST(FalloutScriptCrimeTest, the_trespasser_is_trespassing_while_any_watcher_minds_it)
    {
        Fixture f;
        EXPECT_FALSE(f.mLaw.anyTrespass());
        f.mLaw.trespass(doc, 0.0, false);
        // the watcher has the package; whoever asks about the trespasser is told by any of them
        EXPECT_TRUE(f.mLaw.anyTrespass());
        f.mLaw.trespass(trudy, 0.0, false);
        f.mLaw.endTrespass(doc);
        EXPECT_TRUE(f.mLaw.anyTrespass());
        f.mLaw.endTrespass(trudy);
        EXPECT_FALSE(f.mLaw.anyTrespass());
    }

    TEST(FalloutScriptCrimeTest, every_watcher_with_a_trespass_package_is_listed_even_after_a_load)
    {
        Fixture f;
        f.mLaw.trespass(doc, 0.0, false);
        f.mLaw.trespass(trudy, 0.0, false);
        FalloutScript::SaveWriter out;
        FalloutScript::writeLaw(f.mLaw, out);
        const std::string bytes = out.take();

        Fixture g;
        EXPECT_TRUE(g.mLaw.trespassWatchers().empty());
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readLaw(g.mLaw, in, FalloutScript::lawSectionVersion);
        // the host reconciles these against the ones that mind the player now, and ends the rest
        std::vector<FalloutScript::FormId> watchers = g.mLaw.trespassWatchers();
        std::sort(watchers.begin(), watchers.end());
        std::vector<FalloutScript::FormId> expected{ doc, trudy };
        std::sort(expected.begin(), expected.end());
        EXPECT_EQ(watchers, expected);
        g.mLaw.endTrespass(doc);
        g.mLaw.endTrespass(trudy);
        EXPECT_TRUE(g.mLaw.trespassWatchers().empty());
        EXPECT_FALSE(g.mLaw.anyTrespass());
    }

    TEST(FalloutScriptCrimeTest, the_characters_an_alarm_turned_on_the_player_are_kept_in_a_save)
    {
        Fixture f;
        f.mLaw.addAlarmResponder(doc);
        f.mLaw.addAlarmResponder(trudy);
        f.mLaw.addAlarmResponder(0); // no one
        f.mLaw.removeAlarmResponder(trudy);
        f.mLaw.addAlarmResponder(sheriff);
        EXPECT_TRUE(f.mLaw.isAlarmResponder(doc));
        EXPECT_FALSE(f.mLaw.isAlarmResponder(trudy));
        FalloutScript::SaveWriter out;
        FalloutScript::writeLaw(f.mLaw, out);
        const std::string bytes = out.take();

        Fixture g;
        g.mLaw.addAlarmResponder(raider); // what the game before the load had is not part of it
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readLaw(g.mLaw, in, FalloutScript::lawSectionVersion);
        EXPECT_THAT(g.mLaw.alarmResponders(), UnorderedElementsAre(doc, sheriff));
        EXPECT_FALSE(g.mLaw.isAlarmResponder(raider));
        g.mLaw.clear();
        EXPECT_THAT(g.mLaw.alarmResponders(), IsEmpty());
    }

    TEST(FalloutScriptCrimeTest, a_law_saved_before_the_alarm_characters_were_kept_loads_with_none)
    {
        Fixture f;
        FalloutScript::SaveWriter out;
        FalloutScript::writeLaw(f.mLaw, out);
        std::string bytes = out.take();
        // version 1 ends where the list of the alarm characters (here an empty one, a count of 4 bytes) begins
        ASSERT_GE(bytes.size(), 4u);
        bytes.resize(bytes.size() - 4);

        Fixture g;
        g.mLaw.addAlarmResponder(doc);
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readLaw(g.mLaw, in, 1);
        EXPECT_THAT(g.mLaw.alarmResponders(), IsEmpty());
        EXPECT_EQ(in.remaining(), 0u);
    }

    TEST(FalloutScriptCrimeTest, a_cell_that_is_off_limits_gives_no_warning)
    {
        Fixture f;
        EXPECT_EQ(f.mLaw.trespass(doc, 0.0, true), Law::TrespassStep::Alarm);
        EXPECT_EQ(f.mLaw.trespassLevel(doc), 0);
    }

    TEST(FalloutScriptCrimeTest, the_time_between_warnings_is_the_setting)
    {
        Fixture f;
        f.mRecords.mSettings.mTrespassWarningSeconds = 30.0;
        EXPECT_EQ(f.mLaw.trespass(doc, 0.0, false), Law::TrespassStep::Warn);
        EXPECT_EQ(f.mLaw.trespass(doc, 29.0, false), Law::TrespassStep::Wait);
        EXPECT_EQ(f.mLaw.trespass(doc, 1.0, false), Law::TrespassStep::Warn);
    }

    TEST(FalloutScriptCrimeTest, a_crime_is_found_by_its_victim_criminal_witness_kind_and_number)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        const int number = f.mLaw.attack(blow, { watcher(trudy, { town }) }).mCrime;
        const int attack = static_cast<int>(CrimeKind::Attack);
        EXPECT_TRUE(f.mLaw.hasCrime(doc, player, attack));
        EXPECT_TRUE(f.mLaw.hasCrime(0, player, attack));
        EXPECT_TRUE(f.mLaw.hasCrime(doc, player, -1, trudy));
        EXPECT_TRUE(f.mLaw.hasCrime(doc, player, attack, doc, number));
        EXPECT_FALSE(f.mLaw.hasCrime(doc, player, attack, raider));
        EXPECT_FALSE(f.mLaw.hasCrime(doc, player, static_cast<int>(CrimeKind::Murder)));
        EXPECT_FALSE(f.mLaw.hasCrime(trudy, player, attack));
        EXPECT_FALSE(f.mLaw.hasCrime(doc, doc, attack));
        EXPECT_FALSE(f.mLaw.hasCrime(doc, player, attack, doc, number + 1));
    }

    TEST(FalloutScriptCrimeTest, stopping_an_alarm_forgets_the_crimes_and_clears_the_flags_that_nothing_holds)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        f.mLaw.attack(blow, { watcher(trudy, { town }) });
        blow.mVictim = sheriff;
        blow.mVictimFactions = { town, rangers };
        f.mLaw.attack(blow, { watcher(sheriff, { town, rangers }) });
        EXPECT_TRUE(f.mObjects.enemyOfFaction(rangers));
        // The first crime is gone but the second holds the town, and the rangers
        EXPECT_THAT(f.mLaw.stopAlarm(doc), IsEmpty());
        EXPECT_TRUE(f.mObjects.enemyOfFaction(town));
        EXPECT_THAT(f.mLaw.stopAlarm(sheriff), UnorderedElementsAre(town, rangers));
        EXPECT_FALSE(f.mObjects.enemyOfFaction(town));
        EXPECT_FALSE(f.mObjects.enemyOfFaction(rangers));
        EXPECT_EQ(f.mLaw.crimeCount(), 0u);
        EXPECT_THAT(f.mLaw.stopAlarm(doc), IsEmpty());
    }

    TEST(FalloutScriptCrimeTest, the_yield_forgets_the_crimes_of_the_last_seconds_and_what_they_held)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        // an old crime, then (20 seconds of play later) a recent one that holds the town and the rangers
        f.mLaw.attack(blow, { watcher(trudy, { town }) });
        f.mLaw.advance(20.0);
        blow.mVictim = sheriff;
        blow.mVictimFactions = { town, rangers };
        f.mLaw.attack(blow, { watcher(sheriff, { town, rangers }) });
        f.mLaw.advance(4.0);
        ASSERT_TRUE(f.mObjects.enemyOfFaction(rangers));
        // fCombatYieldTime is 10: the crime of 4 seconds ago goes (0x971FD0), the one of 24 seconds ago stays; the town is
        // still held by the old crime and the rangers are not
        EXPECT_THAT(f.mLaw.forgetRecent(10.0), UnorderedElementsAre(rangers));
        EXPECT_EQ(f.mLaw.crimeCount(), 1u);
        EXPECT_TRUE(f.mObjects.enemyOfFaction(town));
        EXPECT_FALSE(f.mObjects.enemyOfFaction(rangers));
        // a crime as old as the limit still goes (the age is not above it); one a moment older stays
        f.mLaw.advance(6.0);
        EXPECT_THAT(f.mLaw.forgetRecent(10.0), IsEmpty());
        EXPECT_EQ(f.mLaw.crimeCount(), 1u);
        f.mLaw.clear();
        f.mLaw.attack(blow, { watcher(sheriff, { town, rangers }) });
        f.mLaw.advance(10.0);
        EXPECT_THAT(f.mLaw.forgetRecent(10.0), UnorderedElementsAre(town, rangers));
        EXPECT_EQ(f.mLaw.crimeCount(), 0u);
    }

    TEST(FalloutScriptCrimeTest, a_crime_that_was_loaded_counts_as_made_at_the_moment_of_the_load)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        f.mLaw.attack(blow, { watcher(trudy, { town }) });
        FalloutScript::SaveWriter out;
        FalloutScript::writeLaw(f.mLaw, out);
        const std::string bytes = out.take();

        // The game that loads it has been running for longer than the yield looks back
        Fixture g;
        g.mLaw.advance(500.0);
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readLaw(g.mLaw, in, FalloutScript::lawSectionVersion);
        ASSERT_EQ(g.mLaw.crimeCount(), 1u);
        g.mLaw.advance(4.0);
        EXPECT_THAT(g.mLaw.forgetRecent(10.0), UnorderedElementsAre(town));
        EXPECT_EQ(g.mLaw.crimeCount(), 0u);
    }

    TEST(FalloutScriptCrimeTest, a_faction_held_only_by_a_crime_that_is_forgotten_for_newer_ones_is_released)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        f.mLaw.attack(blow, { watcher(trudy, { town }) });
        ASSERT_TRUE(f.mObjects.enemyOfFaction(town));
        // the crimes that are kept are a few hundred; the town is held by the first only
        blow.mVictim = sheriff;
        blow.mVictimFactions = { rangers };
        for (int i = 0; i < 600; ++i)
            f.mLaw.attack(blow, { watcher(sheriff, { rangers }) });
        EXPECT_FALSE(f.mLaw.hasCrime(doc, player, -1));
        EXPECT_FALSE(f.mObjects.enemyOfFaction(town));
        EXPECT_TRUE(f.mObjects.enemyOfFaction(rangers));
    }

    TEST(FalloutScriptCrimeTest, the_flags_of_a_faction_are_set_and_cleared_one_by_one)
    {
        Fixture f;
        EXPECT_FALSE(f.mLaw.flag(town, FactionFlag::PlayerExpelled));
        f.mLaw.setFlag(town, FactionFlag::PlayerExpelled, true);
        f.mLaw.setFlag(town, FactionFlag::PlayerAttack, true);
        f.mLaw.setFlag(town, FactionFlag::PlayerMurder, true);
        f.mLaw.setFlag(town, FactionFlag::PlayerEnemy, true);
        EXPECT_TRUE(f.mLaw.flag(town, FactionFlag::PlayerExpelled));
        EXPECT_TRUE(f.mLaw.flag(town, FactionFlag::PlayerAttack));
        EXPECT_TRUE(f.mLaw.flag(town, FactionFlag::PlayerMurder));
        EXPECT_TRUE(f.mLaw.anyEnemy({ gang, town }));
        f.mLaw.setFlag(town, FactionFlag::PlayerAttack, false);
        EXPECT_FALSE(f.mLaw.flag(town, FactionFlag::PlayerAttack));
        EXPECT_TRUE(f.mLaw.flag(town, FactionFlag::PlayerMurder));
        f.mLaw.clearEnemyFlags({ town });
        EXPECT_FALSE(f.mLaw.anyEnemy({ gang, town }));
        // The null faction has no flags
        f.mLaw.setFlag(0, FactionFlag::PlayerExpelled, true);
        EXPECT_FALSE(f.mLaw.flag(0, FactionFlag::PlayerExpelled));
    }

    TEST(FalloutScriptCrimeTest, the_stolen_goods_that_were_sold_and_the_murderer_flag_are_kept)
    {
        Fixture f;
        f.mLaw.modAmountSoldStolen(30);
        f.mLaw.modAmountSoldStolen(-10);
        EXPECT_DOUBLE_EQ(f.mLaw.amountSoldStolen(), 20.0);
        f.mLaw.setMurderer(true);
        EXPECT_TRUE(f.mLaw.murderer());
        f.mLaw.setMurderer(false);
        EXPECT_FALSE(f.mLaw.murderer());
        f.mLaw.clear();
        EXPECT_DOUBLE_EQ(f.mLaw.amountSoldStolen(), 0.0);
    }

    TEST(FalloutScriptCrimeTest, a_change_of_karma_stays_between_the_limits_and_is_major_from_the_threshold)
    {
        const LawSettings settings = LawSettings::forGame(FalloutScript::Game::NewVegas);
        EXPECT_EQ(Law::karma(settings, 0, -5).mApplied, -5);
        EXPECT_FALSE(Law::karma(settings, 0, -5).mMajor);
        EXPECT_EQ(Law::karma(settings, 990, 100).mApplied, 10);
        EXPECT_EQ(Law::karma(settings, -990, -100).mApplied, -10);
        EXPECT_TRUE(Law::karma(settings, 0, 250).mMajor);
        EXPECT_TRUE(Law::karma(settings, 0, -250).mMajor);
        EXPECT_FALSE(Law::karma(settings, 0, 249).mMajor);
    }

    TEST(FalloutScriptCrimeTest, anything_may_be_taken_by_the_owner_the_ownerless_and_the_members_of_the_owning_faction)
    {
        EXPECT_TRUE(Law::mayTake(0, playerBase, {}));
        EXPECT_TRUE(Law::mayTake(playerBase, playerBase, {}));
        EXPECT_TRUE(Law::mayTake(town, playerBase, { gang, town }));
        EXPECT_FALSE(Law::mayTake(town, playerBase, { gang }));
        EXPECT_FALSE(Law::mayTake(doc, playerBase, { town }));
    }

    TEST(FalloutScriptCrimeTest, the_lines_that_are_said_are_the_topics_of_the_default_dialogue_list)
    {
        EXPECT_EQ(Law::lineFor(CrimeKind::Steal, CrimeLine::Crime), 0xdfu);
        EXPECT_EQ(Law::lineFor(CrimeKind::Pickpocket, CrimeLine::Crime), 0xe3u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Attack, CrimeLine::Crime), 0xe4u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Murder, CrimeLine::Crime), 0xe5u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Trespass, CrimeLine::Crime), 0u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Attack, CrimeLine::NoCrime), 0xe7u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Murder, CrimeLine::NoCrime), 0xe8u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Pickpocket, CrimeLine::NoCrime), 0xe9u);
        EXPECT_EQ(Law::lineFor(CrimeKind::Steal, CrimeLine::NoCrime), 0xeau);
        EXPECT_EQ(Law::lineFor(CrimeKind::Trespass, CrimeLine::NoCrime), 0xebu);
        EXPECT_EQ(Law::lineFor(CrimeKind::Steal, CrimeLine::GuardTrespass), 0xf2u);
    }

    TEST(FalloutScriptCrimeTest, the_settings_start_as_the_executables_have_them_and_the_records_change_them)
    {
        const LawSettings nv = LawSettings::forGame(FalloutScript::Game::NewVegas);
        const LawSettings fo3 = LawSettings::forGame(FalloutScript::Game::Fallout3);
        EXPECT_TRUE(nv.mReputations);
        EXPECT_FALSE(fo3.mReputations);
        EXPECT_DOUBLE_EQ(nv.mKarmaMurderNonEvilNpc, 0.0);
        EXPECT_DOUBLE_EQ(fo3.mKarmaMurderNonEvilNpc, -100.0);
        EXPECT_EQ(nv.mStealWarnings, 2);
        EXPECT_EQ(nv.mTrespassWarnings, 3);
        const std::map<std::string, double> values{ { "iStealWarnings", 5 }, { "fAITrespassWarningTimer", 7.5 },
            { "fKarmaModStealing", -9 }, { "fKarmaModKillingEvilActor", 100 }, { "iKarmaMin", -500 },
            { "fReputationMajorCrimeNeg", 12 }, { "fCrimeDispAttack", -3 }, { "iCrimeAlarmRecDistance", 500 },
            { "fKarmaModMurderingGoodNPC", -60 } };
        const LawSettings changed = LawSettings::fromGameSettings(
            FalloutScript::Game::NewVegas, [&values](std::string_view name) -> std::optional<double> {
                const auto found = values.find(std::string(name));
                if (found == values.end())
                    return std::nullopt;
                return found->second;
            });
        EXPECT_EQ(changed.mStealWarnings, 5);
        EXPECT_DOUBLE_EQ(changed.mTrespassWarningSeconds, 7.5);
        EXPECT_DOUBLE_EQ(changed.mKarmaStealing, -9.0);
        EXPECT_DOUBLE_EQ(changed.mKarmaKillingEvil, 100.0);
        EXPECT_EQ(changed.mKarmaMin, -500);
        EXPECT_DOUBLE_EQ(changed.mMajorCrimeInfamy, 12.0);
        EXPECT_DOUBLE_EQ(changed.mCrimeDispAttack, -3.0);
        EXPECT_DOUBLE_EQ(changed.mAlarmDistance, 500.0);
        EXPECT_DOUBLE_EQ(changed.mKarmaMurderGoodNpc, -60.0);
        // No lookup leaves the executable's values
        EXPECT_EQ(LawSettings::fromGameSettings(FalloutScript::Game::NewVegas, {}).mStealWarnings, 2);
    }

    TEST(FalloutScriptCrimeTest, the_state_of_the_law_survives_a_save)
    {
        Fixture f;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        f.mLaw.attack(blow, { watcher(trudy, { town }) });
        Law::Theft theft;
        theft.mVictim = doc;
        theft.mVictimFactions = { town };
        theft.mVictimWatching = true;
        theft.mValue = 15;
        theft.mDay = 9;
        f.mLaw.steal(theft);
        Law::Killing killing;
        killing.mVictim = trudy;
        killing.mVictimFactions = { town };
        f.mLaw.murder(killing, { watcher(doc, { town }) });
        f.mLaw.alarm({ { watcher(sheriff, { town, rangers }) } });
        f.mLaw.setFlag(rangers, FactionFlag::PlayerExpelled, true);
        f.mLaw.setFlag(rangers, FactionFlag::PlayerMurder, true);
        f.mLaw.modAmountSoldStolen(44);
        f.mLaw.trespass(doc, 0.0, false);
        f.mLaw.trespass(doc, 10.0, false);

        FalloutScript::SaveWriter out;
        FalloutScript::writeLaw(f.mLaw, out);
        const std::string bytes = out.take();

        Fixture g;
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readLaw(g.mLaw, in, FalloutScript::lawSectionVersion);
        EXPECT_TRUE(in.atEnd());
        EXPECT_EQ(g.mLaw.playerMajorCrimes(), f.mLaw.playerMajorCrimes());
        EXPECT_EQ(g.mLaw.playerMinorCrimes(), 1);
        EXPECT_EQ(g.mLaw.minorCrimes({ town }), f.mLaw.minorCrimes({ town }));
        EXPECT_EQ(g.mLaw.majorCrimes({ town }), f.mLaw.majorCrimes({ town }));
        EXPECT_EQ(g.mLaw.actorMinorCrimes(doc), 1);
        EXPECT_EQ(g.mLaw.crimeCount(), f.mLaw.crimeCount());
        EXPECT_EQ(g.mLaw.valueOf(CrimeKind::Steal), 15);
        EXPECT_TRUE(g.mLaw.murderer());
        EXPECT_TRUE(g.mLaw.flag(rangers, FactionFlag::PlayerExpelled));
        EXPECT_TRUE(g.mLaw.flag(rangers, FactionFlag::PlayerMurder));
        EXPECT_FALSE(g.mLaw.flag(rangers, FactionFlag::PlayerAttack));
        EXPECT_DOUBLE_EQ(g.mLaw.amountSoldStolen(), 44.0);
        EXPECT_EQ(g.mLaw.trespassLevel(doc), 2);
        EXPECT_TRUE(g.mLaw.hasCrime(doc, player, static_cast<int>(CrimeKind::Steal), doc));
        // The warning of the day is kept: the next theft that day is answered with blows
        theft.mDay = 9;
        EXPECT_TRUE(said(g.mLaw.steal(theft), doc, Reaction::Kind::Attack));
        // A crime that is numbered after the saved ones does not repeat a number
        EXPECT_GT(g.mLaw.attack(blow, {}).mCrime, 3);
    }

    TEST(FalloutScriptCrimeTest, a_save_drops_what_a_missing_plugin_held_and_refuses_what_is_cut_short)
    {
        Fixture f;
        Law::Theft theft;
        theft.mVictim = doc;
        theft.mVictimFactions = { town };
        theft.mVictimWatching = true;
        f.mLaw.steal(theft);
        f.mLaw.setFlag(rangers, FactionFlag::PlayerExpelled, true);
        FalloutScript::SaveWriter out;
        FalloutScript::writeLaw(f.mLaw, out);
        const std::string bytes = out.take();

        Fixture g;
        FalloutScript::SaveReader gone(bytes, [](FormId form) -> std::optional<FormId> {
            return form == doc || form == rangers ? std::nullopt : std::optional<FormId>(form);
        });
        FalloutScript::readLaw(g.mLaw, gone, FalloutScript::lawSectionVersion);
        EXPECT_EQ(g.mLaw.crimeCount(), 0u);
        EXPECT_FALSE(g.mLaw.flag(rangers, FactionFlag::PlayerExpelled));
        EXPECT_EQ(g.mLaw.actorMinorCrimes(doc), 0);

        FalloutScript::SaveReader cut(std::string_view(bytes).substr(0, bytes.size() / 2));
        EXPECT_THROW(FalloutScript::readLaw(g.mLaw, cut, FalloutScript::lawSectionVersion), FalloutScript::SaveError);
    }

    struct Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        Fixture mFixture;

        Runner() { FalloutScript::addLawCommands(mInterpreter, mFixture.mLaw, mFixture.mObjects, mFixture.mRecords, mFixture.mWorld); }

        double call(std::uint16_t opcode, FormId reference, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, reference, reference != 0, std::move(arguments));
        }
    };

    constexpr std::uint16_t OpGetMinorCrimeCount = 0x1074;
    constexpr std::uint16_t OpGetMajorCrimeCount = 0x1075;
    constexpr std::uint16_t OpGetCrime = 0x107A;
    constexpr std::uint16_t OpGetCrimeKnown = 0x1077;
    constexpr std::uint16_t OpIsGuard = 0x107D;
    constexpr std::uint16_t OpIsPCAMurderer = 0x10B0;
    constexpr std::uint16_t OpGetPCExpelled = 0x10C1;
    constexpr std::uint16_t OpSetPCExpelled = 0x10C2;
    constexpr std::uint16_t OpGetPCFactionMurder = 0x10C3;
    constexpr std::uint16_t OpSetPCFactionMurder = 0x10C4;
    constexpr std::uint16_t OpGetPCFactionAttack = 0x10C7;
    constexpr std::uint16_t OpSetPCFactionAttack = 0x10C8;
    constexpr std::uint16_t OpGetAlarmed = 0x103D;
    constexpr std::uint16_t OpGetDetectionLevel = 0x10B4;
    constexpr std::uint16_t OpIsActorDetected = 0x10B5;
    constexpr std::uint16_t OpGetAmountSoldStolen = 0x10BE;
    constexpr std::uint16_t OpModAmountSoldStolen = 0x10BF;
    constexpr std::uint16_t OpSendAssaultAlarm = 0x105F;
    constexpr std::uint16_t OpSendTrespassAlarm = 0x115C;
    constexpr std::uint16_t OpClearFactionPlayerEnemyFlag = 0x11C9;
    constexpr std::uint16_t OpClearActorsFactionsPlayerEnemyFlag = 0x11CA;
    constexpr std::uint16_t OpGetActorCrimePlayerEnemy = 0x11CB;
    constexpr std::uint16_t OpGetActorFactionPlayerEnemy = 0x11CC;
    constexpr std::uint16_t OpGetTrespassWarningLevel = 0x1090;
    constexpr std::uint16_t OpIsTrespassing = 0x1091;
    constexpr std::uint16_t OpIsInMyOwnedCell = 0x1092;
    constexpr std::uint16_t OpIsCellOwner = 0x1118;
    constexpr std::uint16_t OpClearOwnership = 0x118C;

    TEST(FalloutScriptCrimeCommandsTest, the_counts_of_the_player_are_its_own_and_those_of_a_character_are_its_factions)
    {
        Runner run;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        run.mFixture.mLaw.attack(blow, { watcher(doc, { town }) });
        blow.mMinor = true;
        run.mFixture.mLaw.attack(blow, { watcher(doc, { town }) });
        EXPECT_EQ(run.call(OpGetMajorCrimeCount, player), 1.0);
        EXPECT_EQ(run.call(OpGetMinorCrimeCount, player), 0.0);
        EXPECT_EQ(run.call(OpGetMajorCrimeCount, trudy), 1.0);
        EXPECT_EQ(run.call(OpGetMinorCrimeCount, trudy), 1.0);
        EXPECT_EQ(run.call(OpGetMajorCrimeCount, raider), 0.0);
        EXPECT_EQ(run.call(OpGetMajorCrimeCount, 0), 0.0);
        EXPECT_EQ(run.call(OpGetMinorCrimeCount, 0), 0.0);
    }

    TEST(FalloutScriptCrimeCommandsTest, a_crime_is_asked_of_the_victim_and_the_crime_known_of_a_witness)
    {
        Runner run;
        Law::Blow blow;
        blow.mVictim = doc;
        blow.mVictimFactions = { town };
        run.mFixture.mLaw.attack(blow, { watcher(trudy, { town }) });
        const double attack = static_cast<double>(CrimeKind::Attack);
        EXPECT_EQ(run.call(OpGetCrime, doc, { player, attack }), 1.0);
        EXPECT_EQ(run.call(OpGetCrime, doc, { player }), 1.0);
        EXPECT_EQ(run.call(OpGetCrime, doc, { player, static_cast<double>(CrimeKind::Murder) }), 0.0);
        EXPECT_EQ(run.call(OpGetCrime, trudy, { player, attack }), 0.0);
        EXPECT_EQ(run.call(OpGetCrime, 0, { player }), 0.0);
        // Trudy knows of the crime against the doctor, and the sheriff does not
        EXPECT_EQ(run.call(OpGetCrimeKnown, trudy, { attack, player, doc }), 1.0);
        EXPECT_EQ(run.call(OpGetCrimeKnown, sheriff, { attack, player, doc }), 0.0);
        EXPECT_EQ(run.call(OpGetCrimeKnown, 0, { attack, player, doc }), 0.0);
    }

    TEST(FalloutScriptCrimeCommandsTest, the_flags_the_marks_and_the_questions_about_characters_answer_from_the_rule)
    {
        Runner run;
        EXPECT_EQ(run.call(OpIsGuard, sheriff), 1.0);
        EXPECT_EQ(run.call(OpIsGuard, doc), 0.0);
        EXPECT_EQ(run.call(OpIsGuard, 0), 0.0);
        EXPECT_EQ(run.call(OpIsPCAMurderer, 0), 0.0);
        run.mFixture.mLaw.setMurderer(true);
        EXPECT_EQ(run.call(OpIsPCAMurderer, 0), 1.0);
        EXPECT_EQ(run.call(OpGetPCExpelled, 0, { rangers }), 0.0);
        run.call(OpSetPCExpelled, 0, { rangers, 1 });
        run.call(OpSetPCFactionAttack, 0, { town, 1 });
        run.call(OpSetPCFactionMurder, 0, { town, 1 });
        EXPECT_EQ(run.call(OpGetPCExpelled, 0, { rangers }), 1.0);
        EXPECT_EQ(run.call(OpGetPCFactionAttack, 0, { town }), 1.0);
        EXPECT_EQ(run.call(OpGetPCFactionMurder, 0, { town }), 1.0);
        run.call(OpSetPCExpelled, 0, { rangers, 0 });
        EXPECT_EQ(run.call(OpGetPCExpelled, 0, { rangers }), 0.0);
        EXPECT_EQ(run.call(OpGetPCExpelled, 0, { 0 }), 0.0);
        run.call(OpSetPCExpelled, 0, { 0, 1 });
        run.mFixture.mWorld.mAlarmed.insert(doc);
        run.mFixture.mWorld.mDetected.insert(doc);
        EXPECT_EQ(run.call(OpGetAlarmed, doc), 1.0);
        EXPECT_EQ(run.call(OpGetAlarmed, trudy), 0.0);
        EXPECT_EQ(run.call(OpGetAlarmed, 0), 0.0);
        EXPECT_EQ(run.call(OpIsActorDetected, doc), 1.0);
        EXPECT_EQ(run.call(OpIsActorDetected, trudy), 0.0);
        EXPECT_EQ(run.call(OpGetDetectionLevel, doc, { player }), 3.0);
        EXPECT_EQ(run.call(OpGetDetectionLevel, trudy, { player }), 0.0);
        EXPECT_EQ(run.call(OpGetDetectionLevel, doc, {}), 0.0);
        run.call(OpModAmountSoldStolen, 0, { 12 });
        EXPECT_EQ(run.call(OpGetAmountSoldStolen, 0), 12.0);
    }

    TEST(FalloutScriptCrimeCommandsTest, the_enemy_flags_are_asked_and_cleared_by_faction_and_by_character)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetActorFactionPlayerEnemy, doc), 0.0);
        run.mFixture.mLaw.setFlag(town, FactionFlag::PlayerEnemy, true);
        EXPECT_EQ(run.call(OpGetActorFactionPlayerEnemy, doc), 1.0);
        // the flag of a faction is no crime of the character (0x5A4930 reads the character's own list of crimes)
        EXPECT_EQ(run.call(OpGetActorCrimePlayerEnemy, doc), 0.0);
        Law::Blow hit;
        hit.mVictim = doc;
        hit.mVictimFactions = { town };
        run.mFixture.mLaw.attack(hit, { watcher(trudy, { town }) });
        EXPECT_EQ(run.call(OpGetActorCrimePlayerEnemy, doc), 1.0);
        EXPECT_EQ(run.call(OpGetActorCrimePlayerEnemy, trudy), 1.0);
        EXPECT_EQ(run.call(OpGetActorCrimePlayerEnemy, raider), 0.0);
        EXPECT_EQ(run.call(OpGetActorCrimePlayerEnemy, 0), 0.0);
        EXPECT_EQ(run.call(OpGetActorFactionPlayerEnemy, raider), 0.0);
        EXPECT_EQ(run.call(OpGetActorFactionPlayerEnemy, 0), 0.0);
        run.call(OpClearActorsFactionsPlayerEnemyFlag, doc);
        EXPECT_EQ(run.call(OpGetActorFactionPlayerEnemy, doc), 0.0);
        run.mFixture.mLaw.setFlag(town, FactionFlag::PlayerEnemy, true);
        run.mFixture.mLaw.setFlag(gang, FactionFlag::PlayerEnemy, true);
        run.call(OpClearFactionPlayerEnemyFlag, 0, { town });
        EXPECT_FALSE(run.mFixture.mObjects.enemyOfFaction(town));
        EXPECT_TRUE(run.mFixture.mObjects.enemyOfFaction(gang));
        run.call(OpClearActorsFactionsPlayerEnemyFlag, 0);
        run.call(OpClearFactionPlayerEnemyFlag, 0, { 0 });
        EXPECT_TRUE(run.mFixture.mObjects.enemyOfFaction(gang));
    }

    TEST(FalloutScriptCrimeCommandsTest, the_alarms_go_through_the_rule_and_the_world_carries_out_what_it_decides)
    {
        Runner run;
        run.mFixture.mWorld.mWatchers = { watcher(trudy, { town }) };
        run.call(OpSendAssaultAlarm, doc);
        ASSERT_EQ(run.mFixture.mWorld.mApplied.size(), 1u);
        EXPECT_EQ(run.mFixture.mWorld.mAppliedVictim, doc);
        EXPECT_TRUE(said(run.mFixture.mWorld.mApplied[0], doc, Reaction::Kind::Attack));
        EXPECT_EQ(run.mFixture.mLaw.playerMajorCrimes(), 1);
        // With a character named, the alarm is about that one
        run.call(OpSendAssaultAlarm, 0, { trudy });
        EXPECT_EQ(run.mFixture.mWorld.mAppliedVictim, trudy);
        EXPECT_EQ(run.mFixture.mLaw.playerMajorCrimes(), 2);
        run.call(OpSendAssaultAlarm, 0);
        EXPECT_EQ(run.mFixture.mWorld.mApplied.size(), 2u);

        run.mFixture.mWorld.mResponders = { watcher(sheriff, { town, rangers }) };
        run.call(OpSendTrespassAlarm, doc, { player });
        ASSERT_EQ(run.mFixture.mWorld.mApplied.size(), 3u);
        EXPECT_TRUE(said(run.mFixture.mWorld.mApplied[2], sheriff, Reaction::Kind::Attack));
        EXPECT_EQ(run.mFixture.mLaw.playerMinorCrimes(), 1);
    }

    TEST(FalloutScriptCrimeCommandsTest, the_trespass_and_cell_questions_answer_from_the_rule_and_the_marks)
    {
        Runner run;
        EXPECT_EQ(run.call(OpGetTrespassWarningLevel, doc), -1.0);
        run.mFixture.mLaw.trespass(doc, 0.0, false);
        EXPECT_EQ(run.call(OpGetTrespassWarningLevel, doc), 1.0);
        EXPECT_EQ(run.call(OpGetTrespassWarningLevel, 0), -1.0);
        EXPECT_EQ(run.call(OpIsTrespassing, doc), 0.0);
        run.mFixture.mWorld.mTrespassing.insert(doc);
        EXPECT_EQ(run.call(OpIsTrespassing, doc), 1.0);
        EXPECT_EQ(run.call(OpIsTrespassing, 0), 0.0);

        // The cell of the doctor is the town's: the doctor is of the town, the raider is not
        EXPECT_EQ(run.call(OpIsInMyOwnedCell, doc), 0.0);
        run.mFixture.mObjects.setCellOwner(cell, town);
        EXPECT_EQ(run.call(OpIsInMyOwnedCell, doc), 1.0);
        EXPECT_EQ(run.call(OpIsInMyOwnedCell, raider), 0.0);
        EXPECT_EQ(run.call(OpIsInMyOwnedCell, trudy), 0.0);
        EXPECT_EQ(run.call(OpIsInMyOwnedCell, 0), 0.0);
        EXPECT_EQ(run.call(OpIsCellOwner, 0, { cell, town }), 1.0);
        EXPECT_EQ(run.call(OpIsCellOwner, 0, { cell, gang }), 0.0);
        EXPECT_EQ(run.call(OpIsCellOwner, 0, { 0, town }), 0.0);
        run.mFixture.mObjects.setCellOwner(cell, doc);
        // Without an owner named the owner is the character that is asked
        EXPECT_EQ(run.call(OpIsCellOwner, doc, { cell }), 1.0);
        EXPECT_EQ(run.call(OpIsCellOwner, raider, { cell }), 0.0);
        // Without a cell named, the cell of the character
        EXPECT_EQ(run.call(OpIsCellOwner, doc), 1.0);

        run.mFixture.mObjects.setOwner(crate, doc);
        EXPECT_EQ(run.mFixture.mObjects.owner(crate), doc);
        run.call(OpClearOwnership, crate);
        EXPECT_EQ(run.mFixture.mObjects.owner(crate), 0u);
        run.call(OpClearOwnership, 0);
    }

    TEST(FalloutScriptCrimeCommandsTest, the_victim_of_the_crime_whose_line_is_said_is_the_one_that_is_asked_about)
    {
        Runner run;
        constexpr std::uint16_t OpIsActorAVictim = 0x113A;
        EXPECT_EQ(run.call(OpIsActorAVictim, doc), 0.0);
        run.mFixture.mLaw.setLineVictim(doc);
        EXPECT_EQ(run.mFixture.mLaw.lineVictim(), doc);
        EXPECT_EQ(run.call(OpIsActorAVictim, doc), 1.0);
        EXPECT_EQ(run.call(OpIsActorAVictim, trudy), 0.0);
        EXPECT_EQ(run.call(OpIsActorAVictim, 0), 0.0);
        run.mFixture.mLaw.clear();
        EXPECT_EQ(run.call(OpIsActorAVictim, doc), 0.0);
    }
}
