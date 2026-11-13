#include "triggerfeel.hpp"

#include <algorithm>
#include <cmath>

namespace OFInput::TriggerFeel
{
    namespace
    {
        // The animation types of the weapon records (ESM4::Weapon::GameData::AnimationType)
        enum AnimationType : std::uint32_t
        {
            pistolBallistic = 3,
            pistolEnergy = 4,
            rifleBallistic = 5,
            rifleAutomatic = 6,
            rifleEnergy = 7,
            handle2Hand = 8,
            launcher2Hand = 9,
        };

        // A weapon that fires as long as the trigger is held resists lightly to the end of the pull, so that holding it
        // does not tire the finger
        constexpr Resistance held{ 0.2f, 1.f, 0.35f };

        // The longest and shortest buzz of a shot: a record that asks for more would hum through the next shots
        constexpr float longestShot = 0.5f;
        constexpr float shortestShot = 0.03f;
    }

    Feel forWeapon(const Weapon& weapon)
    {
        // A gun's trigger gives way at its break, heavier for a rifle than for a pistol and heaviest for a launcher.
        // The hands, a melee weapon and what is thrown or laid have no trigger to feel.
        Feel feel;
        switch (weapon.mAnimationType)
        {
            case pistolBallistic:
                feel = { Resistance{ 0.3f, 0.6f, 0.6f }, Pulse{ 0.5f, 0.08f } };
                break;
            case pistolEnergy:
                feel = { Resistance{ 0.25f, 0.55f, 0.45f }, Pulse{ 0.4f, 0.06f } };
                break;
            case rifleBallistic:
                feel = { Resistance{ 0.35f, 0.7f, 0.8f }, Pulse{ 0.7f, 0.1f } };
                break;
            case rifleEnergy:
                feel = { Resistance{ 0.3f, 0.65f, 0.6f }, Pulse{ 0.5f, 0.08f } };
                break;
            case rifleAutomatic:
                feel = { held, Pulse{ 0.4f, 0.05f } };
                break;
            case handle2Hand: // a minigun, a flamer, a gatling laser
                feel = { Resistance{ 0.15f, 1.f, 0.5f }, Pulse{ 0.6f, 0.06f } };
                break;
            case launcher2Hand:
                feel = { Resistance{ 0.4f, 0.8f, 1.f }, Pulse{ 1.f, 0.2f } };
                break;
            default:
                feel = { std::nullopt, Pulse{ 0.3f, 0.06f } };
                break;
        }
        if (weapon.mAutomatic && feel.mResistance && feel.mResistance->mEnd < 1.f)
            feel.mResistance = held;
        // The record's own rumble for a shot, when it has one, is what the trigger motor does
        const float strength = std::max(weapon.mRumbleLeft, weapon.mRumbleRight);
        if (strength > 0.f && weapon.mRumbleSeconds > 0.f)
            feel.mShot = Pulse{ std::min(strength, 1.f), std::clamp(weapon.mRumbleSeconds, shortestShot, longestShot) };
        return feel;
    }

    Command Planner::update(const Situation& situation, const std::optional<Resistance>& weapon)
    {
        std::optional<Resistance> wanted = pulseAllowed(situation) ? weapon : std::nullopt;
        if (wanted && wanted->mStrength <= 0.f)
            wanted.reset();
        if (mKnown && wanted == mSent)
            return {};
        // A pad that was told nothing yet is told even that there is nothing: another game may have left it resisting
        mKnown = true;
        mSent = wanted;
        Command command;
        command.mKind = wanted ? Command::Kind::Set : Command::Kind::Clear;
        if (wanted)
            command.mResistance = *wanted;
        return command;
    }

    bool Planner::pulseAllowed(const Situation& situation)
    {
        return situation.mEnabled && situation.mControllerEnabled && situation.mFocused && !situation.mMenu
            && situation.mWeaponDrawn;
    }

    void Planner::forget()
    {
        mKnown = false;
        mSent.reset();
    }

    std::array<std::uint8_t, sDualSenseEffectSize> dualSenseEffect(
        Trigger trigger, const std::optional<Resistance>& resistance)
    {
        // SDL's DS5EffectsState_t: the bits of what the report changes at 0, the effect of the right trigger at 10 and
        // of the left one at 21, eleven bytes each (a mode, then its parameters)
        constexpr std::uint8_t rightTriggerBit = 0x04;
        constexpr std::uint8_t leftTriggerBit = 0x08;
        constexpr std::size_t rightTriggerEffect = 10;
        constexpr std::size_t leftTriggerEffect = 21;
        // The modes that SDL's testgamecontroller and the reverse engineering of the pad name: off, resistance from a
        // position to the end of the pull, resistance between two positions
        constexpr std::uint8_t modeOff = 0x05;
        constexpr std::uint8_t modeFromPosition = 0x01;
        constexpr std::uint8_t modeSection = 0x02;
        const auto byteOf
            = [](float value) { return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.f, 1.f) * 255.f)); };

        std::array<std::uint8_t, sDualSenseEffectSize> report{};
        const bool right = trigger == Trigger::Right;
        report[0] = right ? rightTriggerBit : leftTriggerBit;
        std::uint8_t* effect = report.data() + (right ? rightTriggerEffect : leftTriggerEffect);
        if (!resistance || resistance->mStrength <= 0.f)
            effect[0] = modeOff;
        else if (resistance->mEnd >= 1.f)
        {
            effect[0] = modeFromPosition;
            effect[1] = byteOf(resistance->mStart);
            effect[2] = byteOf(resistance->mStrength);
        }
        else
        {
            effect[0] = modeSection;
            effect[1] = byteOf(resistance->mStart);
            effect[2] = byteOf(resistance->mEnd);
            effect[3] = byteOf(resistance->mStrength);
        }
        return report;
    }
}
