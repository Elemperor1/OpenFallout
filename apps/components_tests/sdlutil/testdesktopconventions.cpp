#include <components/sdlutil/desktopconventions.hpp>

#include <gtest/gtest.h>

namespace
{
    using namespace SDLUtil;

    constexpr DesktopPlatform mac = DesktopPlatform::Mac;
    constexpr DesktopPlatform other = DesktopPlatform::Other;

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

    TEST(SDLUtilToggleFullScreenTest, theGameLeavesItsOwnFullScreenModesToAWindow)
    {
        // SDL greys out its menu item Toggle Full Screen while the window is in an SDL full-screen mode
        const Uint32 resizable = SDL_WINDOW_RESIZABLE;
        EXPECT_EQ(windowModeForToggleFullScreen(resizable | SDL_WINDOW_FULLSCREEN_DESKTOP, mac),
            Settings::WindowMode::Windowed);
        EXPECT_EQ(
            windowModeForToggleFullScreen(resizable | SDL_WINDOW_FULLSCREEN, mac), Settings::WindowMode::Windowed);
    }

    TEST(SDLUtilToggleFullScreenTest, aResizableWindowIsLeftToTheMenuBar)
    {
        // the menu item takes it to a full-screen Space of its own and back
        EXPECT_EQ(windowModeForToggleFullScreen(SDL_WINDOW_RESIZABLE, mac), std::nullopt);
    }

    TEST(SDLUtilToggleFullScreenTest, aWindowTheMenuBarCanNotTakeGoesFullScreen)
    {
        EXPECT_EQ(windowModeForToggleFullScreen(0, mac), Settings::WindowMode::WindowedFullscreen);
    }

    TEST(SDLUtilToggleFullScreenTest, nothingChangesElsewhere)
    {
        EXPECT_EQ(windowModeForToggleFullScreen(SDL_WINDOW_FULLSCREEN_DESKTOP, other), std::nullopt);
        EXPECT_EQ(windowModeForToggleFullScreen(0, other), std::nullopt);
    }

    TEST(SDLUtilWheelAccumulatorTest, aMouseWheelNotchIsOneWholeStep)
    {
        WheelAccumulator wheel(myGuiUnitsPerNotch, mac);
        EXPECT_EQ(wheel.add(1.f, SDL_MOUSEWHEEL_NORMAL), 120);
        EXPECT_EQ(wheel.add(-1.f, SDL_MOUSEWHEEL_NORMAL), -120);
    }

    TEST(SDLUtilWheelAccumulatorTest, trackpadFractionsMoveOnEveryEventAndAddUpToANotch)
    {
        WheelAccumulator wheel(myGuiUnitsPerNotch, mac);
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
        WheelAccumulator wheel(myGuiUnitsPerNotch, mac);
        EXPECT_EQ(wheel.add(-1.f, SDL_MOUSEWHEEL_FLIPPED), -120);
        EXPECT_EQ(wheel.add(1.f, SDL_MOUSEWHEEL_FLIPPED), 120);
    }

    TEST(SDLUtilWheelAccumulatorTest, wholeNotchesFromSmallTrackpadDeltas)
    {
        WheelAccumulator notches(1.0, mac);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(notches.add(2.5f, SDL_MOUSEWHEEL_NORMAL), 2);
    }

    TEST(SDLUtilWheelAccumulatorTest, tenDeltasOfSevenTenthsAreSevenNotches)
    {
        // 0.7f is a little under 0.7: the sum must not fall a notch short by rounding
        WheelNotches notches(mac);
        int total = 0;
        for (int i = 0; i < 10; ++i)
            total += notches.add(0.7f, SDL_MOUSEWHEEL_NORMAL);
        EXPECT_EQ(total, 7);
        for (int i = 0; i < 10; ++i)
            total += notches.add(0.7f, SDL_MOUSEWHEEL_NORMAL);
        EXPECT_EQ(total, 14);
        int back = 0;
        for (int i = 0; i < 10; ++i)
            back += notches.add(-0.7f, SDL_MOUSEWHEEL_NORMAL);
        EXPECT_EQ(back, -7);
    }

