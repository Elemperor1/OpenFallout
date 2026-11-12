#include <components/falloutscript/music.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{
    using namespace FalloutScript::Music;

    Fighter hunter(float strength)
    {
        Fighter fighter;
        fighter.mHuntsPlayer = true;
        fighter.mInCombat = true;
        fighter.mStrength = strength;
        return fighter;
    }

    Fighter bystander(float distance, float strength, bool ally)
    {
        Fighter fighter;
        fighter.mInCombat = true;
        fighter.mDistance = distance;
        fighter.mStrength = strength;
        fighter.mAllyNear = ally;
        return fighter;
    }

    TEST(FalloutScriptMusicTest, combat_music_starts_against_an_enemy_that_is_a_threat)
    {
        CombatMusic music;
        // The player counts for 200: an enemy of 100 is not a threat (200 / 100 is not under 2), one of 101 is
        EXPECT_FALSE(music.update(1.f, 200.f, std::vector{ hunter(100.f) }));
        EXPECT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(101.f) }));
        EXPECT_TRUE(music.on());
    }

    TEST(FalloutScriptMusicTest, combat_music_counts_all_the_hunters_together)
    {
        CombatMusic music;
        EXPECT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(60.f), hunter(60.f), hunter(60.f) }));
    }

    TEST(FalloutScriptMusicTest, combat_music_waits_for_its_update_time)
    {
        CombatMusic music;
        EXPECT_FALSE(music.update(0.1f, 200.f, std::vector<Fighter>{}));
        // The next look is a second later; a fight that begins in between is seen then
        EXPECT_FALSE(music.update(0.5f, 200.f, std::vector{ hunter(500.f) }));
        EXPECT_FALSE(music.update(0.25f, 200.f, std::vector{ hunter(500.f) }));
        EXPECT_TRUE(music.update(0.25f, 200.f, std::vector{ hunter(500.f) }));
    }

    TEST(FalloutScriptMusicTest, combat_music_stops_five_seconds_after_the_fight)
    {
        CombatMusic music;
        ASSERT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(500.f) }));
        // The fight is over: the timer starts at the next look and has run out five seconds later
        EXPECT_TRUE(music.update(1.f, 200.f, std::vector<Fighter>{}));
        for (int i = 0; i < 4; ++i)
            EXPECT_TRUE(music.update(1.f, 200.f, std::vector<Fighter>{}));
        EXPECT_FALSE(music.update(1.f, 200.f, std::vector<Fighter>{}));
        EXPECT_FALSE(music.on());
    }

    TEST(FalloutScriptMusicTest, combat_music_goes_on_with_any_hunter_while_it_plays)
    {
        CombatMusic music;
        ASSERT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(500.f) }));
        // A weak hunter is enough to keep it
        for (int i = 0; i < 20; ++i)
            EXPECT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(1.f) }));
    }

    TEST(FalloutScriptMusicTest, a_fight_near_the_player_with_a_friend_in_it_starts_the_music)
    {
        CombatMusic music;
        // Inside 2500 units, with a friend among them, and stronger than the player
        EXPECT_FALSE(music.update(1.f, 200.f, std::vector{ bystander(2000.f, 300.f, false) }));
        EXPECT_FALSE(music.update(1.f, 200.f, std::vector{ bystander(2000.f, 150.f, true) }));
        EXPECT_FALSE(music.update(1.f, 200.f, std::vector{ bystander(3000.f, 900.f, true) }));
        EXPECT_TRUE(music.update(1.f, 200.f, std::vector{ bystander(2000.f, 300.f, true) }));
    }

    TEST(FalloutScriptMusicTest, a_fight_nearby_keeps_the_music_up_to_the_outer_radius)
    {
        CombatMusic music;
        ASSERT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(500.f) }));
        // The hunter is gone, a fight that is not the player's goes on at 3000 units: the music stays; at 4000 it
        // goes the way of a fight that is over
        for (int i = 0; i < 10; ++i)
            EXPECT_TRUE(music.update(1.f, 200.f, std::vector{ bystander(3000.f, 10.f, false) }));
        bool on = true;
        for (int i = 0; i < 10; ++i)
            on = music.update(1.f, 200.f, std::vector{ bystander(4000.f, 10.f, false) });
        EXPECT_FALSE(on);
    }

    TEST(FalloutScriptMusicTest, combat_music_resets)
    {
        CombatMusic music;
        ASSERT_TRUE(music.update(1.f, 200.f, std::vector{ hunter(500.f) }));
        music.reset();
        EXPECT_FALSE(music.on());
    }

    TEST(FalloutScriptMusicTest, the_next_track_is_random_and_never_repeats)
    {
        // Fallout's rule (FalloutNV.exe 0x591A40): the pick that equals the last one (not the first) becomes the first,
        // the first that follows the first becomes the second
        std::size_t last = 3;
        EXPECT_EQ(nextTrack(5, last, [](int, int) { return 3; }), 0u);
        EXPECT_EQ(last, 0u);
        EXPECT_EQ(nextTrack(5, last, [](int, int) { return 0; }), 1u);
        EXPECT_EQ(last, 1u);
        EXPECT_EQ(nextTrack(5, last, [](int, int) { return 4; }), 4u);
        EXPECT_EQ(last, 4u);
        EXPECT_EQ(nextTrack(5, last, [](int, int) { return 2; }), 2u);
        EXPECT_EQ(last, 2u);

        // A folder of one track plays it, an empty folder none
        EXPECT_EQ(nextTrack(1, last, [](int, int) { return 0; }), 0u);
        EXPECT_FALSE(nextTrack(0, last, [](int, int) { return 0; }).has_value());
    }

    TEST(FalloutScriptMusicTest, no_track_follows_itself_for_any_random_sequence)
    {
        std::size_t last = 0;
        for (int round = 0; round < 1000; ++round)
        {
            const std::size_t before = last;
            const int roll = round % 7;
            const auto pick = nextTrack(7, last, [roll](int, int) { return roll; });
            ASSERT_TRUE(pick.has_value());
            if (round > 0)
                EXPECT_NE(*pick, before) << "round " << round;
        }
    }

    TEST(FalloutScriptMusicTest, the_first_track_is_a_random_one)
    {
        EXPECT_EQ(firstTrack(6, [](int low, int high) {
            EXPECT_EQ(low, 0);
            EXPECT_EQ(high, 5);
            return 4;
        }),
            4u);
        EXPECT_EQ(firstTrack(0, [](int, int) { return 9; }), 0u);
    }

    TEST(FalloutScriptMusicTest, the_attenuation_of_a_record_is_its_size_and_a_positive_one_loops)
    {
        // FalloutNV.exe 0x830050 and 0x830080
        EXPECT_FLOAT_EQ(attenuationOf(6.f).mDb, -6.f);
        EXPECT_TRUE(attenuationOf(6.f).mLoop);
        EXPECT_FLOAT_EQ(attenuationOf(-6.f).mDb, -6.f);
        EXPECT_FALSE(attenuationOf(-6.f).mLoop);
        EXPECT_FLOAT_EQ(attenuationOf(0.f).mDb, 0.f);
        EXPECT_FALSE(attenuationOf(0.f).mLoop);
        EXPECT_TRUE(attenuationOf(0.0001f).mLoop);
        EXPECT_FLOAT_EQ(gainOfDb(0.f), 1.f);
        EXPECT_NEAR(gainOfDb(-6.f), 0.501f, 0.001f);
        EXPECT_NEAR(gainOfDb(-20.f), 0.1f, 1e-5f);
    }

    TEST(FalloutScriptMusicTest, the_day_runs_from_six_to_the_night_start)
    {
        // The defaults: 64 and 255 of 256 (6:00 and 23:54)
        EXPECT_TRUE(isDay(12.f, 0, 0, true));
        EXPECT_FALSE(isDay(5.9f, 0, 0, true));
        EXPECT_TRUE(isDay(6.f, 0, 0, true));
        EXPECT_TRUE(isDay(23.8f, 0, 0, true));
        EXPECT_FALSE(isDay(23.95f, 0, 0, true));
        // The record's own times: 8:00 (85/256 * 24 = 7.97) to 20:00 (213/256 * 24 = 19.97)
        EXPECT_FALSE(isDay(7.f, 85, 213, false));
        EXPECT_TRUE(isDay(12.f, 85, 213, false));
        EXPECT_FALSE(isDay(22.f, 85, 213, false));
        // A day that wraps around midnight
        EXPECT_TRUE(isDay(1.f, 200, 40, false));
        EXPECT_FALSE(isDay(12.f, 200, 40, false));
    }

    TEST(FalloutScriptMusicTest, the_innermost_layer_that_holds_plays)
    {
        const std::array<float, 3> boundaries{ 100.f, 66.6f, 33.3f };
        EXPECT_EQ(layerAt(0.9f, 0b111, boundaries), 0);
        EXPECT_EQ(layerAt(0.6f, 0b111, boundaries), 1);
        EXPECT_EQ(layerAt(0.2f, 0b111, boundaries), 2);
        EXPECT_EQ(layerAt(0.0f, 0b111, boundaries), 2);
        EXPECT_FALSE(layerAt(1.2f, 0b111, boundaries).has_value());
        // A layer that is not switched on is skipped: the inner is off, so the middle plays at the middle of the radius
        EXPECT_EQ(layerAt(0.2f, 0b011, boundaries), 1);
        // Only the outer layer on
        EXPECT_EQ(layerAt(0.2f, 0b001, boundaries), 0);
        EXPECT_FALSE(layerAt(0.2f, 0b000, boundaries).has_value());
        // All layers with a boundary of 100 percent: the innermost on plays everywhere
        EXPECT_EQ(layerAt(0.95f, 0b111, std::array<float, 3>{ 100.f, 100.f, 100.f }), 2);
    }

    struct DirectorRun
    {
        Director mDirector;
        bool mPlaying = false;
        std::uint32_t mExplore = 0;
        std::uint32_t mBattle = 0;
        bool mCombat = false;
        bool mLoops = false;
        double mUnit = 0.5;

        DirectorAction step(float seconds)
        {
            DirectorInput input;
            input.mSeconds = seconds;
            input.mExplore = mExplore;
            input.mBattle = mBattle;
            input.mCombat = mCombat;
            input.mPlaying = mPlaying;
            input.mLoops = mLoops;
            return mDirector.update(input, [this] { return mUnit; });
        }
    };

    TEST(FalloutScriptMusicTest, the_music_of_a_place_starts_after_the_start_delay)
    {
        DirectorRun run;
        run.mExplore = 7;
        float time = 0.f;
        DirectorAction action;
        while (action.mKind == DirectorAction::Kind::None && time < 30.f)
        {
            action = run.step(0.5f);
            time += 0.5f;
        }
        EXPECT_EQ(action.mKind, DirectorAction::Kind::Start);
        EXPECT_EQ(action.mMusic, 7u);
        EXPECT_FLOAT_EQ(action.mFade, 1.f);
        EXPECT_NEAR(time, 8.f, 0.51f);
    }

    TEST(FalloutScriptMusicTest, nothing_plays_for_a_place_without_music)
    {
        DirectorRun run;
        for (int i = 0; i < 400; ++i)
            EXPECT_EQ(run.step(1.f).mKind, DirectorAction::Kind::None);
    }

    TEST(FalloutScriptMusicTest, a_track_that_ends_is_followed_by_a_rest)
    {
        DirectorRun run;
        run.mExplore = 7;
        DirectorAction action;
        do
            action = run.step(1.f);
        while (action.mKind != DirectorAction::Kind::Start);
        run.mPlaying = true;
        for (int i = 0; i < 100; ++i)
            EXPECT_EQ(run.step(1.f).mKind, DirectorAction::Kind::None);
        // The track ends; with a random number of 0.5 the rest is 120 seconds
        run.mPlaying = false;
        int seconds = 0;
        action = DirectorAction();
        while (action.mKind == DirectorAction::Kind::None && seconds < 400)
        {
            action = run.step(1.f);
            ++seconds;
        }
        EXPECT_EQ(action.mKind, DirectorAction::Kind::Start);
        EXPECT_NEAR(seconds, 120, 2);
    }

    TEST(FalloutScriptMusicTest, a_track_that_loops_starts_again_at_once)
    {
        DirectorRun run;
        run.mExplore = 7;
        DirectorAction action;
        do
            action = run.step(1.f);
        while (action.mKind != DirectorAction::Kind::Start);
        run.mPlaying = true;
        run.step(1.f);
        run.mPlaying = false;
        run.mLoops = true;
        EXPECT_EQ(run.step(1.f).mKind, DirectorAction::Kind::Start);
    }

    TEST(FalloutScriptMusicTest, a_change_of_place_fades_out_and_starts_the_next_after_the_delay)
    {
        DirectorRun run;
        run.mExplore = 7;
        DirectorAction action;
        do
            action = run.step(1.f);
        while (action.mKind != DirectorAction::Kind::Start);
        run.mPlaying = true;
        run.step(1.f);
        run.mExplore = 9;
        action = run.step(1.f);
        EXPECT_EQ(action.mKind, DirectorAction::Kind::FadeOut);
        EXPECT_FLOAT_EQ(action.mFade, 1.f);
        // It does not ask again while it fades
        EXPECT_EQ(run.step(0.1f).mKind, DirectorAction::Kind::None);
        run.mPlaying = false;
        int seconds = 0;
        action = DirectorAction();
        while (action.mKind == DirectorAction::Kind::None && seconds < 60)
        {
            action = run.step(1.f);
            ++seconds;
        }
        EXPECT_EQ(action.mKind, DirectorAction::Kind::Start);
        EXPECT_EQ(action.mMusic, 9u);
        EXPECT_LE(seconds, 9);
    }

    TEST(FalloutScriptMusicTest, a_fight_takes_the_music_over_and_gives_it_back_after_the_battle_wait)
    {
        DirectorRun run;
        run.mExplore = 7;
        run.mBattle = 20;
        DirectorAction action;
        do
            action = run.step(1.f);
        while (action.mKind != DirectorAction::Kind::Start);
        run.mPlaying = true;
        run.step(1.f);
        // The fight begins: the place's track fades out
        run.mCombat = true;
        action = run.step(1.f);
        EXPECT_EQ(action.mKind, DirectorAction::Kind::FadeOut);
        run.mPlaying = false;
        // and the battle music starts at once, with its own fade
        action = run.step(1.f);
        EXPECT_EQ(action.mKind, DirectorAction::Kind::Start);
        EXPECT_EQ(action.mMusic, 20u);
        EXPECT_FLOAT_EQ(action.mFade, 3.f);
        run.mPlaying = true;
        run.step(1.f);
        // The battle track ends while the fight goes on: the next one is at once
        run.mPlaying = false;
        EXPECT_EQ(run.step(1.f).mKind, DirectorAction::Kind::Start);
        run.mPlaying = true;
        run.step(1.f);
        // The fight is over: the battle track fades out over 3 seconds, and the place's music is back after 10
        run.mCombat = false;
        action = run.step(1.f);
        EXPECT_EQ(action.mKind, DirectorAction::Kind::FadeOut);
        EXPECT_FLOAT_EQ(action.mFade, 3.f);
        run.mPlaying = false;
        int seconds = 0;
        action = DirectorAction();
        while (action.mKind == DirectorAction::Kind::None && seconds < 60)
        {
            action = run.step(1.f);
            ++seconds;
        }
        EXPECT_EQ(action.mKind, DirectorAction::Kind::Start);
        EXPECT_EQ(action.mMusic, 7u);
        EXPECT_NEAR(seconds, 10, 1);
    }

    TEST(FalloutScriptMusicTest, a_blocked_director_starts_nothing)
    {
        DirectorRun run;
        run.mExplore = 7;
        for (int i = 0; i < 50; ++i)
        {
            DirectorInput input;
            input.mSeconds = 1.f;
            input.mExplore = 7;
            input.mBlocked = true;
            EXPECT_EQ(run.mDirector.update(input, [] { return 0.5; }).mKind, DirectorAction::Kind::None);
        }
        // When the block lifts the music of the place comes after the delay
        int seconds = 0;
        DirectorAction action;
        while (action.mKind == DirectorAction::Kind::None && seconds < 60)
        {
            action = run.step(1.f);
            ++seconds;
        }
        EXPECT_EQ(action.mKind, DirectorAction::Kind::Start);
        EXPECT_NEAR(seconds, 8, 1);
    }
}
