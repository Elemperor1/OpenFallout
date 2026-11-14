#include "desktopconventions.hpp"

#include <cmath>

#include <SDL_hints.h>

namespace SDLUtil
{
    namespace
    {
        enum ModifierSet : unsigned
        {
            Command = 1,
            Control = 2,
            Option = 4,
            Shift = 8,
        };

        unsigned modifierSet(Uint16 mod)
        {
            unsigned set = 0;
            if (mod & KMOD_GUI)
                set |= Command;
            if (mod & KMOD_CTRL)
                set |= Control;
            if (mod & KMOD_ALT)
                set |= Option;
            if (mod & KMOD_SHIFT)
                set |= Shift;
            return set;
        }
    }

    DesktopShortcut desktopShortcut(SDL_Keycode key, Uint16 mod)
    {
        const unsigned set = modifierSet(mod);
        if (set == Command)
        {
            switch (key)
            {
                case SDLK_q:
                    return DesktopShortcut::Quit;
                case SDLK_h:
                    return DesktopShortcut::Hide;
                case SDLK_m:
                    return DesktopShortcut::Minimise;
                case SDLK_w:
                    return DesktopShortcut::CloseWindow;
                default:
                    break;
            }
        }
        else if (set == (Command | Control) && key == SDLK_f)
            return DesktopShortcut::ToggleFullScreen;
        return DesktopShortcut::None;
    }

    WheelAccumulator::WheelAccumulator(double unitsPerNotch, DesktopPlatform platform)
        : mUnitsPerNotch(unitsPerNotch)
        , mPlatform(platform)
    {
    }

    int WheelAccumulator::add(float precise, Uint32 /*direction*/)
    {
        const double units = static_cast<double>(precise) * mUnitsPerNotch;
        if (mPlatform == DesktopPlatform::Mac && ((units > 0 && mPending < 0) || (units < 0 && mPending > 0)))
            mPending = 0.0;
        mPending += units;
        if (mPlatform == DesktopPlatform::Mac)
        {
            // Deltas such as 0.7f are a little under their decimal value: a sum that is a hair short of a whole step
            // counts as the whole step, so ten deltas of 0.7 are seven steps and not six
            const double nearest = std::round(mPending);
            if (std::abs(mPending - nearest) < 1e-4)
                mPending = nearest;
        }
        const int whole = static_cast<int>(mPending);
        mPending -= whole;
        return whole;
    }

    WheelNotches::WheelNotches(DesktopPlatform platform)
        : mPlatform(platform)
        , mAccumulator(1.0, platform)
    {
    }

    int WheelNotches::add(float precise, Uint32 direction)
    {
        if (mPlatform == DesktopPlatform::Mac)
            return mAccumulator.add(precise, direction);
        if (precise > 0.f)
            return static_cast<int>(std::ceil(precise));
        if (precise < 0.f)
            return static_cast<int>(std::floor(precise));
        return 0;
    }

    DesktopShortcutFilter::DesktopShortcutFilter(bool enabled)
        : mEnabled(enabled)
    {
    }

    bool DesktopShortcutFilter::swallowKeyDown(SDL_Keycode key, Uint16 mod, bool repeat)
    {
        if (repeat)
            return mSwallowed.count(key) > 0;
        mSwallowed.erase(key);
        if (!mEnabled || desktopShortcut(key, mod) == DesktopShortcut::None)
            return false;
        mSwallowed.insert(key);
        return true;
    }

    bool DesktopShortcutFilter::swallowKeyUp(SDL_Keycode key)
    {
        return mSwallowed.erase(key) > 0;
    }

    float pixelsPerPoint(int windowPoints, int drawablePixels, DesktopPlatform platform)
    {
        if (platform == DesktopPlatform::Other)
        {
            // the integer division of before; only the division by zero of a window with no size is left out
            if (windowPoints <= 0)
                return 1.f;
            return static_cast<float>(drawablePixels / windowPoints);
        }
        if (windowPoints <= 0 || drawablePixels <= 0)
            return 1.f;
        return static_cast<float>(drawablePixels) / static_cast<float>(windowPoints);
    }

    int pointsToPixels(int points, float pixelsPerPoint)
    {
        return static_cast<int>(std::lround(points * pixelsPerPoint));
    }

    int pixelsToPoints(int pixels, float pixelsPerPoint, DesktopPlatform platform)
    {
        if (platform == DesktopPlatform::Other)
        {
            const int whole = static_cast<int>(pixelsPerPoint);
            return whole > 0 ? pixels / whole : pixels;
        }
        if (pixelsPerPoint <= 0.f)
            return pixels;
        return static_cast<int>(std::lround(pixels / pixelsPerPoint));
    }

