#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadtact.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"

namespace OFScript
{
    namespace
    {
        constexpr int radioLogLimit = 60;
        /// The record flag of a talking activator that is a radio station
        constexpr std::uint32_t radioStationFlag = 0x00020000;
        /// The record flags of a radio station that is not proxied for the Pip-Boy, and of one that broadcasts all the time
        constexpr std::uint32_t radioNonProxyFlag = 0x10000000;
        constexpr std::uint32_t radioContinuousFlag = 0x40000000;
        /// The seconds between the end of what a station said and the next chain (the games have no pause that was read: a
        /// chain begins when the last one is over)
        constexpr float radioRest = 1.f;
        /// The seconds a station that has nothing to say waits before it is asked again
        constexpr float radioNothingRest = 10.f;
        constexpr const char* radioTopic = "RadioHello";
    }

    void FalloutScripts::logRadio(const std::string& text)
    {
        if (mRadioPlayback.mLogged >= radioLogLimit)
            return;
        ++mRadioPlayback.mLogged;
        Log(Debug::Info) << "Radio: " << text;
    }

    void FalloutScripts::stopRadioVoice()
    {
        RadioPlayback& radio = mRadioPlayback;
        OFBase::SoundManager* sound = nullptr;
        if (OFBase::Environment::exists())
            sound = OFBase::Environment::get().getSoundManager();
        if (sound != nullptr && radio.mTrack != 0)
            sound->stopMusicTrack(radio.mTrack);
        radio.mTrack = 0;
        radio.mLines.clear();
        radio.mNext = 0;
        radio.mStarted = false;
        radio.mStation = 0;
        radio.mNextTopic = 0;
    }

    void FalloutScripts::resetRadio()
    {
        RadioPlayback& radio = mRadioPlayback;
        stopRadioVoice();
        radio.mReceiver.reset();
        radio.mRoomRest.clear();
        radio.mRoomTopic.clear();
        radio.mRoomInReach.clear();
        radio.mRest = 0.f;
        radio.mUntilLook = 0.f;
        radio.mSinceLook = 0.f;
        radio.mDialogueOnly = false;
        radio.mLastTuned = 0;
        radio.mPending.clear();
        radio.mPendingNpc.clear();
        radio.mNpc.clear();
        radio.mGroupCell = ESM::RefId();
        radio.mGroup = {};
        const std::lock_guard lock(mRadioMutex);
        radio.mListing.clear();
    }

    void FalloutScripts::setRadioBroadcast(FalloutScript::FormId station, bool on)
    {
        const std::lock_guard lock(mRadioMutex);
        mRadioBroadcast[station] = on;
    }

    bool FalloutScripts::radioBroadcast(FalloutScript::FormId station)
    {
        const std::lock_guard lock(mRadioMutex);
        if (const auto set = mRadioBroadcast.find(station); set != mRadioBroadcast.end())
            return set->second;
        if (!mRadioPlayback.mBuilt)
            buildRadioStations();
        for (const RadioStation& entry : mRadioPlayback.mStations)
            if (entry.mReference == station)
                return entry.mBroadcast;
        return false;
    }

