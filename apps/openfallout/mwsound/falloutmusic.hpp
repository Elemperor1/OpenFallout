#ifndef OPENFALLOUT_MWSOUND_FALLOUTMUSIC_H
#define OPENFALLOUT_MWSOUND_FALLOUTMUSIC_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwbase/soundmanager.hpp"
#include "sounddecoder.hpp"

namespace ESM4
{
    struct Music;
}

namespace VFS
{
    class Manager;
}

namespace OFWorld
{
    class ESMStore;
}

namespace OFSound
{
    /// A decoder that starts the file again when it has been read to its end, so that a stream plays it for as long
    /// as it plays at all (the streams of the sound output cannot loop). The decoder it is given has the file open.
    class LoopingDecoder final : public SoundDecoder
    {
    public:
        LoopingDecoder(std::shared_ptr<SoundDecoder> inner, VFS::Path::NormalizedView path);

        void open(VFS::Path::NormalizedView fname) override;
        void close() override;
        std::string getName() override;
        void getInfo(int* samplerate, ChannelConfig* chans, SampleType* type) override;
        size_t read(char* buffer, size_t bytes) override;
        size_t getSampleOffset() override;

    private:
        std::shared_ptr<SoundDecoder> mInner;
        VFS::Path::Normalized mPath;
        bool mReadSomething = false;
    };

    /// The tracks that a music record names. A name that ends in a backslash is a folder: the executable plays the mp3
    /// files in it (not those of the folders below it, FalloutNV.exe 0x5917B0); any other name is a file of the music
    /// folder (or the mp3 that a file of another kind was converted to). Empty when the files have none.
    std::vector<VFS::Path::Normalized> falloutMusicFiles(std::string_view name, const VFS::Manager& vfs);

    /// A music record as it is played
    struct FalloutMusic
    {
        ESM::FormId mId;
        std::string mEditorId;
        std::vector<VFS::Path::Normalized> mFiles;
        bool mFolder = false;
        /// How much quieter the track is, in decibels (never above 0), and whether it loops
        float mDb = 0.f;
        bool mLoop = false;
        /// The track played last, for the pick of the next (see FalloutScript::Music::nextTrack)
        std::size_t mLast = 0;
        bool mStarted = false;
    };

    /// The tracks that play together as the music of a New Vegas place: the layers of a location set, which keep
    /// playing in step with each other while the one that is wanted is the one that is heard. Each update says which
    /// files should play and how loud; a file that is not wanted is faded out and stopped, a file that is new is
    /// started, a gain that changed is faded to.
    class MusicMixer
    {
    public:
        struct Stem
        {
            VFS::Path::Normalized mFile;
            float mGain = 1.f;
            bool mLoop = true;
        };

