#ifndef OPENFALLOUT_MWSOUND_FALLOUTSOUND_H
#define OPENFALLOUT_MWSOUND_FALLOUTSOUND_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <components/esm/formid.hpp>
#include <components/falloutscript/sounddata.hpp>
#include <components/vfs/pathutil.hpp>

namespace ESM4
{
    struct Sound;
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
    /// A sound record of Fallout 3 or New Vegas as it is played: the files that it can play (one, or those of the
    /// folder that it names, of which the games play one at random) and how they are played
    struct FalloutSound
    {
        ESM::FormId mId;
        std::string mEditorId;
        FalloutScript::SoundData mData;
        std::vector<VFS::Path::Normalized> mFiles;
    };

    /// The settings of the data of a sound record
    FalloutScript::SoundData falloutSoundData(const ESM4::Sound& record);

    /// The files that a sound record names. The name of a folder (it ends in a slash, as half of the sound records of
    /// New Vegas have it) is every sound file in it and below it; a name that is a file gives that file, or the mp3
    /// that a wav was converted to when that is what there is. Empty when the files have none.
    std::vector<VFS::Path::Normalized> falloutSoundFiles(std::string_view name, const VFS::Manager& vfs);

    /// The sound records, by editor id and by form: what a script or a text key of an animation names a sound by
    class FalloutSounds
    {
    public:
        FalloutSounds(const VFS::Manager& vfs, const OFWorld::ESMStore& store);

        /// The sound with the editor id (the case does not matter), null when no record has it. The executable's table
        /// of editor ids keeps the last form put in under a name, so a later plugin's record wins.
        const FalloutSound* find(std::string_view editorId);

        const FalloutSound* find(ESM::FormId id);

        /// How many sounds were looked at and had no files, for the log (once each)
        bool noteMissing(const FalloutSound& sound);

    private:
        const FalloutSound* make(const ESM4::Sound& record);

        const VFS::Manager& mVFS;
        const OFWorld::ESMStore& mStore;
        std::mutex mMutex;
        bool mNamesBuilt = false;
        std::unordered_map<std::string, ESM::FormId> mNames;
        std::unordered_map<std::uint32_t, std::unique_ptr<FalloutSound>> mSounds;
        std::unordered_map<std::uint32_t, bool> mMissingSaid;
    };
}

#endif