    void FalloutScripts::buildRadioStations()
    {
        // (called with the radio mutex held)
        RadioPlayback& radio = mRadioPlayback;
        radio.mBuilt = true;
        // The doors that lead from one cell to another (the cell and the door it opens on), for the stations that reach the
        // interiors linked to a place
        std::vector<std::pair<ESM::RefId, ESM::FormId>> doors;
        for (const ESM4::Reference& reference : mStore.get<ESM4::Reference>())
        {
            if (!reference.mDoor.destDoor.isZeroOrUnset())
                doors.emplace_back(reference.mParent, reference.mDoor.destDoor);
            if (!reference.mRadio.present)
                continue;
            // A station is a talking activator that has the flag; the others that carry the subrecord are speakers
            const ESM4::TalkingActivator* base = mStore.get<ESM4::TalkingActivator>().search(reference.mBaseObj);
            if (base == nullptr || (base->mFlags & radioStationFlag) == 0)
                continue;
            RadioStation station;
            station.mReference = reference.mId.toUint32();
            station.mPositionReference = reference.mRadio.posReference.toUint32();
            station.mName = base->mFullName.empty() ? base->mEditorId : base->mFullName;
            station.mNonProxy = (base->mFlags & radioNonProxyFlag) != 0;
            station.mBroadcast = (base->mFlags & radioContinuousFlag) != 0;
            station.mReach = static_cast<FalloutScript::Radio::Reach>(std::min<std::uint32_t>(reference.mRadio.broadcastRange, 4));
            station.mRadius = reference.mRadio.rangeRadius;
            station.mStaticPercent = reference.mRadio.staticPercentage;
            station.mCell = reference.mParent;
            if (const ESM4::Cell* cell = mStore.get<ESM4::Cell>().search(reference.mParent))
            {
                station.mExterior = cell->isExterior();
                station.mWorldspace = station.mExterior ? cell->mParent : reference.mParent;
            }
            radio.mStations.push_back(std::move(station));
        }
        const auto& cells = mStore.get<ESM4::Cell>();
        for (const auto& [cellId, destination] : doors)
        {
            const ESM4::Cell* from = cells.search(cellId);
            if (from == nullptr || from->isExterior())
                continue;
            const ESM4::Reference* door = mStore.get<ESM4::Reference>().search(destination);
            const ESM4::Cell* to = door != nullptr ? cells.search(door->mParent) : nullptr;
            if (to == nullptr)
                continue;
            if (to->isExterior())
                radio.mLinks.addExit(cellId, to->mParent);
            else
                radio.mLinks.addDoor(cellId, door->mParent);
        }
        logRadio("the world has " + std::to_string(radio.mStations.size()) + " radio station(s)");
    }

    bool FalloutScripts::radioCanSay(FalloutScript::FormId station)
    {
        ESM::FormId base;
        if (!conditionBaseOf(station, base))
            return false;
        const std::lock_guard lock(mDialogueMutex);
        const FalloutScript::FormId topic = mDialogue.findTopic(radioTopic);
        return topic != 0 && mDialogue.responseFor(topic, { station, base.toUint32() }) != nullptr;
    }

    std::vector<FalloutScripts::RadioPlayback::Line> FalloutScripts::radioStep(
        FalloutScript::FormId station, FalloutScript::FormId topic, FalloutScript::FormId& next)
    {
        std::vector<RadioPlayback::Line> result;
        next = 0;
        ESM::FormId base;
        if (!conditionBaseOf(station, base))
            return result;
        const std::lock_guard lock(mDialogueMutex);
        if (topic == 0)
            topic = mDialogue.findTopic(radioTopic);
        if (topic == 0)
            return result;
        const FalloutScript::DialogueSpeaker speaker{ station, base.toUint32() };
        // The station says a response of the topic: the conditions of the responses (the quests that run the station)
        // choose which, and its end script runs
        const FalloutScript::DialogueView view = mDialogue.say(speaker, topic, 0, true, false, 0);
        for (const FalloutScript::DialogueLine& line : view.mLines)
        {
            RadioPlayback::Line said;
            said.mVoice = voiceFile(line.mSpeaker != 0 ? line.mSpeaker : station, line);
            said.mText = line.mText;
            said.mSeconds = lineSeconds(line.mText);
            said.mTopic = topic;
            said.mSpeakerBase = line.mSpeakerBase != 0 ? line.mSpeakerBase : base.toUint32();
            said.mSong = line.mSound != 0 && !said.mVoice.empty() && !soundFilesOf(line.mSound).empty();
            result.push_back(std::move(said));
        }
        if (view.mLines.empty())
            return result;
        // What goes on: the topics that the response offers, in order, the first that the station has a response for
        if (const ESM4::DialogInfo* info
            = mStore.get<ESM4::DialogInfo>().search(refIdOf(view.mLines.back().mInfo)))
            for (const ESM::FormId choice : info->mChoices)
            {
                const FalloutScript::FormId candidate = choice.toUint32();
                if (mDialogue.responseFor(candidate, speaker) != nullptr)
                {
                    next = candidate;
                    break;
                }
            }
        return result;
    }

