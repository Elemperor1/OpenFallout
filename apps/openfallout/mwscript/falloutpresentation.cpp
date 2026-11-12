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

    void FalloutScripts::markerDiscovered(FalloutScript::FormId marker)
    {
        // The executable gives the display two lines, the word of the games' settings and the name of the place (it
        // is written under the word one character at a time, as the name of a quest that was added is), with the sound
        // of a quest that was added
        FalloutScript::Hud::QuestText text;
        for (const PlacedMarker& placed : placedMarkers())
            if (placed.mId == marker)
            {
                text.mName = placed.mName;
                break;
            }
        text.mHeading = gameSettingText("sDiscoveredText");
        text.mSound = "UIPopUpQuestNew";
        text.mKind = 3;
        mHudQuestText.push(std::move(text));
    }

    void FalloutScripts::playSound(FalloutScript::FormId sound, FalloutScript::FormId reference)
    {
        // The world of the sounds is not touched while scripts run on another thread
        runOnMainThread([this, sound, reference] {
            OFBase::SoundManager* manager = OFBase::Environment::get().getSoundManager();
            if (manager == nullptr)
                return;
            const ESM::FormId form = ESM::FormId::fromUint32(sound);
            // The player hears a sound that no one makes as it is, and the sound of a reference comes from it; either
            // way the record says how loud it is, how far it is heard, its pitch, and whether it loops
            if (const OFWorld::Ptr ptr
                = reference != 0 && reference != mPlayer ? findReference(reference) : OFWorld::Ptr();
                !ptr.isEmpty())
                manager->playFalloutSound(form, ptr, 1.f, OFSound::Type::Sfx, OFSound::PlayMode::Normal);
            else if (reference == 0 || reference == mPlayer)
                manager->playFalloutSound(
                    form, OFWorld::ConstPtr(), 1.f, OFSound::Type::Sfx, OFSound::PlayMode::NoEnv);
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
        if (manager == nullptr || std::this_thread::get_id() != mMainThread)
            return false;
        // Music is the track of a script, the one of the place, the layers of a media set and the playlist
        return manager->isMusicPlaying()
            || (mMusicPlayback.mScriptTrack != 0 && manager->isMusicTrackPlaying(mMusicPlayback.mScriptTrack))
            || (mMusicPlayback.mTrack != 0 && manager->isMusicTrackPlaying(mMusicPlayback.mTrack))
            || mMusicPlayback.mMixer.playing(*manager);
    }

    bool FalloutScripts::playMusic(FalloutScript::FormId track)
    {
        const ESM::FormId id = ESM::FormId::fromUint32(track);
        const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem();
        if (id.isZeroOrUnset() || resources == nullptr)
            return false;
        {
            // The records of music, with the tracks they name (a folder names all the mp3 files in it)
            const std::lock_guard lock(mMusicMutex);
            if (!mMusicPlayback.mRecords)
                mMusicPlayback.mRecords = std::make_unique<OFSound::FalloutMusicRecords>(*resources->getVFS(), mStore);
        }
        OFSound::FalloutMusic* record = mMusicPlayback.mRecords->find(id);
        if (record == nullptr)
            return false;
        if (record->mFiles.empty())
        {
            Log(Debug::Warning) << "The music " << record->mEditorId << " has no file";
            return false;
        }
        // The track replaces what plays, with a fade, as loud as the record says (ANAM), looping when it says so. The
        // music of the place comes back after it.
        runOnMainThread([this, id] {
            OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
            OFSound::FalloutMusic* music = mMusicPlayback.mRecords ? mMusicPlayback.mRecords->find(id) : nullptr;
            if (sound == nullptr || music == nullptr || music->mFiles.empty())
                return;
            const auto random = [this](int low, int high) {
                return std::uniform_int_distribution<int>(low, high)(mMusicPlayback.mRandom);
            };
            if (!music->mStarted)
            {
                music->mStarted = true;
                music->mLast = FalloutScript::Music::firstTrack(music->mFiles.size(), random);
            }
            const auto index = FalloutScript::Music::nextTrack(music->mFiles.size(), music->mLast, random);
            if (!index)
                return;
            const VFS::Path::Normalized& file = music->mFiles[*index];
            if (mMusicPlayback.mScriptTrack != 0)
                sound->fadeMusicTrack(mMusicPlayback.mScriptTrack, 0.f, 1.f, true);
            if (sound->isMusicPlaying())
                sound->stopMusic();
            // The music of the place is quiet while the track plays, now: the update of the music does not run while a menu
            // is open (a script of a menu may call PlayMusic), so it is not left for it
            if (mMusicPlayback.mTrack != 0)
                sound->fadeMusicTrack(mMusicPlayback.mTrack, 0.f, 1.f, true);
            mMusicPlayback.mTrack = 0;
            // (and the director is told that what it played was cut short, as when a script track blocks it)
            mMusicPlayback.mDirector.reset();
            mMusicPlayback.mMixer.stopAll(*sound, 1.f);
            mMusicPlayback.mStemKey.clear();
            mMusicPlayback.mStems.clear();
            mMusicPlayback.mScriptTrack = sound->startMusicTrack(
                file, FalloutScript::Music::gainOfDb(music->mDb), 1.f, music->mLoop);
            logMusic("a script plays " + music->mEditorId + ": " + std::string(file.value())
                + (music->mLoop ? ", loops" : ""));
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
