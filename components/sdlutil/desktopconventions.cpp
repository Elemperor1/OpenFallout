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

    WheelAccumulator::WheelAccumulator(double unitsPerNotch)
        : mUnitsPerNotch(unitsPerNotch)
    {
    }

    int WheelAccumulator::add(float precise, Uint32 /*direction*/)
    {
        const double units = static_cast<double>(precise) * mUnitsPerNotch;
        if ((units > 0 && mPending < 0) || (units < 0 && mPending > 0))
            mPending = 0.0;
        mPending += units;
        const int whole = static_cast<int>(mPending);
        mPending -= whole;
        return whole;
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

    float pixelsPerPoint(int windowPoints, int drawablePixels)
    {
        if (windowPoints <= 0 || drawablePixels <= 0)
            return 1.f;
        return static_cast<float>(drawablePixels) / static_cast<float>(windowPoints);
    }

    int pointsToPixels(int points, float pixelsPerPoint)
    {
        return static_cast<int>(std::lround(points * pixelsPerPoint));
    }

    int pixelsToPoints(int pixels, float pixelsPerPoint)
    {
        if (pixelsPerPoint <= 0.f)
            return pixels;
        return static_cast<int>(std::lround(pixels / pixelsPerPoint));
    }

    float interfaceScale(float userScale, int windowPoints, int drawablePixels)
    {
        return userScale * pixelsPerPoint(windowPoints, drawablePixels);
    }

    MouseCapture decideMouseCapture(const MouseCaptureRequest& request)
    {
        const bool pointerHere = request.hasFocus && (request.mouseInWindow || request.fullScreen);
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

    bool followsMacDesktopConventions()
    {
#ifdef __APPLE__
        return true;
#else
        return false;
#endif
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
