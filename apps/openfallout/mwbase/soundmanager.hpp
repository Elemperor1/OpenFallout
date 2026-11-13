#ifndef GAME_MWBASE_SOUNDMANAGER_H
#define GAME_MWBASE_SOUNDMANAGER_H

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include <components/esm/formid.hpp>
#include <components/falloutscript/sounddata.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwsound/type.hpp"
#include "../mwworld/ptr.hpp"

namespace OFWorld
{
    class CellStore;
}

namespace ESM
{
    class RefId;
}

namespace OFSound
{
    // Each entry excepts of MaxCount should be used only in one place
    enum BlockerType
    {
        VideoPlayback,

        MaxCount
    };

    enum class MusicType
    {
        Normal,
        MWScript
    };

    class Sound;
    class Stream;
    struct SoundDecoder;
    typedef std::shared_ptr<SoundDecoder> DecoderPtr;

    /* These must all fit together */
    enum class PlayMode
    {
        Normal = 0, /* non-looping, affected by environment */
        Loop = 1 << 0, /* Sound will continually loop until explicitly stopped */
        NoEnv = 1 << 1, /* Do not apply environment effects (eg, underwater filters) */
        RemoveAtDistance = 1 << 2, /* (3D only) If the listener gets further than 2000 units away
                                    * from the sound source, the sound is removed.
                                    * This is weird stuff but apparently how vanilla works for sounds
                                    * played by the PlayLoopSound family of script functions. Perhaps
                                    * we can make this cut off a more subtle fade later, but have to
                                    * be careful to not change the overall volume of areas by too
                                    * much. */
        NoPlayerLocal = 1 << 3, /* (3D only) Don't play the sound local to the listener even if the
                                 * player is making it. */
        NoScaling = 1 << 4, /* Don't scale audio with simulation time */
        Overlap = 1 << 10, /* Do not stop an earlier copy of the same sound that still plays (a weapon that fires
                            * faster than its shot sounds) */
        OverlapNoEnv = Overlap | NoEnv,
        NoEnvNoScaling = NoEnv | NoScaling,
        LoopNoEnv = Loop | NoEnv,
        LoopNoEnvNoScaling = Loop | NoEnv | NoScaling,
        LoopRemoveAtDistance = Loop | RemoveAtDistance
    };

    // Used for creating a type mask for SoundManager::pauseSounds and resumeSounds
    inline int operator~(Type a)
    {
        return ~static_cast<int>(a);
    }
    inline int operator&(Type a, Type b)
    {
        return static_cast<int>(a) & static_cast<int>(b);
    }
    inline int operator&(int a, Type b)
    {
        return a & static_cast<int>(b);
    }
    inline int operator|(Type a, Type b)
    {
        return static_cast<int>(a) | static_cast<int>(b);
    }
}

namespace OFBase
{
    using Sound = OFSound::Sound;
    using SoundStream = OFSound::Stream;

    /// \brief Interface for sound manager (implemented in OFSound)
    class SoundManager
    {
        SoundManager(const SoundManager&);
        ///< not implemented

        SoundManager& operator=(const SoundManager&);
        ///< not implemented

    protected:
        using PlayMode = OFSound::PlayMode;
        using Type = OFSound::Type;

        float mSimulationTimeScale = 1.0;

    public:
        SoundManager() {}
        virtual ~SoundManager() {}

        virtual void processChangedSettings(const std::set<std::pair<std::string, std::string>>& settings) = 0;

        virtual bool isEnabled() const = 0;
        ///< Returns true if sound system is enabled

        virtual void stopMusic() = 0;
        ///< Stops music if it's playing

        virtual OFSound::MusicType getMusicType() const = 0;

        virtual void streamMusic(VFS::Path::NormalizedView filename, OFSound::MusicType type, float fade = 1.f) = 0;
        ///< Play a soundifle
        /// \param filename name of a sound file in the data directory.
        /// \param type music type.
        /// \param fade time in seconds to fade out current track before start this one.

        virtual bool isMusicPlaying() = 0;
        ///< Returns true if music is playing