    TEST(SDLUtilWheelAccumulatorTest, reversingDropsTheUnfinishedPartOfTheOtherWay)
    {
        WheelAccumulator notches(1.0, mac);
        EXPECT_EQ(notches.add(0.9f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(-0.5f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(-0.5f, SDL_MOUSEWHEEL_NORMAL), -1);
    }

    TEST(SDLUtilPixelsPerPointTest, retinaWindowHasTwoPixelsToAPoint)
    {
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 2880, mac), 2.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 1440, mac), 1.f);
    }

    TEST(SDLUtilPixelsPerPointTest, fractionalScalingIsKept)
    {
        EXPECT_FLOAT_EQ(pixelsPerPoint(1000, 1500, mac), 1.5f);
    }

    TEST(SDLUtilPixelsPerPointTest, aWindowWithNoSizeCountsAsOne)
    {
        // a minimised or hidden window can report 0 x 0
        EXPECT_FLOAT_EQ(pixelsPerPoint(0, 0, mac), 1.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(0, 2880, mac), 1.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 0, mac), 1.f);
    }

    TEST(SDLUtilPixelsPerPointTest, pointsAndPixelsConvertBothWays)
    {
        EXPECT_EQ(pointsToPixels(100, 2.f), 200);
        EXPECT_EQ(pointsToPixels(101, 1.5f), 152);
        EXPECT_EQ(pixelsToPoints(2880, 2.f, mac), 1440);
        EXPECT_EQ(pixelsToPoints(1920, 1.5f, mac), 1280);
    }

    TEST(SDLUtilInterfaceScaleTest, interfaceIsSizedInPointsTimesTheUserScale)
    {
        // The same window in points gives the same interface in points on a 1x and on a 2x display
        EXPECT_FLOAT_EQ(interfaceScale(1.f, 1440, 2880, mac), 2.f);
        EXPECT_FLOAT_EQ(interfaceScale(1.f, 1440, 1440, mac), 1.f);
        EXPECT_FLOAT_EQ(interfaceScale(1.25f, 1440, 2880, mac), 2.5f);
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
        const MouseCapture capture = decideMouseCapture(playing(), mac);
        EXPECT_TRUE(capture.grab);
        EXPECT_TRUE(capture.windowGrab);
        EXPECT_TRUE(capture.relative);
        EXPECT_FALSE(capture.cursorVisible);
    }

    TEST(SDLUtilMouseCaptureTest, switchingAwayGivesThePointerBack)
    {
        MouseCaptureRequest request = playing();
        request.hasFocus = false;
        const MouseCapture capture = decideMouseCapture(request, mac);
        EXPECT_FALSE(capture.grab);
        EXPECT_FALSE(capture.windowGrab);
        EXPECT_FALSE(capture.relative);
        EXPECT_TRUE(capture.cursorVisible);
    }

    TEST(SDLUtilMouseCaptureTest, aWindowedGameDoesNotCaptureAPointerOutsideIt)
    {
        MouseCaptureRequest request = playing();
        request.mouseInWindow = false;
        const MouseCapture capture = decideMouseCapture(request, mac);
        EXPECT_FALSE(capture.grab);
        EXPECT_FALSE(capture.relative);
    }

    TEST(SDLUtilMouseCaptureTest, fullScreenRecapturesOnFocusEvenIfThePointerLeftDuringTheSpaceSwitch)
    {
        // The window covers the display, so the pointer is in it whatever the last enter or leave event said
        MouseCaptureRequest request = playing();
        request.fullScreen = true;
        request.mouseInWindow = false;
        const MouseCapture capture = decideMouseCapture(request, mac);
        EXPECT_TRUE(capture.grab);
        EXPECT_TRUE(capture.relative);
    }

    TEST(SDLUtilMouseCaptureTest, noGrabOptionLeavesTheWindowUngrabbed)
    {
        MouseCaptureRequest request = playing();
        request.allowGrab = false;
        const MouseCapture capture = decideMouseCapture(request, mac);
        EXPECT_TRUE(capture.grab);
        EXPECT_FALSE(capture.windowGrab);
        EXPECT_TRUE(capture.relative);
    }

    TEST(SDLUtilMouseCaptureTest, menusShowTheCursor)
    {
        MouseCaptureRequest request = playing();
        request.wantRelative = false;
        request.wantCursorVisible = true;
        const MouseCapture capture = decideMouseCapture(request, mac);
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

    // Off macOS (Windows, Linux) every rule below is what the engine did before the desktop conventions

    TEST(SDLUtilOtherDesktopTest, wheelLeftoverIsKeptWhenTurningTheOtherWay)
    {
        // 1/64 of a notch is 1.875 units: 1 now, 0.875 left over
        WheelAccumulator wheel(myGuiUnitsPerNotch, other);
        EXPECT_EQ(wheel.add(0.015625f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(wheel.add(-0.5f, SDL_MOUSEWHEEL_NORMAL), -59);
        EXPECT_EQ(wheel.add(0.25f, SDL_MOUSEWHEEL_NORMAL), 29);

        WheelAccumulator macWheel(myGuiUnitsPerNotch, mac);
        EXPECT_EQ(macWheel.add(0.015625f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(macWheel.add(-0.5f, SDL_MOUSEWHEEL_NORMAL), -60);
        EXPECT_EQ(macWheel.add(0.25f, SDL_MOUSEWHEEL_NORMAL), 30);
    }

    TEST(SDLUtilOtherDesktopTest, wheelFractionsStillAddUp)
    {
        WheelAccumulator wheel(myGuiUnitsPerNotch, other);
        EXPECT_EQ(wheel.add(0.004f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(wheel.add(0.005f, SDL_MOUSEWHEEL_NORMAL), 1);
        // 0.08 of a unit is left over and kept
        EXPECT_EQ(wheel.add(-1.f, SDL_MOUSEWHEEL_FLIPPED), -119);
    }

    TEST(SDLUtilWheelNotchesTest, onTheMacSmallDeltasAddUpToWholeNotches)
    {
        WheelNotches notches(mac);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(0.5f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(notches.add(-0.6f, SDL_MOUSEWHEEL_NORMAL), 0);
        EXPECT_EQ(notches.add(-0.6f, SDL_MOUSEWHEEL_NORMAL), -1);
    }

    TEST(SDLUtilWheelNotchesTest, elsewhereEveryEventIsANotchInItsDirection)
    {
        // as before: each wheel event moved a Fallout menu by one row, whatever its size
        WheelNotches notches(other);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(notches.add(0.3f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(notches.add(-0.1f, SDL_MOUSEWHEEL_NORMAL), -1);
        EXPECT_EQ(notches.add(1.f, SDL_MOUSEWHEEL_NORMAL), 1);
        EXPECT_EQ(notches.add(-2.f, SDL_MOUSEWHEEL_NORMAL), -2);
        EXPECT_EQ(notches.add(0.f, SDL_MOUSEWHEEL_NORMAL), 0);
    }

    TEST(SDLUtilOtherDesktopTest, pixelsPerPointIsAWholeNumber)
    {
        EXPECT_FLOAT_EQ(pixelsPerPoint(1440, 2880, other), 2.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(1000, 1500, other), 1.f);
        EXPECT_FLOAT_EQ(pixelsPerPoint(1920, 1920, other), 1.f);
    }

    TEST(SDLUtilOtherDesktopTest, pixelsToPointsDividesByTheWholeNumber)
    {
        EXPECT_EQ(pixelsToPoints(2880, 2.f, other), 1440);
        EXPECT_EQ(pixelsToPoints(1921, 2.f, other), 960);
        EXPECT_EQ(pixelsToPoints(1921, 1.f, other), 1921);
    }

    TEST(SDLUtilOtherDesktopTest, interfaceScaleUsesTheWholeNumber)
    {
        EXPECT_FLOAT_EQ(interfaceScale(1.25f, 1440, 2880, other), 2.5f);
        EXPECT_FLOAT_EQ(interfaceScale(1.25f, 1000, 1500, other), 1.25f);
    }

    TEST(SDLUtilInterfaceRescaleTest, onTheMacAResizeToAnotherScalingRescales)
    {
        EXPECT_EQ(interfaceScaleAfterResize(1.f, 1440, 1440, 2.f, mac), std::optional<float>(1.f));
        EXPECT_EQ(interfaceScaleAfterResize(1.f, 1440, 2880, 2.f, mac), std::nullopt);
    }

    TEST(SDLUtilInterfaceRescaleTest, elsewhereTheInterfaceKeepsItsStartScale)
    {
        EXPECT_EQ(interfaceScaleAfterResize(1.f, 1440, 1440, 2.f, other), std::nullopt);
    }

    TEST(SDLUtilOtherDesktopTest, fullScreenDoesNotCaptureAPointerOutsideTheWindow)
    {
        MouseCaptureRequest request = playing();
        request.fullScreen = true;
        request.mouseInWindow = false;
        const MouseCapture capture = decideMouseCapture(request, other);
        EXPECT_FALSE(capture.grab);
        EXPECT_FALSE(capture.windowGrab);
        EXPECT_FALSE(capture.relative);
        EXPECT_FALSE(capture.cursorVisible);
    }

    TEST(SDLUtilOtherDesktopTest, captureIsOtherwiseTheSame)
    {
        EXPECT_TRUE(decideMouseCapture(playing(), other).grab);
        EXPECT_TRUE(decideMouseCapture(playing(), other).relative);
        MouseCaptureRequest request = playing();
        request.hasFocus = false;
        EXPECT_TRUE(decideMouseCapture(request, other).cursorVisible);
        request = playing();
        request.allowGrab = false;
        EXPECT_FALSE(decideMouseCapture(request, other).windowGrab);
    }

    TEST(SDLUtilWindowEventTest, onTheMacASizeChangeRescalesFirstAndUpdatesTheCapture)
    {
        const WindowEventResponse response = respondToWindowEvent(SDL_WINDOWEVENT_SIZE_CHANGED, mac);
        EXPECT_TRUE(response.resizeDrawable);
        EXPECT_TRUE(response.scaleBeforeListener);
        EXPECT_TRUE(response.updateMouseCapture);
    }

    TEST(SDLUtilWindowEventTest, onTheMacADisplayChangeResizesTheDrawable)
    {
        const WindowEventResponse response = respondToWindowEvent(SDL_WINDOWEVENT_DISPLAY_CHANGED, mac);
        EXPECT_TRUE(response.resizeDrawable);
        EXPECT_TRUE(response.scaleBeforeListener);
        EXPECT_FALSE(response.updateMouseCapture);
    }

    TEST(SDLUtilWindowEventTest, elsewhereASizeChangeOnlyResizesAndADisplayChangeDoesNothing)
    {
        const WindowEventResponse sizeChanged = respondToWindowEvent(SDL_WINDOWEVENT_SIZE_CHANGED, other);
        EXPECT_TRUE(sizeChanged.resizeDrawable);
        EXPECT_FALSE(sizeChanged.scaleBeforeListener);
        EXPECT_FALSE(sizeChanged.updateMouseCapture);
        const WindowEventResponse displayChanged = respondToWindowEvent(SDL_WINDOWEVENT_DISPLAY_CHANGED, other);
        EXPECT_FALSE(displayChanged.resizeDrawable);
        EXPECT_FALSE(displayChanged.updateMouseCapture);
    }

    TEST(SDLUtilDesktopPlatformTest, theBridgeNamesThisPlatform)
    {
#ifdef __APPLE__
        EXPECT_EQ(thisDesktopPlatform(), mac);
#else
        EXPECT_EQ(thisDesktopPlatform(), other);
#endif
        EXPECT_EQ(followsMacDesktopConventions(), thisDesktopPlatform() == mac);
    }
}