    void FalloutScripts::refreshRadioListing()
    {
        std::vector<PipboyStation> listing;
        const RadioPlayback& radio = mRadioPlayback;
        for (const RadioStation& station : radio.mStations)
        {
            if (station.mNonProxy || !radio.mReceiver.inReach(station.mReference))
                continue;
            PipboyStation shown;
            shown.mStation = station.mReference;
            shown.mName = station.mName;
            shown.mTuned = radio.mReceiver.tuned() == station.mReference;
            shown.mSignal = radio.mReceiver.signal(station.mReference).mStrength;
            listing.push_back(std::move(shown));
        }
        const std::lock_guard lock(mRadioMutex);
        mRadioPlayback.mListing = std::move(listing);
    }

    std::vector<FalloutScripts::PipboyStation> FalloutScripts::pipboyRadio()
    {
        // (what the main thread listed at its last look: the Pip-Boy's scripts may ask from another thread)
        const std::lock_guard lock(mRadioMutex);
        return mRadioPlayback.mListing;
    }

    bool FalloutScripts::tuneRadio(FalloutScript::FormId station)
    {
        if (std::this_thread::get_id() == mMainThread)
            return tuneRadioNow(station);
        // The sounds and the receiver are the main thread's: it tunes when it next looks, and the answer is what it listed
        // last
        if (station != 0)
        {
            const std::lock_guard lock(mRadioMutex);
            const auto& listing = mRadioPlayback.mListing;
            if (std::none_of(listing.begin(), listing.end(),
                    [station](const PipboyStation& shown) { return shown.mStation == station; }))
                return false;
        }
        runOnMainThread([this, station] { tuneRadioNow(station); });
        return true;
    }

    bool FalloutScripts::tuneRadioNow(FalloutScript::FormId station)
    {
        RadioPlayback& radio = mRadioPlayback;
        if (station == 0)
        {
            if (radio.mReceiver.tuned() != 0)
                logRadio("the radio is turned off");
            radio.mReceiver.untune();
            stopRadioVoice();
            refreshRadioListing();
            return true;
        }
        if (radio.mReceiver.tuned() == station)
        {
            radio.mLastTuned = station;
            return true;
        }
        if (!radio.mReceiver.tune(station))
            return false;
        // (the receiver only holds stations that are proxied for the Pip-Boy)
        stopRadioVoice();
        radio.mRest = 0.f;
        radio.mLastTuned = station;
        logRadio("the radio is tuned to " + std::to_string(station));
        refreshRadioListing();
        return true;
    }

    void FalloutScripts::pipboyRadioNow(FalloutScript::Radio::PipboyAction action, FalloutScript::FormId station)
    {
        using Action = FalloutScript::Radio::PipboyAction;
        RadioPlayback& radio = mRadioPlayback;
        switch (action)
        {
            case Action::None:
                return;
            case Action::Off:
                tuneRadioNow(0);
                return;
            case Action::On:
            case Action::Tune:
                // Without a station the radio goes back to the one it was tuned to, and one that is not in reach leaves
                // it off (the executable turns the radio off when the station cannot be tuned)
                if (station == 0)
                    station = radio.mLastTuned;
                if (station == 0 || !tuneRadioNow(station))
                    tuneRadioNow(0);
                return;
        }
    }