        /// A track of the music of a place of Fallout: played next to the music of the playlist and of scripts, with a
        /// fade of its own, as loud as the music volume says times its gain, and looping when it is told to. The
        /// layers of a New Vegas media set are several of them at once. 0 is no track.
        using MusicTrackId = std::uint32_t;
        virtual MusicTrackId startMusicTrack(VFS::Path::NormalizedView filename, float gain, float fadeIn, bool loop)
            = 0;
        /// Fades the track to the gain in the seconds; with `stopAtEnd` it stops when it has faded
        /// The loop of a sound record of Fallout (an ambient sound), as a track that the calls below follow (it is as loud as
        /// the sound effects, not the music). 0 when there is no such record, it has no file, or there is no sound.
        virtual MusicTrackId startFalloutLoop(ESM::FormId sound, float fadeIn) = 0;
        /// A track of a radio station (a song or a spoken line): the same calls follow it, and it is as loud as the radio
        /// volume says times its gain, not the music volume
        virtual MusicTrackId startRadioTrack(VFS::Path::NormalizedView filename, float gain) = 0;
        virtual void fadeMusicTrack(MusicTrackId track, float gain, float seconds, bool stopAtEnd) = 0;
        virtual bool isMusicTrackPlaying(MusicTrackId track) const = 0;
        virtual void stopMusicTrack(MusicTrackId track) = 0;

        virtual void say(const OFWorld::ConstPtr& reference, VFS::Path::NormalizedView filename,
            OFSound::Type type = OFSound::Type::Voice) = 0;
        ///< Make an actor say some text.
        /// \param filename name of a sound file in the VFS
        /// \param type the volume channel it follows (a radio that a character plays follows the radio's)

        virtual void say(VFS::Path::NormalizedView filename) = 0;
        ///< Say some text, without an actor ref
        /// \param filename name of a sound file in the VFS

        virtual bool sayActive(const OFWorld::ConstPtr& reference = OFWorld::ConstPtr()) const = 0;
        ///< Is actor not speaking?

        virtual bool sayDone(const OFWorld::ConstPtr& reference = OFWorld::ConstPtr()) const = 0;
        ///< For scripting backward compatibility

        virtual void stopSay(const OFWorld::ConstPtr& reference = OFWorld::ConstPtr()) = 0;
        ///< Stop an actor speaking

        virtual float getSaySoundLoudness(const OFWorld::ConstPtr& reference) const = 0;
        ///< Check the currently playing say sound for this actor
        /// and get an average loudness value (scale [0,1]) at the current time position.
        /// If the actor is not saying anything, returns 0.

        virtual SoundStream* playTrack(const OFSound::DecoderPtr& decoder, Type type) = 0;
        ///< Play a 2D audio track, using a custom decoder. The caller is expected to call
        /// stopTrack with the returned handle when done.

        virtual void stopTrack(SoundStream* stream) = 0;
        ///< Stop the given audio track from playing

        virtual double getTrackTimeDelay(SoundStream* stream) = 0;
        ///< Retives the time delay, in seconds, of the audio track (must be a sound
        /// returned by \ref playTrack). Only intended to be called by the track
        /// decoder's read method.

        virtual Sound* playSound(const ESM::RefId& soundId, float volume, float pitch, Type type = Type::Sfx,
            PlayMode mode = PlayMode::Normal, float offset = 0)
            = 0;
        ///< Play a sound, independently of 3D-position
        ///< @param offset Number of seconds into the sound to start playback.

        virtual Sound* playSound(VFS::Path::NormalizedView fileName, float volume, float pitch, Type type = Type::Sfx,
            PlayMode mode = PlayMode::Normal, float offset = 0)
            = 0;
        ///< Play a sound, independently of 3D-position
        ///< @param offset Number of seconds into the sound to start playback.

        virtual Sound* playSound3D(const OFWorld::ConstPtr& reference, const ESM::RefId& soundId, float volume,
            float pitch, Type type = Type::Sfx, PlayMode mode = PlayMode::Normal, float offset = 0)
            = 0;
        ///< Play a 3D sound attached to an OFWorld::Ptr. Will be updated automatically with the Ptr's position, unless
        ///< Play_NoTrack is specified.
        ///< @param offset Number of seconds into the sound to start playback.

        virtual Sound* playSound3D(const OFWorld::ConstPtr& reference, VFS::Path::NormalizedView fileName, float volume,
            float pitch, Type type = Type::Sfx, PlayMode mode = PlayMode::Normal, float offset = 0)
            = 0;
        ///< Play a 3D sound attached to an OFWorld::Ptr. Will be updated automatically with the Ptr's position, unless
        ///< Play_NoTrack is specified.
        ///< @param offset Number of seconds into the sound to start playback.

        virtual Sound* playSound3D(const osg::Vec3f& initialPos, const ESM::RefId& soundId, float volume, float pitch,
            Type type = Type::Sfx, PlayMode mode = PlayMode::Normal, float offset = 0)
            = 0;
        ///< Play a 3D sound at \a initialPos. If the sound should be moving, it must be updated using
        ///< Sound::setPosition.

