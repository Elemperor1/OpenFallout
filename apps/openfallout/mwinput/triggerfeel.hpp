#ifndef MWINPUT_TRIGGERFEEL_H
#define MWINPUT_TRIGGERFEEL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// How the triggers of a pad feel: the resistance of a DualSense trigger that follows the weapon in the hands, and the
// buzz of the trigger motors of an Xbox pad when the weapon fires. These are the rules alone (no SDL, no game): the
// controller manager asks them what to tell the pad and tells it through a TriggerDevice.
namespace OFInput::TriggerFeel
{
    enum class Trigger
    {
        Left,
        Right,
    };

    /// How a trigger resists its pull: from where the resistance starts to where it ends (0 at rest, 1 pulled all the
    /// way) and how hard it is (0 to 1). One that ends at 1 holds to the end of the pull; one that ends before gives
    /// way there, as the break of a gun's trigger does.
    struct Resistance
    {
        float mStart = 0.f;
        float mEnd = 1.f;
        float mStrength = 0.f;

        bool operator==(const Resistance& other) const = default;
    };

    /// A short buzz of the motor of a trigger: its strength (0 to 1) for the seconds
    struct Pulse
    {
        float mStrength = 0.f;
        float mSeconds = 0.f;
    };

    /// What the rule needs of a weapon: the animation type and flags of its game data, and the rumble the record gives
    /// a shot (DNAM of Fallout 3 and New Vegas: motor strengths 0 to 1, seconds)
    struct Weapon
    {
        std::uint32_t mAnimationType = 0; // ESM4::Weapon::GameData::AnimationType
        bool mAutomatic = false;
        float mRumbleLeft = 0.f;
        float mRumbleRight = 0.f;
        float mRumbleSeconds = 0.f;
    };

    /// How the fire trigger feels with a weapon: its resistance (none for the hands, a melee weapon or one that is
    /// thrown) and the buzz of a shot
    struct Feel
    {
        std::optional<Resistance> mResistance;
        Pulse mShot;
    };

    Feel forWeapon(const Weapon& weapon);

    /// What the game says about the player, once a frame
    struct PlayerState
    {
        std::optional<Weapon> mWeapon; // the weapon in the hands; none: no weapon
        bool mWeaponDrawn = false; // out of the holster
        bool mMenuOpen = false; // a menu or another screen of the game is open
    };

    /// What the game is doing, as far as the triggers care
    struct Situation
    {
        bool mEnabled = true; // the setting for trigger effects
        bool mFocused = true; // the game window has the keyboard focus
        bool mMenu = false; // a menu or another screen is open
        bool mWeaponDrawn = false; // the player's weapon is out of its holster
    };

    /// What to tell the trigger now
    struct Command
    {
        enum class Kind
        {
            None,
            Set,
            Clear,
        };

        Kind mKind = Kind::None;
        Resistance mResistance;
    };

    /// Remembers what the trigger was told last and says what to tell it now. The resistance of the weapon is on while
    /// the weapon is out in play with the window in front; a menu, the holster or a window that loses the focus clears
    /// it. Nothing is told twice.
    class Planner
    {
    public:
        Command update(const Situation& situation, const std::optional<Resistance>& weapon);

        /// Whether a shot may buzz the trigger now (only in play, with the window in front and the weapon out)
        static bool pulseAllowed(const Situation& situation);

        /// The pad changed (another one, or none): what it was told is unknown, and the next update tells it again
        void forget();

    private:
        bool mKnown = false;
        std::optional<Resistance> mSent;
    };

    /// The effect report that SDL_GameControllerSendEffect takes for a DualSense (SDL's DS5EffectsState_t): it changes
    /// one trigger only, to the resistance or to none
    constexpr std::size_t sDualSenseEffectSize = 47;
    std::array<std::uint8_t, sDualSenseEffectSize> dualSenseEffect(
        Trigger trigger, const std::optional<Resistance>& resistance);
}

#endif
