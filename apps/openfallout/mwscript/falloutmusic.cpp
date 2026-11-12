#include "falloutscripts.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <random>
#include <thread>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadaloc.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loaddobj.hpp>
#include <components/esm4/loadmset.hpp>
#include <components/esm4/loadmusc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/timestamp.hpp"

namespace OFScript
{
    namespace
    {
        using FalloutScript::Music::gainOfDb;

        /// How many lines about the music the log keeps (each change of the music is one)
        constexpr int musicLogLimit = 80;

        /// The settings of the games' combat music (the defaults are the ones the executables register; the plugins
        /// hold the ones that were changed)
        template <class Get>
        FalloutScript::Music::CombatSettings combatSettings(const Get& get)
        {
            FalloutScript::Music::CombatSettings settings;
            settings.mUpdateTime = get("fCombatMusicUpdateTime", settings.mUpdateTime);
            settings.mStopTime = get("fCombatMusicStopTime", settings.mStopTime);
            settings.mNearInner = get("fCombatMusicNearCombatInnerRadius", settings.mNearInner);
            settings.mNearOuter = get("fCombatMusicNearCombatOuterRadius", settings.mNearOuter);
            settings.mTargetedThreatRatio = get("fCombatMusicPlayerTargetedThreatRatio", settings.mTargetedThreatRatio);
            settings.mNearStrengthMult = get("fCombatMusicPlayerNearStrengthMult", settings.mNearStrengthMult);
            return settings;
        }

        std::string formText(ESM::FormId id)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%08X", id.toUint32());
            return text;
        }