    float interfaceScale(float userScale, int windowPoints, int drawablePixels, DesktopPlatform platform)
    {
        return userScale * pixelsPerPoint(windowPoints, drawablePixels, platform);
    }

    std::optional<float> interfaceScaleAfterResize(
        float userScale, int windowPoints, int drawablePixels, float current, DesktopPlatform platform)
    {
        if (platform != DesktopPlatform::Mac)
            return std::nullopt;
        const float scale = interfaceScale(userScale, windowPoints, drawablePixels, platform);
        if (scale == current)
            return std::nullopt;
        return scale;
    }

    WindowEventResponse respondToWindowEvent(Uint8 windowEvent, DesktopPlatform platform)
    {
        const bool mac = platform == DesktopPlatform::Mac;
        WindowEventResponse response;
        switch (windowEvent)
        {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                response.resizeDrawable = true;
                response.scaleBeforeListener = mac;
                response.updateMouseCapture = mac;
                break;
            case SDL_WINDOWEVENT_DISPLAY_CHANGED:
                response.resizeDrawable = mac;
                response.scaleBeforeListener = mac;
                break;
            default:
                break;
        }
        return response;
    }

    MouseCapture decideMouseCapture(const MouseCaptureRequest& request, DesktopPlatform platform)
    {
        const bool fullScreenHasThePointer = platform == DesktopPlatform::Mac && request.fullScreen;
        const bool pointerHere = request.hasFocus && (request.mouseInWindow || fullScreenHasThePointer);
        MouseCapture capture;
        capture.grab = request.wantGrab && pointerHere;
        capture.windowGrab = capture.grab && request.allowGrab;
        capture.relative = request.wantRelative && pointerHere;
        capture.cursorVisible = request.wantCursorVisible || !request.hasFocus;
        return capture;
    }

    Uint32 fullScreenFlag(Settings::WindowMode mode, bool useSpaces)
    {
        switch (mode)
        {
            case Settings::WindowMode::Fullscreen:
                return useSpaces ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;
            case Settings::WindowMode::WindowedFullscreen:
                return SDL_WINDOW_FULLSCREEN_DESKTOP;
            case Settings::WindowMode::Windowed:
                break;
        }
        return 0;
    }

    std::optional<Settings::WindowMode> windowModeForToggleFullScreen(Uint32 windowFlags, DesktopPlatform platform)
    {
        if (platform != DesktopPlatform::Mac)
            return std::nullopt;
        // SDL_cocoawindow.m validateMenuItem: the item is greyed out in these two cases, so the key would do nothing
        if (windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP))
            return Settings::WindowMode::Windowed;
        if ((windowFlags & SDL_WINDOW_RESIZABLE) == 0)
            return Settings::WindowMode::WindowedFullscreen;
        return std::nullopt;
    }

    namespace
    {
        bool fillsASpace(Uint32 windowFlags, DesktopPlatform platform)
        {
            // SDL_WINDOW_FULLSCREEN is part of SDL_WINDOW_FULLSCREEN_DESKTOP; it is also set while the window is in a
            // Space it entered through the green button or the menu bar
            return platform == DesktopPlatform::Mac && (windowFlags & SDL_WINDOW_FULLSCREEN) != 0;
        }
    }

    WindowSize windowSizeAfterResize(
        WindowSize window, WindowSize drawable, Uint32 windowFlags, DesktopPlatform platform)
    {
        return fillsASpace(windowFlags, platform) ? window : drawable;
    }

    WindowSize sizeForWindowMode(
        WindowSize setting, WindowSize window, Settings::WindowMode mode, DesktopPlatform platform)
    {
        return platform == DesktopPlatform::Mac && mode == Settings::WindowMode::Windowed ? window : setting;
    }

    WindowSize resolutionToSave(WindowSize setting, WindowSize window, Uint32 windowFlags, DesktopPlatform platform)
    {
        return fillsASpace(windowFlags, platform) ? window : setting;
    }

    DesktopPlatform thisDesktopPlatform()
    {
#ifdef __APPLE__
        return DesktopPlatform::Mac;
#else
        return DesktopPlatform::Other;
#endif
    }

    bool followsMacDesktopConventions()
    {
        return thisDesktopPlatform() == DesktopPlatform::Mac;
    }

    void applyDesktopHints()
    {
#ifdef __APPLE__
        // Full screen goes to a Space of its own (SDL's default on macOS, set so that it stays so), and the menu bar
        // SDL puts up answers Command-Q, -H, -M, -W and Control-Command-F; closing the only window quits through
        // SDL_QUIT, which the engine takes as its normal quit.
        SDL_SetHint(SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES, "1");
        SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "1");
#endif
    }
}
