#ifndef OPENFALLOUT_COMPONENTS_SDLUTIL_DESKTOPCONVENTIONS_H
#define OPENFALLOUT_COMPONENTS_SDLUTIL_DESKTOPCONVENTIONS_H

#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_video.h>

#include <components/settings/windowmode.hpp>

#include <optional>
#include <set>

namespace SDLUtil
{
    /// Whose desktop conventions apply. The rules below take it as a parameter so that both sides are unit-tested;
    /// on Other (Windows, Linux) each rule is what the engine did before the macOS conventions were added.
    enum class DesktopPlatform
    {
        Mac,
        Other,
    };

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
    /// A window that reports no size (minimised, hidden) counts as 1. On the Mac the ratio is kept as it is (1.5 with
    /// a scaled display); elsewhere it is the whole number of the integer division, as before.
    float pixelsPerPoint(int windowPoints, int drawablePixels, DesktopPlatform platform);

    /// Exact for the whole-number ratios of Other
    int pointsToPixels(int points, float pixelsPerPoint);
    /// On the Mac rounded; elsewhere the integer division by the whole-number ratio, as before
    int pixelsToPoints(int pixels, float pixelsPerPoint, DesktopPlatform platform);

    /// The factor from interface units to drawable pixels: the user's interface scale in points, so that the interface
    /// keeps its size in points when the window moves to a display of another scaling.
    float interfaceScale(float userScale, int windowPoints, int drawablePixels, DesktopPlatform platform);

    /// The interface scale after the drawable was resized, when it changes: on the Mac the interface is rescaled when
    /// the window moves to a display of another scaling; elsewhere it keeps the scale it started with (nullopt).
    std::optional<float> interfaceScaleAfterResize(
        float userScale, int windowPoints, int drawablePixels, float current, DesktopPlatform platform);

    /// The window mode the game itself goes to on Control-Command-F, for the SDL flags of the window; nullopt when
    /// SDL's menu bar item Toggle Full Screen does it (a resizable window outside SDL's full-screen modes) and on other
    /// systems. SDL greys that item out while the window is in one of its full-screen modes, or is not resizable.
    std::optional<Settings::WindowMode> windowModeForToggleFullScreen(Uint32 windowFlags, DesktopPlatform platform);

    /// The SDL window flag for a window mode. With the system's full-screen Spaces (macOS), both full-screen modes go
    /// to a Space of their own: exclusive full screen would change the display mode and keep the window out of Mission
    /// Control, the green button and Control-Command-F.
    Uint32 fullScreenFlag(Settings::WindowMode mode, bool useSpaces);

    /// A size in drawable pixels, as the resolution setting
    struct WindowSize
    {
        int width = 0;
        int height = 0;

        friend bool operator==(const WindowSize&, const WindowSize&) = default;
    };

    /// The size of the game's window on the desktop after the drawable was resized, given the size it had and the SDL
    /// flags of the window. The resolution setting follows the drawable (the picture is drawn at it), so on the Mac it
    /// holds the size of the Space while the window fills one; the window keeps its own size through that, to go back
    /// to. Elsewhere the size of the window is the resolution setting, as before.
    WindowSize windowSizeAfterResize(
        WindowSize window, WindowSize drawable, Uint32 windowFlags, DesktopPlatform platform);

    /// The size to set a window mode with, from the resolution setting and the size of the window on the desktop: on
    /// the Mac a window that leaves full screen comes back at its own size, not at the size of the Space.
    WindowSize sizeForWindowMode(
        WindowSize setting, WindowSize window, Settings::WindowMode mode, DesktopPlatform platform);

    /// The resolution to save for the next start: on the Mac the size of the window on the desktop when the game is
    /// quit in full screen, so that the window does not open as large as the screen the next time it is one.
    WindowSize resolutionToSave(WindowSize setting, WindowSize window, Uint32 windowFlags, DesktopPlatform platform);

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

    /// The mouse capture for a request. Only a window with focus and the pointer in it captures. On the Mac a
    /// full-screen window always has the pointer in it, so leaving and re-entering a full-screen Space or switching
    /// apps and back restores the capture as soon as focus comes back, whatever enter or leave events the transition
    /// sent; elsewhere the last enter or leave event decides, as before.
    MouseCapture decideMouseCapture(const MouseCaptureRequest& request, DesktopPlatform platform);

    /// What the input wrapper does on a window event about the drawable
    struct WindowEventResponse
    {
        bool resizeDrawable = false; // tell the graphics context, the event queue and the window listener the new size
        bool scaleBeforeListener = false; // update the pointer scale before the window listener hears of the size
        bool updateMouseCapture = false; // decide the mouse capture again (it depends on full screen)
    };

    /// SDL_WINDOWEVENT_SIZE_CHANGED resizes everywhere; on the Mac it also updates the capture (entering or leaving
    /// full screen changes the size), and SDL_WINDOWEVENT_DISPLAY_CHANGED (another display, maybe another scaling)
    /// resizes too. Other events: nothing here.
    WindowEventResponse respondToWindowEvent(Uint8 windowEvent, DesktopPlatform platform);

    /// Turns the precise deltas of wheel events (a mouse wheel gives whole notches, a trackpad gives many small
    /// fractions) into whole steps of `unitsPerNotch` to a notch, keeping the rest for the next event, so that a
    /// trackpad and a mouse wheel drive the same scroll path at the same speed.
    ///
    /// The direction of an event (SDL_MOUSEWHEEL_FLIPPED when the system's natural scrolling is on) is not applied:
    /// SDL's values already follow the system setting, and the flag only says how they differ from the device.
    /// On the Mac turning the other way drops what was left over of the old way; elsewhere it is kept, as before.
    class WheelAccumulator
    {
    public:
        WheelAccumulator(double unitsPerNotch, DesktopPlatform platform);

        /// Whole units this event moves by
        int add(float precise, Uint32 direction);

    private:
        double mUnitsPerNotch;
        DesktopPlatform mPlatform;
        double mPending = 0.0;
    };

    /// The whole notches of the wheel that the interface's wheel events carry (Lua `MouseWheelEvent.notches`). On the
    /// Mac they are counted from the precise deltas (a trackpad sends many small ones); elsewhere every event with a
    /// delta is at least one notch in its direction, so a Fallout menu moves a row per event, as before.
    class WheelNotches
    {
    public:
        explicit WheelNotches(DesktopPlatform platform);

        int add(float precise, Uint32 direction);

    private:
        DesktopPlatform mPlatform;
        WheelAccumulator mAccumulator;
    };

    // The platform bridge: what this platform's desktop does. Everything above is the same on every platform; only
    // these say whether it applies.

    /// Mac on macOS, Other elsewhere
    DesktopPlatform thisDesktopPlatform();

    /// True on macOS: Command shortcuts are the desktop's and full screen is a Space of its own
    bool followsMacDesktopConventions();

    /// Sets the SDL hints for this desktop; call before the window is made
    void applyDesktopHints();
}

#endif
