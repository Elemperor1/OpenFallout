#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include <components/falloutscript/specialscreen.hpp>

namespace
{
    using namespace FalloutScript::SpecialScreen;

    constexpr std::array<int, 7> fives{ 5, 5, 5, 5, 5, 5, 5 };

    const ShapeState* shapeOf(const std::vector<ShapeState>& shapes, const std::string& name)
    {
        for (const ShapeState& shape : shapes)
            if (shape.mShape == name)
                return &shape;
        return nullptr;
    }

    /// Plays the clip of the last turn to its end
    void settle(Screen& screen)
    {
        screen.update(5.f);
    }

    TEST(FalloutScriptSpecialScreenTest, testerOpensOnTheFirstAttributeWithTheClipPlaying)
    {
        Screen screen(Kind::Tester, 40, fives);
        EXPECT_EQ(screen.page(), 1);
        EXPECT_EQ(screen.sequence(), "Forward");
        EXPECT_EQ(screen.opening(), Sound::PageTurn);
        EXPECT_FALSE(screen.ready());
        screen.update(1.0f);
        EXPECT_FALSE(screen.ready());
        EXPECT_FLOAT_EQ(screen.sequenceTime(), 1.0f);
        screen.update(0.4f);
        EXPECT_TRUE(screen.ready());
        EXPECT_FLOAT_EQ(screen.sequenceTime(), 1.3f);
    }

    TEST(FalloutScriptSpecialScreenTest, bookOpensOnItsCoverAndIsReadyAtOnce)
    {
        Screen screen(Kind::Book, 40, fives);
        EXPECT_EQ(screen.page(), 0);
        EXPECT_EQ(screen.sequence(), "Forward");
        EXPECT_EQ(screen.opening(), Sound::None);
        EXPECT_TRUE(screen.ready());
    }

    TEST(FalloutScriptSpecialScreenTest, aPageTurnsOnlyWhenTheClipIsDoneAndPlaysTheClipOfThePageItGoesTo)
    {
        Screen screen(Kind::Tester, 40, fives);
        EXPECT_EQ(screen.next().mSound, Sound::None);
        EXPECT_EQ(screen.page(), 1);

        settle(screen);
        Outcome outcome = screen.next();
        EXPECT_EQ(outcome.mSound, Sound::PageTurn);
        EXPECT_EQ(screen.page(), 2);
        EXPECT_EQ(screen.sequence(), "Backward");
        EXPECT_FLOAT_EQ(screen.sequenceTime(), 0.f);
        EXPECT_FALSE(screen.ready());

        settle(screen);
        EXPECT_EQ(screen.previous().mSound, Sound::PageTurn);
        EXPECT_EQ(screen.page(), 1);
        EXPECT_EQ(screen.sequence(), "Aim");
        settle(screen);
        screen.previous();
        EXPECT_EQ(screen.page(), 0);
        EXPECT_EQ(screen.sequence(), "AimUp");
        // The cover is ready whatever the clip, and there is no page before it
        EXPECT_TRUE(screen.ready());
        EXPECT_EQ(screen.previous().mSound, Sound::None);
        EXPECT_EQ(screen.page(), 0);
    }

    TEST(FalloutScriptSpecialScreenTest, theTablesOfTheBookEndWithTheNinthPage)
    {
        Screen screen(Kind::Book, 40, fives);
        for (int page = 1; page <= 9; ++page)
        {
            settle(screen);
            ASSERT_EQ(screen.next().mSound, Sound::PageTurn) << page;
            EXPECT_EQ(screen.page(), page);
        }
        EXPECT_EQ(screen.sequence(), "TurnLeft");
        settle(screen);
        EXPECT_EQ(screen.next().mSound, Sound::None);
        EXPECT_EQ(screen.page(), 9);
        screen.previous();
        EXPECT_EQ(screen.sequence(), "TurnRight");
        EXPECT_EQ(screen.page(), 8);
    }

    TEST(FalloutScriptSpecialScreenTest, theRightTriggerOnTheLastPageOfTheTesterIsTheExit)
    {
        Screen screen(Kind::Tester, 35, fives);
        for (int page = 2; page <= 8; ++page)
        {
            settle(screen);
            screen.next();
        }
        EXPECT_EQ(screen.page(), 8);
        settle(screen);
        // 35 points of 35: it leaves
        const Outcome outcome = screen.next();
        EXPECT_TRUE(outcome.mClose);
        EXPECT_EQ(outcome.mSound, Sound::Finished);
    }

