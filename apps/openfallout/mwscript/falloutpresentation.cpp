#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadimad.hpp>
#include <components/esm4/loadmusc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        /// How long an image space modifier lasts when the files do not say, in seconds. A modifier that never ended
        /// would hold a script that waits for it to be over.
        constexpr double unknownImageSpaceSeconds = 5.0;
        /// The longest an image space modifier of the files is taken to last
        constexpr double longestImageSpaceSeconds = 120.0;
        /// Where the duration is in the settings of the modifier (DNAM): after the flags, a float
        constexpr std::size_t imageSpaceDurationOffset = 4;

        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }
    }

    bool FalloutScripts::markerFlags(FalloutScript::FormId marker, std::uint8_t& flags)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(marker);
        if (id.isZeroOrUnset())
            return false;
        const ESM4::Reference* record = mStore.get<ESM4::Reference>().search(id);
        if (record == nullptr || !record->mIsMapMarker)
            return false;
        flags = record->mMapMarkerFlags;
        return true;
    }

    void FalloutScripts::markerShown(FalloutScript::FormId)
    {
        const std::string text = gameSettingText("sMapMarkerAdded");
        if (!text.empty())
            tellPlayer(text);
    }

    void FalloutScripts::playSound(FalloutScript::FormId sound, FalloutScript::FormId reference)
    {
        // The world of the sounds is not touched while scripts run on another thread
        runOnMainThread([this, sound, reference] {
            OFBase::SoundManager* manager = OFBase::Environment::get().getSoundManager();
            const std::vector<std::string> files = soundFilesOf(sound);
            if (manager == nullptr || files.empty())
                return;
            const std::string& file = files.size() == 1
                ? files[0]
                : files[std::uniform_int_distribution<std::size_t>(0, files.size() - 1)(mWeaponSoundRandom)];
            const VFS::Path::Normalized path(file);
            // The player hears a sound that no one makes as it is, and the sound of a reference comes from it
            if (const OFWorld::Ptr ptr
                = reference != 0 && reference != mPlayer ? findReference(reference) : OFWorld::Ptr();
                !ptr.isEmpty())
                manager->playSound3D(ptr, path, 1.f, 1.f, OFSound::Type::Sfx, OFSound::PlayMode::Normal);
            else if (reference == 0 || reference == mPlayer)
                manager->playSound(path, 1.f, 1.f, OFSound::Type::Sfx, OFSound::PlayMode::NoEnv);
        });
    }

    bool FalloutScripts::isSoundPlaying(FalloutScript::FormId sound, FalloutScript::FormId reference)
    {
        OFBase::SoundManager* manager = OFBase::Environment::get().getSoundManager();
        if (manager == nullptr || std::this_thread::get_id() != mMainThread)
            return false;
        const OFWorld::Ptr ptr = reference != 0 ? findReference(reference) : OFWorld::Ptr();
        // A sound that no one makes is the player's: it is the answer for that reference and for none other
        const bool unattached = reference == 0 || reference == mPlayer;
        for (const std::string& file : soundFilesOf(sound))
        {
            const VFS::Path::Normalized path(file);
            if ((!ptr.isEmpty() && manager->getSoundPlaying(ptr, path))
                || (unattached && manager->getSoundPlaying(OFWorld::Ptr(), path)))
                return true;
        }
        return false;
    }

    bool FalloutScripts::isMusicPlaying()
    {
        OFBase::SoundManager* manager = OFBase::Environment::get().getSoundManager();
        return manager != nullptr && std::this_thread::get_id() == mMainThread && manager->isMusicPlaying();
    }

    bool FalloutScripts::playMusic(FalloutScript::FormId track)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(track);
        const ESM4::Music* record = id.isZeroOrUnset() ? nullptr : mStore.get<ESM4::Music>().search(id);
        if (record == nullptr || record->mMusicFile.empty())
            return false;
        const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem();
        if (resources == nullptr)
            return false;
        // The file is named from the folder of the music, as the sound records name theirs from the folder of the
        // sounds
        constexpr VFS::Path::NormalizedView musicDir("music");
        constexpr VFS::Path::ExtensionView mp3("mp3");
        const VFS::Path::Normalized path = Misc::ResourceHelpers::correctResourcePath(
            { { musicDir } }, VFS::Path::toNormalized(record->mMusicFile), *resources->getVFS(), mp3);
        if (!resources->getVFS()->exists(path))
        {
            Log(Debug::Warning) << "The music " << record->mEditorId << " has no file " << path;
            return false;
        }
        Log(Debug::Info) << "Script music: " << record->mEditorId << " plays " << path;
        // The track replaces what plays, with the fade of the sound manager, and the playlist goes on when it ends
        runOnMainThread([path] {
            if (OFBase::SoundManager* manager = OFBase::Environment::get().getSoundManager())
                manager->streamMusic(path, OFSound::MusicType::MWScript);
        });
        return true;
    }

    void FalloutScripts::forceWeather(FalloutScript::FormId weather, bool hold)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(weather);
        if (id.isZeroOrUnset())
            return;
        runOnMainThread([id, hold] {
            if (OFBase::World* world = OFBase::Environment::get().getWorld())
                world->forceWeather(id, hold);
        });
    }

    void FalloutScripts::releaseWeather()
    {
        runOnMainThread([] {
            if (OFBase::World* world = OFBase::Environment::get().getWorld())
                world->releaseWeatherOverride();
        });
    }

    double FalloutScripts::imageSpaceDuration(FalloutScript::FormId modifier)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(modifier);
        const ESM4::ImageSpaceModifier* record
            = id.isZeroOrUnset() ? nullptr : mStore.get<ESM4::ImageSpaceModifier>().search(id);
        if (record != nullptr && record->mData.size() >= imageSpaceDurationOffset + sizeof(float))
        {
            float seconds = 0;
            std::memcpy(&seconds, record->mData.data() + imageSpaceDurationOffset, sizeof(float));
            if (std::isfinite(seconds) && seconds > 0 && seconds <= longestImageSpaceSeconds)
                return seconds;
        }
        return unknownImageSpaceSeconds;
    }

    void FalloutScripts::achievement(int id)
    {
        Log(Debug::Info) << "Achievement earned: " << id;
    }

    FalloutScript::FormId FalloutScripts::parentCell(FalloutScript::FormId reference)
    {
        Place place;
        if (!whereIs(reference, place))
            return 0;
        const ESM::FormId* id = place.mCell.getIf<ESM::FormId>();
        return id != nullptr ? numberOf(*id) : 0;
    }

    FalloutScript::FormId FalloutScripts::parentWorldspace(FalloutScript::FormId reference)
    {
        return worldspaceOf(reference);
    }

    bool FalloutScripts::gameRestarted(FalloutScript::FormId script)
    {
        const std::lock_guard lock(mSessionMutex);
        return mRestartAsked.insert(script).second;
    }

    bool FalloutScripts::gameLoaded(FalloutScript::FormId script)
    {
        const std::lock_guard lock(mSessionMutex);
        return mLoadAsked.insert(script).second;
    }
}
