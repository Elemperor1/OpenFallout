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

        /// Whether the triggers can resist (a DualSense that takes the effect report). Clear still goes to a pad that
        /// can resist no more, and is nothing on a pad that never could.
        virtual bool supported() const = 0;
        virtual void setResistance(TriggerFeel::Trigger trigger, const TriggerFeel::Resistance& resistance) = 0;
        /// False when the pad may still hold a resistance it was given (the clear did not reach it): ask again
        virtual bool clear(TriggerFeel::Trigger trigger) = 0;

        /// Whether the triggers have motors of their own (the impulse triggers of an Xbox One or Series pad)
        virtual bool pulseSupported() const = 0;
        virtual void pulse(TriggerFeel::Trigger trigger, const TriggerFeel::Pulse& pulse) = 0;
    };

    /// The triggers through SDL, which stays the controller layer: the effect report of SDL's DualSense driver
    /// (SDL_GameControllerSendEffect) for the resistance, SDL_GameControllerRumbleTriggers for the buzz. A pad that
    /// refuses the effect report (another pad, a DualSense that SDL can not reach in full over Bluetooth) is not given
    /// a resistance again, but is still cleared: a refusal may leave the resistance it had before on the trigger.
    class SdlTriggerDevice final : public TriggerDevice
    {
    public:
        /// SDL_GameControllerSendEffect, or a stand-in for it
        using SendEffect = int (*)(SDL_GameController* controller, const void* data, int size);

        explicit SdlTriggerDevice(SDL_GameController* controller);
        /// A pad whose kind is already known (resistance: a DualSense; pulse: trigger motors)
        SdlTriggerDevice(SDL_GameController* controller, bool resistance, bool pulse, SendEffect sendEffect);

        bool supported() const override { return mResistancePad && !mRefused; }
        void setResistance(TriggerFeel::Trigger trigger, const TriggerFeel::Resistance& resistance) override;
        bool clear(TriggerFeel::Trigger trigger) override;

        bool pulseSupported() const override { return mPulseSupported; }
        void pulse(TriggerFeel::Trigger trigger, const TriggerFeel::Pulse& pulse) override;

    private:
        bool send(TriggerFeel::Trigger trigger, const std::optional<TriggerFeel::Resistance>& resistance);

        SDL_GameController* mController;
        bool mResistancePad; // a DualSense: the only pad that is ever sent the effect report
        bool mRefused = false; // it refused a report: no resistance from now on, only clears
        bool mResisting = false; // it took a resistance and no clear has reached it since
        bool mPulseSupported;
        SendEffect mSendEffect;
    };

    /// Tells the trigger what the planner says for the situation. A pad that can not resist (or refused it) is only
    /// ever cleared, so a resistance it was given before it refused is taken back. A clear that did not reach a pad
    /// that resists is told again on the next call.
    void driveTriggers(TriggerFeel::Planner& planner, TriggerDevice& device, const TriggerFeel::Situation& situation,
        const std::optional<TriggerFeel::Resistance>& resistance);
}

#endif