    TEST(FalloutScriptSpecialScreenTest, aRiseOfTheTesterSoundsBeforeThePointsAreLookedAt)
    {
        Screen tester(Kind::Tester, 40, fives);
        Outcome outcome = tester.increase();
        EXPECT_EQ(outcome.mAttribute, 0);
        EXPECT_EQ(outcome.mValue, 6);
        EXPECT_EQ(outcome.mSound, Sound::Number);
        EXPECT_EQ(tester.total(), 36);
        for (int i = 0; i < 4; ++i)
            tester.increase();
        EXPECT_EQ(tester.total(), 40);
        EXPECT_EQ(tester.value(0), 10);

        // All 40 given: a button still sounds on an attribute below the top (the Tester only), a key does nothing
        settle(tester);
        tester.next();
        outcome = tester.increase();
        EXPECT_EQ(outcome.mAttribute, -1);
        EXPECT_EQ(outcome.mSound, Sound::Number);
        EXPECT_EQ(tester.increase(true).mSound, Sound::None);
        EXPECT_EQ(tester.total(), 40);

        Screen book(Kind::Book, 40, fives);
        settle(book);
        book.next();
        for (int i = 0; i < 5; ++i)
            book.increase();
        EXPECT_EQ(book.total(), 40);
        EXPECT_EQ(book.increase().mSound, Sound::None);
    }

    TEST(FalloutScriptSpecialScreenTest, anAttributeStaysBetweenOneAndTen)
    {
        Screen screen(Kind::Tester, 100, { 10, 1, 5, 5, 5, 5, 5 });
        EXPECT_EQ(screen.increase().mSound, Sound::None);
        EXPECT_EQ(screen.value(0), 10);
        EXPECT_EQ(screen.decrease().mSound, Sound::NumberDown);
        EXPECT_EQ(screen.value(0), 9);
        settle(screen);
        screen.next();
        EXPECT_EQ(screen.decrease().mSound, Sound::None);
        EXPECT_EQ(screen.value(1), 1);
    }

    TEST(FalloutScriptSpecialScreenTest, theBookSoundsOnceForEitherWay)
    {
        Screen screen(Kind::Book, 40, fives);
        settle(screen);
        screen.next();
        EXPECT_EQ(screen.decrease().mSound, Sound::Number);
        EXPECT_EQ(screen.increase().mSound, Sound::Number);
    }

    TEST(FalloutScriptSpecialScreenTest, theExitNeedsAllThePoints)
    {
        Screen tester(Kind::Tester, 40, fives);
        Outcome outcome = tester.exitAttempt();
        EXPECT_FALSE(outcome.mClose);
        EXPECT_EQ(outcome.mSound, Sound::Nothing);
        for (int i = 0; i < 5; ++i)
            tester.increase();
        outcome = tester.exitAttempt();
        EXPECT_TRUE(outcome.mClose);
        EXPECT_EQ(outcome.mSound, Sound::Finished);

        Screen book(Kind::Book, 35, fives);
        outcome = book.exitAttempt();
        EXPECT_TRUE(outcome.mClose);
        EXPECT_EQ(outcome.mSound, Sound::None);
    }

    TEST(FalloutScriptSpecialScreenTest, theIndexHasNoAttributeWithoutAPadAndTheCursorWithOne)
    {
        Screen screen(Kind::Tester, 40, fives);
        for (int page = 2; page <= 8; ++page)
        {
            settle(screen);
            screen.next();
        }
        ASSERT_EQ(screen.page(), 8);
        EXPECT_EQ(screen.increase().mAttribute, -1);
        EXPECT_EQ(screen.moveCursor(1).mSound, Sound::None);

        Screen pad(Kind::Tester, 40, fives, true);
        for (int page = 2; page <= 8; ++page)
        {
            settle(pad);
            pad.next();
        }
        EXPECT_EQ(pad.moveCursor(1).mSound, Sound::Focus);
        EXPECT_EQ(pad.cursor(), 1);
        EXPECT_EQ(pad.increase().mAttribute, 1);
        EXPECT_EQ(pad.moveCursor(-1).mSound, Sound::Focus);
        EXPECT_EQ(pad.moveCursor(-1).mSound, Sound::None);
    }

