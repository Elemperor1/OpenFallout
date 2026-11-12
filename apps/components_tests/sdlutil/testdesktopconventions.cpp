#include <components/sdlutil/desktopconventions.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace SDLUtil;

    TEST(SDLUtilDesktopShortcutTest, commandQIsQuit)
    {
        EXPECT_EQ(desktopShortcut(SDLK_q, KMOD_LGUI), DesktopShortcut::Quit);
        EXPECT_EQ(desktopShortcut(SDLK_q, KMOD_RGUI), DesktopShortcut::Quit);
    }

    TEST(SDLUtilDesktopShortcutTest, commandHMAndWAreHideMinimiseAndCloseWindow)
    {
        EXPECT_EQ(desktopShortcut(SDLK_h, KMOD_LGUI), DesktopShortcut::Hide);
        EXPECT_EQ(desktopShortcut(SDLK_m, KMOD_LGUI), DesktopShortcut::Minimise);
        EXPECT_EQ(desktopShortcut(SDLK_w, KMOD_LGUI), DesktopShortcut::CloseWindow);
    }

    TEST(SDLUtilDesktopShortcutTest, controlCommandFTogglesFullScreen)
    {
        EXPECT_EQ(desktopShortcut(SDLK_f, KMOD_LGUI | KMOD_LCTRL), DesktopShortcut::ToggleFullScreen);
        EXPECT_EQ(desktopShortcut(SDLK_f, KMOD_LGUI), DesktopShortcut::None);
    }

    TEST(SDLUtilDesktopShortcutTest, theLettersAloneAreGameKeys)
    {
        EXPECT_EQ(desktopShortcut(SDLK_q, KMOD_NONE), DesktopShortcut::None);
        EXPECT_EQ(desktopShortcut(SDLK_w, KMOD_LSHIFT), DesktopShortcut::None);
        EXPECT_EQ(desktopShortcut(SDLK_m, KMOD_LCTRL), DesktopShortcut::None);
    }

    TEST(SDLUtilDesktopShortcutTest, extraModifiersMakeADifferentShortcut)
    {
        // Shift-Command-Q is the system's Log Out, Option-Command-H is Hide Others
        EXPECT_EQ(desktopShortcut(SDLK_q, KMOD_LGUI | KMOD_LSHIFT), DesktopShortcut::None);
        EXPECT_EQ(desktopShortcut(SDLK_h, KMOD_LGUI | KMOD_LALT), DesktopShortcut::None);
    }

    TEST(SDLUtilDesktopShortcutTest, lockKeysDoNotMatter)
    {
        EXPECT_EQ(desktopShortcut(SDLK_q, KMOD_LGUI | KMOD_CAPS | KMOD_NUM), DesktopShortcut::Quit);
    }

    // The units MyGUI counts the wheel in: 120 to a notch of a mouse wheel
    constexpr double myGuiUnitsPerNotch = 120.0;

    TEST(SDLUtilWheelAccumulatorTest, aMouseWheelNotchIsOneWholeStep)
    {
        WheelAccumulator wheel(myGuiUnitsPerNotch);
        EXPECT_EQ(wheel.add(1.f, SDL_MOUSEWHEEL_NORMAL), 120);
        EXPECT_EQ(wheel.add(-1.f, SDL_MOUSEWHEEL_NORMAL), -120);
    }

    TEST(SDLUtilWheelAccumulatorTest, trackpadFractionsMoveOnEveryEventAndAddUpToANotch)
    {
        WheelAccumulator wheel(myGuiUnitsPerNotch);
        int total = 0;
        for (int i = 0; i < 4; ++i)
        {
            const int step = wheel.add(0.25f, SDL_MOUSEWHEEL_NORMAL);
            EXPECT_EQ(step, 30);
            total += step;
        }
        EXPECT_EQ(total, 120);
    }

    TEST(SDLUtilWheelAccumulatorTest, theSystemDirectionSettingIsNotAppliedTwice)
    {
        // With natural scrolling on, SDL marks the event flipped but its values already go the way the system setting
        // says; inverting them again would undo the setting
        WheelAccumulator wheel(myGuiUnitsPerNotch);
        EXPECT_EQ(wheel.add(-1.f, SDL_MOUSEWHEEL_FLIPPED), -120);
        EXPECT_EQ(wheel.add(1.f, SDL_MOUSEWHEEL_FLIPPED), 120);
    }

    TEST(SDLUtilWheelAccumulatorTest, wholeNotchesFromSmallTrackpadDeltas)
    {
        WheelAccumulator notches(1.0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(notches.add(2.5f, SDL_MOUSEWHEEL_NORMAL), 2);
    }

    TEST(SDLUtilWheelAccumulatorTest, reversingDropsTheUnfinishedPartOfTheOtherWay)
    {
        WheelAccumulator notches(1.0);
        EXPECT_EQ(notches.add(0.9f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(-0.5f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(-0.5f, SDL_MOUSEWHEEL_NORMAL), -1);
    }

    TEST(SDLUtilPixelsPerPointTest, retinaWindowHasTwoPixelsToAPoint)
    {
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 2880), 2.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 1440), 1.f);
    }

    TEST(SDLUtilPixelsPerPointTest, fractionalScalingIsKept)
    {
        EXPECT_FLOAT_EQ(pixelsPerPoint(1000, 1500), 1.5f);
    }

    TEST(SDLUtilPixelsPerPointTest, aWindowWithNoSizeCountsAsOne)
    {
        // a minimised or hidden window can report 0 x 0
        EXPECT_FLOAT_EQ(pixelsPerPoint(0, 0), 1.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(0, 2880), 1.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 0), 1.f);
    }

    TEST(SDLUtilPixelsPerPointTest, pointsAndPixelsConvertBothWays)
    {
        EXPECT_EQ(pointsToPixels(100, 2.f), 200);
        EXPECT_EQ(pointsToPixels(101, 1.5f), 152);
        EXPECT_EQ(pixelsToPoints(2880, 2.f), 1440);
        EXPECT_EQ(pixelsToPoints(1920, 1.5f), 1280);
    }

    TEST(SDLUtilInterfaceScaleTest, interfaceIsSizedInPointsTimesTheUserScale)
    {
        // The same window in points gives the same interface in points on a 1x and on a 2x display
        EXPECT_FLOAT_EQ(interfaceScale(1.f, 1440, 2880), 2.f);
        EXPECT_FLOAT_EQ(interfaceScale(1.f, 1440, 1440), 1.f);
        EXPECT_FLOAT_EQ(interfaceScale(1.25f, 1440, 2880), 2.5f);
    }

    MouseCaptureRequest playing()
    {
        MouseCaptureRequest request;
        request.wantGrab = true;
        request.wantRelative = true;
        request.wantCursorVisible = false;
        request.allowGrab = true;
        request.hasFocus = true;
        request.mouseInWindow = true;
        request.fullScreen = false;
        return request;
    }

    TEST(SDLUtilMouseCaptureTest, playingWithFocusCapturesTheMouse)
    {
        const MouseCapture capture = decideMouseCapture(playing());
        EXPECT_TRUE(capture.grab);
        EXPECT_TRUE(capture.windowGrab);
        EXPECT_TRUE(capture.relative);
        EXPECT_FALSE(capture.cursorVisible);
    }

    TEST(SDLUtilMouseCaptureTest, switchingAwayGivesThePointerBack)
    {
        MouseCaptureRequest request = playing();
        request.hasFocus = false;
        const MouseCapture capture = decideMouseCapture(request);
        EXPECT_FALSE(capture.grab);
        EXPECT_FALSE(capture.windowGrab);
        EXPECT_FALSE(capture.relative);
        EXPECT_TRUE(capture.cursorVisible);
    }

    TEST(SDLUtilMouseCaptureTest, aWindowedGameDoesNotCaptureAPointerOutsideIt)
    {
        MouseCaptureRequest request = playing();
        request.mouseInWindow = false;
        const MouseCapture capture = decideMouseCapture(request);
        EXPECT_FALSE(capture.grab);
        EXPECT_FALSE(capture.relative);
    }

    TEST(SDLUtilMouseCaptureTest, fullScreenRecapturesOnFocusEvenIfThePointerLeftDuringTheSpaceSwitch)
    {
        // The window covers the display, so the pointer is in it whatever the last enter or leave event said
        MouseCaptureRequest request = playing();
        request.fullScreen = true;
        request.mouseInWindow = false;
        const MouseCapture capture = decideMouseCapture(request);
        EXPECT_TRUE(capture.grab);
        EXPECT_TRUE(capture.relative);
    }

    TEST(SDLUtilMouseCaptureTest, noGrabOptionLeavesTheWindowUngrabbed)
    {
        MouseCaptureRequest request = playing();
        request.allowGrab = false;
        const MouseCapture capture = decideMouseCapture(request);
        EXPECT_TRUE(capture.grab);
        EXPECT_FALSE(capture.windowGrab);
        EXPECT_TRUE(capture.relative);
    }

    TEST(SDLUtilMouseCaptureTest, menusShowTheCursor)
    {
        MouseCaptureRequest request = playing();
        request.wantRelative = false;
        request.wantCursorVisible = true;
        const MouseCapture capture = decideMouseCapture(request);
        EXPECT_FALSE(capture.relative);
        EXPECT_TRUE(capture.cursorVisible);
    }

    TEST(SDLUtilFullScreenFlagTest, withSpacesBothFullScreenModesUseTheSystemSpace)
    {
        EXPECT_EQ(fullScreenFlag(Settings::WindowMode::Fullscreen, true), Uint32(SDL_WINDOW_FULLSCREEN_DESKTOP));
        EXPECT_EQ(
            fullScreenFlag(Settings::WindowMode::WindowedFullscreen, true), Uint32(SDL_WINDOW_FULLSCREEN_DESKTOP));
        EXPECT_EQ(fullScreenFlag(Settings::WindowMode::Windowed, true), 0u);
    }

    TEST(SDLUtilFullScreenFlagTest, withoutSpacesExclusiveFullScreenStays)
    {
        EXPECT_EQ(fullScreenFlag(Settings::WindowMode::Fullscreen, false), Uint32(SDL_WINDOW_FULLSCREEN));
        EXPECT_EQ(
            fullScreenFlag(Settings::WindowMode::WindowedFullscreen, false), Uint32(SDL_WINDOW_FULLSCREEN_DESKTOP));
        EXPECT_EQ(fullScreenFlag(Settings::WindowMode::Windowed, false), 0u);
    }

    TEST(SDLUtilDesktopShortcutFilterTest, theGameSeesNeitherThePressNorTheReleaseOfAShortcut)
    {
        DesktopShortcutFilter filter(true);
        EXPECT_TRUE(filter.swallowKeyDown(SDLK_w, KMOD_LGUI, false));
        EXPECT_TRUE(filter.swallowKeyDown(SDLK_w, KMOD_LGUI, true));
        EXPECT_TRUE(filter.swallowKeyUp(SDLK_w));
    }

    TEST(SDLUtilDesktopShortcutFilterTest, gameKeysGoThrough)
    {
        DesktopShortcutFilter filter(true);
        EXPECT_FALSE(filter.swallowKeyDown(SDLK_w, KMOD_NONE, false));
        EXPECT_FALSE(filter.swallowKeyUp(SDLK_w));
    }

    TEST(SDLUtilDesktopShortcutFilterTest, aKeyHeldForTheGameIsNotTakenWhenCommandIsPressedOnTop)
    {
        // Walking forward with W, then Command: the repeats and the release of W are still the game's
        DesktopShortcutFilter filter(true);
        EXPECT_FALSE(filter.swallowKeyDown(SDLK_w, KMOD_NONE, false));
        EXPECT_FALSE(filter.swallowKeyDown(SDLK_w, KMOD_LGUI, true));
        EXPECT_FALSE(filter.swallowKeyUp(SDLK_w));
    }

    TEST(SDLUtilDesktopShortcutFilterTest, aReleaseLostDuringASpaceSwitchDoesNotEatTheNextPress)
    {
        DesktopShortcutFilter filter(true);
        EXPECT_TRUE(filter.swallowKeyDown(SDLK_f, KMOD_LGUI | KMOD_LCTRL, false));
        // the release of F never came: the window went to its Space
        EXPECT_FALSE(filter.swallowKeyDown(SDLK_f, KMOD_NONE, false));
        EXPECT_FALSE(filter.swallowKeyUp(SDLK_f));
    }

    TEST(SDLUtilDesktopShortcutFilterTest, offTheMacNothingIsSwallowed)
    {
        DesktopShortcutFilter filter(false);
        EXPECT_FALSE(filter.swallowKeyDown(SDLK_q, KMOD_LGUI, false));
        EXPECT_FALSE(filter.swallowKeyUp(SDLK_q));
    }
}
