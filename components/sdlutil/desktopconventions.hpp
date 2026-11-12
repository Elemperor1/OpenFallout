#ifndef OPENFALLOUT_COMPONENTS_SDLUTIL_DESKTOPCONVENTIONS_H
#define OPENFALLOUT_COMPONENTS_SDLUTIL_DESKTOPCONVENTIONS_H

#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_video.h>

#include <components/settings/windowmode.hpp>

#include <set>

namespace SDLUtil
{
    /// A key combination the desktop answers itself (on macOS through the menu bar SDL puts up: the application and
    /// Window menus). The game must not also see it as a key press of its own.
    enum class DesktopShortcut
    {
        None,
        Quit, // Command-Q
        Hide, // Command-H
        Minimise, // Command-M
        CloseWindow, // Command-W
        ToggleFullScreen, // Control-Command-F
    };

    /// The macOS standard shortcut that a key with these modifiers is, or None. Lock keys (Caps, Num) are ignored;
    /// any other modifier beyond the ones the shortcut uses makes it a different combination.
    DesktopShortcut desktopShortcut(SDL_Keycode key, Uint16 mod);

    /// Keeps the key presses and releases of desktop shortcuts from the game: the menu bar acts on them, the game
    /// would take Command-W for a step forward. A key the game already holds stays the game's, and a release that never
    /// came (the window went to another Space) does not take the next press of that key.
    class DesktopShortcutFilter
    {
    public:
        explicit DesktopShortcutFilter(bool enabled);

        bool swallowKeyDown(SDL_Keycode key, Uint16 mod, bool repeat);
        bool swallowKeyUp(SDL_Keycode key);

    private:
        bool mEnabled;
        std::set<SDL_Keycode> mSwallowed;
    };

    /// How many drawable pixels make one window point along an axis (2 on a Retina display, 1 on most others).
    /// A window that reports no size (minimised, hidden) counts as 1.
    float pixelsPerPoint(int windowPoints, int drawablePixels);

    int pointsToPixels(int points, float pixelsPerPoint);
    int pixelsToPoints(int pixels, float pixelsPerPoint);

    /// The factor from interface units to drawable pixels: the user's interface scale in points, so that the interface
    /// keeps its size in points when the window moves to a display of another scaling.
    float interfaceScale(float userScale, int windowPoints, int drawablePixels);

    /// The SDL window flag for a window mode. With the system's full-screen Spaces (macOS), both full-screen modes go
    /// to a Space of their own: exclusive full screen would change the display mode and keep the window out of Mission
    /// Control, the green button and Control-Command-F.
    Uint32 fullScreenFlag(Settings::WindowMode mode, bool useSpaces);

    /// What the game asks of the mouse, and the state of the window it is in
    struct MouseCaptureRequest
    {
        bool wantGrab = false;
        bool wantRelative = false;
        bool wantCursorVisible = false;
        bool allowGrab = true; // false with the no-grab option
        bool hasFocus = false;
        bool mouseInWindow = false;
        bool fullScreen = false;
    };

    struct MouseCapture
    {
        bool grab = false; // the pointer is held by the game
        bool windowGrab = false; // the window confines the pointer
        bool relative = false; // relative mouse mode (mouse look)
        bool cursorVisible = true;
    };

    /// The mouse capture for a request. Only a window with focus captures; a full-screen window always has the pointer
    /// in it, so leaving and re-entering a full-screen Space or switching apps and back restores the capture as soon as
    /// focus comes back, whatever enter or leave events the transition sent.
    MouseCapture decideMouseCapture(const MouseCaptureRequest& request);

    /// Turns the precise deltas of wheel events (a mouse wheel gives whole notches, a trackpad gives many small
    /// fractions) into whole steps of `unitsPerNotch` to a notch, keeping the rest for the next event, so that a
    /// trackpad and a mouse wheel drive the same scroll path at the same speed.
    ///
    /// The direction of an event (SDL_MOUSEWHEEL_FLIPPED when the system's natural scrolling is on) is not applied:
    /// SDL's values already follow the system setting, and the flag only says how they differ from the device.
    /// Turning the other way drops what was left over of the old way.
    class WheelAccumulator
    {
    public:
        explicit WheelAccumulator(double unitsPerNotch);

        /// Whole units this event moves by
        int add(float precise, Uint32 direction);

    private:
        double mUnitsPerNotch;
        double mPending = 0.0;
    };

    // The platform bridge: what this platform's desktop does. Everything above is the same on every platform; only
    // these say whether it applies.

    /// True on macOS: Command shortcuts are the desktop's and full screen is a Space of its own
    bool followsMacDesktopConventions();

    /// Sets the SDL hints for this desktop; call before the window is made
    void applyDesktopHints();
}

#endif