    void FalloutScripts::startRadioConversationNow(FalloutScript::FormId station, FalloutScript::FormId topic)
    {
        RadioPlayback& radio = mRadioPlayback;
        if (station == 0)
            return;
        // What is said now is cut off: the conversation is the next thing the station says, as soon as the radio is asked
        if (radio.mReceiver.tuned() == station)
        {
            stopRadioVoice();
            radio.mStation = station;
            radio.mNextTopic = topic;
            radio.mRest = 0.f;
        }
        else
            radio.mPending[station] = topic;
        // (the characters that play the station start it as well, tuned or not)
        for (const auto& [actor, entry] : radio.mNpc)
            if (entry.mStation == station)
            {
                radio.mPendingNpc[station] = topic;
                break;
            }
        logRadio("the station " + std::to_string(station) + " is asked to start a conversation");
    }

    void FalloutScripts::setNpcRadioNow(FalloutScript::FormId actor, FalloutScript::FormId station, bool on)
    {
        if (actor == 0)
            return;
        RadioPlayback& radio = mRadioPlayback;
        // What the character was saying of the station stops when the radio goes off or to another station
        if (const auto old = radio.mNpc.find(actor); old != radio.mNpc.end() && (!on || old->second.mStation != station))
            cutOffSaying(actor);
        if (!on)
        {
            radio.mNpc.erase(actor);
            return;
        }
        RadioPlayback::NpcRadio& entry = radio.mNpc[actor];
        entry.mStation = station;
        entry.mTopic = 0;
        entry.mRest = 0.f;
        logRadio("the character " + std::to_string(actor) + " has the station " + std::to_string(station) + " on");
    }