        /// (the sound is the sound manager, or anything that starts, fades and asks after tracks as it does)
        template <class Sound>
        void update(Sound& sound, const std::vector<Stem>& wanted, float fadeSeconds)
        {
            const auto isWanted = [&wanted](const VFS::Path::Normalized& file) {
                return std::find_if(wanted.begin(), wanted.end(), [&file](const Stem& stem) { return stem.mFile == file; });
            };
            // What was unwanted for a while may start again
            mDone.erase(std::remove_if(
                            mDone.begin(), mDone.end(), [&](const auto& file) { return isWanted(file) == wanted.end(); }),
                mDone.end());
            for (auto it = mPlaying.begin(); it != mPlaying.end();)
            {
                const auto stem = isWanted(it->mFile);
                if (!sound.isMusicTrackPlaying(it->mId))
                {
                    if (stem != wanted.end())
                    {
                        mDone.push_back(it->mFile);
                        mEnded = true;
                    }
                    it = mPlaying.erase(it);
                    continue;
                }
                if (stem == wanted.end())
                {
                    sound.fadeMusicTrack(it->mId, 0.f, fadeSeconds, true);
                    mFading.push_back(it->mId);
                    it = mPlaying.erase(it);
                    continue;
                }
                if (stem->mLoop != it->mLoop)
                {
                    // The stream cannot change whether it loops: it is faded out, and started again below as the stem wants
                    sound.fadeMusicTrack(it->mId, 0.f, fadeSeconds, true);
                    mFading.push_back(it->mId);
                    it = mPlaying.erase(it);
                    continue;
                }
                if (std::abs(stem->mGain - it->mGain) > 0.002f)
                {
                    sound.fadeMusicTrack(it->mId, stem->mGain, fadeSeconds, false);
                    it->mGain = stem->mGain;
                }
                ++it;
            }
            for (const Stem& stem : wanted)
            {
                const auto same = [&stem](const auto& other) { return other.mFile == stem.mFile; };
                if (std::any_of(mPlaying.begin(), mPlaying.end(), same)
                    || std::find(mDone.begin(), mDone.end(), stem.mFile) != mDone.end())
                    continue;
                const OFBase::SoundManager::MusicTrackId id
                    = sound.startMusicTrack(stem.mFile, stem.mGain, fadeSeconds, stem.mLoop);
                if (id == 0)
                {
                    mDone.push_back(stem.mFile);
                    continue;
                }
                mPlaying.push_back({ stem.mFile, id, stem.mGain, stem.mLoop });
            }
        }
        /// Fades everything out
        template <class Sound>
        void stopAll(Sound& sound, float fadeSeconds)
        {
            for (const Playing& playing : mPlaying)
            {
                sound.fadeMusicTrack(playing.mId, 0.f, fadeSeconds, true);
                mFading.push_back(playing.mId);
            }
            mPlaying.clear();
            mDone.clear();
            mEnded = false;
        }
        /// Whether a stem plays that is not on its way out
        template <class Sound>
        bool playing(Sound& sound)
        {
            return std::any_of(mPlaying.begin(), mPlaying.end(),
                [&sound](const Playing& p) { return sound.isMusicTrackPlaying(p.mId); });
        }
        /// Whether anything of the mixer is heard: a stem that plays, or one that was faded out and still fades
        template <class Sound>
        bool audible(Sound& sound)
        {
            mFading.erase(std::remove_if(mFading.begin(), mFading.end(),
                              [&sound](OFBase::SoundManager::MusicTrackId id) { return !sound.isMusicTrackPlaying(id); }),
                mFading.end());
            return !mFading.empty() || playing(sound);
        }
        /// Whether the stems that were wanted have all played to their end and none plays now (a set that does not loop is
        /// over, and the controller picks one again)
        bool finished() const { return mPlaying.empty() && mEnded; }
        /// Lets the stems that ended play again
        void restart()
        {
            mDone.clear();
            mEnded = false;
        }
        /// Forgets what plays, without touching it (the sound manager was cleared)
        void forget();
        std::size_t size() const { return mPlaying.size(); }

    private:
        struct Playing
        {
            VFS::Path::Normalized mFile;
            OFBase::SoundManager::MusicTrackId mId = 0;
            float mGain = 0.f;
            bool mLoop = true;
        };
        std::vector<Playing> mPlaying;
        /// The tracks that were faded out (and stop at the end of the fade): they are not the mixer's stems any more, but
        /// they are heard until then
        std::vector<OFBase::SoundManager::MusicTrackId> mFading;
        /// Files that ended or could not be started while they were wanted: they are not started again until they
        /// have been unwanted
        std::vector<VFS::Path::Normalized> mDone;
        /// A stem that was wanted played to its end (the ones that could not be started do not count)
        bool mEnded = false;
    };

    /// The music records, with their tracks looked up once
    class FalloutMusicRecords
    {
    public:
        FalloutMusicRecords(const VFS::Manager& vfs, const OFWorld::ESMStore& store);

        FalloutMusic* find(ESM::FormId id);
        /// By editor id (not case sensitive), null when no record has it
        FalloutMusic* find(std::string_view editorId);
        /// Forgets the tracks that were looked up (a game was loaded)
        void clear();

    private:
        FalloutMusic* make(const ESM4::Music& record);

        const VFS::Manager& mVFS;
        const OFWorld::ESMStore& mStore;
        std::mutex mMutex;
        std::unordered_map<std::uint32_t, std::unique_ptr<FalloutMusic>> mMusic;
        bool mNamesBuilt = false;
        std::unordered_map<std::string, ESM::FormId> mNames;
    };
}

#endif
