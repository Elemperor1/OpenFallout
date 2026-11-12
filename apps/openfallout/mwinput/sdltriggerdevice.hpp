#ifndef MWINPUT_SDLTRIGGERDEVICE_H
#define MWINPUT_SDLTRIGGERDEVICE_H

#include <SDL_gamecontroller.h>

#include "triggerfeel.hpp"

namespace OFInput
{
    /// The one narrow interface to the triggers of a pad: set the resistance of a trigger, clear it, say whether the
    /// pad can do it. The buzz of a trigger's own motor (the impulse triggers of an Xbox pad) goes through it too. A
    /// pad that can not do one of them takes the call as nothing: an optional effect never stops play.
    class TriggerDevice
    {
    public:
        virtual ~TriggerDevice() = default;

        /// Whether the triggers can resist (a DualSense that takes the effect report)
        virtual bool supported() const = 0;
        virtual void setResistance(TriggerFeel::Trigger trigger, const TriggerFeel::Resistance& resistance) = 0;
        virtual void clear(TriggerFeel::Trigger trigger) = 0;

        /// Whether the triggers have motors of their own (the impulse triggers of an Xbox One or Series pad)
        virtual bool pulseSupported() const = 0;
        virtual void pulse(TriggerFeel::Trigger trigger, const TriggerFeel::Pulse& pulse) = 0;
    };

    /// The triggers through SDL, which stays the controller layer: the effect report of SDL's DualSense driver
    /// (SDL_GameControllerSendEffect) for the resistance, SDL_GameControllerRumbleTriggers for the buzz. A pad that
    /// refuses the effect report (another pad, a DualSense that SDL can not reach in full over Bluetooth) is taken as
    /// unsupported from then on.
    class SdlTriggerDevice final : public TriggerDevice
    {
    public:
        explicit SdlTriggerDevice(SDL_GameController* controller);

        bool supported() const override { return mSupported; }
        void setResistance(TriggerFeel::Trigger trigger, const TriggerFeel::Resistance& resistance) override;
        void clear(TriggerFeel::Trigger trigger) override;

        bool pulseSupported() const override { return mPulseSupported; }
        void pulse(TriggerFeel::Trigger trigger, const TriggerFeel::Pulse& pulse) override;

    private:
        void send(TriggerFeel::Trigger trigger, const std::optional<TriggerFeel::Resistance>& resistance);

        SDL_GameController* mController;
        bool mSupported;
        bool mPulseSupported;
    };
}

#endif
