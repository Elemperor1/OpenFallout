#ifndef OPENFALLOUT_MWSOUND_FALLOUTMUSIC_H
#define OPENFALLOUT_MWSOUND_FALLOUTMUSIC_H

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

        void update(OFBase::SoundManager& sound, const std::vector<Stem>& wanted, float fadeSeconds);
        /// Fades everything out
        void stopAll(OFBase::SoundManager& sound, float fadeSeconds);
        /// Whether a stem plays that is not on its way out
        bool playing(OFBase::SoundManager& sound);
        /// Forgets what plays, without touching it (the sound manager was cleared)
        void forget();
        std::size_t size() const { return mPlaying.size(); }

    private:
        struct Playing
        {
            VFS::Path::Normalized mFile;
            OFBase::SoundManager::MusicTrackId mId = 0;
            float mGain = 0.f;
        };
        std::vector<Playing> mPlaying;
        /// Files that ended or could not be started while they were wanted: they are not started again until they
        /// have been unwanted
        std::vector<VFS::Path::Normalized> mDone;
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