    TEST(FalloutScriptSpecialScreenTest, thePointsLeftAreShownInTwoDigits)
    {
        Screen screen(Kind::Tester, 40, fives);
        auto shapes = screen.shapes();
        EXPECT_EQ(shapeOf(shapes, "P1_PointsRemainDigit1:0")->mTexture, "textures/terminals/bbnumber0.dds");
        EXPECT_EQ(shapeOf(shapes, "P1_PointsRemainDigit2:0")->mTexture, "textures/terminals/bbnumber5.dds");

        Screen many(Kind::Tester, 135, fives);
        shapes = many.shapes();
        EXPECT_EQ(shapeOf(shapes, "P1_PointsRemainDigit1:0")->mTexture, "textures/terminals/bbnumber9.dds");
        EXPECT_EQ(shapeOf(shapes, "P1_PointsRemainDigit2:0")->mTexture, "textures/terminals/bbnumber0.dds");

        Screen over(Kind::Tester, 30, fives);
        shapes = over.shapes();
        EXPECT_EQ(shapeOf(shapes, "P1_PointsRemainDigit1:0")->mTexture, "textures/terminals/bbnumber0.dds");
        EXPECT_EQ(shapeOf(shapes, "P1_PointsRemainDigit2:0")->mTexture, "textures/terminals/bbnumber0.dds");
    }

    TEST(FalloutScriptSpecialScreenTest, theButtonsShowOnlyWhatTheyCanDo)
    {
        Screen screen(Kind::Tester, 36, { 1, 10, 5, 5, 5, 5, 5 });
        auto shapes = screen.shapes();
        // Strength is at the bottom, and 36 of 36 are given
        EXPECT_FALSE(shapeOf(shapes, "P1_Decrease_Btn:0")->mVisible);
        EXPECT_FALSE(shapeOf(shapes, "P1_Increase_Btn:0")->mVisible);
        EXPECT_TRUE(shapeOf(shapes, "Index_PerceptionDecrease_Btn:0")->mVisible);
        EXPECT_FALSE(shapeOf(shapes, "Index_PerceptionIncrease_Btn:0")->mVisible);
        EXPECT_EQ(shapeOf(shapes, "Index_PerceptionPointVal:0")->mTexture, "textures/terminals/bbnumber10.dds");

        Screen room(Kind::Tester, 40, fives);
        shapes = room.shapes();
        EXPECT_TRUE(shapeOf(shapes, "P1_Decrease_Btn:0")->mVisible);
        EXPECT_TRUE(shapeOf(shapes, "P1_Increase_Btn:0")->mVisible);
    }

    TEST(FalloutScriptSpecialScreenTest, theLampsOfTheCabinetGlowUpToTheValue)
    {
        Screen screen(Kind::Tester, 40, { 5, 5, 6, 5, 5, 5, 5 });
        settle(screen);
        screen.next();
        settle(screen);
        screen.next();
        ASSERT_EQ(screen.page(), 3);
        const auto shapes = screen.shapes();
        EXPECT_TRUE(shapeOf(shapes, "P1_PointVal_06_GLOW:4")->mVisible);
        EXPECT_TRUE(shapeOf(shapes, "P1_PointVal_06_GLOW:5")->mVisible);
        EXPECT_FALSE(shapeOf(shapes, "P1_PointVal_07_GLOW:4")->mVisible);
        EXPECT_FALSE(shapeOf(shapes, "P1_PointVal_10_GLOW:5")->mVisible);
        EXPECT_TRUE(shapeOf(shapes, "P1_PointVal_01_GLOW:4")->mVisible);

        // None on the cover
        Screen cover(Kind::Book, 40, fives);
        EXPECT_EQ(shapeOf(cover.shapes(), "P1_PointVal_01_GLOW:4"), nullptr);
    }

