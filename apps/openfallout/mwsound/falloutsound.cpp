#include "falloutsound.hpp"

#include <algorithm>
#include <cstring>

#include <components/esm4/loadsoun.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFSound
{
    FalloutScript::SoundData falloutSoundData(const ESM4::Sound& record)
    {
        // The two structs of the loader are the 12 bytes of the settings and the 24 of the curve, in the order of the
        // subrecord
        static_assert(sizeof(ESM4::Sound::SNDX) == 12);
        static_assert(sizeof(ESM4::Sound::SoundData) == 24);
        unsigned char bytes[36] = {};
        std::memcpy(bytes, &record.mData, sizeof(ESM4::Sound::SNDX));
        std::memcpy(bytes + sizeof(ESM4::Sound::SNDX), &record.mExtra, sizeof(ESM4::Sound::SoundData));
        return FalloutScript::soundDataFromBytes(bytes, std::min<std::size_t>(record.mDataSize, sizeof(bytes)));
    }

    std::vector<VFS::Path::Normalized> falloutSoundFiles(std::string_view name, const VFS::Manager& vfs)
    {
        std::vector<VFS::Path::Normalized> files;
        if (name.empty())
            return files;
        std::string folder = VFS::Path::normalizeFilename(name);
        if (!folder.empty() && folder.back() == VFS::Path::separator)
        {
            if (folder.compare(0, 6, "sound/") != 0)
                folder.insert(0, "sound/");
            for (const VFS::Path::Normalized& file : vfs.getRecursiveDirectoryIterator(folder))
            {
                const std::string_view extension = file.extension().value();
                if (extension == "wav" || extension == "ogg" || extension == "mp3")
                    files.push_back(file);
            }
            return files;
        }
        constexpr VFS::Path::NormalizedView soundDir("sound");
        constexpr VFS::Path::ExtensionView mp3("mp3");
        VFS::Path::Normalized path
            = Misc::ResourceHelpers::correctResourcePath({ { soundDir } }, VFS::Path::toNormalized(name), vfs, mp3);
        if (vfs.exists(path))
            files.push_back(std::move(path));
        return files;
    }

    FalloutSounds::FalloutSounds(const VFS::Manager& vfs, const OFWorld::ESMStore& store)
        : mVFS(vfs)
        , mStore(store)
    {
    }

    const FalloutSound* FalloutSounds::make(const ESM4::Sound& record)
    {
        auto sound = std::make_unique<FalloutSound>();
        sound->mId = record.mId;
        sound->mEditorId = record.mEditorId;
        sound->mData = falloutSoundData(record);
        sound->mFiles = falloutSoundFiles(record.mSoundFile, mVFS);
        const FalloutSound* result = sound.get();
        mSounds[record.mId.toUint32()] = std::move(sound);
        return result;
    }

    const FalloutSound* FalloutSounds::find(ESM::FormId id)
    {
        if (id.isZeroOrUnset())
            return nullptr;
        const std::lock_guard lock(mMutex);
        const auto found = mSounds.find(id.toUint32());
        if (found != mSounds.end())
            return found->second.get();
        const ESM4::Sound* record = mStore.get<ESM4::Sound>().search(ESM::RefId::formIdRefId(id));
        return record != nullptr ? make(*record) : nullptr;
    }

    const FalloutSound* FalloutSounds::find(std::string_view editorId)
    {
        ESM::FormId id;
        {
            const std::lock_guard lock(mMutex);
            if (!mNamesBuilt)
            {
                mNamesBuilt = true;
                for (const ESM4::Sound& sound : mStore.get<ESM4::Sound>())
                    if (!sound.mEditorId.empty())
                        mNames[Misc::StringUtils::lowerCase(sound.mEditorId)] = sound.mId;
            }
            const auto found = mNames.find(Misc::StringUtils::lowerCase(editorId));
            if (found == mNames.end())
                return nullptr;
            id = found->second;
        }
        return find(id);
    }

    bool FalloutSounds::noteMissing(const FalloutSound& sound)
    {
        const std::lock_guard lock(mMutex);
        return mMissingSaid.emplace(sound.mId.toUint32(), true).second;
    }
}
