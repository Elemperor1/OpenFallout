#include "presentation.hpp"

#include <algorithm>
#include <cmath>

#include "arguments.hpp"

namespace FalloutScript
{
    void Presentation::clear()
    {
        const std::lock_guard lock(mMutex);
        mImageSpaces.clear();
        mLooks.clear();
    }

    void Presentation::applyImageSpace(FormId modifier, double seconds)
    {
        const std::lock_guard lock(mMutex);
        // Once more starts it again
        mImageSpaces[modifier] = seconds > 0 ? seconds : 0.0;
    }

    void Presentation::removeImageSpace(FormId modifier)
    {
        const std::lock_guard lock(mMutex);
        mImageSpaces.erase(modifier);
    }

    bool Presentation::imageSpaceActive(FormId modifier)
    {
        const std::lock_guard lock(mMutex);
        return mImageSpaces.count(modifier) != 0;
    }

    void Presentation::update(double seconds)
    {
        const std::lock_guard lock(mMutex);
        for (auto it = mImageSpaces.begin(); it != mImageSpaces.end();)
        {
            // A value of 0 is one that lasts until it is removed
            if (it->second > 0 && (it->second -= seconds) <= 0)
                it = mImageSpaces.erase(it);
            else
                ++it;
        }
    }

    void Presentation::look(FormId actor, FormId target)
    {
        const std::lock_guard lock(mMutex);
        mLooks[actor] = target;
    }

    void Presentation::stopLooking(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        mLooks.erase(actor);
    }

    FormId Presentation::lookingAt(FormId actor)
    {
        const std::lock_guard lock(mMutex);
        const auto found = mLooks.find(actor);
        return found == mLooks.end() ? 0 : found->second;
    }

    void addPresentationCommands(Interpreter& interpreter, Presentation& presentation, PresentationWorld& world)
    {
        // The sound of a script plays on its own, and the reference the command is called on is where it comes from
        interpreter.setHandler("PlaySound", [&world](CallContext& call) {
            if (const FormId sound = formArgument(call, 0); sound != 0)
                world.playSound(sound, 0);
            return 0.0;
        });
        interpreter.setHandler("PlaySound3D", [&world](CallContext& call) {
            if (const FormId sound = formArgument(call, 0); sound != 0)
                world.playSound(sound, call.mReference);
            return 0.0;
        });
        // The commands of JIP LN NVSE that the intro of Tale of Two Wastelands asks the sound with
        interpreter.setHandler("IsSoundPlaying", [&world](CallContext& call) {
            const FormId sound = formArgument(call, 0);
            // The reference the sound comes from is the second argument; without it the one the command is called on
            FormId source = formArgument(call, 1);
            if (source == 0)
                source = call.mReference;
            return sound != 0 && world.isSoundPlaying(sound, source) ? 1.0 : 0.0;
        });
        interpreter.setHandler("IsMusicPlaying", [&world](CallContext&) { return world.isMusicPlaying() ? 1.0 : 0.0; });
        interpreter.setHandler("PlayMusic", [&world](CallContext& call) {
            if (const FormId track = formArgument(call, 0); track != 0)
                world.playMusic(track);
            return 0.0;
        });
        // The weather comes at once. With a second argument that is not 0 it also stays, whatever the region says,
        // until ReleaseWeatherOverride (the executable sets the weather of the sky in the one case and its override in
        // the other).
        interpreter.setHandler("ForceWeather", [&world](CallContext& call) {
            if (const FormId weather = formArgument(call, 0); weather != 0)
                world.forceWeather(weather, intArgument(call, 1) != 0);
            return 0.0;
        });
        interpreter.setHandler("ReleaseWeatherOverride", [&world](CallContext&) {
            world.releaseWeather();
            return 0.0;
        });
        interpreter.setHandler("ApplyImageSpaceModifier", [&presentation, &world](CallContext& call) {
            if (const FormId modifier = formArgument(call, 0); modifier != 0)
                presentation.applyImageSpace(modifier, world.imageSpaceDuration(modifier));
            return 0.0;
        });
        interpreter.setHandler("RemoveImageSpaceModifier", [&presentation](CallContext& call) {
            presentation.removeImageSpace(formArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("IsImageSpaceActive", [&presentation](CallContext& call) {
            return presentation.imageSpaceActive(formArgument(call, 0)) ? 1.0 : 0.0;
        });
        interpreter.setHandler("Look", [&presentation](CallContext& call) {
            if (const FormId target = formArgument(call, 0); call.mReference != 0 && target != 0)
                presentation.look(call.mReference, target);
            return 0.0;
        });
        interpreter.setHandler("StopLook", [&presentation](CallContext& call) {
            if (call.mReference != 0)
                presentation.stopLooking(call.mReference);
            return 0.0;
        });
        interpreter.setHandler("AddAchievement", [&world](CallContext& call) {
            world.achievement(intArgument(call, 0));
            return 0.0;
        });
        interpreter.setHandler("Abs", [](CallContext& call) {
            const double value = argument(call, 0);
            return std::isfinite(value) ? std::abs(value) : 0.0;
        });
    }
}
