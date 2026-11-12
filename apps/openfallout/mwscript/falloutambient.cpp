#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <numbers>
#include <random>
#include <thread>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadaspc.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadregn.hpp>
#include <components/esm4/loadwthr.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/timestamp.hpp"
#include "../mwworld/weather.hpp"

namespace OFScript
{
    namespace
    {
        constexpr int ambientLogLimit = 60;
        // The seconds between two looks at what the region and the weather ask for
        constexpr float ambientLook = 0.25f;
        // The chance in millionths with which a sound of the weather that does not loop (thunder) comes in an interval
        constexpr std::uint32_t weatherSoundChance = 300000;

        std::string formText(std::uint32_t id)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%08X", id);
            return text;
        }
    }

    void FalloutScripts::logAmbient(const std::string& text)
    {
        if (mAmbientPlayback.mLogged >= ambientLogLimit)
            return;
        ++mAmbientPlayback.mLogged;
        Log(Debug::Info) << "Ambient: " << text;
    }

    void FalloutScripts::resetAmbient()
    {
        AmbientPlayback& ambient = mAmbientPlayback;
        OFBase::SoundManager* sound
            = OFBase::Environment::exists() ? OFBase::Environment::get().getSoundManager() : nullptr;
        if (sound != nullptr)
            for (const auto& [id, loop] : ambient.mLoops)
                if (loop.mTrack != 0)
                    sound->stopMusicTrack(loop.mTrack);
        ambient.mLoops.clear();
        ambient.mSpace = ESM::FormId();
        ambient.mSpaceTime = 0.f;
        ambient.mUntilLook = 0.f;
        ambient.mRoller.reset();
    }

    void FalloutScripts::updateAmbient(float frametime)
    {
        if (!OFBase::Environment::exists())
            return;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (sound == nullptr || world == nullptr || !sound->isEnabled() || std::this_thread::get_id() != mMainThread)
            return;
        const OFWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell() || mPlayer == 0)
            return;
        const OFWorld::Cell* cell = player.getCell()->getCell();
        if (cell == nullptr)
            return;

        namespace Ambient = FalloutScript::Ambient;
        AmbientPlayback& ambient = mAmbientPlayback;
        const Ambient::Settings settings;

        // ---- The space the player is in ----
        const ESM::FormId spaceId = cell->getAcousticSpace();
        const ESM4::AcousticSpace* space
            = spaceId.isZeroOrUnset() ? nullptr : mStore.get<ESM4::AcousticSpace>().search(spaceId);
        if (spaceId != ambient.mSpace)
        {
            ambient.mSpace = spaceId;
            ambient.mSpaceTime = 0.f;
            ambient.mRoller.reset();
            if (space != nullptr)
            {
                const ESM4::Region* region = space->mSoundRegion.isZeroOrUnset()
                    ? nullptr
                    : mStore.get<ESM4::Region>().search(space->mSoundRegion);
                logAmbient("the player is in the acoustic space "
                    + (space->mEditorId.empty() ? formText(spaceId.toUint32()) : space->mEditorId)
                    + (space->mIsInterior != 0 ? " (interior)" : "") + ", region "
                    + (region != nullptr ? (region->mEditorId.empty() ? formText(region->mId.toUint32()) : region->mEditorId)
                                         : std::string("none"))
                    + " with " + std::to_string(region != nullptr ? region->mSounds.size() : 0) + " sound(s)");
            }
            else
                logAmbient("the player is in a place with no acoustic space");
        }
        ambient.mSpaceTime += frametime;

        // ---- What is wanted ----
        const bool interior = !cell->isExterior();
        // The weather matters out of doors; a place indoors has the pleasant one
        const OFWorld::Weather& weather = world->getCurrentWeather();
        const std::uint32_t classification = interior ? 1u : (weather.mClassification & 0x0Fu);
        const auto isLoop = [sound](std::uint32_t id) {
            const auto data = sound->falloutSoundData(ESM::FormId::fromUint32(id));
            return data && data->isLooping();
        };

        std::vector<Ambient::RegionSound> entries;
        const ESM4::Region* region = space == nullptr || space->mSoundRegion.isZeroOrUnset()
            ? nullptr
            : mStore.get<ESM4::Region>().search(space->mSoundRegion);
        if (region != nullptr)
            for (const ESM4::Region::RegionSound& entry : region->mSounds)
                entries.push_back({ entry.sound, entry.flags, entry.chance });
        // The sounds of the weather (rain, wind, thunder) are heard out of doors. A weather says only which sound it is, not
        // how often: a sound that loops is on as long as the weather is, and the others (thunder) are rolled like the sounds
        // of a region, with the chance below in every interval (the port's choice: nothing that was read says it).
        if (!interior && weather.mFromRecord)
            if (const ESM4::Weather* record = mStore.get<ESM4::Weather>().search(weather.mId))
                for (const ESM4::Weather::WeatherSound& entry : record->mSounds)
                    if (entry.mSound != 0)
                        entries.push_back({ entry.mSound, classification == 0 ? 1u : classification, isLoop(entry.mSound) ? 1000000u : weatherSoundChance });

        ambient.mUntilLook -= frametime;
        if (ambient.mUntilLook <= 0.f)
        {
            ambient.mUntilLook = ambientLook;
            std::map<std::uint32_t, bool> wanted; // the sound, and whether it is the space's
            if (space != nullptr && ambient.mSpaceTime >= settings.mSpaceStart)
            {
                const Ambient::TimeOfDay period = Ambient::timeOfDay(world->getTimeStamp().getHour(), settings);
                std::vector<std::uint32_t> slots;
                for (const ESM::FormId id : space->mAmbientLoopSounds)
                    slots.push_back(id.toUint32());
                const std::uint32_t loop = Ambient::loopOfSpace(slots, space->mIsInterior != 0, period);
                if (loop != 0)
                    wanted[loop] = true;
            }
            for (const Ambient::RegionSound& entry : entries)
                if (entry.mSound != 0 && entry.mChance > 0 && Ambient::weatherAllows(entry.mFlags, classification)
                    && isLoop(entry.mSound))
                    wanted.emplace(entry.mSound, false);

            for (auto it = ambient.mLoops.begin(); it != ambient.mLoops.end();)
            {
                const bool gone = wanted.find(it->first) == wanted.end();
                if (gone)
                {
                    if (it->second.mTrack != 0)
                    {
                        sound->fadeMusicTrack(it->second.mTrack,
                            0.f, it->second.mOfSpace ? settings.mSpaceFadeOut : settings.mRegionFadeOut, true);
                        logAmbient("the loop " + formText(it->first) + " fades out");
                    }
                    it = ambient.mLoops.erase(it);
                }
                else if (it->second.mTrack != 0 && !sound->isMusicTrackPlaying(it->second.mTrack))
                    it = ambient.mLoops.erase(it); // it ended or was stopped: it starts again below
                else
                    ++it;
            }
            for (const auto& [id, ofSpace] : wanted)
            {
                if (ambient.mLoops.find(id) != ambient.mLoops.end())
                    continue;
                AmbientPlayback::Loop loop;
                loop.mOfSpace = ofSpace;
                loop.mTrack = sound->startFalloutLoop(
                    ESM::FormId::fromUint32(id), ofSpace ? settings.mSpaceFadeIn : settings.mRegionFadeIn);
                ambient.mLoops.emplace(id, loop);
                logAmbient("the loop " + formText(id) + (ofSpace ? " of the space" : " of the region")
                    + (loop.mTrack != 0 ? " starts" : " could not start"));
            }
        }

        // ---- The sounds that come now and then ----
        const auto unit = [&ambient] { return std::uniform_real_distribution<double>(0.0, 1.0)(ambient.mRandom); };
        for (const std::uint32_t id : ambient.mRoller.update(frametime, entries, classification, unit, isLoop))
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            const auto data = sound->falloutSoundData(form);
            if (!data)
                continue;
            const bool placed = (data->mFlags & FalloutScript::SoundData::RandomLocation) != 0 && !data->isTwoDimensional();
            auto* played = static_cast<decltype(sound->playFalloutSound(form, OFWorld::ConstPtr(), 1.f))>(nullptr);
            if (placed)
            {
                // A random place around the player, between a quarter and three quarters of how far the sound is heard
                const float far = data->hasDistance() ? data->maxDistance() : 2000.f;
                const double angle = unit() * 2.0 * std::numbers::pi;
                const float distance = far * static_cast<float>(0.25 + 0.5 * unit());
                osg::Vec3f position = player.getRefData().getPosition().asVec3();
                position += osg::Vec3f(static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0.f) * distance;
                played = sound->playFalloutSoundAt(form, position, 1.f);
            }
            else
                played = sound->playFalloutSound(form, OFWorld::ConstPtr(), 1.f);
            logAmbient("the sound " + formText(id) + (placed ? " at a random place" : "")
                + (played != nullptr ? " plays" : " could not play"));
        }
    }
}
