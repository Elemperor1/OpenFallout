#include "falloutmusic.hpp"

#include <algorithm>
#include <cmath>

#include <components/esm4/loadmusc.hpp>
#include <components/falloutscript/music.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwworld/esmstore.hpp"

namespace OFSound
{
    LoopingDecoder::LoopingDecoder(std::shared_ptr<SoundDecoder> inner, VFS::Path::NormalizedView path)
        : SoundDecoder(inner->mResourceMgr)
        , mInner(std::move(inner))
        , mPath(path)
    {
    }

    void LoopingDecoder::open(VFS::Path::NormalizedView fname)
    {
        mPath = VFS::Path::Normalized(fname);
        mReadSomething = false;
        mInner->open(fname);
    }

    void LoopingDecoder::close()
    {
        mInner->close();
    }

    std::string LoopingDecoder::getName()
    {
        return mInner->getName();
    }

    void LoopingDecoder::getInfo(int* samplerate, ChannelConfig* chans, SampleType* type)
    {
        mInner->getInfo(samplerate, chans, type);
    }

    size_t LoopingDecoder::read(char* buffer, size_t bytes)
    {
        size_t total = 0;
        while (total < bytes)
        {
            const size_t got = mInner->read(buffer + total, bytes - total);
            if (got > 0)
            {
                total += got;
                mReadSomething = true;
                continue;
            }
            // The end: the file starts again, unless it gave nothing the last time (a file that is empty or cannot be
            // opened again would make this go on for ever)
            if (!mReadSomething)
                break;
            mReadSomething = false;
            mInner->close();
            try
            {
                mInner->open(mPath);
            }
            catch (const std::exception&)
            {
                break;
            }
        }
        return total;
    }

    size_t LoopingDecoder::getSampleOffset()
    {
        return mInner->getSampleOffset();
    }

    std::vector<VFS::Path::Normalized> falloutMusicFiles(std::string_view name, const VFS::Manager& vfs)
    {
        std::vector<VFS::Path::Normalized> files;
        if (name.empty())
            return files;
        std::string folder = VFS::Path::normalizeFilename(name);
        if (folder.back() == VFS::Path::separator)
        {
            if (folder.compare(0, 6, "music/") != 0)
                folder.insert(0, "music/");
            for (const VFS::Path::Normalized& file : vfs.getRecursiveDirectoryIterator(folder))
            {
                const std::string_view value = file.value();
                // Only the files of the folder itself, as the executable lists them
                if (value.find(VFS::Path::separator, folder.size()) != std::string_view::npos)
                    continue;
                if (file.extension().value() == "mp3")
                    files.push_back(file);
            }
            // The order of a folder is the order of the names, so that what a game does with it does not depend on the
            // way the archives are listed
            std::sort(files.begin(), files.end(),
                [](const VFS::Path::Normalized& a, const VFS::Path::Normalized& b) { return a.value() < b.value(); });
            return files;
        }
        constexpr VFS::Path::NormalizedView musicDir("music");
        constexpr VFS::Path::ExtensionView mp3("mp3");
        VFS::Path::Normalized path
            = Misc::ResourceHelpers::correctResourcePath({ { musicDir } }, VFS::Path::toNormalized(name), vfs, mp3);
        if (vfs.exists(path))
            files.push_back(std::move(path));
        return files;
    }

    void MusicMixer::forget()
    {
        mPlaying.clear();
        mDone.clear();
        mEnded = false;
    }

    FalloutMusicRecords::FalloutMusicRecords(const VFS::Manager& vfs, const OFWorld::ESMStore& store)
        : mVFS(vfs)
        , mStore(store)
    {
    }

    FalloutMusic* FalloutMusicRecords::make(const ESM4::Music& record)
    {
        auto music = std::make_unique<FalloutMusic>();
        music->mId = record.mId;
        music->mEditorId = record.mEditorId;
        music->mFiles = falloutMusicFiles(record.mMusicFile, mVFS);
        music->mFolder = !record.mMusicFile.empty()
            && (record.mMusicFile.back() == '\\' || record.mMusicFile.back() == '/');
        if (record.mHasAttenuation)
        {
            const FalloutScript::Music::Attenuation attenuation = FalloutScript::Music::attenuationOf(record.mAttenuation);
            music->mDb = attenuation.mDb;
            music->mLoop = attenuation.mLoop;
        }
        FalloutMusic* result = music.get();
        mMusic[record.mId.toUint32()] = std::move(music);
        return result;
    }

    FalloutMusic* FalloutMusicRecords::find(ESM::FormId id)
    {
        if (id.isZeroOrUnset())
            return nullptr;
        const std::lock_guard lock(mMutex);
        const auto found = mMusic.find(id.toUint32());
        if (found != mMusic.end())
            return found->second.get();
        const ESM4::Music* record = mStore.get<ESM4::Music>().search(ESM::RefId::formIdRefId(id));
        return record != nullptr ? make(*record) : nullptr;
    }

    FalloutMusic* FalloutMusicRecords::find(std::string_view editorId)
    {
        ESM::FormId id;
        {
            const std::lock_guard lock(mMutex);
            if (!mNamesBuilt)
            {
                mNamesBuilt = true;
                for (const ESM4::Music& music : mStore.get<ESM4::Music>())
                    if (!music.mEditorId.empty())
                        mNames[Misc::StringUtils::lowerCase(music.mEditorId)] = music.mId;
            }
            const auto found = mNames.find(Misc::StringUtils::lowerCase(editorId));
            if (found == mNames.end())
                return nullptr;
            id = found->second;
        }
        return find(id);
    }

    void FalloutMusicRecords::clear()
    {
        const std::lock_guard lock(mMutex);
        mMusic.clear();
    }
}