        /// Play a sound record of Fallout 3 or New Vegas by its editor id (the case does not matter) or by its form,
        /// as that record says: its volume, the curve of its volume over the distance, its pitch, whether it loops or
        /// is heard as it is, and, for a record that names a folder, one of the files in it at random. With a reference
        /// the sound comes from it, and without one (an empty reference) it is heard as it is. Null when there is no
        /// such record, it has no file, or there is no sound.
        virtual Sound* playFalloutSound(std::string_view editorId, const OFWorld::ConstPtr& reference, float volume,
            Type type = Type::Sfx, PlayMode mode = PlayMode::Normal)
            = 0;
        virtual Sound* playFalloutSound(ESM::FormId sound, const OFWorld::ConstPtr& reference, float volume,
            Type type = Type::Sfx, PlayMode mode = PlayMode::Normal)
            = 0;
        /// The same, from a place in the world (a sound that the record says is two-dimensional is heard as it is)
        virtual Sound* playFalloutSoundAt(ESM::FormId sound, const osg::Vec3f& position, float volume,
            Type type = Type::Sfx, PlayMode mode = PlayMode::Normal)
            = 0;
        /// Stops the sounds of the record that play from the reference (the player for an empty one)
        virtual void stopFalloutSound(ESM::FormId sound, const OFWorld::ConstPtr& reference) = 0;
        /// The settings of the sound record of Fallout with this form (its flags, volume and distances); none when the
        /// record does not exist or has no file to play
        virtual std::optional<FalloutScript::SoundData> falloutSoundData(ESM::FormId sound) = 0;
        /// Whether a sound record of Fallout has this editor id and files to play
        virtual bool hasFalloutSound(std::string_view editorId) = 0;

        virtual void stopSound(Sound* sound) = 0;
        ///< Stop the given sound from playing

        virtual void stopSound3D(const OFWorld::ConstPtr& reference, const ESM::RefId& soundId) = 0;
        ///< Stop the given object from playing the given sound.

        virtual void stopSound3D(const OFWorld::ConstPtr& reference, VFS::Path::NormalizedView fileName) = 0;
        ///< Stop the given object from playing the given sound.

        virtual void stopSound3D(const OFWorld::ConstPtr& reference) = 0;
        ///< Stop the given object from playing all sounds.

        virtual void stopSound(const OFWorld::CellStore* cell) = 0;
        ///< Stop all sounds for the given cell.

        virtual void fadeOutSound3D(const OFWorld::ConstPtr& reference, const ESM::RefId& soundId, float duration) = 0;
        ///< Fade out given sound (that is already playing) of given object
        ///< @param reference Reference to object, whose sound is faded out
        ///< @param soundId ID of the sound to fade out.
        ///< @param duration Time until volume reaches 0.

        virtual bool getSoundPlaying(const OFWorld::ConstPtr& reference, const ESM::RefId& soundId) const = 0;
        ///< Is the given sound currently playing on the given object?
        ///  If you want to check if sound played with playSound is playing, use empty Ptr

        virtual bool getSoundPlaying(const OFWorld::ConstPtr& reference, VFS::Path::NormalizedView fileName) const = 0;
        ///< Is the given sound currently playing on the given object?
        ///  If you want to check if sound played with playSound is playing, use empty Ptr

        virtual void pauseSounds(OFSound::BlockerType blocker, int types = int(Type::Mask)) = 0;
        ///< Pauses all currently playing sounds, including music.

        virtual void resumeSounds(OFSound::BlockerType blocker) = 0;
        ///< Resumes all previously paused sounds.

        virtual void pausePlayback() = 0;
        virtual void resumePlayback() = 0;

        virtual void setListenerPosDir(
            const osg::Vec3f& pos, const osg::Vec3f& dir, const osg::Vec3f& up, bool underwater)
            = 0;

        virtual void setListenerVel(const osg::Vec3f& vel) = 0;

        /// Where the listener is (the camera)
        virtual osg::Vec3f getListenerPosition() const = 0;

        virtual void updatePtr(const OFWorld::ConstPtr& old, const OFWorld::ConstPtr& updated) = 0;

        void setSimulationTimeScale(float scale) { mSimulationTimeScale = scale; }
        float getSimulationTimeScale() const { return mSimulationTimeScale; }

        virtual void clear() = 0;
    };
}

#endif