    TEST(FalloutScriptSpecialScreenTest, theBookHasAPanelForEachAttributeAndAMessageByThePointsLeft)
    {
        Screen screen(Kind::Book, 40, fives);
        auto shapes = screen.shapes();
        EXPECT_EQ(shapeOf(shapes, "P3_PointVal:0")->mTexture, "textures/terminals/bbnumber5.dds");
        EXPECT_EQ(shapeOf(shapes, "P3_Message:0")->mTexture, "textures/terminals/english/bbmessage03.dds");
        settle(screen);
        screen.next();
        screen.increase();
        screen.increase();
        screen.increase();
        screen.increase();
        shapes = screen.shapes();
        EXPECT_EQ(shapeOf(shapes, "P1_Message:0")->mTexture, "textures/terminals/english/bbmessage04.dds");
        screen.increase();
        shapes = screen.shapes();
        EXPECT_EQ(shapeOf(shapes, "Index_Message:0")->mTexture, "textures/terminals/english/bbmessage02.dds");
        EXPECT_EQ(shapeOf(shapes, "P1_PointVal:0")->mTexture, "textures/terminals/bbnumber10.dds");
    }

    TEST(FalloutScriptSpecialScreenTest, aShapeThePointerFindsDoesWhatItsButtonDoes)
    {
        Screen screen(Kind::Tester, 40, fives);
        settle(screen);
        Outcome outcome = screen.press("P1_Increase_Btn:0");
        EXPECT_EQ(outcome.mAttribute, 0);
        EXPECT_EQ(screen.value(0), 6);
        outcome = screen.press("p1_decrease_btn:0");
        EXPECT_EQ(screen.value(0), 5);
        // A row of the index acts on its own attribute whatever the page
        outcome = screen.press("Index_LuckIncrease_Btn:0");
        EXPECT_EQ(outcome.mAttribute, 6);
        EXPECT_EQ(screen.value(6), 6);
        outcome = screen.press("Index_AgilityDecrease_Btn:0");
        EXPECT_EQ(outcome.mAttribute, 5);
        EXPECT_EQ(screen.value(5), 4);

        EXPECT_EQ(screen.press("P1_RT_Btn:0").mSound, Sound::PageTurn);
        EXPECT_EQ(screen.page(), 2);
        settle(screen);
        EXPECT_EQ(screen.press("P1_LT_Btn:0").mSound, Sound::PageTurn);
        EXPECT_EQ(screen.page(), 1);
        EXPECT_EQ(screen.press("AllDone_Btn:0").mSound, Sound::Nothing);
        EXPECT_EQ(screen.press("Nothing:0").mSound, Sound::None);

        Screen book(Kind::Book, 40, fives);
        EXPECT_EQ(book.press("LookInside_Btn:0").mSound, Sound::PageTurn);
        EXPECT_EQ(book.page(), 1);
        EXPECT_EQ(book.press("P1_Increase_Btn:0").mAttribute, 0);
    }

    TEST(FalloutScriptSpecialScreenTest, theButtonsOfThePageAreTheOnesThePointerLooksFor)
    {
        Screen tester(Kind::Tester, 40, fives);
        auto buttons = tester.buttons();
        EXPECT_NE(std::find(buttons.begin(), buttons.end(), "P1_RT_Btn:0"), buttons.end());
        EXPECT_EQ(std::find(buttons.begin(), buttons.end(), "AllDone_Btn:0"), buttons.end());
        for (int page = 2; page <= 8; ++page)
        {
            settle(tester);
            tester.next();
        }
        buttons = tester.buttons();
        EXPECT_NE(std::find(buttons.begin(), buttons.end(), "AllDone_Btn:0"), buttons.end());
        EXPECT_NE(std::find(buttons.begin(), buttons.end(), "Index_LuckIncrease_Btn:0"), buttons.end());

        Screen book(Kind::Book, 40, fives);
        buttons = book.buttons();
        EXPECT_EQ(buttons, std::vector<std::string>{ "LookInside_Btn:0" });
        settle(book);
        book.next();
        settle(book);
        book.next();
        buttons = book.buttons();
        EXPECT_NE(std::find(buttons.begin(), buttons.end(), "P2_Increase_Btn:0"), buttons.end());
        EXPECT_EQ(std::find(buttons.begin(), buttons.end(), "P1_Increase_Btn:0"), buttons.end());
    }

    TEST(FalloutScriptSpecialScreenTest, theSoundsHaveTheNamesOfTheGamesRecords)
    {
        EXPECT_STREQ(soundName(Sound::PageTurn), "OBJBookSpecialPageTurn");
        EXPECT_STREQ(soundName(Sound::Finished), "OBJBookSpecialNumberFinished");
        EXPECT_STREQ(soundName(Sound::Nothing), "UIActivateNothing");
        EXPECT_STREQ(soundName(Sound::None), "");
    }
}
