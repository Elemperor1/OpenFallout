#include <algorithm>

#include <components/falloutscript/lockpick.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace testing;
    using namespace FalloutScript::Lockpick;

    TEST(FalloutScriptLockpick, letsACharacterTryALockOnlyAtTheSkillOfItsClass)
    {
        EXPECT_TRUE(mayTry(0, 0));
        EXPECT_FALSE(mayTry(24, 1));
        EXPECT_TRUE(mayTry(25, 1));
        EXPECT_FALSE(mayTry(99, 4));
        EXPECT_TRUE(mayTry(100, 4));
        // A lock that only its key opens is beyond the skill scale
        EXPECT_FALSE(mayTry(100, 5));
    }

    TEST(FalloutScriptLockpick, sizesTheSweetSpotByTheClassAndNotBySkill)
    {
        const double width = 1200;
        EXPECT_EQ(sweetSpot(0, width, 0.5).mLength, 400);
        EXPECT_EQ(sweetSpot(1, width, 0.5).mLength, 200);
        EXPECT_EQ(sweetSpot(2, width, 0.5).mLength, 100);
        EXPECT_EQ(sweetSpot(3, width, 0.5).mLength, 50);
        EXPECT_EQ(sweetSpot(4, width, 0.5).mLength, 25);
        EXPECT_EQ(sweetSpot(4, width, 0.5).mConcentric, 200);
    }

    TEST(FalloutScriptLockpick, keepsTheWholeSweetSpotOnTheMeter)
    {
        const double width = 1200;
        const SweetSpot left = sweetSpot(2, width, 0.0);
        EXPECT_DOUBLE_EQ(left.mCenter, left.outer());
        const SweetSpot right = sweetSpot(2, width, 1.0);
        EXPECT_DOUBLE_EQ(right.mCenter, width - right.outer());
        EXPECT_DOUBLE_EQ(sweetSpot(2, width, 0.5).mCenter, 600);
    }

    TEST(FalloutScriptLockpick, putsTheSpotInTheMiddleOfAMeterThatIsTooNarrowForIt)
    {
        EXPECT_DOUBLE_EQ(sweetSpot(0, 700, 0.1).mCenter, 350);
    }

    TEST(FalloutScriptLockpick, turnsTheCylinderNinetyDegreesOnlyInTheInnermostBand)
    {
        const SweetSpot spot = sweetSpot(2, 1200, 0.5);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter, spot, 1200), 90);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + 50, spot, 1200), 90);
        // Just beyond the first band (half the sweet spot) is the second
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + 50.5, spot, 1200), 75);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter - spot.halfWidth(2) + 1, spot, 1200), 60);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + spot.halfWidth(3) - 1, spot, 1200), 45);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + spot.halfWidth(4) - 1, spot, 1200), 30);
        EXPECT_DOUBLE_EQ(maxTurn(spot.mCenter + spot.outer(), spot, 1200), 15);
    }

    TEST(FalloutScriptLockpick, fallsFromFifteenToNothingBeyondTheOutermostBand)
    {
        const double width = 1200;
        const SweetSpot spot = sweetSpot(2, width, 0.5);
        // The spot is in the middle: the far edge of the meter is 600 from it, the outermost band ends at 250
        EXPECT_NEAR(maxTurn(spot.mCenter + spot.outer() + 0.001, spot, width), 15, 1e-3);
        const double middle = maxTurn(spot.mCenter + (spot.outer() + width / 2) / 2, spot, width);
        EXPECT_GT(middle, 0);
        EXPECT_LT(middle, 15);
        // It is nothing only at the end of the meter that is furthest from a spot at the other end
        const SweetSpot atTheEdge = sweetSpot(2, width, 0.0);
        EXPECT_NEAR(maxTurn(width, atTheEdge, width), 0, 1e-9);
        EXPECT_NEAR(maxTurn(width, spot, width), 7.5, 1e-9);
    }

    TEST(FalloutScriptLockpick, holdsAPickLongerForALongerSkill)
    {
        EXPECT_DOUBLE_EQ(pickHealth(0), 1.0);
        EXPECT_DOUBLE_EQ(pickHealth(15), 1.15);
        EXPECT_DOUBLE_EQ(pickHealth(100), 2.0);
    }

    TEST(FalloutScriptLockpick, roundsTheChanceToForceIntoTheScale)
    {
        EXPECT_DOUBLE_EQ(forceChance(30, 2), 0); // 30 - 50 + 10 is below nothing
        EXPECT_DOUBLE_EQ(forceChance(50, 2), 10);
        EXPECT_DOUBLE_EQ(forceChance(60, 2), 20);
        EXPECT_DOUBLE_EQ(forceChance(100, 0), 100);
        EXPECT_DOUBLE_EQ(forceChance(15, 0), 25);
    }

    // ---- The game ----

    struct LockpickGameTest : Test
    {
        static Game::Setup setup(int lockClass = 2, double skill = 15, int picks = 3)
        {
            Game::Setup result;
            result.mLockClass = lockClass;
            result.mSkill = skill;
            result.mPicks = picks;
            result.mMeterWidth = 1200;
            return result;
        }

        static bool has(const std::vector<Event>& events, Event event)
        {
            return std::find(events.begin(), events.end(), event) != events.end();
        }

        // The sweet spot is in the middle of the meter, as the first number from `random` is a half
        static Game make(const Game::Setup& setup)
        {
            return Game(setup, [] { return 0.5; });
        }

        // Steps for some seconds, with the same input; every event of the steps
        static std::vector<Event> run(Game& game, double seconds, const Game::Input& input)
        {
            std::vector<Event> all;
            const double step = 1.0 / 60;
            for (double t = 0; t < seconds - 1e-9; t += step)
            {
                Game::Input in = input;
                in.mSeconds = step;
                const auto events = game.step(in);
                all.insert(all.end(), events.begin(), events.end());
            }
            return all;
        }

        static Game ready(const Game::Setup& s)
        {
            Game game = make(s);
            run(game, Game::lockForward + 0.05, {});
            return game;
        }
    };

    TEST_F(LockpickGameTest, flysTheLockAndThePinInAndSaysSoOnTheFirstFrame)
    {
        Game game = make(setup());
        EXPECT_EQ(game.state(), Game::State::Intro);
        EXPECT_EQ(game.view().mLockSequence, "Forward");
        EXPECT_TRUE(has(run(game, 1.0 / 60, {}), Event::Enter));
        EXPECT_FALSE(has(run(game, 1.0, {}), Event::Enter));
        EXPECT_EQ(game.state(), Game::State::Intro);
        EXPECT_FALSE(has(run(game, 0.5, {}), Event::Enter));
        EXPECT_EQ(game.state(), Game::State::Idle);
    }

    TEST_F(LockpickGameTest, saysThereAreNoPicksInsteadOfTheEnterSoundWhenThereAreNone)
    {
        Game game = make(setup(2, 15, 0));
        const auto first = run(game, 1.0 / 60, {});
        EXPECT_TRUE(has(first, Event::OutOfPicks));
        EXPECT_FALSE(has(first, Event::Enter));
        const auto rest = run(game, 1.5, {});
        EXPECT_FALSE(has(rest, Event::OutOfPicks));
        EXPECT_FALSE(has(rest, Event::Enter));
    }

    TEST_F(LockpickGameTest, doesNotTurnTheCylinderWithoutAPick)
    {
        Game game = make(setup(2, 15, 0));
        run(game, 1.5, {});
        Game::Input turn;
        turn.mTurn = 1;
        run(game, 1.0, turn);
        EXPECT_EQ(game.state(), Game::State::Idle);
        EXPECT_EQ(game.view().mAngle, 0);
    }

    TEST_F(LockpickGameTest, opensTheLockWhenThePickIsInTheInnermostBand)
    {
        Game game = ready(setup());
        EXPECT_EQ(game.limit(), 90);
        Game::Input turn;
        turn.mTurn = 1;
        const auto events = run(game, 1.2, turn);
        EXPECT_TRUE(has(events, Event::Turn));
        EXPECT_TRUE(has(events, Event::SqueakA));
        EXPECT_TRUE(has(events, Event::SqueakB));
        EXPECT_TRUE(has(events, Event::Unlock));
        EXPECT_EQ(game.state(), Game::State::Opened);
    }

    TEST_F(LockpickGameTest, bindsAtTheLimitOfAPickThatIsOffTheSpotAndBreaksItAfterItsHealth)
    {
        Game game = ready(setup(2, 15, 2));
        // Move the pick 100 units from the middle: the third band (half-widths 50, 90, 130 ...), a turn of 60 degrees
        Game::Input move;
        move.mMove = 100;
        run(game, 1.0 / 60, move);
        const double limit = game.limit();
        EXPECT_LT(limit, 90);
        EXPECT_GT(limit, 0);

        Game::Input turn;
        turn.mTurn = 1;
        auto events = run(game, 1.0, turn);
        EXPECT_TRUE(has(events, Event::Stop));
        EXPECT_TRUE(has(events, Event::TensionStart));
        EXPECT_EQ(game.state(), Game::State::Binding);
        EXPECT_NEAR(game.view().mAngle, limit, 1e-9);
        EXPECT_FALSE(has(events, Event::Break));
        // The pick has 1.15 of health and loses one in a second of strain: the strain began before the end of that
        // second of turning, so a second more is more than it holds
        events = run(game, 1.2, turn);
        EXPECT_TRUE(has(events, Event::Break));
        EXPECT_TRUE(has(events, Event::TensionEnd));
        EXPECT_EQ(game.picks(), 1);
        EXPECT_EQ(game.state(), Game::State::Snapping);
        EXPECT_EQ(game.view().mPinSequence, "Left");
    }

    TEST_F(LockpickGameTest, takesANewPickAfterABreakWithTheSpotWhereItWas)
    {
        Game game = ready(setup(2, 0, 2));
        const double center = game.sweetSpot().mCenter;
        Game::Input move;
        move.mMove = 200;
        run(game, 1.0 / 60, move);
        Game::Input turn;
        turn.mTurn = 1;
        run(game, 2.0, turn);
        ASSERT_EQ(game.state(), Game::State::Snapping);
        run(game, Game::resetSeconds + 0.1, {});
        EXPECT_EQ(game.state(), Game::State::Waiting);
        EXPECT_EQ(game.view().mAngle, 0);
        EXPECT_DOUBLE_EQ(game.view().mPick, 600);
        EXPECT_EQ(game.sweetSpot().mCenter, center);
        EXPECT_EQ(game.view().mPinSequence, "Forward");
        run(game, Game::pinForward + 0.1, {});
        EXPECT_EQ(game.state(), Game::State::Idle);
        EXPECT_NEAR(game.view().mHealth, 1.0, 1e-9);
    }

    TEST_F(LockpickGameTest, saysThePicksAreGoneWhenTheLastOneBreaks)
    {
        Game game = ready(setup(2, 0, 1));
        Game::Input move;
        move.mMove = 200;
        run(game, 1.0 / 60, move);
        Game::Input turn;
        turn.mTurn = 1;
        run(game, 2.0, turn);
        EXPECT_EQ(game.picks(), 0);
        EXPECT_TRUE(has(run(game, Game::resetSeconds + 0.1, {}), Event::OutOfPicks));
        run(game, Game::pinForward + 0.1, {});
        run(game, 1.0, turn);
        EXPECT_EQ(game.state(), Game::State::Idle);
    }

    TEST_F(LockpickGameTest, lowersTheCylinderAgainWhenTheTurnIsLetGo)
    {
        Game game = ready(setup());
        Game::Input move;
        move.mMove = 150;
        run(game, 1.0 / 60, move);
        Game::Input turn;
        turn.mTurn = 1;
        run(game, 0.3, turn);
        const double turned = game.view().mAngle;
        EXPECT_GT(turned, 20);
        EXPECT_EQ(game.state(), Game::State::Turning);
        run(game, 0.1, {});
        EXPECT_EQ(game.state(), Game::State::Idle);
        EXPECT_LT(game.view().mAngle, turned);
        run(game, 1.1, {});
        EXPECT_EQ(game.view().mAngle, 0);
    }

    TEST_F(LockpickGameTest, forcesALockWithTheChanceOfTheSkill)
    {
        // Skill 60 against an average lock (gate 50) is 20 per cent
        Game game = ready(setup(2, 60, 1));
        const auto won = game.force(19.9);
        EXPECT_TRUE(has(won, Event::Turn));
        EXPECT_TRUE(game.forcing());
        EXPECT_TRUE(has(run(game, 1.2, {}), Event::Unlock));
        EXPECT_EQ(game.picks(), 1);

        Game lost = ready(setup(2, 60, 1));
        EXPECT_TRUE(has(lost.force(20.0), Event::ForceFailed));
        EXPECT_EQ(lost.state(), Game::State::Failed);
        EXPECT_EQ(lost.picks(), 1);
    }

    TEST_F(LockpickGameTest, aForceThatWonIsNotReversedByAnotherRequestWhileTheCylinderTurns)
    {
        Game game = ready(setup(2, 60, 1));
        EXPECT_TRUE(has(game.force(0.0), Event::Turn));
        EXPECT_TRUE(game.forcing());
        // Pressing the key again, or the button, while the cylinder turns by itself
        EXPECT_TRUE(game.force(99.0).empty());
        EXPECT_EQ(game.state(), Game::State::Turning);
        EXPECT_TRUE(has(run(game, 1.2, {}), Event::Unlock));
        EXPECT_NE(game.state(), Game::State::Failed);
    }

    TEST_F(LockpickGameTest, putsThePinStraightUpAtTheMiddleAndPointsItToTheSidesAtTheEnds)
    {
        Game game = ready(setup());
        EXPECT_NEAR(game.view().mPickAngle, 0, 1e-9);
        Game::Input left;
        left.mMove = -5000;
        run(game, 1.0 / 60, left);
        EXPECT_NEAR(game.view().mPickAngle, 90, 1e-9);
        Game::Input right;
        right.mMove = 5000;
        run(game, 1.0 / 60, right);
        EXPECT_NEAR(game.view().mPickAngle, -90, 1e-9);
    }

    TEST_F(LockpickGameTest, setsTheLockToTheTimeOfItsSequenceFromTheTurn)
    {
        Game game = ready(setup());
        Game::Input turn;
        turn.mTurn = 1;
        run(game, 0.5, turn);
        EXPECT_NEAR(game.view().mAngle, 45, 1.0);
        EXPECT_EQ(game.view().mLockSequence, "Backward");
        EXPECT_NEAR(game.view().mLockTime, Game::lockBackward * game.view().mAngle / 90, 1e-9);
    }
}
