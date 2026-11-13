#include "sdltriggerdevice.hpp"

#include <algorithm>

#include <SDL_error.h>

#include <components/debug/debuglog.hpp>

namespace OFInput
{
    SdlTriggerDevice::SdlTriggerDevice(SDL_GameController* controller)
        : SdlTriggerDevice(controller,
            controller != nullptr && SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5,
            controller != nullptr && SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE,
            &SDL_GameControllerSendEffect)
    {
        if (controller == nullptr)
            return;
        const char* name = SDL_GameControllerName(controller);
        Log(Debug::Info) << "Pad triggers of " << (name != nullptr ? name : "a pad without a name") << ": resistance "
                         << (mResistancePad ? "yes" : "no") << ", trigger rumble " << (mPulseSupported ? "yes" : "no");
    }

    SdlTriggerDevice::SdlTriggerDevice(
        SDL_GameController* controller, bool resistance, bool pulse, SendEffect sendEffect)
        : mController(controller)
        , mResistancePad(controller != nullptr && resistance)
        , mPulseSupported(controller != nullptr && pulse)
        , mSendEffect(sendEffect)
    {
    }

    void SdlTriggerDevice::send(TriggerFeel::Trigger trigger, const std::optional<TriggerFeel::Resistance>& resistance)
    {
        if (!mResistancePad)
            return;
        const auto report = TriggerFeel::dualSenseEffect(trigger, resistance);
        if (mSendEffect(mController, report.data(), static_cast<int>(report.size())) == 0)
            return;
        // A pad that refuses it once is given no resistance again (the game plays on without the effect), but is
        // still cleared, as it may keep the resistance it had
        if (!mRefused)
            Log(Debug::Warning) << "Pad triggers: the resistance is not available (" << SDL_GetError() << ")";
        mRefused = true;
    }

    void SdlTriggerDevice::setResistance(TriggerFeel::Trigger trigger, const TriggerFeel::Resistance& resistance)
    {
        if (!mRefused)
            send(trigger, resistance);
    }

    void SdlTriggerDevice::clear(TriggerFeel::Trigger trigger)
    {
        send(trigger, std::nullopt);
    }

    void driveTriggers(TriggerFeel::Planner& planner, TriggerDevice& device, const TriggerFeel::Situation& situation,
        const std::optional<TriggerFeel::Resistance>& resistance)
    {
        const TriggerFeel::Command command = planner.update(situation, device.supported() ? resistance : std::nullopt);
        switch (command.mKind)
        {
            case TriggerFeel::Command::Kind::Set:
                device.setResistance(TriggerFeel::Trigger::Right, command.mResistance);
                break;
            case TriggerFeel::Command::Kind::Clear:
                device.clear(TriggerFeel::Trigger::Right);
                break;
            case TriggerFeel::Command::Kind::None:
                break;
        }
    }

    void SdlTriggerDevice::pulse(TriggerFeel::Trigger trigger, const TriggerFeel::Pulse& pulse)
    {
        if (!mPulseSupported || pulse.mStrength <= 0.f || pulse.mSeconds <= 0.f)
            return;
        const auto motor = static_cast<Uint16>(std::clamp(pulse.mStrength, 0.f, 1.f) * 65535.f);
        const auto milliseconds = static_cast<Uint32>(std::clamp(pulse.mSeconds, 0.f, 10.f) * 1000.f);
        const Uint16 left = trigger == TriggerFeel::Trigger::Left ? motor : 0;
        const Uint16 right = trigger == TriggerFeel::Trigger::Right ? motor : 0;
        SDL_GameControllerRumbleTriggers(mController, left, right, milliseconds);
    }
}