        std::string describe(const ESM4::MediaSet& set)
        {
            return set.mEditorId.empty() ? formText(set.mId) : set.mEditorId;
        }
    }

    void FalloutScripts::logMusic(const std::string& text)
    {
        if (mMusicPlayback.mLogged >= musicLogLimit)
            return;
        ++mMusicPlayback.mLogged;
        Log(Debug::Info) << "Music: " << text;
    }

    void FalloutScripts::resetMusic()
    {
        MusicPlayback& music = mMusicPlayback;
        OFBase::SoundManager* sound = OFBase::Environment::exists() ? OFBase::Environment::get().getSoundManager() : nullptr;
        if (sound != nullptr)
        {
            if (music.mTrack != 0)
                sound->stopMusicTrack(music.mTrack);
            music.mMixer.stopAll(*sound, 0.f);
        }
        music.mMixer.forget();
        music.mTrack = 0;
        if (music.mScriptTrack != 0 && sound != nullptr)
            sound->stopMusicTrack(music.mScriptTrack);
        music.mScriptTrack = 0;
        music.mCombat.reset();
        music.mDirector.reset();
        music.mController = ESM::FormId();
        music.mSet = ESM::FormId();
        music.mBattleSet = ESM::FormId();
        music.mStemKey.clear();
        music.mStems.clear();
        music.mTitleStopped = false;
        music.mConfigured = false;
        music.mScriptBlocked = false;
        music.mMarkers.clear();
        if (music.mRecords)
            music.mRecords->clear();
    }

    void FalloutScripts::updateMusic(float frametime)
    {
        if (!OFBase::Environment::exists())
            return;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        OFBase::World* world = OFBase::Environment::get().getWorld();
        const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem();
        if (sound == nullptr || world == nullptr || resources == nullptr || !sound->isEnabled()
            || std::this_thread::get_id() != mMainThread)
            return;
        MusicPlayback& music = mMusicPlayback;
        {
            const std::lock_guard lock(mMusicMutex);
            if (!music.mRecords)
                music.mRecords = std::make_unique<OFSound::FalloutMusicRecords>(*resources->getVFS(), mStore);
        }
        const OFWorld::Ptr player = world->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell() || mPlayer == 0)
            return;
        const OFWorld::Cell* cell = player.getCell()->getCell();
        if (cell == nullptr)
            return;

        // The music of the main menu does not go on into the game
        if (!music.mTitleStopped)
        {
            music.mTitleStopped = true;
            if (sound->isMusicPlaying() && sound->getMusicType() == OFSound::MusicType::Normal)
                sound->stopMusic();
        }

        // The default music records of the game (DOBJ): the one of a place that names none, and of a fight
        ESM::FormId defaultMusic;
        ESM::FormId battleMusic;
        for (const ESM4::DefaultObj& objects : mStore.get<ESM4::DefaultObj>())
        {
            defaultMusic = objects.mData.defaultMusic;
            battleMusic = objects.mData.battleMusic;
            break;
        }

        // ---- The fights ----
        namespace AV = FalloutScript::ActorValue;
        const osg::Vec3f at = player.getRefData().getPosition().asVec3();
        std::vector<FalloutScript::Music::Fighter> fighters;
        const auto& activeCells = world->getActiveCells();
        for (const FalloutScript::FormId attacker : mCombat.attackersOf(mPlayer))
        {
            const OFWorld::Ptr reference = findReference(attacker);
            if (reference.isEmpty() || !reference.isInCell() || !reference.getRefData().isEnabled())
                continue;
            // A fighter whose cell is not loaded around the player is far from the fight (it has lost the player)
            if (activeCells.find(reference.getCell()) == activeCells.end())
                continue;
            FalloutScript::Music::Fighter fighter;
            fighter.mHuntsPlayer = true;
            fighter.mInCombat = true;
            fighter.mDistance = (reference.getRefData().getPosition().asVec3() - at).length();
            fighter.mStrength = static_cast<float>(std::max(mActorValues.current(attacker, AV::health), 0.0));
            fighters.push_back(fighter);
        }
        if (!music.mConfigured)
        {
            // The settings are read once, when the files of the game are loaded
            music.mConfigured = true;
            music.mCombat = FalloutScript::Music::CombatMusic(combatSettings([this](const char* name, float fallback) {
                return static_cast<float>(gameSetting(name, fallback));
            }));
        }
        const bool wasInCombat = music.mCombat.on();
        const bool combat = music.mCombat.update(
            frametime, static_cast<float>(std::max(mActorValues.current(mPlayer, AV::health), 0.0)), fighters);
        if (combat != wasInCombat)
            logMusic(combat ? "combat music on" : "combat music off");

        // A track that a script plays (PlayMusic) has the music; the rest is quiet, and comes back after it
        if (music.mScriptTrack != 0 && !sound->isMusicTrackPlaying(music.mScriptTrack))
            music.mScriptTrack = 0;
        const bool scriptMusic = music.mScriptTrack != 0;
        if (scriptMusic != music.mScriptBlocked)
        {
            music.mScriptBlocked = scriptMusic;
            if (scriptMusic)
            {
                if (music.mTrack != 0)
                    sound->fadeMusicTrack(music.mTrack, 0.f, 1.f, true);
                music.mTrack = 0;
                music.mMixer.stopAll(*sound, 1.f);
                music.mStemKey.clear();
                music.mStems.clear();
            }
        }

        // ---- What the place asks for ----
        ESM::FormId placeMusic = cell->getMusic();
        if (placeMusic.isZeroOrUnset())
        {
            // A cell of Fallout 3 can ask for a kind of music (XCMT) instead of a record
            const char* kind = cell->getMusicType() == 1 ? "DefaultPublic" : (cell->getMusicType() == 2 ? "DefaultDungeon" : nullptr);
            const OFSound::FalloutMusic* byKind = kind != nullptr ? music.mRecords->find(kind) : nullptr;
            placeMusic = byKind != nullptr ? byKind->mId : defaultMusic;
        }
        const OFSound::FalloutMusic* place = music.mRecords->find(placeMusic);
        const bool placeHasTracks = place != nullptr && !place->mFiles.empty();
        const OFSound::FalloutMusic* battle = music.mRecords->find(battleMusic);
        const bool battleHasTracks = battle != nullptr && !battle->mFiles.empty();

        // ---- Fallout 3's music, and the tracks of the records that New Vegas gives a file ----
        {
            FalloutScript::Music::DirectorInput input;
            input.mSeconds = frametime;
            input.mExplore = placeHasTracks ? placeMusic.toUint32() : 0;
            input.mBattle = battleHasTracks ? battleMusic.toUint32() : 0;
            input.mCombat = combat;
            input.mBlocked = scriptMusic;
            input.mPlaying = music.mTrack != 0 && sound->isMusicTrackPlaying(music.mTrack);
            input.mLoops = music.mTrackLoops;
            const FalloutScript::Music::DirectorAction action = music.mDirector.update(input, [&music] {
                return std::uniform_real_distribution<double>(0.0, 1.0)(music.mRandom);
            });
            using Kind = FalloutScript::Music::DirectorAction::Kind;
            if (action.mKind == Kind::FadeOut && music.mTrack != 0)
            {
                sound->fadeMusicTrack(music.mTrack, 0.f, action.mFade, true);
                logMusic("fades out the track of " + std::to_string(action.mFade) + " s");
            }
            else if (action.mKind == Kind::Start)
            {
                OFSound::FalloutMusic* record = music.mRecords->find(ESM::FormId::fromUint32(action.mMusic));
                if (record != nullptr && !record->mFiles.empty())
                {
                    const auto random = [&music](int low, int high) {
                        return std::uniform_int_distribution<int>(low, high)(music.mRandom);
                    };
                    if (!record->mStarted)
                    {
                        record->mStarted = true;
                        record->mLast = FalloutScript::Music::firstTrack(record->mFiles.size(), random);
                    }
                    if (const auto index
                        = FalloutScript::Music::nextTrack(record->mFiles.size(), record->mLast, random))
                    {
                        const VFS::Path::Normalized& file = record->mFiles[*index];
                        music.mTrack = sound->startMusicTrack(file, gainOfDb(record->mDb), action.mFade, record->mLoop);
                        music.mTrackLoops = record->mLoop;
                        logMusic(record->mEditorId + " starts " + std::string(file.value()) + " (" + std::to_string(*index + 1)
                            + " of " + std::to_string(record->mFiles.size()) + ")"
                            + (record->mLoop ? ", loops" : ""));
                    }
                }
            }
        }

        // ---- New Vegas's location sets and battle sets ----
        // The cell's own music record decides over the markers' sets; a cell that asks for no music (1NoMusic) has no music
        // of its record but keeps the sets of its markers (the doctor's house of Goodsprings has both)
        const bool locations = !placeHasTracks && !scriptMusic;
        const MusicMarker* marker = nullptr;
        float fraction = 2.f;
        if (locations)
        {
            for (const OFWorld::CellStore* store : world->getActiveCells())
            {
                const OFWorld::Cell* around = store->getCell();
                if (around == nullptr)
                    continue;
                auto found = music.mMarkers.find(around->getId());
                if (found == music.mMarkers.end())
                {
                    std::vector<MusicMarker> markers;
                    for (const ESM4::Reference* reference : mStore.get<ESM4::Reference>().getByCell(around->getId()))
                    {
                        if (!reference->mAudioMarker.mIsMarker || reference->mAudioLocation.isZeroOrUnset()
                            || reference->mAudioMarker.mRadius <= 0.f)
                            continue;
                        MusicMarker placed;
                        placed.mPosition = reference->mPos.asVec3();
                        placed.mRadius = reference->mAudioMarker.mRadius;
                        placed.mController = reference->mAudioLocation;
                        markers.push_back(placed);
                    }
                    if (!markers.empty())
                        logMusic("the cell " + around->getId().toDebugString() + " has " + std::to_string(markers.size())
                            + " audio marker(s)");
                    found = music.mMarkers.emplace(around->getId(), std::move(markers)).first;
                }
                for (const MusicMarker& candidate : found->second)
                {
                    const float distance = (candidate.mPosition - at).length();
                    if (distance >= candidate.mRadius)
                        continue;
                    // The marker the player is deepest in wins
                    const float share = distance / candidate.mRadius;
                    if (share < fraction)
                    {
                        fraction = share;
                        marker = &candidate;
                    }
                }
            }
        }

        const ESM::FormId controllerId = marker != nullptr ? marker->mController : ESM::FormId();
        const ESM4::MediaLocationController* controller
            = controllerId.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaLocationController>().search(controllerId);
        if (controllerId != music.mController)
        {
            music.mController = controllerId;
            music.mSet = ESM::FormId();
            music.mBattleSet = ESM::FormId();
            if (controller != nullptr)
            {
                // The set of the place is one of the controller's, picked at random when the player comes in
                // A controller that names no location set of its own has them by how its faction stands to the player: a
                // set for an enemy, a friend, an ally, and one for the rest
                const std::vector<ESM::FormId>* wanted = &controller->mLocationSets;
                if (wanted->empty())
                {
                    using Relation = FalloutScript::CombatState::Relation;
                    const Relation relation = FalloutScript::relationOfFaction(mCombat, mActors, *this,
                        controller->mConditionalFaction.isZeroOrUnset() ? 0 : controller->mConditionalFaction.toUint32(),
                        mPlayer);
                    wanted = relation == Relation::Enemy ? &controller->mEnemySets
                        : relation == Relation::Friend   ? &controller->mFriendSets
                        : relation == Relation::Ally     ? &controller->mAllySets
                                                         : &controller->mNeutralSets;
                    if (wanted->empty())
                        wanted = &controller->mNeutralSets;
                }
                std::vector<ESM::FormId> sets;
                for (const ESM::FormId id : *wanted)
                    if (mStore.get<ESM4::MediaSet>().search(id) != nullptr)
                        sets.push_back(id);
                if (!sets.empty())
                    music.mSet = sets[std::uniform_int_distribution<std::size_t>(0, sets.size() - 1)(music.mRandom)];
                logMusic("the player is in " + (controller->mEditorId.empty() ? formText(controllerId) : controller->mEditorId)
                    + ", " + std::to_string(sets.size()) + " location set(s)");
            }
        }
        const bool wantBattle = combat && controller != nullptr && !controller->mBattleSets.empty();
        if (!wantBattle)
            music.mBattleSet = ESM::FormId();
        else if (music.mBattleSet.isZeroOrUnset())
        {
            std::vector<ESM::FormId> sets;
            for (const ESM::FormId id : controller->mBattleSets)
                if (mStore.get<ESM4::MediaSet>().search(id) != nullptr)
                    sets.push_back(id);
            if (!sets.empty())
                music.mBattleSet = sets[std::uniform_int_distribution<std::size_t>(0, sets.size() - 1)(music.mRandom)];
        }

        const ESM4::MediaSet* locationSet = music.mSet.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaSet>().search(music.mSet);
        const ESM4::MediaSet* battleSet
            = music.mBattleSet.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaSet>().search(music.mBattleSet);
        bool day = true;
        std::optional<int> layer;
        std::string key;
        if (locations && controller != nullptr)
        {
            const bool useDefaults = controller->mMediaFlags.loopingOptions >= 4;
            day = FalloutScript::Music::isDay(
                world->getTimeStamp().getHour(), controller->mDayStart, controller->mNightStart, useDefaults);
            if (battleSet != nullptr)
                key = "b" + formText(battleSet->mId);
            else if (locationSet != nullptr)
            {
                if (locationSet->mSetType == 1)
                {
                    const std::array<float, 3> boundaries = day
                        ? std::array<float, 3>{ locationSet->mBoundaryDayOuter, locationSet->mBoundaryDayMiddle,
                            locationSet->mBoundaryDayInner }
                        : std::array<float, 3>{ locationSet->mBoundaryNightOuter, locationSet->mBoundaryNightMiddle,
                            locationSet->mBoundaryNightInner };
                    const std::uint8_t enabled = static_cast<std::uint8_t>((locationSet->mEnabled >> (day ? 0 : 3)) & 7);
                    layer = FalloutScript::Music::layerAt(fraction, enabled, boundaries);
                }
                key = "l" + formText(locationSet->mId) + (day ? "d" : "n")
                    + std::to_string(layer.value_or(-1)) + (combat ? "c" : "-");
            }
        }

        if (key != music.mStemKey)
        {
            music.mStemKey = key;
            music.mStems.clear();
            music.mStemFade = 1.f;
            const auto stem = [&](const std::string& name, float db, bool audible) {
                const auto files = OFSound::falloutMusicFiles(name, *resources->getVFS());
                if (files.empty())
                    return;
                OFSound::MusicMixer::Stem entry;
                entry.mFile = files.front();
                entry.mGain = audible ? gainOfDb(std::min(db, 0.f)) : 0.f;
                music.mStems.push_back(std::move(entry));
            };
            if (battleSet != nullptr)
            {
                music.mStemFade = 3.f;
                stem(battleSet->mSet2, battleSet->mLevel8, true);
            }
            else if (locationSet != nullptr && locations)
            {
                if (locationSet->mSetType == 1)
                {
                    // All the layers of the half of the day play in step; the one the place asks for is heard
                    const std::array<const std::string*, 3> files = day
                        ? std::array<const std::string*, 3>{ &locationSet->mSet2, &locationSet->mSet3, &locationSet->mSet4 }
                        : std::array<const std::string*, 3>{ &locationSet->mSet5, &locationSet->mSet6, &locationSet->mSet7 };
                    const std::array<float, 3> levels = day
                        ? std::array<float, 3>{ locationSet->mLevel8, locationSet->mLevel9, locationSet->mLevel0 }
                        : std::array<float, 3>{ locationSet->mLevelA, locationSet->mLevelB, locationSet->mLevelC };
                    const std::uint8_t enabled = static_cast<std::uint8_t>((locationSet->mEnabled >> (day ? 0 : 3)) & 7);
                    for (int i = 0; i < 3; ++i)
                        if ((enabled & (1 << i)) != 0)
                            stem(*files[static_cast<std::size_t>(i)], levels[static_cast<std::size_t>(i)], layer == i);
                }
                else if (locationSet->mSetType == 2)
                {
                    // A set of a dungeon: its battle track in a fight, otherwise the one for exploring
                    if (combat && !locationSet->mSet2.empty())
                    {
                        music.mStemFade = 3.f;
                        stem(locationSet->mSet2, locationSet->mLevel8, true);
                    }
                    else
                        stem(locationSet->mSet3, locationSet->mLevel9, true);
                }
                else if (locationSet->mSetType == 0)
                    stem(locationSet->mSet2, locationSet->mLevel8, true);
            }
            if (!key.empty())
            {
                std::string files;
                for (const auto& entry : music.mStems)
                    files += (files.empty() ? "" : ", ") + std::string(entry.mFile.value()) + " x" + std::to_string(entry.mGain).substr(0, 4);
                const ESM4::MediaSet* shown = battleSet != nullptr ? battleSet : locationSet;
                logMusic((battleSet != nullptr ? "battle set " : "location set ") + (shown != nullptr ? describe(*shown) : std::string())
                    + (day ? ", day" : ", night") + (layer ? ", layer " + std::to_string(*layer) : std::string())
                    + ": " + (files.empty() ? std::string("no files") : files));
            }
        }
        if (scriptMusic)
            return;
        music.mMixer.update(*sound, music.mStems, music.mStemFade);
    }
}
