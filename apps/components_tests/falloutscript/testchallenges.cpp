#include "scriptbuilder.hpp"

#include <components/falloutscript/challenges.hpp>
#include <components/falloutscript/conditions.hpp>
#include <components/falloutscript/savestate.hpp>
#include <components/falloutscript/vanillacommands.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    using namespace testing;
    using namespace FalloutScriptTest;
    using FalloutScript::ChallengeEvent;
    namespace ChallengeFlag = FalloutScript::ChallengeFlag;
    using FalloutScript::ChallengeNotice;
    using FalloutScript::ChallengeRecord;
    using FalloutScript::Challenges;
    using FalloutScript::FormId;
    namespace Type = FalloutScript::ChallengeType;

    constexpr FormId kills = 0x00100001;
    constexpr FormId killsOfList = 0x00100002;
    constexpr FormId anyKill = 0x00100003;
    constexpr FormId hits = 0x00100004;
    constexpr FormId places = 0x00100005;
    constexpr FormId itemUse = 0x00100006;
    constexpr FormId scripted = 0x00100007;
    constexpr FormId locked = 0x00100008;
    constexpr FormId repeating = 0x00100009;
    constexpr FormId statCounter = 0x0010000A;
    constexpr FormId completions = 0x0010000B;
    constexpr FormId unnamed = 0x0010000C;
    constexpr FormId acquired = 0x0010000D;
    constexpr FormId weaponKills = 0x0010000E;
    constexpr FormId skillUse = 0x0010000F;

    constexpr FormId raider = 0x00200001;
    constexpr FormId gecko = 0x00200002;
    constexpr FormId raiderList = 0x00300001;
    constexpr FormId pistol = 0x00400001;
    constexpr FormId rifle = 0x00400002;
    constexpr FormId pistolList = 0x00300002;
    constexpr FormId marker = 0x00500001;
    constexpr FormId markerList = 0x00300003;
    constexpr FormId stimpak = 0x00600001;
    constexpr FormId rewardScript = 0x00700001;

    constexpr std::uint16_t OpUnlockChallenge = 0x125E;
    constexpr std::uint16_t OpChallengeLocked = 0x125F;
    constexpr std::uint16_t OpGetChallengeCompleted = 0x1266;
    constexpr std::uint16_t OpRemoveRecurringFromChallenge = 0x126C;
    constexpr std::uint16_t OpIncrementScriptedChallenge = 0x1275;

    ChallengeRecord record(FormId id, int type, int threshold)
    {
        ChallengeRecord result;
        result.mId = id;
        result.mName = "Name";
        result.mDescription = "Description";
        result.mIcon = "icon";
        result.mType = type;
        result.mThreshold = threshold;
        return result;
    }

    class FakeRecords : public FalloutScript::ChallengeRecords
    {
    public:
        std::vector<ChallengeRecord> mChallenges;
        std::map<FormId, std::set<FormId>> mLists;

        FakeRecords()
        {
            mLists[raiderList] = { raider };
            mLists[pistolList] = { pistol };
            mLists[markerList] = { marker };

            ChallengeRecord one = record(kills, Type::killSpecific, 3);
            one.mSnam = raider;
            mChallenges.push_back(one);

            ChallengeRecord listed = record(killsOfList, Type::killFromList, 2);
            listed.mSnam = raiderList;
            mChallenges.push_back(listed);

            mChallenges.push_back(record(anyKill, Type::killAny, 5));

            ChallengeRecord weapon = record(weaponKills, Type::killAny, 1);
            weapon.mXnam = pistolList;
            weapon.mThird = 4;
            mChallenges.push_back(weapon);

            ChallengeRecord hit = record(hits, Type::hit, 4);
            hit.mFirst = 3;
            mChallenges.push_back(hit);

            ChallengeRecord place = record(places, Type::discoverMarker, 1);
            place.mXnam = markerList;
            mChallenges.push_back(place);

            ChallengeRecord item = record(itemUse, Type::useItem, 2);
            item.mSnam = stimpak;
            item.mScript = rewardScript;
            mChallenges.push_back(item);

            ChallengeRecord get = record(acquired, Type::acquireItem, 10);
            get.mSnam = stimpak;
            get.mInterval = 5;
            mChallenges.push_back(get);

            ChallengeRecord skill = record(skillUse, Type::useSkill, 2);
            skill.mSecond = 7;
            mChallenges.push_back(skill);

            mChallenges.push_back(record(scripted, Type::scripted, 2));

            ChallengeRecord gated = record(locked, Type::scripted, 1);
            gated.mFlags = ChallengeFlag::startDisabled;
            mChallenges.push_back(gated);

            ChallengeRecord again = record(repeating, Type::killAny, 2);
            again.mFlags = ChallengeFlag::recurring;
            again.mSecond = 9;
            mChallenges.push_back(again);

            ChallengeRecord stat = record(statCounter, Type::miscStat, 3);
            stat.mFirst = 11;
            mChallenges.push_back(stat);

            ChallengeRecord total = record(completions, Type::miscStat, 100);
            total.mFirst = FalloutScript::challengesCompletedStat;
            mChallenges.push_back(total);

            ChallengeRecord quiet = record(unnamed, Type::scripted, 1);
            quiet.mName.clear();
            mChallenges.push_back(quiet);
        }

        void forEachChallenge(const std::function<void(const ChallengeRecord&)>& function) override
        {
            for (const ChallengeRecord& challenge : mChallenges)
                function(challenge);
        }

        bool inList(FormId list, FormId form) override
        {
            const auto found = mLists.find(list);
            return found != mLists.end() && found->second.count(form) != 0;
        }
    };

    struct Runner
    {
        FakeRecords mRecords;
        Challenges mChallenges{ mRecords };
        std::vector<ChallengeNotice> mNotices;
        std::vector<FormId> mScripts;
        std::vector<std::pair<int, int>> mCounter;

        Runner()
        {
            mChallenges.setNotice([this](const ChallengeNotice& notice) { mNotices.push_back(notice); });
            mChallenges.setScriptRunner([this](FormId script) { mScripts.push_back(script); });
            mChallenges.setCompletedCounter([this](int stat, int amount) {
                mCounter.emplace_back(stat, amount);
                // The counter of the Pip-Boy is one the challenges count too
                ChallengeEvent event;
                event.mType = Type::miscStat;
                event.mFirst = stat;
                event.mAmount = amount;
                mChallenges.event(event);
            });
            mChallenges.reset();
        }

        void kill(FormId who, FormId with = 0, int weaponKind = 0, int kind = 0)
        {
            ChallengeEvent event;
            event.mType = Type::killSpecific;
            event.mFormA = who;
            event.mFormB = with;
            event.mThird = weaponKind;
            event.mSecond = kind;
            mChallenges.event(event);
            event.mType = Type::killFromList;
            mChallenges.event(event);
            event.mType = Type::killAny;
            mChallenges.event(event);
        }
    };

    TEST(FalloutScriptChallengesTest, a_challenge_that_counts_one_kind_of_kill_counts_only_that)
    {
        Runner run;
        run.kill(gecko);
        EXPECT_EQ(run.mChallenges.progress(kills), 0);
        run.kill(raider);
        run.kill(raider);
        EXPECT_EQ(run.mChallenges.progress(kills), 2);
        EXPECT_FALSE(run.mChallenges.completed(kills));
    }

    TEST(FalloutScriptChallengesTest, a_challenge_with_a_list_counts_the_forms_in_it)
    {
        Runner run;
        run.kill(gecko);
        EXPECT_EQ(run.mChallenges.progress(killsOfList), 0);
        run.kill(raider);
        EXPECT_EQ(run.mChallenges.progress(killsOfList), 1);
    }

    TEST(FalloutScriptChallengesTest, a_kill_with_no_form_counts_for_no_challenge_that_names_one)
    {
        Runner run;
        run.kill(0);
        EXPECT_EQ(run.mChallenges.progress(kills), 0);
        EXPECT_EQ(run.mChallenges.progress(killsOfList), 0);
        // But the challenge that counts any kill is no such challenge
        EXPECT_EQ(run.mChallenges.progress(anyKill), 1);
    }

    TEST(FalloutScriptChallengesTest, a_challenge_with_a_weapon_list_and_a_kind_wants_both)
    {
        Runner run;
        run.kill(raider, rifle, 4);
        EXPECT_EQ(run.mChallenges.progress(weaponKills), 0);
        run.kill(raider, pistol, 3);
        EXPECT_EQ(run.mChallenges.progress(weaponKills), 0);
        run.kill(raider, 0, 4);
        EXPECT_EQ(run.mChallenges.progress(weaponKills), 0);
        run.kill(raider, pistol, 4);
        EXPECT_TRUE(run.mChallenges.completed(weaponKills));
    }

    TEST(FalloutScriptChallengesTest, a_second_form_is_asked_for_by_a_record_that_names_one_for_any_type)
    {
        Runner run;
        // The place the player found must be one of the list of the challenge, and a find that names no place counts
        // for none that names one
        ChallengeEvent event;
        event.mType = Type::discoverMarker;
        event.mFormB = 0;
        run.mChallenges.event(event);
        EXPECT_FALSE(run.mChallenges.completed(places));
        event.mFormB = stimpak;
        run.mChallenges.event(event);
        EXPECT_FALSE(run.mChallenges.completed(places));
        event.mFormB = marker;
        run.mChallenges.event(event);
        EXPECT_TRUE(run.mChallenges.completed(places));
    }

    TEST(FalloutScriptChallengesTest, the_first_value_of_a_record_must_be_the_first_of_the_event)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::hit;
        event.mFirst = 2;
        run.mChallenges.event(event);
        event.mFirst = 0;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(hits), 0);
        event.mFirst = 3;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(hits), 1);
    }

    TEST(FalloutScriptChallengesTest, a_hit_that_names_no_second_value_counts_the_blows_of_kind_0_and_1_only)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::hit;
        event.mFirst = 3;
        event.mSecond = 2;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(hits), 0);
        event.mSecond = 1;
        run.mChallenges.event(event);
        event.mSecond = 0;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(hits), 2);
    }

    TEST(FalloutScriptChallengesTest, a_second_value_of_the_record_must_be_the_one_of_the_event_and_not_0)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::useSkill;
        run.mChallenges.event(event);
        event.mSecond = 6;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(skillUse), 0);
        event.mSecond = 7;
        event.mAmount = 2;
        run.mChallenges.event(event);
        EXPECT_TRUE(run.mChallenges.completed(skillUse));
    }

    TEST(FalloutScriptChallengesTest, a_counter_of_the_pipboy_is_matched_by_its_number_even_0)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::miscStat;
        event.mFirst = 10;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(statCounter), 0);
        event.mFirst = 11;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(statCounter), 1);
    }

    TEST(FalloutScriptChallengesTest, the_amount_of_an_event_is_what_it_adds)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::acquireItem;
        event.mFormA = stimpak;
        event.mAmount = 4;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(acquired), 4);
    }

    TEST(FalloutScriptChallengesTest, the_progress_is_told_at_each_interval_and_the_end_at_the_threshold)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::acquireItem;
        event.mFormA = stimpak;
        for (int i = 0; i < 10; ++i)
            run.mChallenges.event(event);
        ASSERT_EQ(run.mNotices.size(), 2u);
        EXPECT_FALSE(run.mNotices[0].mCompleted);
        EXPECT_EQ(run.mNotices[0].mChallenge, acquired);
        // The name, the progress and the threshold, and the description under them
        EXPECT_EQ(run.mNotices[0].mText, "Name   5\\10\nDescription");
        EXPECT_EQ(run.mNotices[0].mIcon, "icon");
        EXPECT_TRUE(run.mNotices[1].mCompleted);
        EXPECT_EQ(run.mNotices[1].mText, "Name   10\\10\nDescription");
    }

    TEST(FalloutScriptChallengesTest, the_interval_is_100_when_the_record_has_none)
    {
        Runner run;
        // A challenge that needs 5 tells nothing before it is done (the progress is no multiple of 100 until then)
        for (int i = 0; i < 4; ++i)
            run.kill(gecko);
        EXPECT_TRUE(run.mNotices.empty() || run.mNotices.back().mChallenge != anyKill);
        run.kill(gecko);
        ASSERT_FALSE(run.mNotices.empty());
        bool told = false;
        for (const ChallengeNotice& notice : run.mNotices)
            told = told || (notice.mChallenge == anyKill && notice.mCompleted);
        EXPECT_TRUE(told);
    }

    TEST(FalloutScriptChallengesTest, a_challenge_that_is_done_runs_its_script_and_counts_for_the_pipboy)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::useItem;
        event.mFormA = stimpak;
        event.mAmount = 2;
        run.mChallenges.event(event);
        EXPECT_TRUE(run.mChallenges.completed(itemUse));
        EXPECT_THAT(run.mScripts, ElementsAre(rewardScript));
        EXPECT_THAT(run.mCounter, ElementsAre(Pair(FalloutScript::challengesCompletedStat, 1)));
        EXPECT_EQ(run.mChallenges.completedBit(itemUse), 2);
        // It is no longer counted
        run.mChallenges.event(event);
        EXPECT_THAT(run.mScripts, ElementsAre(rewardScript));
        EXPECT_THAT(run.mChallenges.active(Type::useItem), IsEmpty());
    }

    TEST(FalloutScriptChallengesTest, the_challenge_that_counts_the_completions_does_not_count_itself)
    {
        Runner run;
        run.mChallenges.unlock(completions);
        // The counter went up once for the other challenge, and that is what the challenge of the counter counts
        ChallengeEvent event;
        event.mType = Type::useItem;
        event.mFormA = stimpak;
        event.mAmount = 2;
        run.mChallenges.event(event);
        EXPECT_EQ(run.mChallenges.progress(completions), 1);
        // And the completion of that challenge does not raise the counter again
        ChallengeEvent big;
        big.mType = Type::miscStat;
        big.mFirst = FalloutScript::challengesCompletedStat;
        big.mAmount = 200;
        run.mCounter.clear();
        run.mChallenges.event(big);
        EXPECT_TRUE(run.mChallenges.completed(completions));
        EXPECT_THAT(run.mCounter, IsEmpty());
    }

    TEST(FalloutScriptChallengesTest, nothing_is_told_without_a_name_and_a_description_or_when_the_setting_is_off)
    {
        Runner run;
        run.mChallenges.incrementScripted(unnamed);
        EXPECT_TRUE(run.mChallenges.completed(unnamed));
        EXPECT_THAT(run.mNotices, IsEmpty());
        // The counter of the Pip-Boy goes up all the same
        EXPECT_THAT(run.mCounter, ElementsAre(Pair(FalloutScript::challengesCompletedStat, 1)));

        run.mChallenges.setShowUpdates(false);
        run.mChallenges.incrementScripted(scripted);
        run.mChallenges.incrementScripted(scripted);
        EXPECT_TRUE(run.mChallenges.completed(scripted));
        EXPECT_THAT(run.mNotices, IsEmpty());
    }

    TEST(FalloutScriptChallengesTest, a_scripted_challenge_counts_one_for_each_call_and_only_that_challenge)
    {
        Runner run;
        run.mChallenges.incrementScripted(scripted);
        EXPECT_EQ(run.mChallenges.progress(scripted), 1);
        // A challenge of another type, or one that is not known, is not counted
        run.mChallenges.incrementScripted(kills);
        run.mChallenges.incrementScripted(0x00999999);
        EXPECT_EQ(run.mChallenges.progress(kills), 0);
        run.mChallenges.incrementScripted(scripted);
        EXPECT_TRUE(run.mChallenges.completed(scripted));
    }

    TEST(FalloutScriptChallengesTest, a_challenge_that_starts_disabled_counts_after_it_is_unlocked)
    {
        Runner run;
        EXPECT_FALSE(run.mChallenges.unlocked(locked));
        run.mChallenges.incrementScripted(locked);
        EXPECT_EQ(run.mChallenges.progress(locked), 0);
        EXPECT_THAT(run.mChallenges.active(Type::scripted), Not(Contains(locked)));

        run.mChallenges.unlock(locked);
        EXPECT_TRUE(run.mChallenges.unlocked(locked));
        EXPECT_THAT(run.mChallenges.active(Type::scripted), Contains(locked));
        run.mChallenges.incrementScripted(locked);
        EXPECT_TRUE(run.mChallenges.completed(locked));
    }

    TEST(FalloutScriptChallengesTest, unlocking_what_is_not_a_challenge_does_nothing)
    {
        Runner run;
        run.mChallenges.unlock(0x00999999);
        run.mChallenges.removeRecurring(0x00999999);
        EXPECT_FALSE(run.mChallenges.known(0x00999999));
        EXPECT_FALSE(run.mChallenges.unlocked(0x00999999));
        EXPECT_FALSE(run.mChallenges.completed(0x00999999));
        EXPECT_FALSE(run.mChallenges.repeated(0x00999999));
        EXPECT_EQ(run.mChallenges.progress(0x00999999), 0);
        EXPECT_TRUE(run.mChallenges.known(kills));
    }

    TEST(FalloutScriptChallengesTest, a_recurring_challenge_starts_over_with_what_went_beyond_the_threshold)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::killAny;
        event.mSecond = 9;
        event.mAmount = 3;
        run.mChallenges.event(event);
        EXPECT_FALSE(run.mChallenges.completed(repeating));
        EXPECT_TRUE(run.mChallenges.repeated(repeating));
        EXPECT_EQ(run.mChallenges.progress(repeating), 1);
        EXPECT_THAT(run.mCounter, ElementsAre(Pair(FalloutScript::challengesCompletedStat, 1)));
        EXPECT_THAT(run.mChallenges.active(Type::killAny), Contains(repeating));

        event.mAmount = 1;
        run.mChallenges.event(event);
        EXPECT_THAT(run.mCounter, SizeIs(2));
        EXPECT_EQ(run.mChallenges.progress(repeating), 0);
    }

    TEST(FalloutScriptChallengesTest, taking_the_recurring_away_makes_the_next_completion_the_last)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::killAny;
        event.mSecond = 9;
        event.mAmount = 2;
        run.mChallenges.event(event);
        EXPECT_FALSE(run.mChallenges.completed(repeating));
        run.mChallenges.removeRecurring(repeating);
        run.mChallenges.event(event);
        EXPECT_TRUE(run.mChallenges.completed(repeating));
        EXPECT_THAT(run.mChallenges.active(Type::killAny), Not(Contains(repeating)));
    }

    TEST(FalloutScriptChallengesTest, a_recurring_challenge_that_was_completed_for_good_is_not_counted_again_when_unlocked)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::killAny;
        event.mSecond = 9;
        event.mAmount = 2;
        run.mChallenges.removeRecurring(repeating);
        run.mChallenges.event(event);
        ASSERT_TRUE(run.mChallenges.completed(repeating));
        ASSERT_THAT(run.mCounter, SizeIs(1));
        // Unlocking puts it on its list again, but a challenge that is completed does not count
        run.mChallenges.unlock(repeating);
        run.mChallenges.event(event);
        EXPECT_THAT(run.mCounter, SizeIs(1));
        EXPECT_EQ(run.mChallenges.progress(repeating), 2);
    }

    TEST(FalloutScriptChallengesTest, a_count_made_by_the_script_of_a_completion_comes_too_late_to_complete_it_again)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = Type::useItem;
        event.mFormA = stimpak;
        event.mAmount = 2;
        int scripts = 0;
        // The reward script counts the same event again (as another thread or another script could)
        run.mChallenges.setScriptRunner([&](FormId) {
            ++scripts;
            run.mChallenges.event(event);
        });
        run.mChallenges.event(event);
        EXPECT_EQ(scripts, 1);
        EXPECT_TRUE(run.mChallenges.completed(itemUse));
        EXPECT_THAT(run.mCounter, ElementsAre(Pair(FalloutScript::challengesCompletedStat, 1)));
    }

    TEST(FalloutScriptChallengesTest, the_list_has_the_challenges_the_player_can_see)
    {
        Runner run;
        run.mChallenges.incrementScripted(scripted);
        run.mChallenges.incrementScripted(scripted);
        const std::vector<Challenges::View> list = run.mChallenges.list();
        bool sawLocked = false;
        bool sawDone = false;
        for (const Challenges::View& view : list)
        {
            sawLocked = sawLocked || view.mRecord.mId == locked;
            sawDone = sawDone || (view.mRecord.mId == scripted && view.mState.mCompleted);
        }
        EXPECT_FALSE(sawLocked);
        EXPECT_TRUE(sawDone);
        run.mChallenges.unlock(locked);
        EXPECT_THAT(run.mChallenges.list(), SizeIs(list.size() + 1));
    }

    TEST(FalloutScriptChallengesTest, an_event_of_a_type_that_does_not_exist_counts_for_nothing)
    {
        Runner run;
        ChallengeEvent event;
        event.mType = 77;
        run.mChallenges.event(event);
        event.mType = -1;
        run.mChallenges.event(event);
        for (const Challenges::View& view : run.mChallenges.list())
            EXPECT_EQ(view.mState.mProgress, 0);
    }

    TEST(FalloutScriptChallengesTest, resetting_starts_every_challenge_again)
    {
        Runner run;
        run.mChallenges.incrementScripted(scripted);
        run.mChallenges.unlock(locked);
        run.mChallenges.reset();
        EXPECT_EQ(run.mChallenges.progress(scripted), 0);
        EXPECT_FALSE(run.mChallenges.unlocked(locked));
    }

    TEST(FalloutScriptChallengesTest, what_has_changed_is_saved_and_comes_back)
    {
        Runner run;
        run.mChallenges.unlock(locked);
        run.mChallenges.incrementScripted(scripted);
        run.mChallenges.incrementScripted(scripted);
        ChallengeEvent event;
        event.mType = Type::killAny;
        event.mSecond = 9;
        event.mAmount = 3;
        run.mChallenges.event(event);
        run.mChallenges.removeRecurring(repeating);
        run.kill(raider);

        FalloutScript::SaveWriter out;
        FalloutScript::writeChallenges(run.mChallenges, out);

        Runner other;
        // Whatever the other had is replaced
        other.mChallenges.incrementScripted(unnamed);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readChallenges(other.mChallenges, in, FalloutScript::challengesSectionVersion);
        EXPECT_EQ(in.remaining(), 0u);

        EXPECT_TRUE(other.mChallenges.completed(scripted));
        EXPECT_TRUE(other.mChallenges.unlocked(locked));
        EXPECT_TRUE(other.mChallenges.repeated(repeating));
        EXPECT_EQ(other.mChallenges.progress(repeating), run.mChallenges.progress(repeating));
        EXPECT_EQ(other.mChallenges.progress(kills), 1);
        EXPECT_FALSE(other.mChallenges.completed(unnamed));
        // The lists are made again: a challenge that is done is on none, one that was unlocked is on its list
        EXPECT_THAT(other.mChallenges.active(Type::scripted), Contains(locked));
        EXPECT_THAT(other.mChallenges.active(Type::scripted), Not(Contains(scripted)));
    }

    TEST(FalloutScriptChallengesTest, a_challenge_a_save_names_that_the_plugins_no_longer_have_is_left_out)
    {
        Runner run;
        run.mChallenges.incrementScripted(scripted);
        FalloutScript::SaveWriter out;
        FalloutScript::writeChallenges(run.mChallenges, out);

        Runner other;
        other.mRecords.mChallenges.erase(
            std::remove_if(other.mRecords.mChallenges.begin(), other.mRecords.mChallenges.end(),
                [](const ChallengeRecord& challenge) { return challenge.mId == scripted; }),
            other.mRecords.mChallenges.end());
        other.mChallenges.reset();
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        FalloutScript::readChallenges(other.mChallenges, in, FalloutScript::challengesSectionVersion);
        EXPECT_FALSE(other.mChallenges.known(scripted));
        EXPECT_EQ(other.mChallenges.progress(scripted), 0);
    }

    struct CommandRunner : Runner
    {
        FalloutScript::CommandTable mCommands = FalloutScript::vanillaCommands(FalloutScript::Game::NewVegas);
        TestHost mHost;
        FalloutScript::Interpreter mInterpreter{ mHost, mCommands };
        FalloutScript::ConditionEvaluator mEvaluator{ mInterpreter };

        CommandRunner()
        {
            FalloutScript::addChallengeCommands(mInterpreter, mChallenges);
            FalloutScript::addChallengeConditionFunctions(mEvaluator, mChallenges);
        }

        double call(std::uint16_t opcode, std::vector<double> numbers = {})
        {
            std::vector<FalloutScript::Value> arguments;
            for (const double number : numbers)
                arguments.push_back({ number, {}, false });
            return mInterpreter.call(opcode, 0, false, std::move(arguments));
        }

        double condition(std::uint32_t function, FormId challenge)
        {
            ESM4::TargetCondition result{};
            result.condition = ESM4::CTF_EqualTo;
            result.functionIndex = function;
            result.param1 = challenge;
            return mEvaluator.value(result, {});
        }
    };

    TEST(FalloutScriptChallengesTest, the_commands_are_views_on_the_rule)
    {
        CommandRunner run;
        EXPECT_EQ(run.call(OpChallengeLocked, { locked }), 0.0);
        run.call(OpUnlockChallenge, { locked });
        EXPECT_EQ(run.call(OpChallengeLocked, { locked }), 1.0);
        EXPECT_TRUE(run.mChallenges.unlocked(locked));

        run.call(OpIncrementScriptedChallenge, { scripted });
        EXPECT_EQ(run.mChallenges.progress(scripted), 1);
        EXPECT_EQ(run.call(OpGetChallengeCompleted, { scripted }), 0.0);
        run.call(OpIncrementScriptedChallenge, { scripted });
        EXPECT_EQ(run.call(OpGetChallengeCompleted, { scripted }), 1.0);
    }

    TEST(FalloutScriptChallengesTest, the_command_tells_1_for_a_recurring_challenge_that_started_over)
    {
        CommandRunner run;
        ChallengeEvent event;
        event.mType = Type::killAny;
        event.mSecond = 9;
        event.mAmount = 2;
        run.mChallenges.event(event);
        EXPECT_FALSE(run.mChallenges.completed(repeating));
        EXPECT_EQ(run.call(OpGetChallengeCompleted, { repeating }), 1.0);
        run.call(OpRemoveRecurringFromChallenge, { repeating });
        run.mChallenges.event(event);
        EXPECT_TRUE(run.mChallenges.completed(repeating));
    }

    TEST(FalloutScriptChallengesTest, the_condition_of_a_completed_challenge_tells_2_as_the_executable_does)
    {
        CommandRunner run;
        EXPECT_EQ(run.condition(0x266, scripted), 0.0);
        run.call(OpIncrementScriptedChallenge, { scripted });
        run.call(OpIncrementScriptedChallenge, { scripted });
        EXPECT_EQ(run.condition(0x266, scripted), 2.0);
        EXPECT_EQ(run.condition(0x25F, locked), 0.0);
        run.call(OpUnlockChallenge, { locked });
        EXPECT_EQ(run.condition(0x25F, locked), 1.0);
    }
}