    void FalloutScripts::updateRadio(float frametime)
    {
        if (!OFBase::Environment::exists())
            return;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (sound == nullptr || world == nullptr || std::this_thread::get_id() != mMainThread || mPlayer == 0
            || !hasDialogue())
            return;
        const OFWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return;
        const OFWorld::Cell* here = player.getCell()->getCell();
        if (here == nullptr)
            return;
        RadioPlayback& radio = mRadioPlayback;
        namespace Radio = FalloutScript::Radio;
        const Radio::Settings& settings = radio.mReceiver.settings();
        {
            const std::lock_guard lock(mRadioMutex);
            if (!radio.mBuilt)
                buildRadioStations();
        }
        radio.mSinceLook += frametime;
        radio.mUntilLook -= frametime;

        // ---- Which stations are in reach (every update interval) ----
        if (radio.mUntilLook <= 0.f)
        {
            radio.mUntilLook = settings.mUpdateInterval;
            const float seconds = radio.mSinceLook;
            radio.mSinceLook = 0.f;
            const osg::Vec3f at = player.getRefData().getPosition().asVec3();
            std::vector<Radio::Sighting> sightings;
            radio.mRoomInReach.clear();
            if (!here->isExterior() && radio.mGroupCell != here->getId())
            {
                radio.mGroupCell = here->getId();
                radio.mGroup = radio.mLinks.groupOf(here->getId());
            }
            // (the stations are only built once, so they are read without the lock; what a script says is on the air is read
            // under it for a moment: the response conditions that are looked at below may ask a script for a broadcast)
            for (const RadioStation& station : radio.mStations)
            {
                bool onTheAir;
                {
                    const std::lock_guard lock(mRadioMutex);
                    const auto set = mRadioBroadcast.find(station.mReference);
                    onTheAir = set != mRadioBroadcast.end() ? set->second : station.mBroadcast;
                }
                if (!onTheAir || isDisabled(station.mReference))
                    continue;
                Radio::Where where;
                where.mExterior = here->isExterior();
                where.mSameCell = here->getId() == station.mCell;
                if (here->isExterior())
                    where.mSameWorldspace = station.mExterior && here->getWorldSpace() == station.mWorldspace;
                else
                {
                    // Indoors: the interiors that a door joins to this one open on a worldspace, or are those of a station
                    // that is placed in an interior
                    where.mSameWorldspace = station.mExterior && radio.mGroup.mWorldspaces.count(station.mWorldspace) != 0;
                    where.mLinkedInterior = !station.mExterior && radio.mGroup.mInteriors.count(station.mCell) != 0;
                }
                if (station.mReach == Radio::Reach::Radius && where.mExterior && where.mSameWorldspace)
                {
                    // The distance is from the reference that the station names, or from the station
                    Place place;
                    const FalloutScript::FormId from
                        = station.mPositionReference != 0 ? station.mPositionReference : station.mReference;
                    if (whereIs(from, place))
                        where.mDistance = (place.mPosition - at).length();
                }
                Radio::Sighting sighting;
                sighting.mStation = station.mReference;
                sighting.mSignal
                    = Radio::signalOf(station.mReach, station.mRadius, station.mStaticPercent, where, settings);
                // A station that has nothing to say is not heard
                if (sighting.mSignal.mInRange && !radioCanSay(station.mReference))
                    sighting.mSignal = Radio::Signal();
                if (station.mNonProxy)
                {
                    // Not on the Pip-Boy: heard from where it is placed, when the player is in its reach
                    if (sighting.mSignal.mInRange)
                        radio.mRoomInReach.insert(station.mReference);
                    continue;
                }
                sightings.push_back(sighting);
            }
            for (const Radio::Event& event : radio.mReceiver.update(seconds, sightings))
            {
                const auto named = std::find_if(radio.mStations.begin(), radio.mStations.end(),
                    [&](const RadioStation& station) { return station.mReference == event.mStation; });
                const std::string name = named != radio.mStations.end() ? named->mName : std::string();
                switch (event.mKind)
                {
                    case Radio::Event::Kind::Discovered:
                    case Radio::Event::Kind::Found:
                        // The games say it with a message (sRadioStationDiscovered, "%s signal found.") and a sound, the
                        // first time only
                        if (event.mKind == Radio::Event::Kind::Discovered)
                            mHudMessages.push(name + " signal found.",
                                "Interface\\Icons\\Message Icons\\glow_message_radio_tower.dds", 0.f, 0, "UIRadioSignalFound");
                        logRadio(name + " (" + std::to_string(event.mStation) + ") "
                            + (event.mKind == Radio::Event::Kind::Discovered ? "is discovered" : "is in reach again"));
                        break;
                    case Radio::Event::Kind::Lost:
                        mHudMessages.push(name + " signal lost.",
                            "Interface\\Icons\\Message Icons\\glow_message_radio_tower.dds", 0.f, 0, "UIRadioSignalLost");
                        logRadio(name + " (" + std::to_string(event.mStation) + ") signal is lost");
                        break;
                }
            }
            refreshRadioListing();
        }

        // ---- What the tuned station says ----
        const FalloutScript::FormId tuned = radio.mReceiver.tuned();
        if (tuned == 0)
        {
            if (radio.mStation != 0)
                stopRadioVoice();
        }
        else
        {
            if (radio.mStation != tuned)
            {
                stopRadioVoice();
                radio.mStation = tuned;
            }
            // (the volume of the radio is the channel of its own: the gain is what is left of the static and, while the
            // player is spoken to, the mute of fRadioDialogMute)
            bool spokenTo;
            {
                const std::lock_guard lock(mDialogueMutex);
                spokenTo = mDialogue.active();
            }
            radio.mGain = Radio::gainOf(radio.mReceiver.signal(tuned), spokenTo, Radio::channelSettings(settings));
            // A station that was asked to start a conversation before it was tuned says it first
            if (const auto pending = radio.mPending.find(tuned); pending != radio.mPending.end())
            {
                radio.mNextTopic = pending->second;
                radio.mRest = 0.f;
                radio.mPending.erase(pending);
            }
            if (radio.mNext >= radio.mLines.size())
            {
                radio.mLines.clear();
                radio.mNext = 0;
                radio.mStarted = false;
                radio.mRest -= frametime;
                if (radio.mRest <= 0.f)
                {
                    FalloutScript::FormId next = 0;
                    radio.mLines = radioStep(tuned, radio.mNextTopic, next);
                    radio.mNextTopic = next;
                    // Between the responses of a chain there is no pause; a chain that is over waits a moment, and a station
                    // with nothing to say is asked again later
                    radio.mRest = radio.mLines.empty() ? radioNothingRest : (next != 0 ? 0.f : radioRest);
                    if (radio.mLines.empty())
                        radio.mNextTopic = 0;
                    logRadio("the station " + std::to_string(tuned) + " says " + std::to_string(radio.mLines.size())
                        + " line(s)" + (next != 0 ? ", and goes on" : ""));
                }
            }
            // A radio that is set to dialogue only says what is said and plays none of the songs
            while (radio.mNext < radio.mLines.size() && radio.mDialogueOnly && radio.mLines[radio.mNext].mSong
                && !radio.mStarted)
                ++radio.mNext;
            if (radio.mNext < radio.mLines.size())
            {
                const RadioPlayback::Line& line = radio.mLines[radio.mNext];
                if (!radio.mStarted)
                {
                    radio.mStarted = true;
                    radio.mTrack = 0;
                    if (!line.mVoice.empty())
                        radio.mTrack = sound->startRadioTrack(VFS::Path::Normalized(line.mVoice), radio.mGain);
                    radio.mWait = radio.mTrack != 0 ? 0.f : line.mSeconds;
                    logRadio(std::string("a line ") + (radio.mTrack != 0 ? "plays " + line.mVoice : std::string("has no file")));
                }
                else
                {
                    if (radio.mTrack != 0)
                    {
                        sound->fadeMusicTrack(radio.mTrack, radio.mGain, 0.25f, false);
                        // A song that plays when the radio is set to dialogue only is cut off
                        if (radio.mDialogueOnly && line.mSong)
                            sound->stopMusicTrack(radio.mTrack);
                        if (!sound->isMusicTrackPlaying(radio.mTrack))
                        {
                            sound->stopMusicTrack(radio.mTrack);
                            radio.mTrack = 0;
                            ++radio.mNext;
                            radio.mStarted = false;
                        }
                    }
                    else
                    {
                        radio.mWait -= frametime;
                        if (radio.mWait <= 0.f)
                        {
                            ++radio.mNext;
                            radio.mStarted = false;
                        }
                    }
                }
            }
        }

        // ---- The stations that are things in a room say what they say where they are ----
        for (const RadioStation& station : radio.mStations)
        {
            if (!station.mNonProxy || radio.mRoomInReach.count(station.mReference) == 0)
                continue;
            // A station that is not in a loaded cell is not a thing the player can hear
            if (findReference(station.mReference).isEmpty())
                continue;
            // (and one the player talks to says what the conversation says)
            if (inConversationWith(station.mReference))
                continue;
            float& rest = radio.mRoomRest[station.mReference];
            // A conversation that was asked for starts at once (what it says is cut off)
            if (const auto pending = radio.mPending.find(station.mReference); pending != radio.mPending.end())
            {
                cutOffSaying(station.mReference);
                radio.mRoomTopic[station.mReference] = pending->second;
                rest = 0.f;
                radio.mPending.erase(pending);
            }
            // The pause between two chains is of the time after the station has spoken
            const bool saying = std::any_of(mSayings.begin(), mSayings.end(),
                [&](const Saying& other) { return other.mReference == station.mReference; });
            if (saying)
                continue;
            rest -= frametime;
            if (rest > 0.f)
                continue;
            // What it says goes on through the topics the responses offer, as the tuned station's does (the topic of
            // a chain is kept for it)
            FalloutScript::FormId& chain = radio.mRoomTopic[station.mReference];
            FalloutScript::FormId next = 0;
            const std::vector<RadioPlayback::Line> steps = radioStep(station.mReference, chain, next);
            const bool said = !steps.empty();
            chain = said ? next : 0;
            rest = !said ? radioNothingRest : (next != 0 ? 0.f : radioRest);
            if (said)
            {
                std::vector<SaidLine> lines;
                for (const RadioPlayback::Line& step : steps)
                {
                    // (the songs are left out when the radio is set to dialogue only)
                    if (radio.mDialogueOnly && step.mSong)
                        continue;
                    SaidLine& line = lines.emplace_back();
                    line.mVoice = step.mVoice;
                    line.mText = step.mText;
                    line.mSeconds = step.mSeconds;
                    line.mTopic = step.mTopic;
                    line.mSpeakerBase = step.mSpeakerBase;
                    line.mRadio = true;
                    line.mSong = step.mSong;
                }
                if (!lines.empty())
                {
                    lines.back().mEnd = true;
                    startSaying(station.mReference, std::move(lines));
                }
            }
            logRadio(station.mName + " in the room says " + (said ? "a chain" : "nothing"));
        }

        // ---- The characters that have a station on (SetNPCRadio) say what it says, where they are ----
        // (a conversation that was asked for starts once for every character that plays the station, and is dropped after
        // them: left in the list it would start again each time one of them has finished)
        std::vector<decltype(radio.mPending)::key_type> dispatched;
        for (auto& [actor, entry] : radio.mNpc)
        {
            // A character that is not in a loaded cell is not heard
            const OFWorld::Ptr npc = findReference(actor);
            if (npc.isEmpty())
                continue;
            // A creature with a radio is heard within fCreatureRadioMax (times 1.1) of the player (FalloutNV.exe 0x835271);
            // beyond that it says nothing (what it would say is not kept: the station goes on without it)
            if (ESM::FormId base; baseOf(actor, base) && mStore.get<ESM4::Creature>().search(base) != nullptr)
            {
                const float reach = FalloutScript::Radio::creatureReach(static_cast<float>(gameSetting("fCreatureRadioMax", 2000.0)));
                if ((npc.getRefData().getPosition().asVec3() - player.getRefData().getPosition().asVec3()).length() > reach)
                    continue;
            }
            // A character that the player talks to says what the conversation says, not what its radio does
            if (inConversationWith(actor))
                continue;
            // A conversation that was asked for starts at once (what is said is cut off)
            if (const auto pending = radio.mPendingNpc.find(entry.mStation); pending != radio.mPendingNpc.end())
            {
                cutOffSaying(actor);
                entry.mTopic = pending->second;
                entry.mRest = 0.f;
                dispatched.push_back(pending->first);
            }
            const bool saying = std::any_of(
                mSayings.begin(), mSayings.end(), [&](const Saying& other) { return other.mReference == actor; });
            if (saying)
                continue;
            entry.mRest -= frametime;
            if (entry.mRest > 0.f)
                continue;
            FalloutScript::FormId next = 0;
            const std::vector<RadioPlayback::Line> steps = radioStep(entry.mStation, entry.mTopic, next);
            entry.mTopic = steps.empty() ? 0 : next;
            entry.mRest = steps.empty() ? radioNothingRest : (next != 0 ? 0.f : radioRest);
            std::vector<SaidLine> lines;
            for (const RadioPlayback::Line& step : steps)
            {
                // (the songs are left out when the radio is set to dialogue only)
                if (radio.mDialogueOnly && step.mSong)
                    continue;
                SaidLine& line = lines.emplace_back();
                line.mVoice = step.mVoice;
                line.mText = step.mText;
                line.mSeconds = step.mSeconds;
                line.mTopic = step.mTopic;
                line.mSpeakerBase = step.mSpeakerBase;
                line.mRadio = true;
                line.mSong = step.mSong;
            }
            if (!lines.empty())
            {
                lines.back().mEnd = true;
                startSaying(actor, std::move(lines));
            }
        }
        for (const auto station : dispatched)
            radio.mPendingNpc.erase(station);
        // (a conversation that was asked of a station that nobody plays any more is dropped)
        if (radio.mPending.size() > 64)
            radio.mPending.clear();
        if (radio.mPendingNpc.size() > 64)
            radio.mPendingNpc.clear();
    }
}
