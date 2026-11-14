#include <components/falloutscript/vatsvalue.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
    namespace VV = FalloutScript::VatsValue;

    VV::Action attack()
    {
        VV::Action action;
        action.mKind = 3;
        action.mHasTarget = true;
        action.mTargetBase = 0x100;
        action.mPart = 25;
        action.mWeapon = 0x200;
        action.mWeaponKind = 5;
        action.mAmmo = 0x300;
        action.mDistance = 321.5;
        return action;
    }

    const std::set<std::pair<std::uint32_t, std::uint32_t>> lists{ { 0x1000, 0x200 }, { 0x1001, 0x100 }, { 0x1002, 0x300 } };

    double ask(int mode, const VV::Action* action, int index, std::uint32_t form = 0)
    {
        return VV::value(mode, action, index, form,
            [](std::uint32_t list, std::uint32_t item) { return lists.count({ list, item }) != 0; });
    }

    TEST(FalloutScriptVatsValueTest, everythingIsZeroWhereThereIsNoAction)
    {
        const VV::Action action = attack();
        for (int index = 0; index < 18; ++index)
        {
            EXPECT_EQ(ask(VV::None, &action, index, 0x200), 0.0) << index;
            EXPECT_EQ(ask(VV::Playback, nullptr, index, 0x200), 0.0) << index;
        }
        EXPECT_FALSE(VV::readsAction(0));
        EXPECT_FALSE(VV::readsAction(1));
        EXPECT_TRUE(VV::readsAction(2));
        EXPECT_TRUE(VV::readsAction(3));
        EXPECT_TRUE(VV::readsAction(4));
        EXPECT_FALSE(VV::readsAction(5));
    }

    TEST(FalloutScriptVatsValueTest, anIndexOutsideTheTableIsZero)
    {
        const VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, -1, 1), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 18, 1), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 100, 1), 0.0);
    }

    TEST(FalloutScriptVatsValueTest, theWeapon)
    {
        const VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, 0, 0x200), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 0, 0x201), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 0, 0), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 1, 0x1000), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 1, 0x1001), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 1, 0), 0.0);
        VV::Action bare = action;
        bare.mWeapon = 0;
        EXPECT_EQ(ask(VV::Playback, &bare, 0, 0x200), 0.0);
        EXPECT_EQ(ask(VV::Playback, &bare, 1, 0x1000), 0.0);
    }

    TEST(FalloutScriptVatsValueTest, theKindOfTheWeaponIsComparedOrZeroForNoWeapon)
    {
        VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, 15, 5), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 15, 4), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 15, 0), 0.0);
        action.mWeapon = 0;
        EXPECT_EQ(ask(VV::Playback, &action, 15, 0), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 15, 5), 0.0);
    }

    TEST(FalloutScriptVatsValueTest, theTarget)
    {
        VV::Action action = attack();
        EXPECT_EQ(ask(VV::Selecting, &action, 2, 0x100), 1.0);
        EXPECT_EQ(ask(VV::Selecting, &action, 2, 0x101), 0.0);
        EXPECT_EQ(ask(VV::Selecting, &action, 3, 0x1001), 1.0);
        EXPECT_EQ(ask(VV::Selecting, &action, 3, 0x1000), 0.0);
        EXPECT_EQ(ask(VV::Selecting, &action, 4), 321.5);
        action.mHasTarget = false;
        EXPECT_EQ(ask(VV::Selecting, &action, 2, 0x100), 0.0);
        EXPECT_EQ(ask(VV::Selecting, &action, 3, 0x1001), 0.0);
        EXPECT_EQ(ask(VV::Selecting, &action, 4), 0.0);
        action.mHasTarget = true;
        action.mTargetBase = 0;
        EXPECT_EQ(ask(VV::Selecting, &action, 2, 0), 0.0);
    }

    TEST(FalloutScriptVatsValueTest, thePartAndTheKind)
    {
        VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, 5, 25), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 5, 26), 0.0);
        action.mPart = -1;
        EXPECT_EQ(ask(VV::Playback, &action, 5, 0xffffffffu), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 6, 3), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 6, 18), 0.0);
        action.mKind = 0x12;
        EXPECT_EQ(ask(VV::Playback, &action, 6, 18), 1.0);
    }

    TEST(FalloutScriptVatsValueTest, theHitAndItsFlags)
    {
        VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, 7), 1.0);
        action.mHits = false;
        EXPECT_EQ(ask(VV::Playback, &action, 7), 0.0);
        const std::vector<std::pair<int, std::uint32_t>> flags{ { 8, 0x04u }, { 11, 0x10u }, { 13, 0x20u }, { 12, 0x40u }, { 14, 0x80u } };
        for (const auto& [index, bit] : flags)
        {
            action.mFlags = 0;
            EXPECT_EQ(ask(VV::Playback, &action, index), 0.0) << index;
            action.mFlags = bit;
            EXPECT_EQ(ask(VV::Playback, &action, index), 1.0) << index;
            // only its own bit counts
            action.mFlags = 0x1000u | (bit == 0x04u ? 0x80u : 0x04u);
            EXPECT_EQ(ask(VV::Playback, &action, index), 0.0) << index;
        }
    }

    TEST(FalloutScriptVatsValueTest, theAmmunition)
    {
        VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, 9, 0x300), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 9, 0x301), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 10, 0x1002), 1.0);
        EXPECT_EQ(ask(VV::Playback, &action, 10, 0x1000), 0.0);
        action.mAmmo = 0;
        EXPECT_EQ(ask(VV::Playback, &action, 9, 0x300), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 10, 0x1002), 0.0);
    }

    TEST(FalloutScriptVatsValueTest, theBytes)
    {
        VV::Action action = attack();
        EXPECT_EQ(ask(VV::Playback, &action, 16), 0.0);
        EXPECT_EQ(ask(VV::Playback, &action, 17), 0.0);
        action.mFlag6 = true;
        EXPECT_EQ(ask(VV::Playback, &action, 16), 1.0);
        action.mFlag6 = false;
        action.mFlag24 = true;
        EXPECT_EQ(ask(VV::Playback, &action, 16), 1.0);
        action.mFlag7 = true;
        EXPECT_EQ(ask(VV::Playback, &action, 17), 1.0);
    }

    TEST(FalloutScriptVatsValueTest, aReferenceCanBeMadeTargetableOrNot)
    {
        // The reference is marked when what the script asks is not what the record says, and the mark turns the record
        // round
        EXPECT_TRUE(VV::targetMark(true, false));
        EXPECT_FALSE(VV::targetMark(true, true));
        EXPECT_TRUE(VV::targetMark(false, true));
        EXPECT_FALSE(VV::targetMark(false, false));
        EXPECT_FALSE(VV::targetable(false, false));
        EXPECT_TRUE(VV::targetable(false, true));
        EXPECT_TRUE(VV::targetable(true, false));
        EXPECT_FALSE(VV::targetable(true, true));
        for (bool record : { false, true })
            for (bool wanted : { false, true })
                EXPECT_EQ(VV::targetable(record, VV::targetMark(wanted, record)), wanted);
    }

    TEST(FalloutScriptVatsValueTest, theSidesAreQuartersOfTheCircle)
    {
        EXPECT_DOUBLE_EQ(VV::sideAngle(VV::Side::Front), 0.0);
        EXPECT_NEAR(VV::sideAngle(VV::Side::Right), 1.5707963, 1e-6);
        EXPECT_NEAR(VV::sideAngle(VV::Side::Back), 3.1415927, 1e-6);
        EXPECT_NEAR(VV::sideAngle(VV::Side::Left), 4.7123890, 1e-6);
    }

    TEST(FalloutScriptVatsValueTest, theFreeAreaIsTheDistanceToTheFirstThing)
    {
        EXPECT_DOUBLE_EQ(VV::freeArea(0.25), 2500.0);
        EXPECT_DOUBLE_EQ(VV::freeArea(0.0), 0.0);
        EXPECT_DOUBLE_EQ(VV::freeArea(-1.0), VV::nothingHit);
        EXPECT_GT(VV::nothingHit, 1e38);
    }

    TEST(FalloutScriptVatsValueTest, theVisibleDistanceStopsAtTheFirstBlockedStep)
    {
        // five steps of 128 (the files' settings); the target is seen from the first three
        const auto threeSteps = [](int step) { return step < 3; };
        EXPECT_DOUBLE_EQ(VV::visibleDistance(128.0, 5.0, threeSteps), 3 * 128.0);
        EXPECT_DOUBLE_EQ(VV::visibleDistance(128.0, 5.0, [](int) { return true; }), 5 * 128.0);
        EXPECT_DOUBLE_EQ(VV::visibleDistance(128.0, 5.0, [](int) { return false; }), 0.0);
        // a later step that is seen does not count once one was blocked
        EXPECT_DOUBLE_EQ(VV::visibleDistance(128.0, 5.0, [](int step) { return step != 1; }), 128.0);
        EXPECT_DOUBLE_EQ(VV::visibleDistance(128.0, 0.0, [](int) { return true; }), 0.0);
    }

    TEST(FalloutScriptVatsValueTest, theTargetHeightIsOnlyKnownInThePlaybackOfAnAttack)
    {
        EXPECT_DOUBLE_EQ(VV::targetHeight(VV::Playback, true, true, 150.0, 20.0), 130.0);
        EXPECT_DOUBLE_EQ(VV::targetHeight(VV::Selecting, true, true, 150.0, 20.0), 0.0);
        EXPECT_DOUBLE_EQ(VV::targetHeight(VV::None, true, true, 150.0, 20.0), 0.0);
        EXPECT_DOUBLE_EQ(VV::targetHeight(VV::Playback, false, true, 150.0, 20.0), 0.0);
        EXPECT_DOUBLE_EQ(VV::targetHeight(VV::Playback, true, false, 150.0, 20.0), 0.0);
        EXPECT_DOUBLE_EQ(VV::targetHeight(VV::Playback, true, true, 10.0, 20.0), -10.0);
    }

    TEST(FalloutScriptVatsValueTest, theKindsThatAreAttacks)
    {
        const std::set<std::uint32_t> attacks{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 0x10, 0x11, 0x13, 0x14, 0x15 };
        for (std::uint32_t kind = 0; kind < 0x20; ++kind)
            EXPECT_EQ(VV::isAttackKind(kind), attacks.count(kind) != 0) << kind;
    }
    TEST(FalloutScriptVatsValueTest, theMarksAreSetClearedAndSaved)
    {
        VV::TargetMarks marks;
        EXPECT_FALSE(marks.marked(0x123));
        marks.set(0x123, true);
        marks.set(0x124, true);
        marks.set(0x123, true);
        EXPECT_TRUE(marks.marked(0x123));
        EXPECT_EQ(marks.all().size(), 2u);
        marks.set(0x124, false);
        marks.set(0x999, false);
        EXPECT_EQ(marks.all().size(), 1u);
        FalloutScript::SaveWriter out;
        VV::writeTargetMarks(marks, out);
        const std::string bytes = out.take();
        FalloutScript::SaveReader in(bytes);
        VV::TargetMarks back;
        back.set(0x77, true);
        VV::readTargetMarks(back, in, VV::targetMarksSectionVersion);
        EXPECT_TRUE(back.marked(0x123));
        EXPECT_FALSE(back.marked(0x77));
        FalloutScript::SaveReader wrong(bytes);
        EXPECT_THROW(VV::readTargetMarks(back, wrong, 7), FalloutScript::SaveError);
        marks.clear();
        EXPECT_TRUE(marks.all().empty());
    }

    TEST(FalloutScriptVatsValueTest, theThreatIsLevelTimesHealthOverWhatTheArmorLetsThrough)
    {
        EXPECT_DOUBLE_EQ(VV::threat(10, 100, 0), 1000.0);
        EXPECT_DOUBLE_EQ(VV::threat(10, 100, 15), 2000.0);
        EXPECT_NEAR(VV::threat(1, 100, 30), 100.0 / 0.01, 1e-6);
        EXPECT_NEAR(VV::threat(1, 100, 300), 100.0 / 0.01, 1e-6);
        EXPECT_DOUBLE_EQ(VV::threat(1, 100, -5), 100.0);
        EXPECT_EQ(VV::threat(0, 100, 0), 0.0);
        EXPECT_EQ(VV::threat(5, 0, 0), 0.0);
        EXPECT_EQ(VV::threat(5, -3, 0), 0.0);
        EXPECT_DOUBLE_EQ(VV::threatRatio(500, 1000), 2.0);
        EXPECT_EQ(VV::threatRatio(0, 1000), 0.0);
    }
}
