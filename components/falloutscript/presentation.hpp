#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PRESENTATION_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_PRESENTATION_H

#include <map>
#include <mutex>

#include "interpreter.hpp"

namespace FalloutScript
{
    /// What the commands that show and play things ask of the game
    class PresentationWorld
    {
    public:
        virtual ~PresentationWorld() = default;

        /// Plays a sound record, from the reference when there is one and not placed anywhere otherwise
        virtual void playSound(FormId sound, FormId reference) = 0;
        /// Whether the sound is playing now, from the reference (the player for 0)
        virtual bool isSoundPlaying(FormId sound, FormId reference) = 0;
        virtual bool isMusicPlaying() = 0;
        /// Has the music of the game be the track of the music type record, which replaces what plays and goes back to
        /// the playlist when it ends. False when the record has no file.
        virtual bool playMusic(FormId track) = 0;
        /// The sky has the weather at once; with `hold` it keeps it, whatever the region says, until it is released
        virtual void forceWeather(FormId weather, bool hold) = 0;
        virtual void releaseWeather() = 0;
        /// How long an image space modifier lasts, in seconds; 0 when the files do not say
        virtual double imageSpaceDuration(FormId modifier) = 0;
        /// An achievement of the game was earned (the number is the id the script names)
        virtual void achievement(int id) = 0;
    };

    /// What scripts ask to be seen or looked at, kept as they asked. The games draw an image space modifier on the
    /// screen for its duration and turn a character's head towards what it looks at; nothing here draws or turns
    /// anything, so these are the answers of IsImageSpaceActive and what the host may use once it can. It can be used
    /// from any thread.
    class Presentation
    {
    public:
        /// Forgets everything, as for a new game
        void clear();

        /// Starts an image space modifier for the seconds it lasts (0 for one that lasts until it is removed)
        void applyImageSpace(FormId modifier, double seconds);
        void removeImageSpace(FormId modifier);
        bool imageSpaceActive(FormId modifier);
        /// Lets the modifiers that have a duration run down
        void update(double seconds);

        /// The actor looks at the target until told to stop
        void look(FormId actor, FormId target);
        void stopLooking(FormId actor);
        /// What the actor looks at, 0 for nothing
        FormId lookingAt(FormId actor);

    private:
        std::mutex mMutex;
        /// The seconds left, 0 or less for the ones that do not end by themselves
        std::map<FormId, double> mImageSpaces;
        std::map<FormId, FormId> mLooks;
    };

    /// Gives the commands PlaySound, PlaySound3D, IsSoundPlaying, IsMusicPlaying, PlayMusic, ForceWeather,
    /// ReleaseWeatherOverride, ApplyImageSpaceModifier, RemoveImageSpaceModifier, IsImageSpaceActive, Look, StopLook,
    /// AddAchievement and Abs to the interpreter. Those
    /// that the command table does not have are left out.
    void addPresentationCommands(Interpreter& interpreter, Presentation& presentation, PresentationWorld& world);
}

#endif
