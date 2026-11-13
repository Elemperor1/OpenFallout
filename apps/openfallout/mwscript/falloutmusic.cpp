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
#include <components/esm4/loadaspc.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loaddobj.hpp>
#include <components/esm4/loadmset.hpp>
#include <components/esm4/loadmusc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadregn.hpp>
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
        OFBase::SoundManager* sound = nullptr;
        if (OFBase::Environment::exists())
            sound = OFBase::Environment::get().getSoundManager();
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
        music.mSetLists.clear();
        music.mRestartLeft = -1.f;
        music.mClock = 0.f;
        music.mLayerSet = ESM::FormId();
        music.mLayer.reset();
        music.mLayerFreeAt = 0.f;
        music.mIntroSet = ESM::FormId();
        music.mIncidental.reset();
        music.mWasDead = false;
        music.mStemKey.clear();
        music.mStems.clear();
        music.mTitleStopped = false;
        music.mConfigured = false;
        music.mScriptBlocked = false;
        music.mMarkers.clear();
        if (music.mRecords)
            music.mRecords->clear();
    }

    void FalloutScripts::playStinger(Stinger kind)
    {
        // (the sounds and the log are the main thread's)
        if (std::this_thread::get_id() != mMainThread)
        {
            runOnMainThread([this, kind] { playStinger(kind); });
            return;
        }
        // The default music records of the game (DOBJ). The executable's callers of these were not found: the death of the
        // player, the completing of a quest and a level gained are what their names in the editor say ("Death Music",
        // "Success Music", "Level Up Music").
        ESM::FormId music;
        for (const ESM4::DefaultObj& objects : mStore.get<ESM4::DefaultObj>())
        {
            music = kind == Stinger::Death ? objects.mData.deathMusic
                : kind == Stinger::Success ? objects.mData.successMusic
                                           : objects.mData.levelUpMusic;
            break;
        }
        if (music.isZeroOrUnset())
            return;
        if (playMusic(music.toUint32()))
            logMusic(std::string("the music of ")
                + (kind == Stinger::Death         ? "a death"
                        : kind == Stinger::Success ? "a quest completed"
                                                   : "a level gained")
                + " plays");
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
        // The music of a death plays once when the player dies
        const bool dead = isPlayerDead();
        if (dead && !music.mWasDead)
            playStinger(Stinger::Death);
        music.mWasDead = dead;

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
        // Fights that are not the player's: characters near the player that fight each other, with a friend of the player
        // (a teammate, or one the player's faction stands by) among them
        for (const auto& [actor, target] : mCombat.fights())
        {
            if (actor == mPlayer || target == mPlayer)
                continue;
            const OFWorld::Ptr reference = findReference(actor);
            if (reference.isEmpty() || !reference.isInCell() || !reference.getRefData().isEnabled()
                || activeCells.find(reference.getCell()) == activeCells.end())
                continue;
            FalloutScript::Music::Fighter fighter;
            fighter.mInCombat = true;
            fighter.mDistance = (reference.getRefData().getPosition().asVec3() - at).length();
            fighter.mStrength = static_cast<float>(std::max(mActorValues.current(actor, AV::health), 0.0));
            if (fighter.mDistance <= music.mNearRange)
            {
                const auto friendOfPlayer = [this](FalloutScript::FormId other) {
                    if (mObjectState.teammate(other))
                        return true;
                    const auto relation = FalloutScript::relationOfActors(mCombat, mActors, *this, other, mPlayer);
                    return relation == FalloutScript::CombatState::Relation::Ally
                        || relation == FalloutScript::CombatState::Relation::Friend;
                };
                fighter.mAllyNear = friendOfPlayer(actor) || friendOfPlayer(target);
            }
            fighters.push_back(fighter);
        }
        if (!music.mConfigured)
        {
            // The settings are read once, when the files of the game are loaded
            music.mConfigured = true;
            const FalloutScript::Music::CombatSettings settings
                = combatSettings([this](const char* name, float fallback) {
                      return static_cast<float>(gameSetting(name, fallback));
                  });
            music.mNearRange = settings.mNearOuter;
            music.mCombat = FalloutScript::Music::CombatMusic(settings);
        }
        music.mClock += frametime;
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

        // ---- New Vegas's location sets and battle sets ----
        // The cell's own music record decides over the markers' sets; a cell that asks for no music (1NoMusic) has no music
        // of its record but keeps the sets of its markers (the doctor's house of Goodsprings has both)
        // (while a script track plays the markers and the controller are still followed, so that the set of the place is not
        // chosen again when the track is over; the mixer is not updated then)
        const bool locations = !placeHasTracks;
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
                        placed.mReference = reference->mId.toUint32();
                        markers.push_back(placed);
                    }
                    if (!markers.empty())
                        logMusic("the cell " + around->getId().toDebugString() + " has " + std::to_string(markers.size())
                            + " audio marker(s)");
                    found = music.mMarkers.emplace(around->getId(), std::move(markers)).first;
                }
                for (const MusicMarker& candidate : found->second)
                {
                    if (isDisabled(candidate.mReference))
                        continue;
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
        // The set of the place is one of the controller's, picked by the executable's rule (FalloutNV.exe 0x595560) when the
        // player comes in, and again when a set that does not loop was played to its end. A controller that names no location
        // set of its own has them by how its faction stands to the player: a set for an enemy, a friend, an ally, and one for
        // the rest. Returns how many sets the list had.
        const auto chooseSet = [&]() -> std::size_t {
            music.mSet = ESM::FormId();
            if (controller == nullptr)
                return 0;
            const std::vector<ESM::FormId>* wanted = &controller->mLocationSets;
            int list = 0;
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
                list = relation == Relation::Enemy ? 1 : relation == Relation::Friend ? 2 : relation == Relation::Ally ? 3 : 4;
                if (wanted->empty())
                {
                    wanted = &controller->mNeutralSets;
                    list = 4;
                }
            }
            std::vector<std::uint32_t> ids;
            for (const ESM::FormId id : *wanted)
                if (mStore.get<ESM4::MediaSet>().search(id) != nullptr)
                    ids.push_back(id.toUint32());
            if (ids.empty())
                return 0;
            // The controller keeps the order of its list from one pick to the next, as long as the list is the same
            std::vector<std::uint32_t>& kept = music.mSetLists[{ controllerId.toUint32(), list }];
            std::vector<std::uint32_t> a = kept;
            std::vector<std::uint32_t> b = ids;
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            if (a != b)
                kept = ids;
            music.mSet = ESM::FormId::fromUint32(FalloutScript::Music::pickSet(kept, [&music](int low, int high) {
                return std::uniform_int_distribution<int>(low, high)(music.mRandom);
            }));
            return ids.size();
        };
        if (controllerId != music.mController)
        {
            music.mController = controllerId;
            music.mSet = ESM::FormId();
            music.mBattleSet = ESM::FormId();
            music.mRestartLeft = -1.f;
            if (controller != nullptr)
            {
                const std::size_t count = chooseSet();
                logMusic("the player is in " + (controller->mEditorId.empty() ? formText(controllerId) : controller->mEditorId)
                    + ", " + std::to_string(count) + " location set(s)"
                    + (FalloutScript::Music::loopsSets(controller->mMediaFlags.loopingOptions) ? ", loops" : ", plays once"));
            }
        }
        else if (controller != nullptr && !music.mSet.isZeroOrUnset()
            && !FalloutScript::Music::loopsSets(controller->mMediaFlags.loopingOptions) && !music.mStems.empty()
            && music.mMixer.finished())
        {
            // The tracks of a set that does not loop played to their end: after the controller's delay (NAM4, none in the
            // data of the games) a set is picked again, which may be another one of its list
            if (music.mRestartLeft < 0.f)
            {
                const float delay = controller->mLocationDelay;
                music.mRestartLeft = std::isfinite(delay) && delay > 0.f ? std::min(delay, 600.f) : 0.f;
            }
            music.mRestartLeft -= frametime;
            if (music.mRestartLeft <= 0.f)
            {
                music.mRestartLeft = -1.f;
                chooseSet();
                music.mStemKey.clear();
                music.mMixer.restart();
                logMusic("the set was played to its end, " + (music.mSet.isZeroOrUnset() ? std::string("none is left") : "picked again: " + formText(music.mSet)));
            }
        }
        else
            music.mRestartLeft = -1.f;
        // The sound region of the place (the region that the acoustic space of the cell names, the one whose sounds are heard
        // there): where no audio marker has music, its sound entry has the battle sets of a fight (RDSB) and the incidental
        // set whose stingers play while nothing else does (RDSI) (FalloutNV.exe 0x82FB70: the controller of the marker is
        // asked first, the region after it)
        const ESM4::Region* soundRegion = nullptr;
        if (controller == nullptr)
            if (const ESM::FormId space = cell->getAcousticSpace(); !space.isZeroOrUnset())
                if (const ESM4::AcousticSpace* acoustic = mStore.get<ESM4::AcousticSpace>().search(space);
                    acoustic != nullptr && !acoustic->mSoundRegion.isZeroOrUnset())
                    soundRegion = mStore.get<ESM4::Region>().search(acoustic->mSoundRegion);
        const std::vector<ESM::FormId>* battleList = nullptr;
        if (controller != nullptr && !controller->mBattleSets.empty())
            battleList = &controller->mBattleSets;
        else if (soundRegion != nullptr && !soundRegion->mBattleSets.empty())
            battleList = &soundRegion->mBattleSets;
        const bool wantBattle = combat && battleList != nullptr;
        if (!wantBattle)
            music.mBattleSet = ESM::FormId();
        else
        {
            // A set that was picked for another place (the player crossed into a sound region whose fights have other
            // sets, in the middle of a fight) is not kept
            if (!music.mBattleSet.isZeroOrUnset()
                && std::find(battleList->begin(), battleList->end(), music.mBattleSet) == battleList->end())
                music.mBattleSet = ESM::FormId();
            if (music.mBattleSet.isZeroOrUnset())
            {
                std::vector<ESM::FormId> sets;
                for (const ESM::FormId id : *battleList)
                    if (mStore.get<ESM4::MediaSet>().search(id) != nullptr)
                        sets.push_back(id);
                if (!sets.empty())
                    music.mBattleSet
                        = sets[std::uniform_int_distribution<std::size_t>(0, sets.size() - 1)(music.mRandom)];
            }
        }

        const ESM4::MediaSet* locationSet = music.mSet.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaSet>().search(music.mSet);
        const ESM4::MediaSet* battleSet
            = music.mBattleSet.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaSet>().search(music.mBattleSet);
        // The default battle music (DOBJ) is for a fight in a place that has no battle set of its own and whose location set
        // has no battle track of its own (the set of a dungeon has one): with either, that is the music of the fight, and the
        // default track does not play over it
        const bool setHasBattleTrack
            = locations && locationSet != nullptr && locationSet->mSetType == 2 && !locationSet->mSet2.empty();
        const bool defaultBattle = battleHasTracks && battleSet == nullptr && !setHasBattleTrack;

        // ---- Fallout 3's music, and the tracks of the records that New Vegas gives a file ----
        {
            FalloutScript::Music::DirectorInput input;
            input.mSeconds = frametime;
            // (the music of a fight that a set of the region has leaves none of the place under it)
            input.mExplore = placeHasTracks && !(combat && battleSet != nullptr) ? placeMusic.toUint32() : 0;
            input.mBattle = defaultBattle ? battleMusic.toUint32() : 0;
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

        bool day = true;
        std::optional<int> layer;
        std::string key;
        // (the sets of a controller; and the battle set of a region, which the day does not change)
        if ((locations && controller != nullptr) || battleSet != nullptr)
        {
            const bool useDefaults = controller == nullptr || controller->mMediaFlags.loopingOptions >= 4;
            day = FalloutScript::Music::isDay(world->getTimeStamp().getHour(),
                controller != nullptr ? controller->mDayStart : 0, controller != nullptr ? controller->mNightStart : 0,
                useDefaults);
            if (battleSet != nullptr)
                key = "b" + formText(battleSet->mId);
            else if (locationSet != nullptr && locations)
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
                    // A layer that the place asks for after the one that is heard is let in only when the time of the set (DNAM,
                    // 6 s) has run since the last change (FalloutNV.exe 0x597E04: the executable's time of the next change is
                    // set at every start of a layer, to the time now plus DNAM)
                    const bool sameState
                        = music.mLayerSet == locationSet->mId && music.mLayerDay == day && music.mLayerCombat == combat;
                    if (sameState && layer != music.mLayer && music.mClock < music.mLayerFreeAt)
                        layer = music.mLayer;
                    else if (!sameState || layer != music.mLayer)
                        music.mLayerFreeAt = music.mClock + std::max(0.f, locationSet->mTime1);
                    music.mLayerSet = locationSet->mId;
                    music.mLayerDay = day;
                    music.mLayerCombat = combat;
                    music.mLayer = layer;
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
            music.mMixer.restart();
            // The tracks of a location set or of a dungeon set loop when the controller's looping option is 0; with any
            // other they play once, and the controller picks a set again when they are over (FalloutNV.exe 0x598000)
            const bool loops = controller == nullptr || FalloutScript::Music::loopsSets(controller->mMediaFlags.loopingOptions);
            const auto stem = [&](const std::string& name, float db, bool audible, bool loop = true) {
                const auto files = OFSound::falloutMusicFiles(name, *resources->getVFS());
                if (files.empty())
                    return;
                OFSound::MusicMixer::Stem entry;
                entry.mFile = files.front();
                entry.mGain = audible ? gainOfDb(std::min(db, 0.f)) : 0.f;
                entry.mLoop = loop;
                music.mStems.push_back(std::move(entry));
            };
            if (battleSet != nullptr)
            {
                // The set fades in and out over its own time (ENAM, 0x598550 reads it at +0xB0 for both); 3 seconds when it
                // gives none
                music.mStemFade = battleSet->mTime2 > 0.f ? battleSet->mTime2 : 3.f;
                stem(battleSet->mSet2, battleSet->mLevel8, true);
            }
            // (while the default battle track is the music of the fight, the location set is quiet)
            else if (locationSet != nullptr && locations && !(combat && defaultBattle))
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
                    // (a set fades in and out over its own time, FNAM: 6 or 9 s in the data of New Vegas)
                    music.mStemFade = locationSet->mTime3 > 0.f ? locationSet->mTime3 : 1.f;
                    for (int i = 0; i < 3; ++i)
                        if ((enabled & (1 << i)) != 0)
                            stem(*files[static_cast<std::size_t>(i)], levels[static_cast<std::size_t>(i)], layer == i, loops);
                }
                else if (locationSet->mSetType == 2)
                {
                    // A set of a dungeon: its battle track in a fight, otherwise the one for exploring
                    if (combat && !locationSet->mSet2.empty())
                    {
                        music.mStemFade = locationSet->mTime2 > 0.f ? locationSet->mTime2 : 3.f;
                        stem(locationSet->mSet2, locationSet->mLevel8, true);
                    }
                    else
                        stem(locationSet->mSet3, locationSet->mLevel9, true, loops);
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

        // ---- The open and close sounds of the sets that have them (the battle sets and the sets of dungeons) ----
        // A set that starts plays its open sound (HNAM) and one that stops its close sound (INAM): a sound record of the
        // folder of stingers, heard as music
        {
            ESM::FormId active;
            if (battleSet != nullptr)
                active = battleSet->mId;
            else if (locationSet != nullptr && locations && locationSet->mSetType == 2)
                active = locationSet->mId;
            if (active != music.mIntroSet)
            {
                const auto play = [&](ESM::FormId id, const char* what, const ESM4::MediaSet& set) {
                    if (id.isZeroOrUnset())
                        return;
                    sound->playFalloutSound(id, OFWorld::ConstPtr(), 1.f, OFSound::Type::Music, OFSound::PlayMode::NoEnv);
                    logMusic(std::string(what) + " sound of " + describe(set) + " plays");
                };
                if (const ESM4::MediaSet* old
                    = music.mIntroSet.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaSet>().search(music.mIntroSet))
                    play(old->mSoundOutro, "the close", *old);
                if (const ESM4::MediaSet* started
                    = active.isZeroOrUnset() ? nullptr : mStore.get<ESM4::MediaSet>().search(active))
                    play(started->mSoundIntro, "the open", *started);
                music.mIntroSet = active;
            }
        }

        // ---- The stingers of the incidental set of the region ----
        // While nothing else plays and no fight goes on, the open sound of the set is heard by day and its close sound by
        // night, one after the other with the waits of the set
        const ESM4::MediaSet* incidental = soundRegion == nullptr || soundRegion->mIncidentalSet.isZeroOrUnset()
            ? nullptr
            : mStore.get<ESM4::MediaSet>().search(soundRegion->mIncidentalSet);
        const bool quiet = music.mStems.empty() && !scriptMusic && !combat
            && (music.mTrack == 0 || !sound->isMusicTrackPlaying(music.mTrack));
        if (incidental != nullptr && incidental->mSetType == 3 && quiet)
        {
            FalloutScript::Music::IncidentalTimes times;
            times.mDayShortest = incidental->mTime1;
            times.mNightShortest = incidental->mTime2;
            times.mDayLongest = incidental->mTime3;
            times.mNightLongest = incidental->mTime4;
            const bool daytime = FalloutScript::Music::isDay(world->getTimeStamp().getHour(), 0, 0, true);
            const auto which = music.mIncidental.update(frametime, daytime, times, [&music] {
                return std::uniform_real_distribution<double>(0.0, 1.0)(music.mRandom);
            });
            if (which != FalloutScript::Music::Incidental::Sound::None)
            {
                const ESM::FormId stinger = which == FalloutScript::Music::Incidental::Sound::Open
                    ? incidental->mSoundIntro
                    : incidental->mSoundOutro;
                if (!stinger.isZeroOrUnset())
                {
                    sound->playFalloutSound(
                        stinger, OFWorld::ConstPtr(), 1.f, OFSound::Type::Music, OFSound::PlayMode::NoEnv);
                    logMusic("a stinger of " + describe(*incidental) + " plays");
                }
            }
        }

        if (scriptMusic)
            return;
        music.mMixer.update(*sound, music.mStems, music.mStemFade);
    }
}
