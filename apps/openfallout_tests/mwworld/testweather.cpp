#include <gtest/gtest.h>

#include <cmath>

#include <components/esm/formid.hpp>
#include <components/esm/refid.hpp>
#include <components/esm4/loadclmt.hpp>
#include <components/esm4/loadwthr.hpp>

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

        // WEATHERS AND CLIMATES OF FALLOUT

        ESM::RefId formIdRefId(std::uint32_t formId)
        {
            return ESM::RefId(ESM::FormId::fromUint32(formId));
        }

        int sum(const std::map<ESM::RefId, uint8_t>& chances)
        {
            int result = 0;
            for (const auto& [id, chance] : chances)
                result += chance;
            return result;
        }

        // Colours as NAM0 has them: for each of the ten types the colours of the times of day, each with an unused byte
        void setColour(ESM4::Weather& record, std::size_t times, ESM4::Weather::ColourType type,
            ESM4::Weather::TimeOfDay time, std::uint8_t red, std::uint8_t green, std::uint8_t blue)
        {
            const std::size_t offset = (static_cast<std::size_t>(type) * times + static_cast<std::size_t>(time)) * 4;
            record.mColours[offset] = red;
            record.mColours[offset + 1] = green;
            record.mColours[offset + 2] = blue;
        }

        ESM4::Weather makeRecord(std::size_t times)
        {
            using Type = ESM4::Weather::ColourType;
            using Time = ESM4::Weather::TimeOfDay;

            ESM4::Weather record;
            record.mId = ESM::FormId::fromUint32(0x01000800);
            record.mEditorId = "TestWeather";
            record.mColours.assign(ESM4::Weather::sColourTypeCount * times * 4, 0);
            setColour(record, times, Type::SkyUpper, Time::Sunrise, 255, 128, 0);
            setColour(record, times, Type::SkyUpper, Time::Day, 51, 102, 204);
            setColour(record, times, Type::SkyUpper, Time::Sunset, 204, 51, 0);
            setColour(record, times, Type::SkyUpper, Time::Night, 0, 0, 51);
            setColour(record, times, Type::Fog, Time::Day, 10, 20, 30);
            setColour(record, times, Type::Ambient, Time::Day, 40, 50, 60);
            setColour(record, times, Type::Sunlight, Time::Day, 255, 255, 204);
            setColour(record, times, Type::Sun, Time::Sunset, 255, 0, 0);
            record.mFog.mDayNear = 100.f;
            record.mFog.mDayFar = 9000.f;
            record.mFog.mNightNear = 50.f;
            record.mFog.mNightFar = 4000.f;
            record.mData.mWindSpeed = 51;
            record.mData.mSunGlare = 255;
            return record;
        }

        TEST(OFWorldWeatherTest, aWeatherOfFalloutTakesItsColoursFogAndWindFromTheRecord)
        {
            const ESM4::Weather record = makeRecord(4);
            const Weather weather(formIdRefId(0x01000800), 10, record, 0.8f);

            EXPECT_EQ(weather.mScriptId, 10);
            EXPECT_EQ(weather.mName, "TestWeather");
            EXPECT_TRUE(weather.mFromRecord);

            EXPECT_EQ(weather.mSkyColor.getSunriseValue(), osg::Vec4f(1.f, 128 / 255.f, 0.f, 1.f));
            EXPECT_EQ(weather.mSkyColor.getDayValue(), osg::Vec4f(0.2f, 0.4f, 0.8f, 1.f));
            EXPECT_EQ(weather.mSkyColor.getSunsetValue(), osg::Vec4f(0.8f, 0.2f, 0.f, 1.f));
            EXPECT_EQ(weather.mSkyColor.getNightValue(), osg::Vec4f(0.f, 0.f, 0.2f, 1.f));
            EXPECT_EQ(weather.mFogColor.getDayValue(), osg::Vec4f(10 / 255.f, 20 / 255.f, 30 / 255.f, 1.f));
            EXPECT_EQ(weather.mAmbientColor.getDayValue(), osg::Vec4f(40 / 255.f, 50 / 255.f, 60 / 255.f, 1.f));
            EXPECT_EQ(weather.mSunColor.getDayValue(), osg::Vec4f(1.f, 1.f, 0.8f, 1.f));
            EXPECT_EQ(weather.mSunDiscSunsetColor, osg::Vec4f(1.f, 0.f, 0.f, 1.f));

            EXPECT_TRUE(weather.mHasFogRange);
            EXPECT_EQ(weather.mFogNear.getSunriseValue(), 100.f);
            EXPECT_EQ(weather.mFogFar.getDayValue(), 9000.f);
            EXPECT_EQ(weather.mFogNear.getSunsetValue(), 100.f);
            EXPECT_EQ(weather.mFogNear.getNightValue(), 50.f);
            EXPECT_EQ(weather.mFogFar.getNightValue(), 4000.f);

            EXPECT_FLOAT_EQ(weather.mWindSpeed, 0.2f);
            EXPECT_FLOAT_EQ(weather.mGlareView, 1.f);
            EXPECT_FALSE(weather.mIsStorm);
        }

        TEST(OFWorldWeatherTest, aWeatherOfFalloutWithSixTimesOfDayUsesTheFirstFour)
        {
            const ESM4::Weather record = makeRecord(6);
            const Weather weather(formIdRefId(0x01000800), 10, record, 0.8f);

            EXPECT_EQ(weather.mSkyColor.getSunriseValue(), osg::Vec4f(1.f, 128 / 255.f, 0.f, 1.f));
            EXPECT_EQ(weather.mSkyColor.getDayValue(), osg::Vec4f(0.2f, 0.4f, 0.8f, 1.f));
            EXPECT_EQ(weather.mSkyColor.getNightValue(), osg::Vec4f(0.f, 0.f, 0.2f, 1.f));
            EXPECT_EQ(weather.mSunColor.getDayValue(), osg::Vec4f(1.f, 1.f, 0.8f, 1.f));
        }

        TEST(OFWorldWeatherTest, aWeatherOfFalloutWithoutFogDistancesHasNoFogRange)
        {
            ESM4::Weather record = makeRecord(4);
            record.mFog = {};
            const Weather weather(formIdRefId(0x01000800), 10, record, 0.8f);

            EXPECT_FALSE(weather.mHasFogRange);
        }

        TEST(OFWorldWeatherTest, aWeatherOfFalloutWithAFogRangeForTheDayOnlyHasNoFogRange)
        {
            ESM4::Weather record = makeRecord(4);
            record.mFog.mNightNear = 0.f;
            record.mFog.mNightFar = 0.f;
            const Weather weather(formIdRefId(0x01000800), 10, record, 0.8f);

            EXPECT_FALSE(weather.mHasFogRange);
        }

        TEST(OFWorldWeatherTest, aWeatherOfMorrowindIsNotFromARecord)
        {
            const Weather weather(ESM::RefId::stringRefId("Clear"), 0, "Clear", 0.8f, 1.f, 0.f, {});

            EXPECT_FALSE(weather.mFromRecord);
            EXPECT_FALSE(weather.mHasFogRange);
        }

        TEST(OFWorldWeatherTest, chancesThatAddUpTo100AreKept)
        {
            const ESM::RefId clear = formIdRefId(0x01000001);
            const ESM::RefId cloudy = formIdRefId(0x01000002);

            const std::map<ESM::RefId, uint8_t> chances = normaliseChances({ { clear, 70 }, { cloudy, 30 } });

            EXPECT_EQ(chances.size(), 2u);
            EXPECT_EQ(chances.at(clear), 70);
            EXPECT_EQ(chances.at(cloudy), 30);
        }

        TEST(OFWorldWeatherTest, chancesAreScaledToAddUpTo100)
        {
            const ESM::RefId clear = formIdRefId(0x01000001);
            const ESM::RefId cloudy = formIdRefId(0x01000002);

            const std::map<ESM::RefId, uint8_t> chances = normaliseChances({ { clear, 6 }, { cloudy, 3 } });

            EXPECT_EQ(chances.at(clear), 67);
            EXPECT_EQ(chances.at(cloudy), 33);
        }

        TEST(OFWorldWeatherTest, chancesThatDoNotDivideEvenlyGetTheRemainderOneByOne)
        {
            const std::map<ESM::RefId, uint8_t> chances = normaliseChances(
                { { formIdRefId(0x01000001), 1 }, { formIdRefId(0x01000002), 1 }, { formIdRefId(0x01000003), 1 } });

            EXPECT_EQ(chances.size(), 3u);
            EXPECT_EQ(sum(chances), 100);
            for (const auto& [id, chance] : chances)
                EXPECT_TRUE(chance == 33 || chance == 34) << static_cast<int>(chance);
        }

        TEST(OFWorldWeatherTest, aWeatherListedTwiceHasTheChancesAddedUp)
        {
            const ESM::RefId clear = formIdRefId(0x01000001);
            const ESM::RefId cloudy = formIdRefId(0x01000002);

            const std::map<ESM::RefId, uint8_t> chances
                = normaliseChances({ { clear, 10 }, { cloudy, 20 }, { clear, 10 } });

            EXPECT_EQ(chances.at(clear), 50);
            EXPECT_EQ(chances.at(cloudy), 50);
        }

        TEST(OFWorldWeatherTest, aChanceOfZeroOrLessIsLeftOut)
        {
            const ESM::RefId clear = formIdRefId(0x01000001);

            const std::map<ESM::RefId, uint8_t> chances
                = normaliseChances({ { clear, 5 }, { formIdRefId(0x01000002), 0 }, { formIdRefId(0x01000003), -4 } });

            EXPECT_EQ(chances.size(), 1u);
            EXPECT_EQ(chances.at(clear), 100);
        }

        TEST(OFWorldWeatherTest, withoutAnyChanceThereAreNoChances)
        {
            EXPECT_TRUE(normaliseChances({}).empty());
            EXPECT_TRUE(normaliseChances({ { formIdRefId(0x01000001), 0 } }).empty());
        }

        TEST(OFWorldWeatherTest, theChancesOfAClimateLeaveOutTheWeathersTheStoreDoesNotHave)
        {
            WeatherStore store;
            ESM4::Weather record = makeRecord(4);
            store.insertStatic(Weather(formIdRefId(0x01000800), 0, record, 0.8f));
            store.insertStatic(Weather(formIdRefId(0x01000801), 1, record, 0.8f));

            ESM4::Climate climate;
            climate.mWeathers.push_back({ 0x01000800, 50, 0 });
            climate.mWeathers.push_back({ 0x01000801, 25, 0 });
            climate.mWeathers.push_back({ 0x01000802, 25, 0 }); // not in the store

            const std::map<ESM::RefId, uint8_t> chances = climateChances(climate, store);

            EXPECT_EQ(chances.size(), 2u);
            EXPECT_EQ(chances.at(formIdRefId(0x01000800)), 67);
            EXPECT_EQ(chances.at(formIdRefId(0x01000801)), 33);
        }
    }
}
