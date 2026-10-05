#include <gtest/gtest.h>

#include <cmath>

#include "apps/openfallout/mwworld/timestamp.hpp"
#include "apps/openfallout/mwworld/weather.hpp"

namespace OFWorld
{
    namespace
    {
        TEST(OFWorldWeatherTest, moonPhasesHaveMwscriptCompatibleValues)
        {
            using Phase = OFRender::MoonState::Phase;

            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::New), 0);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::WaxingCrescent), 1);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::WaningCrescent), 1);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::FirstQuarter), 2);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::ThirdQuarter), 2);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::WaxingGibbous), 3);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::WaningGibbous), 3);
            EXPECT_EQ(OFRender::MoonState::phaseToInt(Phase::Full), 4);
        }

        // MASSER PHASES

        TEST(OFWorldWeatherTest, masserPhasesFullToWaningGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 2 and 26, 11:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 2 + 11.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 2 + 11.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 26 + 11.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 26 + 11.0f + 58.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(0));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(1));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(0));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(1));
        }

        TEST(OFWorldWeatherTest, masserPhasesWaningGibbousToThirdQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 5 and 29, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 5 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 29 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(1));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(2));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(1));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(2));
        }

        TEST(OFWorldWeatherTest, masserPhasesThirdQuarterToWaningCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 8 and 32, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 7 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 8 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 31 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 32 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(2));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(3));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(2));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(3));
        }

        TEST(OFWorldWeatherTest, masserPhasesWaningCrescentToNewAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 11 and 35, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 10 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 11 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 34 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 35 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(3));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(4));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(3));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(4));
        }

        TEST(OFWorldWeatherTest, masserPhasesNewToWaxingCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 14 and 38, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 13 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 14 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 37 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 38 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(4));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(5));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(4));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(5));
        }

        TEST(OFWorldWeatherTest, masserPhasesWaxingCrescentToFirstQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 17 and 41, 2:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 17 + 2.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 17 + 2.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 41 + 2.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 41 + 2.0f + 58.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(5));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(6));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(5));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(6));
        }

        TEST(OFWorldWeatherTest, masserPhasesFirstQuarterToWaxingGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 20 and 44, 5:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 20 + 5.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 20 + 5.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 44 + 5.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 44 + 5.0f + 58.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(6));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(7));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(6));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(7));
        }

        TEST(OFWorldWeatherTest, masserPhasesWaxingGibbousToFullAtCorrectTimes)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 35.0f;

            // Days 23 and 47, 8:57
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 23 + 8.0f + 56.0f / 60.0f);
            timeStampAfter += (24.0f * 23 + 8.0f + 58.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 47 + 8.0f + 56.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 47 + 8.0f + 58.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(7));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(0));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(7));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(0));
        }

        // SECUNDA PHASES

        TEST(OFWorldWeatherTest, secundaPhasesFullToWaningGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 2 and 26, 14:19
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 2 + 14.0f + 18.0f / 60.0f);
            timeStampAfter += (24.0f * 2 + 14.0f + 20.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 26 + 14.0f + 18.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 26 + 14.0f + 20.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(0));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(1));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(0));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(1));
        }

        TEST(OFWorldWeatherTest, secundaPhasesWaningGibbousToThirdQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 5 and 29, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 5 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 29 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(1));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(2));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(1));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(2));
        }

        TEST(OFWorldWeatherTest, secundaPhasesThirdQuarterToWaningCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 8 and 32, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 7 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 8 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 31 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 32 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(2));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(3));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(2));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(3));
        }

        TEST(OFWorldWeatherTest, secundaPhasesWaningCrescentToNewAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 11 and 35, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 10 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 11 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 34 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 35 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(3));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(4));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(3));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(4));
        }

        TEST(OFWorldWeatherTest, secundaPhasesNewToWaxingCrescentAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 14 and 38, 0:00
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 13 + 23.0f + 59.0f / 60.0f);
            timeStampAfter += (24.0f * 14 + 0.0f + 1.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 37 + 23.0f + 59.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 38 + 0.0f + 1.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(4));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(5));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(4));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(5));
        }

        TEST(OFWorldWeatherTest, secundaPhasesWaxingCrescentToFirstQuarterAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 17 and 41, 3:31
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 17 + 3.0f + 30.0f / 60.0f);
            timeStampAfter += (24.0f * 17 + 3.0f + 32.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 41 + 3.0f + 30.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 41 + 3.0f + 32.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(5));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(6));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(5));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(6));
        }

        TEST(OFWorldWeatherTest, secundaPhasesFirstQuarterToWaxingGibbousAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 20 and 44, 7:07
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 20 + 7.0f + 6.0f / 60.0f);
            timeStampAfter += (24.0f * 20 + 7.0f + 8.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 44 + 7.0f + 6.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 44 + 7.0f + 8.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(6));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(7));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(6));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(7));
        }

        TEST(OFWorldWeatherTest, secundaPhasesWaxingGibbousToFullAtCorrectTimes)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 23 and 47, 10:43
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 23 + 10.0f + 42.0f / 60.0f);
            timeStampAfter += (24.0f * 23 + 10.0f + 44.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 47 + 10.0f + 42.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 47 + 10.0f + 44.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_EQ(beforeState.mPhase, static_cast<OFRender::MoonState::Phase>(7));
            EXPECT_EQ(afterState.mPhase, static_cast<OFRender::MoonState::Phase>(0));
            EXPECT_EQ(beforeStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(7));
            EXPECT_EQ(afterStatePostLoop.mPhase, static_cast<OFRender::MoonState::Phase>(0));
        }

        // OFFSETS

        TEST(OFWorldWeatherTest, secundaShouldApplyIncrementOffsetAfterFirstLoop)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 14.0f;
            float fadeInFinish = 15.0f;
            float fadeOutStart = 7.0f;
            float fadeOutFinish = 10.0f;
            float axisOffset = 50.0f;

            // Days 8 and 32, 3:16
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 8 + 3.0f + 15.0f / 60.0f);
            timeStampAfter += (24.0f * 8 + 3.0f + 17.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 32 + 3.0f + 15.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 32 + 3.0f + 17.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(OFWorldWeatherTest, moonWithLowIncrementShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 0.9f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 35.0f;

            // Days 7 and 31, 1:44
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 7 + 1.0f + 43.0f / 60.0f);
            timeStampAfter += (24.0f * 7 + 1.0f + 45.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 31 + 1.0f + 43.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 31 + 1.0f + 45.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(OFWorldWeatherTest, masserShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 1.0f;
            float speed = 0.5f;
            float fadeEndAngle = 40.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 35.0f;

            // Days 4 and 28, 1:02
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 1.0f + 1.0f / 60.0f);
            timeStampAfter += (24.0f * 4 + 1.0f + 3.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 1.0f + 1.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 28 + 1.0f + 3.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(OFWorldWeatherTest, secundaShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 1.2f;
            float speed = 0.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 50.0f;

            // Days 3 and 27, 2:04
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 3 + 2.0f + 3.0f / 60.0f);
            timeStampAfter += (24.0f * 3 + 2.0f + 5.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 27 + 2.0f + 3.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 27 + 2.0f + 5.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }

        TEST(OFWorldWeatherTest, moonWithIncreasedSpeedShouldApplyIncrementOffsetAfterCycle)
        {
            float dailyIncrement = 1.2f;
            float speed = 1.6f;
            float fadeEndAngle = 30.0f;
            float fadeStartAngle = 50.0f;
            float moonShadowEarlyFadeAngle = 0.5f;
            float fadeInStart = 0.0f;
            float fadeInFinish = 0.0f;
            float fadeOutStart = 0.0f;
            float fadeOutFinish = 0.0f;
            float axisOffset = 50.0f;

            // Days 4 and 28, 1:13
            TimeStamp timeStampBefore, timeStampAfter, timeStampBeforePostLoop, timeStampAfterPostLoop;
            timeStampBefore += (24.0f * 4 + 1.0f + 12.0f / 60.0f);
            timeStampAfter += (24.0f * 4 + 1.0f + 14.0f / 60.0f);
            timeStampBeforePostLoop += (24.0f * 28 + 1.0f + 12.0f / 60.0f);
            timeStampAfterPostLoop += (24.0f * 28 + 1.0f + 14.0f / 60.0f);

            OFWorld::MoonModel moon = OFWorld::MoonModel(fadeInStart, fadeInFinish, fadeOutStart, fadeOutFinish,
                axisOffset, speed, dailyIncrement, fadeStartAngle, fadeEndAngle, moonShadowEarlyFadeAngle);

            OFRender::MoonState beforeState = moon.calculateState(timeStampBefore);
            OFRender::MoonState afterState = moon.calculateState(timeStampAfter);
            OFRender::MoonState beforeStatePostLoop = moon.calculateState(timeStampBeforePostLoop);
            OFRender::MoonState afterStatePostLoop = moon.calculateState(timeStampAfterPostLoop);

            EXPECT_LE(beforeState.mMoonAlpha, 0.0f);
            EXPECT_GT(afterState.mMoonAlpha, 0.0f);
            EXPECT_LE(beforeStatePostLoop.mMoonAlpha, 0.0f);
            EXPECT_GT(afterStatePostLoop.mMoonAlpha, 0.0f);
        }
    }
}
