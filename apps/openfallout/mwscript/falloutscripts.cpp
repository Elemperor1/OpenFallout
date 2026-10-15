#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>

#include <components/debug/debuglog.hpp>
#include <components/esm/refid.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/creaturemodel.hpp>
#include <components/esm4/levelled.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadavif.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadflst.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadgmst.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadmesg.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/loadperk.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadrace.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/esm4/loadsoun.hpp>
#include <components/esm4/loadvtyp.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/wornarmor.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/messagetext.hpp>
#include <components/falloutscript/playercontrols.hpp>
#include <components/misc/convert.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/nif/extra.hpp>
#include <components/resource/niffilemanager.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/manager.hpp>
#include <components/vfs/recursivedirectoryiterator.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwclass/esm4base.hpp"
#include "../mwclass/esm4creature.hpp"
#include "../mwclass/esm4npc.hpp"
#include "../mwclass/esm4sources.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/falloutpackages.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/scene.hpp"
#include "../mwworld/worldmodel.hpp"

namespace OFScript
{
    namespace
    {
        constexpr std::string_view newVegasMaster = "falloutnv.esm";
        constexpr std::string_view fallout3Master = "fallout3.esm";

        /// A character or creature of Fallout (their classes are not those of Morrowind's actors, so isActor does not
        /// tell them)
        bool isCharacterOrCreature(const OFWorld::Ptr& ptr)
        {
            return !ptr.isEmpty()
                && (ptr.getType() == ESM4::Npc::sRecordId || ptr.getType() == ESM4::Creature::sRecordId);
        }

        /// The id of the record as the store keys it, empty for ids that no record can have
        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
        }

        /// A name of a content file as IsDLCInstalled compares it: lower case, without the spaces and the extension
        std::string pluginKey(std::string_view name)
        {
            std::string key;
            for (const char c : name)
                if (c != ' ')
                    key += Misc::StringUtils::toLower(c);
            const std::size_t dot = key.find_last_of('.');
            if (dot != std::string::npos
                && (key.compare(dot, std::string::npos, ".esm") == 0
                    || key.compare(dot, std::string::npos, ".esp") == 0))
                key.resize(dot);
            return key;
        }

        /// The number a script knows a form by. A form that has none (it is not set, it is one the world made for
        /// itself or its plugin is out of the range of the games) is 0, which names nothing, where converting it would
        /// throw and end the frame of every script after it.
        FalloutScript::FormId numberOf(const ESM::FormId& form)
        {
            if (form.isZeroOrUnset() || !form.hasContentFile() || form.mContentFile > 0xfe)
                return 0;
            return form.toUint32();
        }

        /// The value a global variable has: a short or a long is a whole number, the types are 's', 'l' and 'f'
        double fitToType(std::uint8_t type, double value)
        {
            if (std::isnan(value))
                return 0;
            if (type == 'f')
                return static_cast<float>(value);
            const double whole = std::trunc(value);
            if (type == 's')
                return static_cast<std::int16_t>(std::clamp(whole, -32768.0, 32767.0));
            return static_cast<std::int32_t>(std::clamp(whole, -2147483648.0, 2147483647.0));
        }
    }

    FalloutScript::Game chooseGame(const std::vector<std::string>& contentFiles)
    {
        bool newVegas = false;
        bool fallout3 = false;
        for (const std::string& file : contentFiles)
        {
            newVegas = newVegas || Misc::StringUtils::ciEqual(file, newVegasMaster);
            fallout3 = fallout3 || Misc::StringUtils::ciEqual(file, fallout3Master);
        }
        return fallout3 && !newVegas ? FalloutScript::Game::Fallout3 : FalloutScript::Game::NewVegas;
    }

    FalloutScript::FormId playerReferenceOf(const std::vector<std::string>& contentFiles)
    {
        const int plugin = OFMechanics::falloutFirstPlugin(contentFiles);
        return plugin >= 0 ? (static_cast<FalloutScript::FormId>(plugin) << 24) | FalloutScript::playerReference
                           : FalloutScript::playerReference;
    }

    FalloutScripts::FalloutScripts(
        const OFWorld::ESMStore& store, FalloutScript::Game game, FalloutScript::FormId player)
        : mStore(store)
        , mCommands(FalloutScript::vanillaCommands(game))
        , mInterpreter(*this, mCommands)
        , mConditions(mInterpreter)
        , mQuests(mInterpreter, *this)
        , mObjects(mInterpreter, [this](FalloutScript::FormId id) { return findScript(id); })
        , mActors(*this)
        , mEquipment(*this, mActors)
        , mActorValues(this)
        , mReferenceState(*this)
        , mFormLists(*this)
        , mChargen(game)
        , mDialogue(mInterpreter, mConditions, mQuests, *this)
        , mDialogueRandom(std::random_device()())
        , mPlayer(player)
        , mMainThread(std::this_thread::get_id())
        , mRandom(std::random_device()())
        , mCombatRandom(std::random_device()())
    {
        FalloutScript::addQuestCommands(mInterpreter, mQuests);
        FalloutScript::addObjectCommands(
            mInterpreter, mObjects, *this, [this] { return std::uniform_int_distribution<int>(0, 99)(mRandom); });
        FalloutScript::addPackageCommands(mInterpreter, *this);
        FalloutScript::addPlacementCommands(mInterpreter, *this);
        FalloutScript::addMessageCommands(mInterpreter, *this);
        FalloutScript::addPlayerControlCommands(mInterpreter, *this);
        FalloutScript::addActorCommands(mInterpreter, mActors);
        FalloutScript::addEquipmentCommands(mInterpreter, mEquipment);
        FalloutScript::addCombatCommands(mInterpreter, mCombat, mActors, *this);
        // The container of an item is the holder its script was last run for
        mInterpreter.setHandler("GetContainer", [this](FalloutScript::CallContext& call) {
            const auto found = mItemHolders.find(call.mReference);
            return found == mItemHolders.end() ? 0.0 : static_cast<double>(found->second);
        });
        FalloutScript::addReferenceStateCommands(mInterpreter, mReferenceState);
        FalloutScript::addFormListCommands(mInterpreter, mFormLists);
        FalloutScript::addPlayerProgressCommands(mInterpreter, mProgress);
        FalloutScript::addActorValueCommands(mInterpreter, mActorValues, mActors, mProgress);
        FalloutScript::addActorTraitCommands(mInterpreter, *this);
        FalloutScript::addDialogueCommands(mInterpreter, mDialogue);
        FalloutScript::addChargenCommands(mInterpreter, mChargen, [this] { beginChargenPage(); });
        mDialogue.setGameHours([this] { return std::floor(gameDaysPassed()) * 24.0 + gameHour(); });
        mDialogue.setRandom([this](std::size_t count) {
            return std::uniform_int_distribution<std::size_t>(0, count - 1)(mDialogueRandom);
        });
        // The speaker is the object the command is called on. A conversation with the player opens the interface,
        // which is for the thread of the frame; one with another character only runs what the response does.
        mInterpreter.setHandler("StartConversation", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId speaker = call.mReference;
            const FalloutScript::FormId target = FalloutScript::formArgument(call, 0);
            const FalloutScript::FormId topic = FalloutScript::formArgument(call, 1);
            if (speaker == 0 || speaker == mPlayer)
                return 0.0;
            if (target == 0 || target == mPlayer)
            {
                runOnMainThread([this, speaker, topic] {
                    const OFWorld::Ptr ptr = findReference(speaker);
                    if (!ptr.isEmpty())
                        talkTo(ptr, topic);
                });
            }
            else
                converse(speaker, target, topic);
            return 0.0;
        });
        mInterpreter.setHandler("Say", [this](FalloutScript::CallContext& call) {
            sayTopic(
                call.mReference, FalloutScript::formArgument(call, 0), 0, FalloutScript::intArgument(call, 1) != 0);
            return 0.0;
        });
        mInterpreter.setHandler("SayTo", [this](FalloutScript::CallContext& call) {
            sayTopic(call.mReference, FalloutScript::formArgument(call, 1), FalloutScript::formArgument(call, 0),
                FalloutScript::intArgument(call, 2) != 0);
            return 0.0;
        });
        // A talking activator (an intercom) speaks for the character it is set to, which is who conditions about it are
        // about: the lines of the intercom are those of that character, and the one that talks to it is talking to that
        // character. Without an argument the activator speaks for nobody.
        mInterpreter.setHandler("SetTalkingActivatorActor", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId activator = call.mReference;
            const FalloutScript::FormId actor = FalloutScript::formArgument(call, 0);
            if (activator == 0)
                return 0.0;
            {
                const std::lock_guard lock(mTalkingActorMutex);
                if (actor == 0)
                    mTalkingActors.erase(activator);
                else
                    mTalkingActors[activator] = actor;
            }
            Log(Debug::Info) << "Talking activator " << std::hex << activator << " speaks for " << actor << std::dec;
            return 0.0;
        });
        mInterpreter.setHandler("IsTalkingActivatorActor", [this](FalloutScript::CallContext& call) {
            const FalloutScript::FormId actor = FalloutScript::formArgument(call, 0);
            return actor != 0 && talkingActorOf(call.mReference) == actor ? 1.0 : 0.0;
        });
        // IsDLCInstalled names a plugin ("Caravan Pack"), which is installed when the game loads it
        mInterpreter.setHandler("IsDLCInstalled", [this](FalloutScript::CallContext& call) {
            if (call.mArguments.empty() || !call.mArguments[0].mIsText)
                return 0.0;
            const std::string wanted = pluginKey(call.mArguments[0].mText);
            return !wanted.empty()
                    && std::any_of(mContentFiles.begin(), mContentFiles.end(),
                        [&wanted](const std::string& file) { return pluginKey(file) == wanted; })
                ? 1.0
                : 0.0;
        });
        FalloutScript::addConditionFunctions(mConditions);
        // The conditions of a log entry of a stage are about the player, who nobody else is in the way of
        mQuests.setConditionCheck([this](const std::vector<ESM4::TargetCondition>& conditions) {
            return mConditions.holds(conditions, FalloutScript::ConditionContext());
        });
        // What the player reads of a quest: the text of the log entry of each stage that is reached
        mQuests.setJournalListener([this](FalloutScript::FormId quest, const FalloutScript::JournalEntry& entry) {
            Log(Debug::Info) << "Journal: " << editorId(quest) << " stage " << entry.mStage << ": " << entry.mText;
            tellPlayer(entry.mText);
        });
    }

    FalloutScripts::~FalloutScripts()
    {
        reportMissingCommands();
    }

    void FalloutScripts::reportMissingCommands()
    {
        mMissingTimer = 0;
        const std::string summary = mInterpreter.takeMissingSummary();
        if (!summary.empty())
            log("Commands that scripts reached and that are not implemented, with how often: " + summary);
    }

    void FalloutScripts::setContentFiles(std::vector<std::string> files)
    {
        mContentFiles = std::move(files);
    }

    FalloutScript::FormId FalloutScripts::newGameQuest() const
    {
        // A list with the master of Fallout 3 starts in Vault 101 as Fallout 3 does (Tale of Two Wastelands has both
        // masters); New Vegas has the quest of its own, and still holds the CG00 of Fallout 3 in its records
        const bool fallout3 = std::any_of(mContentFiles.begin(), mContentFiles.end(),
            [](const std::string& file) { return Misc::StringUtils::ciEqual(file, fallout3Master); });
        if (!fallout3)
            if (const FalloutScript::FormId quest = mQuests.findByEditorId("VCG00"))
                return quest;
        return mQuests.findByEditorId("CG00");
    }

    void FalloutScripts::startNewGame()
    {
        const FalloutScript::FormId quest = newGameQuest();
        if (quest == 0)
            return;
        log("New game: the quest of character creation is " + editorId(quest));
        mQuests.start(quest);
        // The main menu is a menu, so the menu mode block of the script of the quest runs and sets its first stage
        const FalloutScript::BlockFilter filter
            = [this](const FalloutScript::Block& block) { return menuBlockRuns(block); };
        mQuests.runBlock(quest, FalloutScript::BlockType::MenuMode, filter);
    }

    void FalloutScripts::reset()
    {
        reportMissingCommands();
        mGlobals.clear();
        mDisabled.clear();
        {
            const std::lock_guard lock(mTalkingActorMutex);
            mTalkingActors.clear();
        }
        mPendingToggle.clear();
        mPendingLoad.clear();
        mPendingMoves.clear();
        mRelocations.clear();
        mMade.clear();
        mMadeIds.clear();
        {
            const std::lock_guard lock(mMadeMutex);
            mPendingMade.clear();
            mMadeCount = 0;
        }
        mReferences.clear();
        mTriggerShapes.clear();
        mTriggers.clear();
        mTriggerActors.clear();
        mScriptPackages.clear();
        {
            const std::lock_guard lock(mFurnitureMutex);
            mFurniture.clear();
        }
        {
            const std::lock_guard lock(mCalendarMutex);
            mCalendarShift = 0;
        }
        mActors.clear();
        mEquipment.clear();
        mCombat.clear();
        {
            const std::lock_guard lock(mDownedMutex);
            mDowned.clear();
        }
        mItemHolders.clear();
        mActorValues.clear();
        mReferenceState.clear();
        mFormLists.clear();
        mProgress.clear();
        mChargen.clear();
        mChargenSeeded = 0;
        mObjects.clear();
        {
            const std::lock_guard lock(mDeferredMutex);
            mDeferred.clear();
            mDeferredDisabled.clear();
            mDeferredPlaces.clear();
        }
        mQuests.reset();
        mMessageQueue.clear();
        mMessageOpen = false;
        mOpenNumbers.clear();
        mOpenOwner = 0;
        mMessageBoxCount = 0;
        mAnswer.clear();
        {
            const std::lock_guard lock(mOpenMessageMutex);
            mOpenMessage.reset();
        }
        // A game starts with the player in control, whatever the one before took away
        for (std::size_t i = 0; i < FalloutScript::playerControlCount; ++i)
            if ((mControlsOff.load() & (1u << i)) != 0)
                setPlayerControl(static_cast<FalloutScript::PlayerControl>(i), true);
        {
            const std::lock_guard lock(mDialogueMutex);
            mDialogue.reset();
            mSpeakerName.clear();
            // What was being said belongs to the game that is gone: the references would name other objects in the
            // next one
            mConversationVoiceFile.clear();
            mConversationVoiceSpeaker = 0;
            mConversationVoice = static_cast<int>(VoiceState::None);
        }
        mSayings.clear();
        mSayLogged.clear();
    }

    void FalloutScripts::update(float frametime, bool paused)
    {
        runPending();
        updateMessageBoxes();
        updateVoices(frametime);
        if (paused)
        {
            // The game is stopped with no window open (a script of Lua paused the world): nothing runs
            if (!menuOpen(0))
                return;
            // A menu is open: the scripts that wait for one, or are to go on while it is open, have their MenuMode
            // blocks run in place of GameMode
            mObjects.setSecondsPassed(frametime);
            // What the blocks of the menu did to the items is told while it is open, not when it is closed
            raiseItemEvents();
            const FalloutScript::BlockFilter filter
                = [this](const FalloutScript::Block& block) { return menuBlockRuns(block); };
            mQuests.updateMenu(frametime, filter);
            mObjects.updateMenu(filter);
            return;
        }
        // A game can be left at any time, so what has not been implemented shows up in the log while it is played
        constexpr float missingReportInterval = 30.f;
        mMissingTimer += frametime;
        if (mMissingTimer >= missingReportInterval)
            reportMissingCommands();
        dropObjectsThatLeft();
        mObjects.setSecondsPassed(frametime);
        updateFurniture();
        // What the items did since the last frame is told before any block of this frame runs, so that an AddItem or
        // EquipItem of a trigger block is told in the next frame
        raiseItemEvents();
        // The actors that came in or went out since the last frame, before the scripts of the frame look at the world
        updateTriggers();
        raiseCombatEvents(frametime);
        mQuests.update(frametime);
        mObjects.update();
    }

    void FalloutScripts::runOnMainThread(std::function<void()> action)
    {
        if (std::this_thread::get_id() == mMainThread)
        {
            action();
            return;
        }
        const std::lock_guard lock(mDeferredMutex);
        mDeferred.push_back(std::move(action));
    }

    FalloutScript::FormId FalloutScripts::referenceOf(const OFWorld::Ptr& ptr) const
    {
        const ESM::RefNum refNum = ptr.getCellRef().getRefNum();
        if (refNum.hasContentFile())
            return numberOf(refNum);
        // An object made while the game runs has a number of the world's own; scripts know it by the one it was given
        const auto made = mMadeIds.find(refNum);
        return made != mMadeIds.end() ? made->second : 0;
    }

    ESM::FormId FalloutScripts::worldIdOf(FalloutScript::FormId reference) const
    {
        const auto made = mMade.find(reference);
        return made != mMade.end() ? made->second : ESM::FormId::fromUint32(reference);
    }

    void FalloutScripts::cellLoaded(OFWorld::CellStore& cell)
    {
        // This runs while the scene is being changed, which scripts must not change in turn (enabling or disabling an
        // object takes it out of the scene or puts it in): what they do waits for the next frame
        const std::size_t volumesBefore = mTriggerShapes.size();
        cell.forEach([&](const OFWorld::Ptr& ptr) {
            const FalloutScript::FormId reference = referenceOf(ptr);
            if (reference == 0)
                return true;
            mReferences[reference] = ptr;
            // An object that scripts moved while its cell was not loaded is put where they wanted it
            if (const auto moved = mRelocations.find(reference); moved != mRelocations.end())
            {
                // The place is the whole place the object was given, of which scripts named the parts in the mask
                Move move = moved->second;
                // Whatever a script asked for since then comes after it
                if (const auto asked = mPendingMoves.find(reference); asked != mPendingMoves.end())
                    move.add(asked->second);
                mPendingMoves[reference] = move;
                mRelocations.erase(moved);
            }
            // An object that scripts enabled or disabled while its cell was not loaded is so now, whatever its record
            // says; from here on the state is that of the object
            if (const auto state = mDisabled.find(reference); state != mDisabled.end())
            {
                mPendingToggle.emplace_back(reference, state->second);
                mDisabled.erase(state);
            }
            if (isCharacterOrCreature(ptr))
                mTriggerActors.insert(reference);
            const FalloutScript::FormId script = scriptOf(ptr, reference);
            if (script != 0 && mObjects.add(reference, script) != nullptr)
            {
                mPendingLoad.push_back(reference);
                registerTrigger(ptr, reference);
            }
            return true;
        });
        if (mTriggerShapes.size() != volumesBefore)
            Log(Debug::Info) << "Trigger volumes: " << (mTriggerShapes.size() - volumesBefore) << " in a loaded cell, "
                             << mTriggerShapes.size() << " in all";
    }

    void FalloutScripts::registerTrigger(const OFWorld::Ptr& ptr, FalloutScript::FormId reference)
    {
        // Only the objects whose scripts have a block that a volume runs, and whose record has a volume (the others are
        // markers, or have no volume in the plugin)
        namespace BlockType = FalloutScript::BlockType;
        if (!mObjects.hasBlock(reference, BlockType::OnTrigger)
            && !mObjects.hasBlock(reference, BlockType::OnTriggerEnter)
            && !mObjects.hasBlock(reference, BlockType::OnTriggerLeave))
            return;
        const ESM4::Reference* record = mStore.get<ESM4::Reference>().search(worldIdOf(reference));
        if (record == nullptr || !record->mPrimitive || record->mPrimitive->mType == ESM4::Primitive::None)
            return;
        FalloutScript::Volume volume;
        switch (record->mPrimitive->mType)
        {
            case ESM4::Primitive::Sphere:
                volume.mShape = FalloutScript::VolumeShape::Sphere;
                break;
            case ESM4::Primitive::Plane:
                volume.mShape = FalloutScript::VolumeShape::Plane;
                break;
            default:
                volume.mShape = FalloutScript::VolumeShape::Box;
                break;
        }
        const float scale = ptr.getCellRef().getScale();
        volume.mExtents = { record->mPrimitive->mBounds[0] * scale, record->mPrimitive->mBounds[1] * scale,
            record->mPrimitive->mBounds[2] * scale };
        mTriggerShapes[reference] = volume;
    }

    void FalloutScripts::updateTriggers()
    {
        if (mTriggerShapes.empty())
            return;
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (world == nullptr)
            return;
        // Where the triggers stand now, in case a script moved one
        std::vector<FalloutScript::FormId> gone;
        for (auto& [reference, volume] : mTriggerShapes)
        {
            const auto found = mReferences.find(reference);
            if (found == mReferences.end() || found->second.isEmpty())
            {
                gone.push_back(reference);
                continue;
            }
            const ESM::Position& position = found->second.getRefData().getPosition();
            const osg::Quat rotation = Misc::Convert::makeOsgQuat(position);
            const osg::Vec3f x = rotation * osg::Vec3f(1, 0, 0);
            const osg::Vec3f y = rotation * osg::Vec3f(0, 1, 0);
            const osg::Vec3f z = rotation * osg::Vec3f(0, 0, 1);
            volume.mCenter = { position.pos[0], position.pos[1], position.pos[2] };
            volume.mAxisX = { x.x(), x.y(), x.z() };
            volume.mAxisY = { y.x(), y.y(), y.z() };
            volume.mAxisZ = { z.x(), z.y(), z.z() };
            mTriggers.set(reference, volume);
        }
        for (const FalloutScript::FormId reference : gone)
        {
            mTriggers.remove(reference);
            mTriggerShapes.erase(reference);
        }

        // The actors that can set a trigger off: the player and the characters and creatures of the loaded cells that
        // are there and alive
        std::vector<FalloutScript::TriggerBody> bodies;
        const auto addBody = [&](const OFWorld::Ptr& ptr, FalloutScript::FormId reference) {
            if (ptr.isEmpty() || !ptr.getRefData().isEnabled() || ptr.mRef->isDeleted())
                return;
            // The player is an actor of the engine, whose death the stats tell; the others are told by the host
            if (mActors.isDead(reference)
                || (ptr.getClass().isActor() && ptr.getClass().getCreatureStats(ptr).isDead()))
                return;
            const ESM::Position& position = ptr.getRefData().getPosition();
            // The characters and creatures of Fallout are solid as the cylinders of their physics bodies (the scale of
            // the reference on top), which the world does not tell (it gives the bounds of their models); the player is
            // an actor of the engine
            osg::Vec3f half;
            if (ptr.getType() == ESM4::Npc::sRecordId)
                half = OFClass::ESM4Npc::getBodyHalfExtents(ptr) * ptr.getCellRef().getScale();
            else if (ptr.getType() == ESM4::Creature::sRecordId)
                half = OFClass::ESM4Creature::getBodyHalfExtents(ptr) * ptr.getCellRef().getScale();
            else
                half = world->getHalfExtents(ptr);
            // An actor the physics has no body for yet stands about as tall as a person
            FalloutScript::TriggerBody body;
            body.mReference = reference;
            body.mFeet = { position.pos[0], position.pos[1], position.pos[2] };
            body.mHeight = half.z() > 0 ? 2 * half.z() : 128.f;
            body.mRadius = std::max(half.x(), half.y()) > 0 ? std::max(half.x(), half.y()) : 20.f;
            bodies.push_back(body);
        };
        addBody(world->getPlayerPtr(), mPlayer);
        for (const FalloutScript::FormId reference : mTriggerActors)
        {
            const auto found = mReferences.find(reference);
            // An actor that was made or moved to a cell that is not loaded is not in a volume of the loaded cells
            if (found != mReferences.end() && reference != mPlayer && !leftLoadedCells(reference))
                addBody(found->second, reference);
        }

        const auto active = [this](FalloutScript::FormId reference) {
            if (!mObjects.loaded(reference))
                return false;
            const auto found = mReferences.find(reference);
            return found != mReferences.end() && !found->second.isEmpty() && found->second.getRefData().isEnabled()
                && !found->second.mRef->isDeleted();
        };
        const FalloutScript::TriggerVolumes::Raise raise
            = [this](FalloutScript::FormId trigger, std::uint16_t blockType, FalloutScript::FormId actor) {
                  // A block that names a form is for that actor only; one that names nothing is for any
                  const FalloutScript::BlockFilter filter = [actor](const FalloutScript::Block& block) {
                      return FalloutScript::blockIsForForm(block, actor);
                  };
                  // OnTrigger repeats every pass, so only the changes are told
                  if (blockType != FalloutScript::BlockType::OnTrigger)
                      Log(Debug::Info) << (blockType == FalloutScript::BlockType::OnTriggerEnter ? "Trigger enter: "
                                                                                                 : "Trigger leave: ")
                                       << std::hex << trigger << " by " << actor;
                  mObjects.trigger(trigger, blockType, actor, filter);
              };
        mTriggers.update(bodies, active, raise);
    }

    void FalloutScripts::runPending()
    {
        std::vector<std::function<void()>> deferred;
        {
            const std::lock_guard lock(mDeferredMutex);
            deferred.swap(mDeferred);
        }
        for (const auto& action : deferred)
            action();
        if (!deferred.empty())
        {
            const std::lock_guard lock(mDeferredMutex);
            // Every toggle and move that was asked for has been made now, and the state of the objects is theirs again
            if (mDeferred.empty())
            {
                mDeferredDisabled.clear();
                mDeferredPlaces.clear();
            }
        }
        // The objects that PlaceAtMe asked for, before anything else is done to them
        std::map<FalloutScript::FormId, MadeObject> made;
        {
            const std::lock_guard lock(mMadeMutex);
            made.swap(mPendingMade);
        }
        for (const auto& [id, request] : made)
            makeObject(id, request);
        // Where scripts put objects. A cell that this loads asks for more, which is for the next frame.
        std::map<FalloutScript::FormId, Move> moves;
        moves.swap(mPendingMoves);
        for (const auto& [reference, move] : moves)
            applyMove(reference, move);
        // Every object has its script before any of them runs OnLoad, so that they can reach each other
        std::vector<std::pair<FalloutScript::FormId, bool>> toggle;
        toggle.swap(mPendingToggle);
        for (const auto& [reference, disabled] : toggle)
        {
            const OFWorld::Ptr ptr = findReference(reference);
            if (ptr.isEmpty())
                continue;
            if (disabled)
                OFBase::Environment::get().getWorld()->disable(ptr);
            else
                OFBase::Environment::get().getWorld()->enable(ptr);
        }
        std::vector<FalloutScript::FormId> load;
        load.swap(mPendingLoad);
        for (const FalloutScript::FormId reference : load)
        {
            // Not if the cell has been unloaded since, or the object has been moved out of the loaded cells
            if (mReferences.count(reference) == 0)
                continue;
            if (leftLoadedCells(reference))
                mReferences.erase(reference);
            else
                mObjects.setLoaded(reference, true);
        }
    }

    void FalloutScripts::cellUnloaded(OFWorld::CellStore& cell)
    {
        cell.forEach([&](const OFWorld::Ptr& ptr) {
            if (const FalloutScript::FormId reference = referenceOf(ptr); reference != 0)
            {
                mObjects.setLoaded(reference, false);
                mReferences.erase(reference);
                mTriggerShapes.erase(reference);
                mTriggers.remove(reference);
                mTriggerActors.erase(reference);
            }
            return true;
        });
    }

    bool FalloutScripts::activated(const OFWorld::Ptr& object, const OFWorld::Ptr& actor)
    {
        const FalloutScript::FormId reference = referenceOf(object);
        if (reference == 0)
            return false;
        const FalloutScript::FormId activator
            = actor == OFBase::Environment::get().getWorld()->getPlayerPtr() ? mPlayer : referenceOf(actor);
        return mObjects.trigger(reference, FalloutScript::BlockType::OnActivate, activator);
    }

    OFWorld::Ptr FalloutScripts::findReference(FalloutScript::FormId reference)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (reference == mPlayer)
            return world->getPlayerPtr();
        if (const auto found = mReferences.find(reference); found != mReferences.end())
        {
            // An object that was moved to another cell has another Ptr, which the world keeps up to date for those
            // that are in the scene
            const OFWorld::Ptr moved = OFBase::Environment::get().getWorldModel()->getPtr(worldIdOf(reference));
            if (!moved.isEmpty() && moved.isInCell() && moved.getCell() != found->second.getCell())
                found->second = moved;
            return found->second;
        }
        // Objects made while the game runs are only known to the world
        const OFWorld::Ptr ptr = OFBase::Environment::get().getWorldModel()->getPtr(worldIdOf(reference));
        return !ptr.isEmpty() && ptr.isInCell() ? ptr : OFWorld::Ptr();
    }

    bool FalloutScripts::isDisabled(FalloutScript::FormId reference)
    {
        {
            // What another thread asked for is not done yet, but it is what the script sees
            const std::lock_guard lock(mDeferredMutex);
            if (const auto asked = mDeferredDisabled.find(reference); asked != mDeferredDisabled.end())
                return asked->second;
        }
        const OFWorld::Ptr ptr = findReference(reference);
        if (!ptr.isEmpty())
            return !ptr.getRefData().isEnabled();
        const auto state = mDisabled.find(reference);
        return state != mDisabled.end() && state->second;
    }

    void FalloutScripts::setDisabled(FalloutScript::FormId reference, bool disabled)
    {
        if (reference == mPlayer)
            return;
        if (std::this_thread::get_id() != mMainThread)
        {
            {
                const std::lock_guard lock(mDeferredMutex);
                mDeferredDisabled[reference] = disabled;
            }
            runOnMainThread([this, reference, disabled] { applyDisabled(reference, disabled); });
            return;
        }
        applyDisabled(reference, disabled);
    }

    void FalloutScripts::applyDisabledAlone(FalloutScript::FormId reference, bool disabled)
    {
        const OFWorld::Ptr ptr = findReference(reference);
        if (ptr.isEmpty())
        {
            // Not in a loaded cell: it is so when the cell is loaded
            mDisabled[reference] = disabled;
            return;
        }
        if (disabled)
            OFBase::Environment::get().getWorld()->disable(ptr);
        else
            OFBase::Environment::get().getWorld()->enable(ptr);
    }

    void FalloutScripts::applyDisabled(FalloutScript::FormId reference, bool disabled)
    {
        applyDisabledAlone(reference, disabled);
        // The objects that have this one for their enable parent follow it, and theirs follow them
        std::size_t followed = 0;
        for (const auto& [follower, followerDisabled] : enableParents().followers(reference, disabled))
        {
            applyDisabledAlone(follower, followerDisabled);
            ++followed;
        }
        if (followed > 0)
            Log(Debug::Info) << "Enable parent " << std::hex << reference << std::dec << " "
                             << (disabled ? "disabled" : "enabled") << ": " << followed << " objects follow";
    }

    const FalloutScript::EnableParents& FalloutScripts::enableParents()
    {
        if (mEnableParentsRead)
            return mEnableParents;
        mEnableParentsRead = true;
        const auto read = [this](const auto& ref) {
            if (!ref.mEsp.parent.isZeroOrUnset())
                mEnableParents.add(numberOf(ref.mId), numberOf(ref.mEsp.parent),
                    (ref.mEsp.flags & ESM4::EnableParent::Flag_Inversed) != 0);
        };
        mStore.get<ESM4::Reference>().forEachPlaced(read);
        mStore.get<ESM4::ActorCharacter>().forEachPlaced(read);
        mStore.get<ESM4::ActorCreature>().forEachPlaced(read);
        return mEnableParents;
    }

    void FalloutScripts::tellPlayer(const std::string& text)
    {
        // Not in the dialogue window: the window of Fallout's conversations is a script's, and the one that takes these
        // notes during a conversation of Morrowind is hidden
        runOnMainThread([text] {
            OFBase::Environment::get().getWindowManager()->messageBox(text, OFGui::ShowInDialogueMode_Never);
        });
    }

    bool FalloutScripts::leftLoadedCells(FalloutScript::FormId reference)
    {
        const OFWorld::Ptr ptr = findReference(reference);
        if (ptr.isEmpty() || ptr.getCell() == nullptr || reference == mPlayer)
            return false;
        const OFWorld::Scene::CellStoreCollection& active
            = OFBase::Environment::get().getWorldScene()->getActiveCells();
        return active.find(const_cast<OFWorld::CellStore*>(ptr.getCell())) == active.end();
    }

    void FalloutScripts::dropObjectsThatLeft()
    {
        // An object that was moved to a cell that is not loaded is not in a loaded cell to be unloaded with it
        for (const FalloutScript::FormId reference : mObjects.loadedReferences())
        {
            if (!leftLoadedCells(reference))
                continue;
            mObjects.setLoaded(reference, false);
            mReferences.erase(reference);
            mTriggerShapes.erase(reference);
            mTriggers.remove(reference);
            mTriggerActors.erase(reference);
        }
    }

    bool FalloutScripts::distance(FalloutScript::FormId first, FalloutScript::FormId second, double& result)
    {
        // Where the script has put the references counts, though the world has not done it yet
        Place a;
        Place b;
        if (!whereIs(first, a) || !whereIs(second, b))
            return false;
        if (!inOneSpace(a, b))
            return false;
        result = (a.mPosition - b.mPosition).length();
        return true;
    }

    bool FalloutScripts::inOneSpace(const Place& a, const Place& b)
    {
        // In the same cell, or in exterior cells of one worldspace
        if (a.mCell == b.mCell)
            return true;
        const OFWorld::WorldModel* model = OFBase::Environment::get().getWorldModel();
        const OFWorld::CellStore* cellA = model->findCell(a.mCell);
        const OFWorld::CellStore* cellB = model->findCell(b.mCell);
        return cellA != nullptr && cellB != nullptr && cellA->isExterior() && cellB->isExterior()
            && cellA->getCell()->getWorldSpace() == cellB->getCell()->getWorldSpace();
    }

    namespace
    {
        constexpr int messageBoxMenu = 1001;
        constexpr int dialogueMenu = 1009;
        // The flag of a message that is a box with buttons and not a note on the screen
        constexpr std::uint32_t messageBoxFlag = 0x1;

        /// The text of the interface shows `#` as the start of a colour, and a message can have it in its words
        std::string escapeTags(const std::string& text)
        {
            std::string result;
            result.reserve(text.size());
            for (const char c : text)
            {
                result += c;
                if (c == '#')
                    result += c;
            }
            return result;
        }
    }

    void FalloutScripts::showMessage(
        FalloutScript::FormId message, const std::vector<double>& values, FalloutScript::FormId owner)
    {
        const ESM4::Message* record = mStore.get<ESM4::Message>().search(refId(message));
        if (record == nullptr)
        {
            log("ShowMessage: the message " + FalloutScript::hex(message) + " is not known");
            return;
        }
        // The answer to the message before is gone as soon as another is shown (the answer to this one is its owner's
        // to read once the player closes the box)
        mAnswer.forget();
        // The conditions before the first button are those of the message, and the ones after a button are its own;
        // they are about the player
        if (!mConditions.holds(record->mConditions, FalloutScript::ConditionContext()))
            return;
        std::string text = FalloutScript::formatMessage(record->mDescription, values);
        const bool isBox = (record->mMessageFlags & messageBoxFlag) != 0;
        if (text.empty() && !isBox)
            return;
        Log(Debug::Info) << "Message: " << text;

        MessageBox box;
        box.mOwner = owner;
        box.mForm = refId(message).serializeText();
        for (std::size_t i = 0; i < record->mButtons.size(); ++i)
        {
            const ESM4::Message::Button& button = record->mButtons[i];
            if (!mConditions.holds(button.mConditions, FalloutScript::ConditionContext()))
                continue;
            std::string label = FalloutScript::formatMessage(button.mText, values);
            box.mButtons.push_back(escapeTags(label));
            box.mPlainButtons.push_back(std::move(label));
            box.mNumbers.push_back(static_cast<int>(i));
        }
        // A message with buttons is a question to the player, and one that is a box is shown as one even when there is
        // nothing to pick: it has the button that closes it
        if (box.mButtons.empty() && !isBox)
        {
            tellPlayer(escapeTags(text));
            return;
        }
        Log(Debug::Info) << "Message buttons: " << box.mButtons.size();
        box.mPlainText = record->mFullName.empty() ? text : record->mFullName + "\n\n" + text;
        box.mText = escapeTags(box.mPlainText);
        // The count is up at once, so that MenuMode tells of the box also when this runs on another thread and the box
        // is queued in the next frame
        ++mMessageBoxCount;
        runOnMainThread([this, box = std::move(box)]() mutable {
            mMessageQueue.push_back(std::move(box));
            updateMessageBoxes();
        });
    }

    void FalloutScripts::updateMessageBoxes()
    {
        OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
        if (windows == nullptr)
            return;
        if (mMessageOpen)
        {
            // The window tells the button once it is closed; a box that went away without one (the game was loaded)
            // picked nothing
            // A box that another part of the game opened in its place (a message of Lua, a question of the menu) was
            // not answered by the player, and the answer that box gets is not ours to take
            const std::uint64_t current = windows->interactiveMessageBoxId();
            const bool replaced = current != 0 && current != mOpenBoxId;
            const int pressed = replaced ? -1 : windows->readPressedButton();
            if (!replaced && pressed < 0 && current != 0)
                return;
            mMessageOpen = false;
            --mMessageBoxCount;
            {
                const std::lock_guard lock(mOpenMessageMutex);
                mOpenMessage.reset();
            }
            if (pressed >= 0 && static_cast<std::size_t>(pressed) < mOpenNumbers.size())
            {
                const int number = mOpenNumbers[static_cast<std::size_t>(pressed)];
                mAnswer.picked(mOpenOwner, number);
                Log(Debug::Info) << "Message button: " << number;
            }
            mOpenNumbers.clear();
            mOpenOwner = 0;
        }
        // Another box (the main menu asking something) is not ours to replace
        if (mMessageQueue.empty() || windows->isInteractiveMessageBoxActive())
            return;
        MessageBox box = std::move(mMessageQueue.front());
        mMessageQueue.pop_front();
        // A box with nothing to pick has the button that closes it, and that is the button 0
        if (box.mButtons.empty())
        {
            box.mButtons.emplace_back(windows->getGameSettingString("sOK", "OK"));
            box.mPlainButtons.push_back(box.mButtons.back());
            box.mNumbers.push_back(0);
        }
        mOpenNumbers = box.mNumbers;
        mOpenOwner = box.mOwner;
        {
            const std::lock_guard lock(mOpenMessageMutex);
            mOpenMessage = OpenMessage{ ++mMessageSerial, box.mPlainText, box.mPlainButtons, box.mForm };
        }
        // The first button has the focus, so that Enter picks it
        windows->interactiveMessageBox(box.mText, box.mButtons, false, 0);
        mOpenBoxId = windows->interactiveMessageBoxId();
        mMessageOpen = true;
    }

    int FalloutScripts::takeButtonPressed(FalloutScript::FormId owner)
    {
        return mAnswer.take(owner);
    }

    std::optional<FalloutScripts::OpenMessage> FalloutScripts::openMessage() const
    {
        const std::lock_guard lock(mOpenMessageMutex);
        return mOpenMessage;
    }

    bool FalloutScripts::pressMessageButton(std::size_t button)
    {
        std::uint32_t serial = 0;
        {
            const std::lock_guard lock(mOpenMessageMutex);
            if (!mOpenMessage || button >= mOpenMessage->mButtons.size())
                return false;
            serial = mOpenMessage->mSerial;
        }
        runOnMainThread([this, button, serial] {
            {
                // A press that waited for the frame is for the box that was open when it was asked for: that box may
                // have been answered since, and another one opened
                const std::lock_guard lock(mOpenMessageMutex);
                if (!mOpenMessage || mOpenMessage->mSerial != serial)
                    return;
            }
            // The box that is open must be the one that was asked for: another one can have taken its place
            OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
            if (windows != nullptr && windows->interactiveMessageBoxId() == mOpenBoxId)
                windows->pressInteractiveMessageBoxButton(button);
        });
        return true;
    }

    void FalloutScripts::setPlayerControl(FalloutScript::PlayerControl control, bool enabled)
    {
        const unsigned bit = 1u << static_cast<std::size_t>(control);
        const unsigned before = enabled ? mControlsOff.fetch_and(~bit) : mControlsOff.fetch_or(bit);
        // The log tells what the scripts changed. The switches are set in any case: a Lua script or a loaded save can
        // have turned one since, and the script asks for the state it names.
        if (((before & bit) == 0) != enabled)
        {
            static constexpr const char* names[]
                = { "movement", "pipboy", "fighting", "point of view", "looking", "rollover", "sneaking" };
            Log(Debug::Info) << "Player controls: " << names[static_cast<std::size_t>(control)]
                             << (enabled ? " on" : " off");
        }
        // The switches of the engine are those of Morrowind: movement is the one that gates every control of the player
        // (it lets the player activate things, fight and open the inventory too, so a call that takes movement alone
        // takes those as well; the scripts of the games always take it with fighting and the Pip-Boy), and the
        // controls that no switch stands for (the Pip-Boy, rollover, sneaking) have nothing to turn yet
        std::vector<std::string_view> switches;
        switch (control)
        {
            case FalloutScript::PlayerControl::Movement:
                switches = { "playercontrols", "playerjumping" };
                break;
            case FalloutScript::PlayerControl::Fighting:
                switches = { "playerfighting", "playermagic" };
                break;
            case FalloutScript::PlayerControl::PointOfView:
                switches = { "playerviewswitch", "vanitymode" };
                break;
            case FalloutScript::PlayerControl::Looking:
                switches = { "playerlooking" };
                break;
            default:
                return;
        }
        runOnMainThread([control, enabled, switches = std::move(switches)] {
            OFBase::InputManager* input = OFBase::Environment::get().getInputManager();
            if (input == nullptr)
                return;
            // Taking looking away turns the player to the front in Morrowind, which would undo the way the player was
            // put to face (a move to a marker turns the player, and the scenes begin with looking taken away)
            OFBase::World* world = OFBase::Environment::get().getWorld();
            OFWorld::Ptr player;
            osg::Vec3f facing;
            if (control == FalloutScript::PlayerControl::Looking && !enabled && world != nullptr)
            {
                player = world->getPlayerPtr();
                if (!player.isEmpty())
                    facing = player.getRefData().getPosition().asRotationVec3();
            }
            for (const std::string_view name : switches)
                input->toggleControlSwitch(name, enabled);
            if (!player.isEmpty())
                world->rotateObject(player, facing);
            // The switches only keep the player from drawing a weapon or readying a spell: one that is already out
            // would go on attacking, so taking fighting away puts it away
            if (control == FalloutScript::PlayerControl::Fighting && !enabled && world != nullptr)
            {
                const OFWorld::Ptr self = world->getPlayerPtr();
                if (!self.isEmpty())
                    self.getClass().getCreatureStats(self).setDrawState(OFMechanics::DrawState::Nothing);
            }
        });
    }

    bool FalloutScripts::menuOpen(int menu)
    {
        const bool message = mMessageBoxCount.load() > 0;
        const bool dialogue = [this] {
            const std::lock_guard lock(mDialogueMutex);
            return mDialogue.active();
        }();
        switch (menu)
        {
            case 0:
            {
                OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
                return message || dialogue || mChargen.menuOpen() || (windows != nullptr && windows->isGuiMode());
            }
            case messageBoxMenu:
                return message;
            case dialogueMenu:
                return dialogue;
            default:
                // The screens of making a character have the numbers of the menus they stand for
                return menu != 0 && mChargen.menu() == menu;
        }
    }

    bool FalloutScripts::menuBlockRuns(const FalloutScript::Block& block)
    {
        if (block.mNames != FalloutScript::BlockNames::Number)
            return true;
        const int menu = block.mNumber;
        if (menu == messageBoxMenu || menu == dialogueMenu || FalloutScript::ChargenMenu::isChargenMenu(menu))
            return menuOpen(menu);
        // The numbers of the other menus (the inventory, a container, resting) are not told apart: the block runs as
        // one that names no menu does
        return true;
    }

    namespace
    {
        /// The record that a character or creature takes something from, which is the first of its templates that does
        /// not: the flag says what is taken. Null when a template is not a record of the same kind (a levelled list
        /// of characters), which can not be followed here.
        template <class Record>
        const Record* templateOwner(const OFWorld::ESMStore& store, const Record* record, std::uint16_t flag)
        {
            // A chain of templates that is longer is a cycle or a mistake (the limit of the model and the worn armor)
            constexpr int maxTemplateDepth = 16;
            for (int step = 0; record != nullptr && record->takesFromTemplate(flag); ++step)
            {
                if (record->mBaseTemplate.isZeroOrUnset() || step >= maxTemplateDepth)
                    return nullptr;
                record = store.get<Record>().search(record->mBaseTemplate);
            }
            return record;
        }

        /// The record that has what a flag of the template says a character takes from its template (its stats, its
        /// inventory): the end of its chain of templates. A levelled list in the place of a template gives one of its
        /// characters, the one that the cell shows for this reference (the chain is followed with the seed of the
        /// reference, as for the level). The record itself when the chain ends without a record that has them.
        const ESM4::Npc& templateRecordOf(
            const OFWorld::ESMStore& store, const ESM4::Npc& npc, std::uint16_t flag, std::uint32_t seed)
        {
            const OFClass::StoreWornArmorSource source(store);
            const ESM4::Npc* owner
                = ESM4::templateOwner(ESM4::templateChain(source, npc, OFClass::ESM4Impl::sDefaultLevel, seed), flag);
            return owner != nullptr ? *owner : npc;
        }

        /// As above, for a creature
        const ESM4::Creature& templateRecordOf(
            const OFWorld::ESMStore& store, const ESM4::Creature& creature, std::uint16_t flag, std::uint32_t seed)
        {
            const OFClass::StoreCreatureSource source(store);
            const ESM4::Creature* owner = ESM4::creatureTemplateOwner(
                ESM4::creatureTemplateChain(source, creature, OFClass::ESM4Impl::sDefaultLevel, seed), flag);
            return owner != nullptr ? *owner : creature;
        }

        /// The record that has the stats of a character
        template <class Record>
        const Record& statsRecordOf(const OFWorld::ESMStore& store, const Record& record, std::uint32_t seed)
        {
            return templateRecordOf(store, record, Record::Template_UseStats, seed);
        }

        /// The attributes of DATA, which are the actor values from strength to luck
        void addAttributes(std::vector<std::pair<int, double>>& values, const ESM4::FalloutStats& stats)
        {
            for (std::size_t i = 0; i < stats.mAttributes.size(); ++i)
                values.emplace_back(FalloutScript::ActorValue::strength + static_cast<int>(i), stats.mAttributes[i]);
        }

        /// What ACBS says: the speed and the karma
        void addConfig(std::vector<std::pair<int, double>>& values, const ESM4::ActorBaseConfig& config)
        {
            if (config.fo3.speedMultiplier > 0)
                values.emplace_back(FalloutScript::ActorValue::speedMult, config.fo3.speedMultiplier);
            values.emplace_back(FalloutScript::ActorValue::karma, config.fo3.karma);
        }
    }

    FalloutScript::FormId FalloutScripts::scriptOf(const OFWorld::Ptr& ptr, FalloutScript::FormId reference)
    {
        const FalloutScript::FormId own = ptr.getClass().getFalloutScript(ptr);
        ESM::FormId base;
        if (!baseOf(reference, base) || base.isZeroOrUnset())
            return own;
        const auto scriptIdOf = [](const auto* record) -> FalloutScript::FormId {
            return record == nullptr ? 0 : numberOf(record->mScriptId);
        };
        // A record that takes its script from a template has the script of the template, and none when the template
        // can not be followed (it is a levelled list of characters)
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            if (!npc->takesFromTemplate(ESM4::Npc::Template_UseScript))
                return own;
            return scriptIdOf(templateOwner(mStore, npc, ESM4::Npc::Template_UseScript));
        }
        if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            if (!creature->takesFromTemplate(ESM4::Creature::Template_UseScript))
                return own;
            return scriptIdOf(templateOwner(mStore, creature, ESM4::Creature::Template_UseScript));
        }
        return own;
    }

    bool FalloutScripts::baseOf(FalloutScript::FormId reference, ESM::FormId& base)
    {
        base = ESM::FormId();
        // An object that PlaceAtMe asked for is of the base it was asked for, until the world makes it
        {
            const std::lock_guard lock(mMadeMutex);
            if (const auto asked = mPendingMade.find(reference); asked != mPendingMade.end())
            {
                base = ESM::FormId::fromUint32(asked->second.mBase);
                return true;
            }
        }
        if (reference == mPlayer)
        {
            // The record of PlayerRef names the character that the player is; without it that is form 7 of the same
            // plugin, which is where the games keep it
            if (const ESM4::ActorCharacter* record
                = mStore.get<ESM4::ActorCharacter>().search(ESM::FormId::fromUint32(reference)))
                base = OFWorld::placedActorBaseOf(mStore, *record);
            else
                base = ESM::FormId::fromUint32((reference & 0xFF000000u) | 0x7u);
            return true;
        }
        // The records of the content files say what a reference is, whether or not its cell is loaded and from any
        // thread: the objects of the loaded cells are for the thread of the frame
        const ESM::FormId form = ESM::FormId::fromUint32(reference);
        if (form.isZeroOrUnset())
            return true;
        // A levelled list in the place of the base of a character or creature stands for the one it gives, as in the
        // cell
        if (const ESM4::ActorCharacter* character = mStore.get<ESM4::ActorCharacter>().search(form))
            base = OFWorld::placedActorBaseOf(mStore, *character);
        else if (const ESM4::ActorCreature* creature = mStore.get<ESM4::ActorCreature>().search(form))
            base = OFWorld::placedActorBaseOf(mStore, *creature);
        else if (const ESM4::Reference* object = mStore.get<ESM4::Reference>().search(form))
            base = object->mBaseObj;
        else
        {
            // Made while the game runs: only the world knows it, and only the thread of the frame may ask
            if (std::this_thread::get_id() != mMainThread)
                return false;
            const OFWorld::Ptr ptr = findReference(reference);
            if (ptr.isEmpty())
                return false;
            if (ptr.getType() == ESM4::Npc::sRecordId)
                base = ptr.get<ESM4::Npc>()->mBase->mId;
            else if (ptr.getType() == ESM4::Creature::sRecordId)
                base = ptr.get<ESM4::Creature>()->mBase->mId;
            else if (ptr.getType() == ESM4::Container::sRecordId)
                base = ptr.get<ESM4::Container>()->mBase->mId;
            else
            {
                // getRefId gives a copy: the pointer into it needs the copy to stay alive
                const ESM::RefId id = ptr.getCellRef().getRefId();
                if (const ESM::FormId* placed = id.getIf<ESM::FormId>())
                    base = *placed;
            }
        }
        return true;
    }

    bool FalloutScripts::initialItems(
        FalloutScript::FormId holder, std::vector<std::pair<FalloutScript::FormId, int>>& items)
    {
        // The player starts with nothing; what the player carries is for the inventory of the game
        if (holder == mPlayer)
            return true;
        ESM::FormId base;
        if (!baseOf(holder, base))
            return false;
        const auto take = [&items](const std::vector<ESM4::InventoryItem>& inventory) {
            for (const ESM4::InventoryItem& item : inventory)
                items.emplace_back(item.item, static_cast<std::int32_t>(item.count));
        };
        if (base.isZeroOrUnset())
            return true;
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(holder), base);
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            // The inventory is the one of the record that has it: a template that a levelled list of characters gives
            // is followed with the seed of the reference, as the armor the character is shown wearing is
            take(templateRecordOf(mStore, *npc, ESM4::Npc::Template_UseInventory, seed).mInventory);
            // The armor that the levelled lists of the inventory give is carried as well, as many of each piece as
            // they gave, whether the character can wear it or not (what is worn is for startingArmor to say). The
            // pieces that the inventory lists itself are in it already.
            if (npc->mIsFONV)
            {
                const OFClass::StoreWornArmorSource source(mStore);
                for (const ESM4::ListedArmor& given :
                    ESM4::characterArmor(source, *npc, OFClass::ESM4Impl::sDefaultLevel, seed).mFromLists)
                {
                    const FalloutScript::FormId piece = numberOf(given.mArmor->mId);
                    const auto count = static_cast<std::int32_t>(std::min<std::uint32_t>(given.mCount, 0x7FFFFFFF));
                    const auto listed = std::find_if(
                        items.begin(), items.end(), [piece](const auto& item) { return item.first == piece; });
                    if (listed == items.end())
                        items.emplace_back(piece, count);
                    else
                        listed->second = static_cast<std::int32_t>(
                            std::min<std::int64_t>(std::int64_t(listed->second) + count, 0x7FFFFFFF));
                }
            }
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            take(templateRecordOf(mStore, *creature, ESM4::Creature::Template_UseInventory, seed).mInventory);
        }
        else if (const ESM4::Container* container = mStore.get<ESM4::Container>().search(base))
            take(container->mInventory);
        return true;
    }

    bool FalloutScripts::itemInfo(FalloutScript::FormId item, FalloutScript::ItemInfo& info)
    {
        const ESM::RefId id = refId(item);
        info = FalloutScript::ItemInfo();
        if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(id))
        {
            info.mKind = FalloutScript::ItemKind::Weapon;
            info.mAnimationType = weapon->mHasGameData ? weapon->mGame.mAnimationType : 0;
            info.mWeight = weapon->mData.weight;
            return true;
        }
        if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(id))
        {
            info.mKind = FalloutScript::ItemKind::Armor;
            info.mSlots = armor->mArmorFlags;
            info.mRating = armor->mHasRating ? armor->mRating : 0.0;
            info.mThreshold = armor->mDamageThreshold;
            info.mWeight = armor->mData.weight;
            return true;
        }
        if (const ESM4::Ammunition* ammo = mStore.get<ESM4::Ammunition>().search(id))
        {
            info.mKind = FalloutScript::ItemKind::Ammo;
            info.mWeight = ammo->mData.mWeight;
            return true;
        }
        if (const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(id))
        {
            info.mKind = FalloutScript::ItemKind::Aid;
            info.mWeight = potion->mData.weight;
            return true;
        }
        if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(id))
        {
            info.mWeight = misc->mData.weight;
            return true;
        }
        if (const ESM4::Book* book = mStore.get<ESM4::Book>().search(id))
        {
            info.mWeight = book->mData.weight;
            return true;
        }
        if (const ESM4::Ingredient* ingredient = mStore.get<ESM4::Ingredient>().search(id))
        {
            info.mWeight = ingredient->mData.weight;
            return true;
        }
        if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(id))
        {
            info.mWeight = mod->mData.mWeight;
            return true;
        }
        return false;
    }

    bool FalloutScripts::startingArmor(FalloutScript::FormId actor, std::vector<FalloutScript::FormId>& armor)
    {
        // The player starts with nothing (see initialItems)
        if (actor == mPlayer)
            return true;
        ESM::FormId base;
        if (!baseOf(actor, base))
            return false;
        if (base.isZeroOrUnset())
            return true;
        const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base);
        if (npc == nullptr || !npc->mIsFONV)
            return true;
        // What the character is shown wearing: the armor of its inventory and of the lists in it, chosen with the seed
        // of the reference and the level that the cell uses, of which the pieces that show are worn (one that covers
        // no part of the body, or has no model for its sex, is not)
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
        const OFClass::StoreWornArmorSource source(mStore);
        const auto chain = ESM4::templateChain(source, *npc, OFClass::ESM4Impl::sDefaultLevel, seed);
        const ESM4::Npc* traitsOwner = ESM4::templateOwner(chain, ESM4::Npc::Template_UseTraits);
        if (traitsOwner == nullptr)
            traitsOwner = npc;
        const bool female = (traitsOwner->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0;
        const ESM4::WornPieces worn
            = ESM4::wornPieces(ESM4::wornArmor(source, *npc, OFClass::ESM4Impl::sDefaultLevel, seed), female);
        for (const ESM4::WornPiece& piece : worn.mPieces)
            armor.push_back(numberOf(piece.mArmor->mId));
        return true;
    }

    FalloutScript::FormId FalloutScripts::itemScript(FalloutScript::FormId item)
    {
        const ESM::RefId id = refId(item);
        const auto scriptOfRecord
            = [](const auto* record) { return record == nullptr ? 0 : numberOf(record->mScriptId); };
        if (const ESM4::Weapon* weapon = mStore.get<ESM4::Weapon>().search(id))
            return scriptOfRecord(weapon);
        if (const ESM4::Armor* armor = mStore.get<ESM4::Armor>().search(id))
            return scriptOfRecord(armor);
        if (const ESM4::Ammunition* ammo = mStore.get<ESM4::Ammunition>().search(id))
            return numberOf(ammo->mScript);
        if (const ESM4::Potion* potion = mStore.get<ESM4::Potion>().search(id))
            return scriptOfRecord(potion);
        if (const ESM4::MiscItem* misc = mStore.get<ESM4::MiscItem>().search(id))
            return scriptOfRecord(misc);
        if (const ESM4::Book* book = mStore.get<ESM4::Book>().search(id))
            return scriptOfRecord(book);
        if (const ESM4::Ingredient* ingredient = mStore.get<ESM4::Ingredient>().search(id))
            return scriptOfRecord(ingredient);
        if (const ESM4::ItemMod* mod = mStore.get<ESM4::ItemMod>().search(id))
            return scriptOfRecord(mod);
        return 0;
    }

    void FalloutScripts::raiseItemEvents()
    {
        namespace BlockType = FalloutScript::BlockType;
        const auto raise = [this](FalloutScript::FormId item, FalloutScript::FormId holder, std::uint16_t blockType) {
            const FalloutScript::FormId script = itemScript(item);
            if (script == 0 || mObjects.add(item, script) == nullptr)
                return;
            mItemHolders[item] = holder;
            // A block that names a container is for that container, or for a character made from that record
            FalloutScript::FormId base = 0;
            baseFormOf(holder, base);
            mObjects.trigger(item, blockType, holder, [holder, base](const FalloutScript::Block& block) {
                return block.mNames != FalloutScript::BlockNames::Form || block.mForm == holder || block.mForm == base;
            });
        };
        // Both lists are taken before a block runs, so that what the blocks do (an OnAdd that equips the item) is told
        // at the next frame and not in this one
        const std::vector<FalloutScript::ActorState::ItemChange> added = mActors.takeChanges();
        const std::vector<FalloutScript::Equipment::WornChange> worn = mEquipment.takeChanges();
        // A script takes items away without a drop (RemoveItem): the OnDrop blocks run when something is dropped
        for (const FalloutScript::ActorState::ItemChange& change : added)
            if (change.mCount > 0)
                raise(change.mItem, change.mHolder, BlockType::OnAdd);
        for (const FalloutScript::Equipment::WornChange& change : worn)
            raise(change.mItem, change.mActor, change.mWorn ? BlockType::OnEquip : BlockType::OnUnequip);
    }

    bool FalloutScripts::pickUp(const OFWorld::Ptr& item, const OFWorld::Ptr& actor)
    {
        const FalloutScript::FormId reference = referenceOf(item);
        const FalloutScript::FormId holder
            = actor == OFBase::Environment::get().getWorld()->getPlayerPtr() ? mPlayer : referenceOf(actor);
        ESM::FormId base;
        if (reference == 0 || holder == 0 || !baseOf(reference, base) || base.isZeroOrUnset())
            return false;
        const FalloutScript::FormId form = numberOf(base);
        FalloutScript::ItemInfo info;
        if (!itemInfo(form, info))
            return false;
        mActors.addItem(holder, form, std::max(item.getCellRef().getCount(), 1));
        setDisabled(reference, true);
        return true;
    }

    namespace
    {
        /// How far a character sees another one (units): the games work it out from the sneak of the target and the
        /// light, which nothing here knows yet, and stop at a distance that is a guess
        constexpr double sightRange = 3000.0;
        /// How long an essential character lies unconscious before it gets up (seconds)
        constexpr float essentialDownSeconds = 15.f;
    }

    bool FalloutScripts::detects(FalloutScript::FormId viewer, FalloutScript::FormId target)
    {
        if (viewer == 0 || target == 0 || mActors.isDead(viewer) || mActors.isDead(target)
            || mCombat.unconscious(viewer))
            return false;
        // Whoever fights the target knows where it is
        if (mCombat.target(viewer) == target)
            return true;
        double apart = 0;
        return distance(viewer, target, apart) && apart <= sightRange;
    }

    bool FalloutScripts::essentialByRecord(FalloutScript::FormId actor)
    {
        ESM::FormId base;
        if (actor == 0 || actor == mPlayer || !baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        // The flag is part of the base data, which a template can give
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            const ESM4::Npc* owner = templateOwner(mStore, npc, ESM4::Npc::Template_UseBaseData);
            return owner != nullptr && (owner->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Essential) != 0;
        }
        if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const ESM4::Creature* owner = templateOwner(mStore, creature, ESM4::Creature::Template_UseBaseData);
            return owner != nullptr && (owner->mBaseConfig.fo3.flags & ESM4::Creature::FO3_Essential) != 0;
        }
        return false;
    }

    std::optional<FalloutScript::CombatState::Relation> FalloutScripts::recordedRelation(
        FalloutScript::FormId first, FalloutScript::FormId second)
    {
        using Relation = FalloutScript::CombatState::Relation;
        // What the faction says about the other: the reaction of its members to a fight that involves it
        const auto reactionOf
            = [this](FalloutScript::FormId from, FalloutScript::FormId to) -> std::optional<Relation> {
            const ESM4::Faction* faction = mStore.get<ESM4::Faction>().search(refId(from));
            if (faction == nullptr)
                return std::nullopt;
            const ESM::FormId target = ESM::FormId::fromUint32(to);
            for (const ESM4::Faction::Relation& relation : faction->mRelations)
            {
                if (relation.mTarget != target)
                    continue;
                switch (relation.mGroupCombatReaction)
                {
                    case ESM4::Faction::Reaction_Enemy:
                        return Relation::Enemy;
                    case ESM4::Faction::Reaction_Ally:
                        return Relation::Ally;
                    case ESM4::Faction::Reaction_Friend:
                        return Relation::Friend;
                    default:
                        return Relation::Neutral;
                }
            }
            return std::nullopt;
        };
        if (const std::optional<Relation> relation = reactionOf(first, second))
            return relation;
        return reactionOf(second, first);
    }

    FalloutScript::WeaponDamage FalloutScripts::describeWeapon(
        FalloutScript::FormId weapon, FalloutScript::FormId attacker)
    {
        namespace AV = FalloutScript::ActorValue;
        FalloutScript::WeaponDamage damage;
        const ESM4::Weapon* record = weapon != 0 ? mStore.get<ESM4::Weapon>().search(refId(weapon)) : nullptr;
        // The skill of the hands is unarmed
        int skill = AV::lastSkill;
        if (record == nullptr)
        {
            // Bare hands: the records have no weapon for a fist here, so it does a point and the strength bonus
            damage.mDamage = 1;
            damage.mMelee = true;
        }
        else
        {
            damage.mDamage = record->mData.damage;
            if (record->mHasCritical)
            {
                damage.mCriticalDamage = record->mCritical.mDamage;
                damage.mCriticalChanceMultiplier = record->mCritical.mChanceMultiplier;
            }
            if (record->mHasGameData)
            {
                damage.mMelee = record->mGame.mAnimationType <= ESM4::Weapon::GameData::Melee2Hand;
                skill = static_cast<int>(record->mGame.mSkill);
            }
            else
                skill = -1;
        }
        if (attacker != 0 && skill >= AV::firstSkill && skill <= AV::lastSkill)
            damage.mSkill = mActorValues.current(attacker, skill);
        return damage;
    }

    FalloutScripts::StrikeResult FalloutScripts::strike(FalloutScript::FormId attacker, FalloutScript::FormId victim,
        FalloutScript::FormId weapon, int limb, double roll)
    {
        namespace AV = FalloutScript::ActorValue;
        StrikeResult result;
        // The dead strike no blows
        if (victim == 0 || victim == attacker || mActors.isDead(victim) || (attacker != 0 && mActors.isDead(attacker)))
            return result;
        if (weapon == 0 && attacker != 0)
            weapon = mEquipment.weapon(attacker);
        const FalloutScript::WeaponDamage blow = describeWeapon(weapon, attacker);
        if (roll < 0)
        {
            const std::lock_guard lock(mCombatRandomMutex);
            roll = std::uniform_real_distribution<double>(0.0, 100.0)(mCombatRandom);
        }
        FalloutScript::Defense defense;
        defense.mThreshold = mEquipment.damageThreshold(victim);
        defense.mResistance = mEquipment.armorRating(victim);
        const double chance = attacker != 0 ? mActorValues.current(attacker, AV::criticalChance) : 0.0;
        const double bonus = attacker != 0 ? mActorValues.current(attacker, AV::meleeDamage) : 0.0;
        const FalloutScript::HitResult hit
            = FalloutScript::Damage::resolve(damageSettings(), blow, chance, bonus, defense, roll);

        // The blows come one after the other from here: a blow that comes late for a death changes nothing
        const std::lock_guard strikeLock(mStrikeMutex);
        if (mActors.isDead(victim) || (attacker != 0 && mActors.isDead(attacker)))
            return result;
        result.mHit = true;
        result.mDamage = hit.mDamage;
        result.mCritical = hit.mCritical;

        FalloutScript::CombatState::Hit struck;
        struck.mAttacker = attacker;
        struck.mWeapon = weapon;
        struck.mDamage = hit.mDamage;
        struck.mCritical = hit.mCritical;
        struck.mLimb = limb;
        mCombat.noteHit(victim, struck);

        mActorValues.damage(victim, AV::health, hit.mDamage);
        if (mActorValues.current(victim, AV::health) > 0)
        {
            // A character that is hit fights back, and the attacker is in the fight too
            if (attacker != 0)
            {
                if (!mCombat.inCombat(victim) && FalloutScript::mayStartFight(mCombat, mActors, victim, attacker))
                    mCombat.start(victim, attacker);
                if (!mCombat.inCombat(attacker) && FalloutScript::mayStartFight(mCombat, mActors, attacker, victim))
                    mCombat.start(attacker, victim);
            }
            return result;
        }

        // Nobody goes for a target that is out
        mCombat.withdraw(victim);
        if (FalloutScript::isEssential(mCombat, mActors, *this, victim))
        {
            mCombat.setUnconscious(victim, true);
            const std::lock_guard lock(mDownedMutex);
            mDowned[victim] = essentialDownSeconds;
            result.mDowned = true;
        }
        else
        {
            // Of two blows that kill at once, the one that marks the death is the killing blow
            result.mKilled = mActors.setDead(victim, true, attacker, limb);
        }
        return result;
    }

    void FalloutScripts::raiseCombatEvents(float frametime)
    {
        namespace BlockType = FalloutScript::BlockType;
        mCombat.advance(frametime);

        // Essential characters that were down long enough get up with their health back
        std::vector<FalloutScript::FormId> risen;
        {
            const std::lock_guard lock(mDownedMutex);
            for (auto it = mDowned.begin(); it != mDowned.end();)
            {
                it->second -= frametime;
                if (it->second <= 0)
                {
                    risen.push_back(it->first);
                    it = mDowned.erase(it);
                }
                else
                    ++it;
            }
        }
        for (const FalloutScript::FormId actor : risen)
        {
            mCombat.setUnconscious(actor, false);
            mActorValues.heal(actor, FalloutScript::ActorValue::health);
        }

        // A block that names a form is for that form, or for a character made from that record
        const auto namesActor = [this](FalloutScript::FormId actor) {
            FalloutScript::FormId base = 0;
            baseFormOf(actor, base);
            return [actor, base](const FalloutScript::Block& block) {
                return block.mNames != FalloutScript::BlockNames::Form || block.mForm == actor
                    || (base != 0 && block.mForm == base);
            };
        };
        for (const FalloutScript::CombatState::HitEvent& event : mCombat.takeHits())
        {
            mObjects.trigger(event.mVictim, BlockType::OnHit, event.mHit.mAttacker, namesActor(event.mHit.mAttacker));
            if (event.mHit.mWeapon != 0)
            {
                const FalloutScript::FormId weapon = event.mHit.mWeapon;
                mObjects.trigger(event.mVictim, BlockType::OnHitWith, event.mHit.mAttacker,
                    [weapon](const FalloutScript::Block& block) {
                        return block.mNames != FalloutScript::BlockNames::Form || block.mForm == weapon;
                    });
            }
        }
        // However a character died (a blow, KillActor, damage to its health), its fights are over
        for (const FalloutScript::ActorState::Death& death : mActors.takeDeaths())
        {
            mCombat.withdraw(death.mActor);
            mObjects.trigger(death.mActor, BlockType::OnDeath, death.mKiller, namesActor(death.mKiller));
        }
        for (const FalloutScript::CombatState::Change& change : mCombat.takeChanges())
        {
            if (change.mTarget == 0)
                mObjects.trigger(change.mActor, BlockType::OnCombatEnd, change.mWas, namesActor(change.mWas));
            else if (change.mWas == 0)
                mObjects.trigger(change.mActor, BlockType::OnStartCombat, change.mTarget, namesActor(change.mTarget));
        }
    }

    bool FalloutScripts::initialFactions(
        FalloutScript::FormId actor, std::vector<std::pair<FalloutScript::FormId, int>>& factions)
    {
        ESM::FormId base;
        if (!baseOf(actor, base))
            return false;
        const auto take = [&factions](const std::vector<ESM4::ActorFaction>& list) {
            for (const ESM4::ActorFaction& faction : list)
                factions.emplace_back(faction.faction, faction.rank);
        };
        if (base.isZeroOrUnset())
            return true;
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            if (const ESM4::Npc* owner = templateOwner(mStore, npc, ESM4::Npc::Template_UseFactions))
                take(owner->mFactions);
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            if (const ESM4::Creature* owner = templateOwner(mStore, creature, ESM4::Creature::Template_UseFactions))
                take(owner->mFactions);
        }
        return true;
    }

    bool FalloutScripts::initialValues(FalloutScript::FormId actor, std::vector<std::pair<int, double>>& values)
    {
        ESM::FormId base;
        if (!baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        // The attributes, the speed and the karma are the values of the record. Its health, fatigue and skills look
        // like the base of a sum (most characters have 50 or 10 health, 50 fatigue and 5 in barter there), which the
        // games add to by formulas that are not known here, so those keep the starting values.
        // A record that takes its stats from a template has those of the template (a levelled list in the place of
        // one gives a character of it).
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            const ESM4::Npc& stats = statsRecordOf(mStore, *npc, seed);
            if (!stats.mIsFONV)
                return true;
            if (stats.mFalloutStats.mKnown)
                addAttributes(values, stats.mFalloutStats);
            addConfig(values, stats.mBaseConfig);
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const ESM4::Creature& stats = statsRecordOf(mStore, *creature, seed);
            if (!stats.mIsFONV)
                return true;
            if (stats.mFalloutStats.mKnown)
                addAttributes(values, stats.mFalloutStats);
            addConfig(values, stats.mBaseConfig);
        }
        return true;
    }

    const std::map<std::string, double>& FalloutScripts::gameSettingValues()
    {
        if (!mGameSettingValues)
        {
            // The settings of the files by their names (the names of settings are not case sensitive)
            std::map<std::string, double> settings;
            for (const ESM4::GameSetting& setting : mStore.get<ESM4::GameSetting>())
            {
                const auto number = std::visit(
                    [](const auto& value) -> std::optional<double> {
                        using Type = std::decay_t<decltype(value)>;
                        if constexpr (std::is_arithmetic_v<Type>)
                            return static_cast<double>(value);
                        else
                            return std::nullopt;
                    },
                    setting.mData);
                if (number)
                    settings[Misc::StringUtils::lowerCase(setting.mEditorId)] = *number;
            }
            mGameSettingValues = std::move(settings);
        }
        return *mGameSettingValues;
    }

    const FalloutScript::DerivedStatSettings& FalloutScripts::derivedSettings()
    {
        const std::lock_guard lock(mDerivedSettingsMutex);
        if (!mDerivedSettings)
        {
            const std::map<std::string, double>& settings = gameSettingValues();
            mDerivedSettings = FalloutScript::DerivedStatSettings::fromGameSettings(
                [&settings](std::string_view name) -> std::optional<double> {
                    const auto found = settings.find(Misc::StringUtils::lowerCase(name));
                    return found != settings.end() ? std::optional<double>(found->second) : std::nullopt;
                });
        }
        return *mDerivedSettings;
    }

    const FalloutScript::DamageSettings& FalloutScripts::damageSettings()
    {
        const std::lock_guard lock(mDerivedSettingsMutex);
        if (!mDamageSettings)
        {
            const std::map<std::string, double>& settings = gameSettingValues();
            mDamageSettings = FalloutScript::DamageSettings::fromGameSettings(
                [&settings](std::string_view name) -> std::optional<double> {
                    const auto found = settings.find(Misc::StringUtils::lowerCase(name));
                    return found != settings.end() ? std::optional<double>(found->second) : std::nullopt;
                });
        }
        return *mDamageSettings;
    }

    std::optional<double> FalloutScripts::derivedValue(
        FalloutScript::FormId actor, int value, const std::function<double(int)>& permanent)
    {
        namespace AV = FalloutScript::ActorValue;
        namespace Derived = FalloutScript::DerivedStats;
        const FalloutScript::DerivedStatSettings& settings = derivedSettings();
        switch (value)
        {
            case AV::actionPoints:
                return Derived::actionPoints(settings, permanent(AV::agility));
            case AV::carryWeight:
                return Derived::carryWeight(settings, permanent(AV::strength));
            case AV::criticalChance:
                return Derived::criticalChance(settings, permanent(AV::luck));
            case AV::meleeDamage:
                return Derived::meleeDamage(settings, permanent(AV::strength));
            case AV::health:
                break;
            default:
                return std::nullopt;
        }
        // The health starts from the base health of the record, which a record that has a template takes from it. A
        // levelled list in the place of the template gives one of its characters, the one that the cell shows for this
        // reference (the chain is followed with the seed of the reference, as for the level)
        ESM::FormId base;
        if (!baseOf(actor, base) || base.isZeroOrUnset())
            return std::nullopt;
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
        const ESM4::FalloutStats* stats = nullptr;
        bool isCreature = false;
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
            stats = &statsRecordOf(mStore, *npc, seed).mFalloutStats;
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            isCreature = true;
            stats = &statsRecordOf(mStore, *creature, seed).mFalloutStats;
        }
        if (stats == nullptr || !stats->mKnown)
            return std::nullopt;
        // A creature has the health of its record (the radroach of the Vault 101 tutorial 5, two blows of a BB gun)
        if (isCreature)
            return std::max<double>(stats->mHealth, 1.0);
        const double endurance = permanent(AV::endurance);
        if (actor == mPlayer)
            return Derived::playerHealth(settings, stats->mHealth, endurance, mProgress.level());
        int level = 1;
        levelOf(actor, mProgress.level(), level);
        return Derived::npcHealth(settings, stats->mHealth, endurance, level);
    }

    void FalloutScripts::forEachTopic(const std::function<void(const ESM4::Dialogue&)>& function) const
    {
        for (const ESM4::Dialogue& topic : mStore.get<ESM4::Dialogue>())
            function(topic);
    }

    void FalloutScripts::forEachInfo(const std::function<void(const ESM4::DialogInfo&)>& function) const
    {
        for (const ESM4::DialogInfo& info : mStore.get<ESM4::DialogInfo>())
            function(info);
    }

    bool FalloutScripts::hasDialogue() const
    {
        return mStore.get<ESM4::Dialogue>().getSize() > 0;
    }

    bool FalloutScripts::sayTopic(FalloutScript::FormId reference, FalloutScript::FormId topic,
        FalloutScript::FormId listener, bool forceSubtitles, bool followUps)
    {
        ESM::FormId base;
        if (reference == 0 || topic == 0 || !conditionBaseOf(reference, base))
        {
            const std::lock_guard lock(mDialogueMutex);
            if (mSayLogged.insert({ reference, topic }).second)
                Log(Debug::Info) << "Say: " << std::hex << reference << " is to say topic " << topic << std::dec
                                 << ", which cannot be done (the speaker or the topic is not there)";
            return false;
        }
        std::vector<SaidLine> lines;
        {
            const std::lock_guard lock(mDialogueMutex);
            // The player is the listener when none is named (and is the one conditions on the target are about then)
            // A character that is spoken to is a speaker as well, when the two talk to each other
            ESM::FormId listenerBase;
            const bool toCharacter = listener != 0 && listener != mPlayer;
            if (toCharacter && !conditionBaseOf(listener, listenerBase))
                listenerBase = ESM::FormId();
            const FalloutScript::DialogueView view = mDialogue.say({ reference, numberOf(base) }, topic,
                toCharacter ? listener : 0, followUps, !followUps, toCharacter ? numberOf(listenerBase) : 0);
            // The first time that a character is told to say a topic is logged, with what came of it: a script that
            // says something to the player that nobody hears, or nothing, is otherwise silent
            if (mSayLogged.insert({ reference, topic }).second)
            {
                const ESM4::Dialogue* record = mDialogue.topic(topic);
                Log(Debug::Info) << "Say: " << std::hex << reference << std::dec << " is to say topic "
                                 << (record != nullptr ? record->mEditorId : std::string("(not there)")) << " ("
                                 << std::hex << topic << std::dec << "): "
                                 << (view.mLines.empty() ? std::string("nothing it can say")
                                                         : std::to_string(view.mLines.size()) + " line(s)");
            }
            for (std::size_t i = 0; i < view.mLines.size(); ++i)
            {
                const FalloutScript::DialogueLine& line = view.mLines[i];
                SaidLine& said = lines.emplace_back();
                // The other character of an exchange says its lines with its own voice
                const FalloutScript::FormId lineSpeaker = line.mSpeaker != 0 ? line.mSpeaker : reference;
                said.mSpeaker = lineSpeaker != reference ? lineSpeaker : 0;
                said.mVoice = voiceFile(lineSpeaker, line);
                said.mText = line.mText;
                said.mSeconds = lineSeconds(line.mText);
                said.mTopic = topic;
                said.mSpeakerBase = line.mSpeakerBase != 0 ? line.mSpeakerBase : numberOf(base);
                said.mForceSubtitles = forceSubtitles;
                // The response is over with its last line: its end script runs then, not when the speaker is told
                // to say it (with follow-ups the end scripts have run, they decide which response comes next)
                if (!followUps && (i + 1 == view.mLines.size() || view.mLines[i + 1].mInfo != line.mInfo))
                    said.mEndInfo = line.mInfo;
            }
        }
        if (lines.empty())
            return false;
        lines.back().mEnd = true;
        runOnMainThread(
            [this, reference, lines = std::move(lines)]() mutable { startSaying(reference, std::move(lines)); });
        return true;
    }

    void FalloutScripts::converse(
        FalloutScript::FormId speaker, FalloutScript::FormId target, FalloutScript::FormId topic)
    {
        // With no topic the exchange starts with the greeting, as it does with the player
        if (const FalloutScript::FormId first = packageTopic(topic); first != 0)
            sayTopic(speaker, first, target, false, true);
    }

    FalloutScript::FormId FalloutScripts::packageTopic(FalloutScript::FormId topic)
    {
        if (topic != 0)
            return topic;
        const std::lock_guard lock(mDialogueMutex);
        return mDialogue.findTopic(FalloutScript::DialogueManager::greetingTopic);
    }

    float FalloutScripts::lineSeconds(const std::string& text)
    {
        // How long a line takes to read: the time that a response stays when there is no voice to wait for
        constexpr float secondsPerCharacter = 0.055f;
        constexpr float shortest = 1.5f;
        constexpr float longest = 12.f;
        return std::clamp(static_cast<float>(text.size()) * secondsPerCharacter, shortest, longest);
    }

    const FalloutScript::VoiceIndex& FalloutScripts::voices()
    {
        const std::lock_guard lock(mVoiceMutex);
        if (!mVoiceIndexBuilt)
        {
            mVoiceIndexBuilt = true;
            if (const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem())
                for (const VFS::Path::Normalized& path :
                    resources->getVFS()->getRecursiveDirectoryIterator("sound/voice/"))
                    mVoiceIndex.add(path.value());
            Log(Debug::Info) << "Voice files: " << mVoiceIndex.size();
        }
        return mVoiceIndex;
    }

    std::string FalloutScripts::voiceFile(FalloutScript::FormId speaker, const FalloutScript::DialogueLine& line)
    {
        // A response can name a sound record (TRDT) to be said in place of a voice file; when its file is there that is
        // what is said, and when it is not the voice files are looked for as for any other response
        if (line.mSound != 0)
            if (const ESM4::Sound* sound = mStore.get<ESM4::Sound>().search(refId(line.mSound)))
                if (!sound->mSoundFile.empty())
                    if (const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem())
                    {
                        const VFS::Manager& vfs = *resources->getVFS();
                        std::string folder = VFS::Path::normalizeFilename(sound->mSoundFile);
                        if (!folder.empty() && folder.back() == VFS::Path::separator)
                        {
                            // A sound record can name a folder (40, 108 and 151 of the responses with a sound in the
                            // three games): the first file in it is said
                            if (folder.compare(0, 6, "sound/") != 0)
                                folder.insert(0, "sound/");
                            for (const VFS::Path::Normalized& file : vfs.getRecursiveDirectoryIterator(folder))
                            {
                                const std::string_view extension = file.extension().value();
                                if (extension == "wav" || extension == "ogg" || extension == "mp3")
                                    return file.value();
                            }
                        }
                        else
                        {
                            // As for every sound record: the mp3 a wav was converted to, when that is what there is
                            constexpr VFS::Path::NormalizedView soundDir("sound");
                            constexpr VFS::Path::ExtensionView mp3("mp3");
                            const VFS::Path::Normalized path = Misc::ResourceHelpers::correctResourcePath(
                                { { soundDir } }, VFS::Path::toNormalized(sound->mSoundFile), vfs, mp3);
                            if (vfs.exists(path))
                                return path.value();
                        }
                    }
        const FalloutScript::VoiceIndex& index = voices();
        if (index.size() == 0 || line.mInfo == 0)
            return {};
        // The folder of a response is that of the plugin that has it: the one that its id names, or one that is loaded
        // after it and overrides it. The plugin has the files of the lines it records, the id counting from its 24
        // bits. The one loaded last is looked in first: a plugin that overrides a response and records it again is
        // heard, and one that overrides it without a recording leaves the voice of the plugin before it.
        std::vector<std::string> plugins;
        const std::size_t own = line.mInfo >> 24;
        for (std::size_t i = mContentFiles.size(); i-- > own;)
        {
            // A plugin that has a response of its own with the same lower digits has the file of that one (the ids of
            // different plugins count from the same numbers)
            if (i != own
                && mStore.get<ESM4::DialogInfo>().search(
                    refId((static_cast<FalloutScript::FormId>(i) << 24) | (line.mInfo & 0xFFFFFF))))
                continue;
            plugins.push_back(Misc::StringUtils::lowerCase(mContentFiles[i]));
        }
        std::string voiceType;
        FalloutScript::ActorTraits traits;
        if (traitsOf(speaker, traits) && traits.mVoice != 0)
            if (const ESM4::VoiceType* type = mStore.get<ESM4::VoiceType>().search(refId(traits.mVoice)))
                voiceType = type->mEditorId;
        return index.find(plugins, voiceType, line.mInfo, line.mNumber);
    }

    void FalloutScripts::startSaying(FalloutScript::FormId reference, std::vector<SaidLine> lines)
    {
        // A character that says a line is not interrupted by the next: they come one after the other
        Saying saying;
        saying.mReference = reference;
        saying.mLines = std::move(lines);
        for (Saying& other : mSayings)
            if (other.mReference == reference)
            {
                for (SaidLine& line : saying.mLines)
                    other.mLines.push_back(std::move(line));
                return;
            }
        mSayings.push_back(std::move(saying));
    }

    void FalloutScripts::updateVoices(float frametime)
    {
        // Nothing to look after in most frames, and the sound is not there before the game is
        const auto conversationState = static_cast<VoiceState>(mConversationVoice.load());
        if (mSayings.empty() && conversationState != VoiceState::Pending && conversationState != VoiceState::Playing)
            return;
        OFBase::SoundManager* sound = OFBase::Environment::get().getSoundManager();
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (sound == nullptr || world == nullptr)
            return;

        // The voice of the conversation
        {
            const std::lock_guard lock(mDialogueMutex);
            const auto state = static_cast<VoiceState>(mConversationVoice.load());
            if (state == VoiceState::Pending || state == VoiceState::Playing)
            {
                const OFWorld::Ptr speaker = findReference(mConversationVoiceSpeaker);
                if (speaker.isEmpty())
                    mConversationVoice = static_cast<int>(VoiceState::Done);
                else if (state == VoiceState::Pending)
                {
                    sound->say(speaker, VFS::Path::Normalized(mConversationVoiceFile));
                    // Without an output to play on (or a file that does not decode) nothing is said and the lines wait
                    // as long as they take to read
                    mConversationVoice
                        = static_cast<int>(sound->sayActive(speaker) ? VoiceState::Playing : VoiceState::None);
                }
                else if (!sound->sayActive(speaker))
                    mConversationVoice = static_cast<int>(VoiceState::Done);
            }
        }

        // The lines that characters say outside of a conversation
        const OFWorld::Ptr player = world->getPlayerPtr();
        // What the lines that are over bring about, in order: the end script of a response that ended with the line,
        // then the script of the speaker, which is told when it has said all that it was told to
        struct Over
        {
            FalloutScript::FormId mSpeaker = 0; // who said the line
            FalloutScript::FormId mTopic = 0;
            FalloutScript::FormId mEndInfo = 0;
            FalloutScript::FormId mBase = 0;
            bool mDone = false;
            FalloutScript::FormId mTold = 0; // the character that was told to say it, whose script is told it is over
        };
        std::vector<Over> finished;
        for (auto saying = mSayings.begin(); saying != mSayings.end();)
        {
            // The line is said by the character that was told to say them, or by the other of an exchange
            const auto sayerOf
                = [&](const SaidLine& line) { return line.mSpeaker != 0 ? line.mSpeaker : saying->mReference; };
            const OFWorld::Ptr speaker = saying->mNext < saying->mLines.size()
                ? findReference(sayerOf(saying->mLines[saying->mNext]))
                : OFWorld::Ptr();
            if (speaker.isEmpty() || saying->mNext >= saying->mLines.size())
            {
                // A speaker who is not there any more does not say what is left, but what the responses do is done
                if (saying->mNext < saying->mLines.size())
                    for (std::size_t i = saying->mNext; i < saying->mLines.size(); ++i)
                        if (saying->mLines[i].mEndInfo != 0)
                            finished.push_back(
                                { sayerOf(saying->mLines[i]), saying->mLines[i].mTopic, saying->mLines[i].mEndInfo,
                                    saying->mLines[i].mSpeakerBase, false, saying->mReference });
                saying = mSayings.erase(saying);
                continue;
            }
            const SaidLine& line = saying->mLines[saying->mNext];
            if (!saying->mStarted)
            {
                saying->mStarted = true;
                saying->mWait = line.mSeconds;
                bool voiced = false;
                if (!line.mVoice.empty())
                {
                    sound->say(speaker, VFS::Path::Normalized(line.mVoice));
                    voiced = sound->sayActive(speaker);
                }
                saying->mWait = voiced ? 0.f : line.mSeconds;
                // What is said can be read when it is far enough to hear it, if subtitles are on, when there is no
                // voice to hear or when the script that said it forced them
                constexpr float hearingDistance = 1500.f;
                // The cells of an exterior worldspace are one place, an interior is the cell
                const auto sameplace = [&] {
                    const auto* at = speaker.getCell();
                    const auto* with = player.getCell();
                    if (at == with)
                        return true;
                    return at != nullptr && with != nullptr && at->isExterior() && with->isExterior()
                        && at->getCell()->getWorldSpace() == with->getCell()->getWorldSpace();
                };
                const bool near = !player.isEmpty() && sameplace()
                    && (speaker.getRefData().getPosition().asVec3() - player.getRefData().getPosition().asVec3())
                            .length()
                        <= hearingDistance;
                if (near && (!voiced || line.mForceSubtitles || Settings::gui().mSubtitles))
                {
                    const std::string name(speaker.getClass().getName(speaker));
                    OFBase::Environment::get().getWindowManager()->messageBox(
                        name.empty() ? line.mText : name + ": " + line.mText, OFGui::ShowInDialogueMode_Never);
                }
            }
            else
            {
                saying->mWait -= frametime;
                // A voice that plays is waited for; a line without one stays as long as it takes to read
                if (!sound->sayActive(speaker) && saying->mWait <= 0)
                {
                    if (line.mEndInfo != 0 || line.mEnd)
                        finished.push_back({ sayerOf(line), line.mTopic, line.mEndInfo, line.mSpeakerBase, line.mEnd,
                            saying->mReference });
                    ++saying->mNext;
                    saying->mStarted = false;
                }
            }
            ++saying;
        }
        // The script of a character that has said what it was told to goes on (Begin SayToDone Topic): the blocks that
        // name the topic that was said, and those that name none. It can say more, so this waits until the lines are
        // no longer being walked.
        for (const Over& over : finished)
        {
            if (over.mEndInfo != 0)
            {
                const std::lock_guard lock(mDialogueMutex);
                mDialogue.finishSaid({ over.mSpeaker, over.mBase }, over.mEndInfo, over.mTopic);
            }
            if (over.mDone)
                mObjects.trigger(over.mTold, FalloutScript::BlockType::SayToDone, 0,
                    [topic = over.mTopic](
                        const FalloutScript::Block& block) { return FalloutScript::blockIsForForm(block, topic); });
        }
    }

    bool FalloutScripts::sayConversationLine(std::size_t index)
    {
        const std::lock_guard lock(mDialogueMutex);
        const FalloutScript::DialogueView& view = mDialogue.view();
        if (!mDialogue.active() || index >= view.mLines.size())
            return false;
        mConversationVoiceFile = voiceFile(mDialogue.speaker().mReference, view.mLines[index]);
        mConversationVoiceSpeaker = mDialogue.speaker().mReference;
        if (mConversationVoiceFile.empty())
        {
            mConversationVoice = static_cast<int>(VoiceState::None);
            return false;
        }
        Log(Debug::Info) << "Voice: " << mConversationVoiceFile;
        mConversationVoice = static_cast<int>(VoiceState::Pending);
        return true;
    }

    void FalloutScripts::stopConversationVoice()
    {
        const std::lock_guard lock(mDialogueMutex);
        if (mConversationVoice.load() == static_cast<int>(VoiceState::None))
            return;
        const FalloutScript::FormId speaker = mConversationVoiceSpeaker;
        mConversationVoice = static_cast<int>(VoiceState::None);
        // The sound is the business of the thread of the frame
        runOnMainThread([this, speaker] {
            const OFWorld::Ptr ptr = findReference(speaker);
            if (!ptr.isEmpty())
                OFBase::Environment::get().getSoundManager()->stopSay(ptr);
        });
    }

    bool FalloutScripts::startConversation(const OFWorld::Ptr& speaker, FalloutScript::FormId topic)
    {
        const FalloutScript::FormId reference = referenceOf(speaker);
        ESM::FormId base;
        // A talking activator speaks for a character (the intercom of Vault 101): the responses are those of the
        // character
        if (reference == 0 || !conditionBaseOf(reference, base) || base.isZeroOrUnset())
            return false;
        const std::lock_guard lock(mDialogueMutex);
        if (mDialogue.active())
            return mDialogue.speaker().mReference == reference;
        const FalloutScript::DialogueView& view = mDialogue.start({ reference, numberOf(base) }, topic);
        // A character with nothing to say and nothing to pick from does not talk
        if (view.mLines.empty() && view.mOptions.empty())
        {
            mDialogue.end();
            return false;
        }
        mSpeakerName = std::string(speaker.getClass().getName(speaker));
        return true;
    }

    bool FalloutScripts::talkTo(const OFWorld::Ptr& speaker, FalloutScript::FormId topic)
    {
        if (!startConversation(speaker, topic))
            return false;
        OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Dialogue, speaker);
        return true;
    }

    bool FalloutScripts::speakPackage(
        const OFWorld::Ptr& speaker, const ESM4::AIPackage& package, const OFWorld::Ptr& listener, std::string* busy)
    {
        const FalloutScript::FormId reference = referenceOf(speaker);
        if (reference == 0 || speaker.isEmpty())
            return true;
        const FalloutScript::FormId topic = package.mDialogue.mTopic;
        const bool toPlayer = listener.isEmpty();
        const FalloutScript::FormId target = toPlayer ? mPlayer : referenceOf(listener);
        // With another character the games make it a conversation of the two, a Say To package as well
        if (!toPlayer)
        {
            converse(reference, target, topic);
            return true;
        }
        if (package.mDialogue.mType == ESM4::AIPackage::DialogueData::SayTo)
        {
            // One response of the topic, said to the player, and that is all
            if (!sayTopic(reference, packageTopic(topic), 0, false, false))
                Log(Debug::Info) << "Package " << package.mEditorId << " of " << speaker.getCellRef().getRefId()
                                 << ": there is nothing it can say";
            return true;
        }
        // A conversation with the player is the interface: it waits for the player to be done with the one that is
        // open, a message or a menu
        OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager();
        if (windows != nullptr && windows->isGuiMode())
        {
            if (busy != nullptr)
            {
                *busy = "mode " + std::to_string(static_cast<int>(windows->getMode()));
                if (windows->isConsoleMode())
                    *busy += ", the console";
                if (windows->isPostProcessorHudVisible())
                    *busy += ", the post-processor HUD";
                if (windows->isInteractiveMessageBoxActive())
                    *busy += ", a message box";
            }
            return false;
        }
        if (!talkTo(speaker, topic))
            Log(Debug::Info) << "Package " << package.mEditorId << " of " << speaker.getCellRef().getRefId()
                             << ": it has nothing to say to the player";
        return true;
    }

    bool FalloutScripts::isSaying(const OFWorld::Ptr& speaker) const
    {
        const FalloutScript::FormId reference = referenceOf(speaker);
        return reference != 0 && std::any_of(mSayings.begin(), mSayings.end(), [reference](const Saying& saying) {
            // The one who was told to say, or the one whose line is being said in an exchange of two characters
            return saying.mReference == reference
                || (saying.mNext < saying.mLines.size() && saying.mLines[saying.mNext].mSpeaker == reference);
        });
    }

    FalloutScripts::Conversation FalloutScripts::conversation() const
    {
        const std::lock_guard lock(mDialogueMutex);
        Conversation result;
        result.mActive = mDialogue.active();
        result.mView = mDialogue.view();
        result.mSpeakerName = mSpeakerName;
        for (const FalloutScript::DialogueLine& line : result.mView.mLines)
        {
            result.mLineDurations.push_back(lineSeconds(line.mText));
            result.mDuration += result.mLineDurations.back();
        }
        return result;
    }

    void FalloutScripts::chooseOption(FalloutScript::FormId topic)
    {
        const std::lock_guard lock(mDialogueMutex);
        mDialogue.choose(topic);
    }

    void FalloutScripts::finishResponse()
    {
        const std::lock_guard lock(mDialogueMutex);
        mDialogue.finish();
    }

    void FalloutScripts::endConversation()
    {
        const std::lock_guard lock(mDialogueMutex);
        mDialogue.end();
    }

    bool FalloutScripts::traitsOf(FalloutScript::FormId reference, FalloutScript::ActorTraits& traits)
    {
        FalloutScript::ActorTraits found;
        const bool known = traitsOfRecord(reference, found);
        if (reference != mPlayer)
        {
            traits = found;
            return known;
        }
        // The player is who the making of the character made it, and the record only says where it began. A sex or race
        // that was picked takes the voice of the race with it.
        const std::optional<bool> female = mChargen.female();
        const FalloutScript::FormId race = mChargen.race();
        if (!female && race == 0)
        {
            traits = found;
            return known;
        }
        if (female)
            found.mFemale = *female;
        if (race != 0)
            found.mRace = race;
        if (const ESM4::Race* record = mStore.get<ESM4::Race>().search(refId(found.mRace)))
        {
            if (const FalloutScript::FormId voice = numberOf(record->mDefaultVoice[found.mFemale ? 1 : 0]))
                found.mVoice = voice;
            found.mChild = (record->mRaceFlags & 0x4) != 0;
        }
        traits = found;
        return true;
    }

    bool FalloutScripts::traitsOfRecord(FalloutScript::FormId reference, FalloutScript::ActorTraits& traits)
    {
        ESM::FormId base;
        if (!baseOf(reference, base) || base.isZeroOrUnset())
            return false;
        // Race, sex and voice are the Traits tab and the class is the Stats tab, which a record that has a template
        // takes from it. A levelled list in the place of the template gives one of its characters, the one that the
        // cell shows for this reference.
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(reference), base);
        FalloutScript::ActorTraits found;
        bool female = false;
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            const OFClass::StoreWornArmorSource source(mStore);
            const auto chain = ESM4::templateChain(source, *npc, OFClass::ESM4Impl::sDefaultLevel, seed);
            const ESM4::Npc* traitsOwner = ESM4::templateOwner(chain, ESM4::Npc::Template_UseTraits);
            const ESM4::Npc* statsOwner = ESM4::templateOwner(chain, ESM4::Npc::Template_UseStats);
            if (traitsOwner == nullptr)
                traitsOwner = npc;
            if (statsOwner == nullptr)
                statsOwner = npc;
            found.mRace = numberOf(traitsOwner->mRace);
            found.mClass = numberOf(statsOwner->mClass);
            found.mVoice = numberOf(traitsOwner->mVoice);
            female = traitsOwner->mIsFONV ? (traitsOwner->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0
                                          : (traitsOwner->mBaseConfig.tes5.flags & ESM4::Npc::TES5_Female) != 0;
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const OFClass::StoreCreatureSource source(mStore);
            const ESM4::Creature* traitsOwner = ESM4::creatureTemplateOwner(
                ESM4::creatureTemplateChain(source, *creature, OFClass::ESM4Impl::sDefaultLevel, seed),
                ESM4::Creature::Template_UseTraits);
            if (traitsOwner == nullptr)
                traitsOwner = creature;
            found.mCreature = true;
            found.mCreatureType = traitsOwner->mCreatureType;
            found.mVoice = numberOf(traitsOwner->mVoice);
        }
        else if (const ESM4::TalkingActivator* activator = mStore.get<ESM4::TalkingActivator>().search(base))
        {
            // An intercom or a radio has the voice type its record names, and no race, class or sex
            traits = FalloutScript::ActorTraits();
            traits.mVoice = numberOf(activator->mVoiceType);
            return true;
        }
        else
            return false;
        // A character with no voice of its own speaks with the voice that its race has for its sex
        if (const ESM4::Race* race = mStore.get<ESM4::Race>().search(refId(found.mRace)))
        {
            if (found.mVoice == 0)
                found.mVoice = numberOf(race->mDefaultVoice[female ? 1 : 0]);
            found.mChild = !found.mCreature && (race->mRaceFlags & 0x4) != 0;
        }
        found.mFemale = female;
        traits = found;
        return true;
    }

    bool FalloutScripts::levelOf(FalloutScript::FormId actor, int playerLevel, int& level)
    {
        ESM::FormId base;
        if (!baseOf(actor, base) || base.isZeroOrUnset())
            return false;
        // The level is a thing of the stats, which a record that has a template takes from it. A levelled list in the
        // place of the template gives one of its characters, the one that the cell shows for this reference: the chain
        // is followed with the level and the seed that the cell uses (the record's own stats count when the chain ends
        // without a record that has them)
        const std::uint32_t seed = OFClass::ESM4Impl::actorSeed(ESM::FormId::fromUint32(actor), base);
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            const OFClass::StoreWornArmorSource source(mStore);
            const ESM4::Npc* stats
                = ESM4::templateOwner(ESM4::templateChain(source, *npc, OFClass::ESM4Impl::sDefaultLevel, seed),
                    ESM4::Npc::Template_UseStats);
            level = ESM4::actorLevel(stats != nullptr ? *stats : *npc, playerLevel);
            return true;
        }
        if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            const OFClass::StoreCreatureSource source(mStore);
            const ESM4::Creature* stats = ESM4::creatureTemplateOwner(
                ESM4::creatureTemplateChain(source, *creature, OFClass::ESM4Impl::sDefaultLevel, seed),
                ESM4::Creature::Template_UseStats);
            level = ESM4::creatureLevel(stats != nullptr ? *stats : *creature, playerLevel);
            return true;
        }
        return false;
    }

    bool FalloutScripts::initialLock(FalloutScript::FormId reference, bool& locked, int& level)
    {
        // A lock is a thing of the placed reference, which the records of the content files have
        const ESM4::Reference* record = mStore.get<ESM4::Reference>().search(ESM::FormId::fromUint32(reference));
        if (record == nullptr)
            return false;
        locked = record->mIsLocked;
        // The level is a byte that the games use up to 255 (a key is needed)
        level = static_cast<std::uint8_t>(record->mLockLevel);
        return true;
    }

    bool FalloutScripts::formListEntries(FalloutScript::FormId list, std::vector<FalloutScript::FormId>& entries)
    {
        const ESM4::FormIdList* record = mStore.get<ESM4::FormIdList>().search(refId(list));
        if (record == nullptr)
            return false;
        for (const ESM::FormId& form : record->mObjects)
            entries.push_back(numberOf(form));
        return true;
    }

    bool FalloutScripts::baseFormOf(FalloutScript::FormId reference, FalloutScript::FormId& base)
    {
        ESM::FormId form;
        if (!conditionBaseOf(reference, form) || form.isZeroOrUnset())
            return false;
        base = numberOf(form);
        return base != 0;
    }

    FalloutScript::FormId FalloutScripts::talkingActorOf(FalloutScript::FormId activator)
    {
        const std::lock_guard lock(mTalkingActorMutex);
        const auto found = mTalkingActors.find(activator);
        return found != mTalkingActors.end() ? found->second : 0;
    }

    bool FalloutScripts::conditionBaseOf(FalloutScript::FormId reference, ESM::FormId& base)
    {
        if (const FalloutScript::FormId actor = talkingActorOf(reference);
            actor != 0 && baseOf(actor, base) && !base.isZeroOrUnset())
            return true;
        return baseOf(reference, base);
    }

    FalloutScript::FormId FalloutScripts::worldspaceOf(FalloutScript::FormId reference)
    {
        Place place;
        if (!whereIs(reference, place))
            return 0;
        const OFWorld::CellStore* cell = cellAt(place);
        if (cell == nullptr || !cell->isExterior())
            return 0;
        // The cell of a building has the default worldspace, which no record is
        const ESM::FormId* worldspace = cell->getCell()->getWorldSpace().getIf<ESM::FormId>();
        return worldspace != nullptr ? numberOf(*worldspace) : 0;
    }

    FalloutScript::FormId FalloutScripts::linkedRef(FalloutScript::FormId reference, FalloutScript::FormId keyword)
    {
        const ESM::FormId form = ESM::FormId::fromUint32(reference);
        if (form.isZeroOrUnset())
            return 0;
        // The link is in the record of the placed object, whether or not its cell is loaded
        const auto find = [keyword](const std::vector<ESM4::LinkedReference>& links) -> FalloutScript::FormId {
            for (const ESM4::LinkedReference& link : links)
            {
                const FalloutScript::FormId linkKeyword = numberOf(link.keyword);
                if (linkKeyword == keyword && !link.reference.isZeroOrUnset())
                    return numberOf(link.reference);
            }
            return 0;
        };
        if (const ESM4::Reference* object = mStore.get<ESM4::Reference>().search(form))
            return find(object->mLinkedRefs);
        if (const ESM4::ActorCharacter* character = mStore.get<ESM4::ActorCharacter>().search(form))
            return find(character->mLinkedRefs);
        if (const ESM4::ActorCreature* creature = mStore.get<ESM4::ActorCreature>().search(form))
            return find(creature->mLinkedRefs);
        return 0;
    }

    bool FalloutScripts::isDead(const OFWorld::Ptr& actor) const
    {
        const FalloutScript::FormId reference = referenceOf(actor);
        return reference != 0 && mActors.isDead(reference);
    }

    FalloutScript::FormId FalloutScripts::scriptReferenceOf(const OFWorld::Ptr& actor) const
    {
        return actor == OFBase::Environment::get().getWorld()->getPlayerPtr() ? mPlayer : referenceOf(actor);
    }

    std::vector<FalloutScript::FurnitureEntry> FalloutScripts::furnitureEntries(const OFWorld::Ptr& furniture)
    {
        const ESM4::Furniture& record = *furniture.get<ESM4::Furniture>()->mBase;
        const FalloutScript::FormId key = record.mId.toUint32();
        {
            const std::lock_guard lock(mFurnitureMutex);
            const auto found = mFurnitureEntries.find(key);
            if (found != mFurnitureEntries.end())
                return found->second;
        }
        std::vector<FalloutScript::FurnitureEntry> entries;
        Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem();
        if (resources != nullptr && !record.mModel.empty())
        {
            try
            {
                // One block of the model has the markers
                const Nif::NIFFilePtr file = resources->getNifFileManager()->get(
                    Misc::ResourceHelpers::correctMeshPath(record.mModel.getNormalized()));
                for (const auto& block : file->mRecords)
                {
                    const auto* marker = dynamic_cast<const Nif::BSFurnitureMarker*>(block.get());
                    if (marker == nullptr)
                        continue;
                    for (const auto& legacy : marker->mLegacyMarkers)
                        entries.push_back({ { legacy.mOffset.x(), legacy.mOffset.y(), legacy.mOffset.z() },
                            legacy.mOrientation / 1000.f, legacy.mPositionRef });
                    for (const auto& position : marker->mMarkers)
                        entries.push_back({ { position.mOffset.x(), position.mOffset.y(), position.mOffset.z() },
                            position.mHeading, 0 });
                    break;
                }
            }
            catch (const std::exception& e)
            {
                Log(Debug::Warning) << "Furniture " << record.mEditorId
                                    << ": the markers of the model cannot be read: " << e.what();
            }
        }
        const std::lock_guard lock(mFurnitureMutex);
        mFurnitureEntries[key] = entries;
        return entries;
    }

    std::optional<FalloutScript::FurnitureSeat> FalloutScripts::chooseSeat(
        FalloutScript::FormId user, const OFWorld::Ptr& furniture, const osg::Vec3f& from)
    {
        const std::vector<FalloutScript::FurnitureEntry> entries = furnitureEntries(furniture);
        if (entries.empty())
            return std::nullopt;
        // The entries that others use are left to them
        const FalloutScript::FormId seat = referenceOf(furniture);
        std::uint32_t used = 0;
        {
            const std::lock_guard lock(mFurnitureMutex);
            for (const auto& [other, use] : mFurniture)
                if (other != user && use.mFurniture == seat && use.mEntry >= 0
                    && static_cast<std::size_t>(use.mEntry) < FalloutScript::maxFurnitureEntries)
                    used |= 1u << use.mEntry;
        }
        const ESM::Position& position = furniture.getRefData().getPosition();
        FalloutScript::FurniturePlacement placement;
        placement.mPosition = { position.pos[0], position.pos[1], position.pos[2] };
        for (int axis = 0; axis < 3; ++axis)
            placement.mRotation[axis] = position.rot[axis];
        placement.mScale = furniture.getCellRef().getScale();
        return FalloutScript::chooseFurnitureSeat(entries, furniture.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags,
            used, placement, { from.x(), from.y(), from.z() });
    }

    bool FalloutScripts::furnitureSeat(
        const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture, osg::Vec3f& position, float& heading)
    {
        if (furniture.isEmpty() || furniture.getType() != ESM4::Furniture::sRecordId)
            return false;
        const std::optional<FalloutScript::FurnitureSeat> seat
            = chooseSeat(scriptReferenceOf(actor), furniture, actor.getRefData().getPosition().asVec3());
        if (!seat)
            return false;
        position = osg::Vec3f(seat->mPosition.x, seat->mPosition.y, seat->mPosition.z);
        heading = seat->mHeading;
        return true;
    }

    bool FalloutScripts::useFurniture(const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture, bool place)
    {
        const FalloutScript::FormId user = scriptReferenceOf(actor);
        const FalloutScript::FormId seat = referenceOf(furniture);
        if (user == 0 || seat == 0)
            return true;
        osg::Vec3f where = actor.getRefData().getPosition().asVec3();
        const std::optional<FalloutScript::FurnitureSeat> entry = chooseSeat(user, furniture, where);
        if (!entry && !furnitureEntries(furniture).empty())
        {
            // The model has places to use and none is free (or the record enables none): the games do nothing then
            Log(Debug::Info) << "Furniture: " << FalloutScript::hex(user) << " cannot use " << FalloutScript::hex(seat)
                             << ": no entry of its marker is free";
            return false;
        }
        Place seatPlace;
        Place userPlace;
        if (place && whereIs(seat, seatPlace) && whereIs(user, userPlace) && inOneSpace(seatPlace, userPlace))
        {
            // The world is not changed in the middle of what activates: the actor is put there in the next frame
            Move move;
            move.mWhere = seatPlace;
            move.mParts = Move::PositionX | Move::PositionY | Move::PositionZ;
            if (entry)
            {
                move.mWhere.mPosition = osg::Vec3f(entry->mPosition.x, entry->mPosition.y, entry->mPosition.z);
                move.mWhere.mAngle.z() = entry->mHeading;
                move.mParts |= Move::AngleZ;
            }
            where = move.mWhere.mPosition;
            askForMove(user, move);
        }
        else
            place = false;
        {
            const std::lock_guard lock(mFurnitureMutex);
            FurnitureUse& use = mFurniture[user];
            use.mFurniture = seat;
            use.mPlace = where;
            use.mFurniturePlace = furniture.getRefData().getPosition().asVec3();
            // An entry is taken only by an actor that is put on it
            use.mEntry = place && entry ? static_cast<int>(entry->mIndex) : -1;
            use.mKind = entry ? entry->mRef : 10;
        }
        Log(Debug::Info) << "Furniture: " << FalloutScript::hex(user) << " uses " << FalloutScript::hex(seat)
                         << (entry ? " at entry " + std::to_string(entry->mIndex) : std::string())
                         << (place ? " (put there)" : "");
        return true;
    }

    void FalloutScripts::activateFurniture(const OFWorld::Ptr& actor, const OFWorld::Ptr& furniture)
    {
        // The characters and creatures of Fallout are not actors of the engine, so isActor does not tell them
        if (actor.isEmpty() || !(actor.getClass().isActor() || isCharacterOrCreature(actor)))
            return;
        if (usesFurniture(actor))
            leaveFurniture(actor);
        else
            useFurniture(actor, furniture, true);
    }

    void FalloutScripts::leaveFurniture(const OFWorld::Ptr& actor)
    {
        const FalloutScript::FormId user = scriptReferenceOf(actor);
        FalloutScript::FormId seat = 0;
        {
            const std::lock_guard lock(mFurnitureMutex);
            const auto found = mFurniture.find(user);
            if (found == mFurniture.end())
                return;
            seat = found->second.mFurniture;
            mFurniture.erase(found);
        }
        Log(Debug::Info) << "Furniture: " << FalloutScript::hex(user) << " gets up from " << FalloutScript::hex(seat);
    }

    bool FalloutScripts::usesFurniture(const OFWorld::Ptr& actor) const
    {
        const FalloutScript::FormId user = scriptReferenceOf(actor);
        const std::lock_guard lock(mFurnitureMutex);
        return user != 0 && mFurniture.count(user) != 0;
    }

    FalloutScript::FormId FalloutScripts::currentFurniture(FalloutScript::FormId actor)
    {
        const std::lock_guard lock(mFurnitureMutex);
        const auto found = mFurniture.find(actor);
        return found != mFurniture.end() ? found->second.mFurniture : 0;
    }

    int FalloutScripts::currentFurnitureKind(FalloutScript::FormId actor)
    {
        const std::lock_guard lock(mFurnitureMutex);
        const auto found = mFurniture.find(actor);
        return found != mFurniture.end() ? found->second.mKind : 0;
    }

    void FalloutScripts::updateFurniture()
    {
        // An actor that is more than a few steps from where it sat down, or is gone, disabled, deleted or dead, or
        // whose furniture is gone, has got up
        constexpr float reach = 128.f;
        std::vector<std::pair<FalloutScript::FormId, FurnitureUse>> uses;
        {
            const std::lock_guard lock(mFurnitureMutex);
            uses.assign(mFurniture.begin(), mFurniture.end());
        }
        for (const auto& [user, use] : uses)
        {
            const OFWorld::Ptr actor = findReference(user);
            const bool present = !actor.isEmpty() && actor.getRefData().isEnabled() && !actor.mRef->isDeleted();
            // The player is an actor of the engine, whose death the stats tell; the others are told by the host
            const bool dead = present
                && (mActors.isDead(user)
                    || (actor.getClass().isActor() && actor.getClass().getCreatureStats(actor).isDead()));
            // The furniture is gone when it is not there, is disabled or deleted
            const OFWorld::Ptr furniture = present ? findReference(use.mFurniture) : OFWorld::Ptr();
            // or has been moved to another place, since the actor was put where the furniture was
            constexpr float furnitureMoved = 16.f;
            const bool furnitureThere = !furniture.isEmpty() && furniture.getRefData().isEnabled()
                && !furniture.mRef->isDeleted()
                && (furniture.getRefData().getPosition().asVec3() - use.mFurniturePlace).length() <= furnitureMoved;
            if (present && !dead && furnitureThere
                && (actor.getRefData().getPosition().asVec3() - use.mPlace).length() <= reach)
                continue;
            {
                const std::lock_guard lock(mFurnitureMutex);
                mFurniture.erase(user);
            }
            Log(Debug::Info) << "Furniture: " << FalloutScript::hex(user) << " gets up from "
                             << FalloutScript::hex(use.mFurniture);
        }
    }

    void FalloutScripts::addScriptPackage(FalloutScript::FormId actor, FalloutScript::FormId package)
    {
        // The packages are read by the characters on the thread of the frame, so they are changed there
        runOnMainThread([this, actor, package] {
            // A package that is among them already is the last again
            std::vector<FalloutScript::FormId>& packages = mScriptPackages[actor];
            packages.erase(std::remove(packages.begin(), packages.end(), package), packages.end());
            packages.push_back(package);
            evaluatePackage(actor);
        });
    }

    void FalloutScripts::removeScriptPackage(FalloutScript::FormId actor)
    {
        runOnMainThread([this, actor] {
            const auto found = mScriptPackages.find(actor);
            if (found == mScriptPackages.end())
                return;
            found->second.pop_back();
            if (found->second.empty())
                mScriptPackages.erase(found);
            evaluatePackage(actor);
        });
    }

    void FalloutScripts::evaluatePackage(FalloutScript::FormId actor)
    {
        // A character that is not in a loaded cell looks at its packages when it is
        runOnMainThread([this, actor] {
            const OFWorld::Ptr ptr = findReference(actor);
            if (!ptr.isEmpty() && ptr != OFBase::Environment::get().getWorld()->getPlayerPtr())
                OFBase::Environment::get().getMechanicsManager()->evaluateFalloutPackages(ptr);
        });
    }

    void FalloutScripts::resetAI(FalloutScript::FormId actor)
    {
        // The character gets up from the furniture it uses, then looks at its packages again
        runOnMainThread([this, actor] {
            const OFWorld::Ptr ptr = findReference(actor);
            if (ptr.isEmpty())
                return;
            leaveFurniture(ptr);
            if (ptr != OFBase::Environment::get().getWorld()->getPlayerPtr())
                OFBase::Environment::get().getMechanicsManager()->evaluateFalloutPackages(ptr);
        });
    }

    FalloutScript::FormId FalloutScripts::currentPackage(FalloutScript::FormId actor)
    {
        // The characters make up their minds on the thread of the frame, which is the only one that may look
        if (std::this_thread::get_id() != mMainThread)
            return 0;
        const OFWorld::Ptr ptr = findReference(actor);
        if (ptr.isEmpty())
            return 0;
        return numberOf(OFBase::Environment::get().getMechanicsManager()->falloutPackageOf(ptr));
    }

    std::vector<const ESM4::AIPackage*> FalloutScripts::scriptPackages(const OFWorld::Ptr& actor) const
    {
        std::vector<const ESM4::AIPackage*> result;
        const auto found = mScriptPackages.find(referenceOf(actor));
        if (found == mScriptPackages.end())
            return result;
        for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
            if (const ESM4::AIPackage* package = mStore.get<ESM4::AIPackage>().search(refId(*it)))
                result.push_back(package);
        return result;
    }

    bool FalloutScripts::packageConditionsHold(const ESM4::AIPackage& package, const OFWorld::Ptr& actor)
    {
        // The conditions of the old layout (those of Oblivion) can not be told
        if (!package.mConditions.empty())
            return false;
        FalloutScript::ConditionContext context;
        context.mSubject = referenceOf(actor);
        return mConditions.holds(package.mTargetConditions, context);
    }

    void FalloutScripts::packageEvent(const OFWorld::Ptr& actor, const ESM4::AIPackage& package, PackageEvent event)
    {
        namespace BlockType = FalloutScript::BlockType;
        const FalloutScript::FormId reference = referenceOf(actor);
        if (reference == 0)
            return;
        const ESM4::AIPackage::Event* scripted = &package.mBegin;
        std::uint16_t blockType = BlockType::OnPackageStart;
        const char* what = "begins";
        switch (event)
        {
            case PackageEvent::Start:
                break;
            case PackageEvent::Done:
                scripted = &package.mEnd;
                blockType = BlockType::OnPackageDone;
                what = "is done";
                break;
            case PackageEvent::Change:
                scripted = &package.mChange;
                blockType = BlockType::OnPackageChange;
                what = "is changed";
                break;
        }
        // The script of the package comes first, then the blocks of the character that name the package or none
        const FalloutScript::FormId form = numberOf(package.mId);
        bool ran = mObjects.runResult(scripted->mScript, reference);
        ran = mObjects.trigger(reference, blockType, 0, [form](const FalloutScript::Block& block) {
            return FalloutScript::blockIsForForm(block, form);
        }) || ran;
        if (ran)
            Log(Debug::Info) << "Package " << package.mEditorId << " of " << std::hex << reference << std::dec << " "
                             << what;
    }

    FalloutScript::FormId FalloutScripts::cellOf(FalloutScript::FormId reference)
    {
        // Where the script has put the reference counts, though the world has not done it yet
        Place place;
        if (!whereIs(reference, place))
            return 0;
        const ESM::FormId* id = place.mCell.getIf<ESM::FormId>();
        return id != nullptr ? numberOf(*id) : 0;
    }

    bool FalloutScripts::inCell(FalloutScript::FormId reference, FalloutScript::FormId cell)
    {
        const FalloutScript::FormId here = cellOf(reference);
        if (here == 0 || cell == 0)
            return false;
        if (here == cell)
            return true;
        // The games compare the names of the cells with the name of the asked one as the beginning (the intro of
        // Fallout 3 asks for "Vault101" in the cells of the vault): the editor ids are what the data gives as names
        const auto& cells = mStore.get<ESM4::Cell>();
        const ESM4::Cell* wanted = cells.search(refId(cell));
        const ESM4::Cell* actual = cells.search(refId(here));
        return wanted != nullptr && actual != nullptr
            && FalloutScript::cellNameBegins(actual->mEditorId, wanted->mEditorId);
    }

    bool FalloutScripts::currentPlace(FalloutScript::FormId reference, Place& result)
    {
        // An object that PlaceAtMe asked for is next to the object it was asked for, until the world makes it
        {
            const std::lock_guard lock(mMadeMutex);
            if (const auto asked = mPendingMade.find(reference); asked != mPendingMade.end())
            {
                result = asked->second.mWhere;
                return true;
            }
        }
        const OFWorld::Ptr ptr = findReference(reference);
        if (!ptr.isEmpty() && ptr.getCell() != nullptr)
        {
            const ESM::Position& position = ptr.getRefData().getPosition();
            result = { ptr.getCell()->getCell()->getId(), position.asVec3(), position.asRotationVec3() };
            return true;
        }
        if (const auto moved = mRelocations.find(reference); moved != mRelocations.end())
        {
            result = moved->second.mWhere;
            return true;
        }
        // Not in a loaded cell: where the record of the reference puts it
        const ESM::FormId id = ESM::FormId::fromUint32(reference);
        if (id.isZeroOrUnset())
            return false;
        const auto fromRecord = [&](const auto& record) {
            result = { record.mParent, record.mPos.asVec3(), record.mPos.asRotationVec3() };
            return true;
        };
        if (const ESM4::Reference* record = mStore.get<ESM4::Reference>().search(id))
            return fromRecord(*record);
        if (const ESM4::ActorCharacter* record = mStore.get<ESM4::ActorCharacter>().search(id))
            return fromRecord(*record);
        if (const ESM4::ActorCreature* record = mStore.get<ESM4::ActorCreature>().search(id))
            return fromRecord(*record);
        return false;
    }

    OFWorld::CellStore* FalloutScripts::cellAt(const Place& place)
    {
        OFWorld::WorldModel* model = OFBase::Environment::get().getWorldModel();
        OFWorld::CellStore* cell = model->findCell(place.mCell);
        // A position outside the square of an exterior cell belongs to the one next to it
        if (cell != nullptr && cell->isExterior())
            cell = &model->getExterior(ESM::positionToExteriorCellLocation(
                place.mPosition.x(), place.mPosition.y(), cell->getCell()->getWorldSpace()));
        return cell;
    }

    bool FalloutScripts::whereIs(FalloutScript::FormId reference, Place& result)
    {
        if (!currentPlace(reference, result))
            return false;
        // What scripts asked for and the world has not done yet is where the object is for them
        unsigned moved = 0;
        if (const auto planned = mPendingMoves.find(reference); planned != mPendingMoves.end())
        {
            result = planned->second.applyTo(result);
            moved |= planned->second.mParts;
        }
        {
            // What another thread asked for is newer still
            const std::lock_guard lock(mDeferredMutex);
            if (const auto asked = mDeferredPlaces.find(reference); asked != mDeferredPlaces.end())
            {
                result = asked->second.applyTo(result);
                moved |= asked->second.mParts;
            }
        }
        // A position outside the square of an exterior cell is in the cell next to it, as the world will have it
        if ((moved & (Move::Cell | Move::PositionX | Move::PositionY)) != 0)
            if (const OFWorld::CellStore* cell = cellAt(result))
                result.mCell = cell->getCell()->getId();
        return true;
    }

    bool FalloutScripts::placement(FalloutScript::FormId reference, FalloutScript::Placement& result)
    {
        Place place;
        if (!whereIs(reference, place))
            return false;
        for (int axis = 0; axis < 3; ++axis)
        {
            result.mPosition[axis] = place.mPosition[axis];
            result.mAngle[axis] = place.mAngle[axis];
        }
        return true;
    }

    void FalloutScripts::moveTo(
        FalloutScript::FormId reference, FalloutScript::FormId target, const std::array<double, 3>& offset)
    {
        Place from;
        Place to;
        if (!whereIs(reference, from) || !whereIs(target, to))
        {
            log("MoveToMarker: " + FalloutScript::hex(reference) + " or " + FalloutScript::hex(target)
                + " has no place the game knows");
            return;
        }
        // Markers point somewhere, and what stands on them turns that way; a tilted marker does not tilt it
        Move move;
        move.mWhere = to;
        move.mWhere.mPosition
            += osg::Vec3f(static_cast<float>(offset[0]), static_cast<float>(offset[1]), static_cast<float>(offset[2]));
        move.mParts = Move::Cell | Move::PositionX | Move::PositionY | Move::PositionZ | Move::AngleZ;
        // The world is not changed while scripts run
        askForMove(reference, move);
    }

    void FalloutScripts::setPosition(FalloutScript::FormId reference, int axis, double value)
    {
        Place place;
        if (axis < 0 || axis > 2 || !whereIs(reference, place))
            return;
        Move move;
        move.mWhere = place;
        move.mWhere.mPosition[axis] = static_cast<float>(value);
        move.mParts = Move::PositionX << axis;
        askForMove(reference, move);
    }

    void FalloutScripts::setAngle(FalloutScript::FormId reference, int axis, double radians)
    {
        Place place;
        if (axis < 0 || axis > 2 || !whereIs(reference, place))
            return;
        Move move;
        move.mWhere = place;
        move.mWhere.mAngle[axis] = static_cast<float>(radians);
        move.mParts = Move::AngleX << axis;
        askForMove(reference, move);
    }

    void FalloutScripts::queueMove(
        std::map<FalloutScript::FormId, Move>& moves, FalloutScript::FormId reference, const Move& move)
    {
        const auto [found, added] = moves.try_emplace(reference, move);
        if (!added)
            found->second.add(move);
    }

    void FalloutScripts::askForMove(FalloutScript::FormId reference, const Move& move)
    {
        if (std::this_thread::get_id() != mMainThread)
        {
            const std::lock_guard lock(mDeferredMutex);
            queueMove(mDeferredPlaces, reference, move);
        }
        runOnMainThread([this, reference, move] { queueMove(mPendingMoves, reference, move); });
    }

    void FalloutScripts::applyMove(FalloutScript::FormId reference, const Move& move)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();
        // The parts that the script named are put on the place the object has now, not on the one it had when the
        // script asked
        Place where;
        if (!currentPlace(reference, where))
        {
            log("MoveToMarker: " + FalloutScript::hex(reference) + " has no place the game knows");
            return;
        }
        where = move.applyTo(where);
        const OFWorld::Ptr ptr = findReference(reference);
        if (ptr.isEmpty())
        {
            // It is put there when its cell is loaded
            Move& relocation = mRelocations[reference];
            relocation.mWhere = where;
            relocation.mParts |= move.mParts;
            return;
        }
        if ((move.mParts & (Move::Cell | Move::PositionX | Move::PositionY | Move::PositionZ)) == 0)
        {
            // Only turned: that is not a teleport, so what a teleport resets (the fall, the movement of the character,
            // the Lua event) is left alone. The player is turned in the order the engine turns it in everywhere else.
            world->rotateObject(ptr, where.mAngle,
                ptr == world->getPlayerPtr() ? OFBase::RotationFlag_inverseOrder : OFBase::RotationFlag_none);
            return;
        }
        OFWorld::CellStore* destination = cellAt(where);
        if (destination == nullptr)
        {
            log("MoveToMarker: the cell " + where.mCell.toDebugString() + " of " + FalloutScript::hex(reference)
                + " is not known");
            return;
        }
        const OFWorld::Class& cls = ptr.getClass();
        if (ptr == world->getPlayerPtr())
        {
            OFMechanics::CreatureStats& stats = cls.getCreatureStats(ptr);
            stats.land(true);
            stats.setTeleported(true);
            world->getPlayer().setTeleported(true);
            // Inside the cell the player is in, and between the exterior cells of one worldspace, the player is only
            // moved (the world takes it from cell to cell): changing to a cell fades the screen out and in again, which
            // a script that sets the position of the player every frame must not do
            const OFWorld::CellStore* current = ptr.getCell();
            const bool sameSpace = current == destination
                || (current->isExterior() && destination->isExterior()
                    && current->getCell()->getWorldSpace() == destination->getCell()->getWorldSpace());
            if (!sameSpace)
            {
                ESM::Position position;
                for (int axis = 0; axis < 3; ++axis)
                {
                    position.pos[axis] = where.mPosition[axis];
                    position.rot[axis] = where.mAngle[axis];
                }
                world->changeToCell(destination->getCell()->getId(), position, false, true);
            }
            const OFWorld::Ptr player = world->getPlayerPtr();
            world->moveObject(player, where.mPosition);
            // The player is turned in the order that the engine turns it in everywhere else (changeToCell does the
            // same), while the references of the cells are turned in the order that the cell loader uses for them
            world->rotateObject(player, where.mAngle);
            OFBase::Environment::get().getLuaManager()->objectTeleported(player);
            return;
        }
        if (cls.isActor())
        {
            OFMechanics::CreatureStats& stats = cls.getCreatureStats(ptr);
            stats.land(false);
            stats.setTeleported(true);
        }
        const OFWorld::Ptr moved = world->moveObject(ptr, destination, where.mPosition);
        world->rotateObject(moved, where.mAngle, OFBase::RotationFlag_none);
        // As the teleport of Lua does: the place a door is moved to is the one it opens and closes from
        if (cls.isDoor())
            moved.getCellRef().setPosition(moved.getRefData().getPosition());
        OFBase::Environment::get().getLuaManager()->objectTeleported(moved);
    }

    FalloutScript::FormId FalloutScripts::placeAtMe(
        FalloutScript::FormId reference, FalloutScript::FormId base, int count, double distance, int direction)
    {
        Place near;
        if (!whereIs(reference, near))
        {
            log("PlaceAtMe: " + FalloutScript::hex(reference) + " has no place the game knows");
            return 0;
        }
        FalloutScript::FormId first = 0;
        {
            const std::lock_guard lock(mMadeMutex);
            for (int i = 0; i < count && mMadeCount < 0x00ffffff; ++i)
            {
                // The numbers of objects made while the game runs begin with 0xFF, as in the games
                const FalloutScript::FormId id = 0xff000000u | ++mMadeCount;
                // Asked for at once, so that the rest of the script finds the object it was told the number of (also
                // when a script that Lua started runs on the thread of Lua); the world makes it at the next frame
                mPendingMade[id] = MadeObject{ base, reference, direction >= 0 && direction <= 3 ? direction : 0,
                    static_cast<float>(distance), near };
                if (first == 0)
                    first = id;
            }
        }
        if (first == 0)
        {
            log("PlaceAtMe: no more objects can be made");
            return 0;
        }
        return first;
    }

    void FalloutScripts::makeObject(FalloutScript::FormId id, const MadeObject& request)
    {
        const OFWorld::Ptr near = findReference(request.mNear);
        if (near.isEmpty() || !near.isInCell())
        {
            log("PlaceAtMe: " + FalloutScript::hex(request.mNear) + " is not in a loaded cell, so nothing is made next "
                "to it");
            return;
        }
        try
        {
            // The same way the Morrowind script command places something next to an actor
            OFWorld::ManualRef ref(mStore, refId(request.mBase), 1);
            // A character or creature of Fallout is an actor too: its body is made at once, as in a cell
            ref.getPtr().mRef->mData.mPhysicsPostponed
                = !ref.getPtr().getClass().isActor() && !isCharacterOrCreature(ref.getPtr());
            const OFWorld::Ptr placed = OFBase::Environment::get().getWorld()->safePlaceObject(
                ref.getPtr(), near, near.getCell(), request.mDirection, request.mDistance);
            const ESM::FormId number = placed.getCellRef().getRefNum();
            mMade[id] = number;
            mMadeIds[number] = id;
            mReferences[id] = placed;
            // An actor that is made sets triggers off like those of the cells (when its cell is loaded again, the cell
            // tells of it)
            if (isCharacterOrCreature(placed))
                mTriggerActors.insert(id);
            // It runs its script like any object that is loaded
            const FalloutScript::FormId script = scriptOf(placed, id);
            if (script != 0 && mObjects.add(id, script) != nullptr)
                mPendingLoad.push_back(id);
        }
        catch (const std::exception& error)
        {
            log("PlaceAtMe: an object of " + FalloutScript::hex(request.mBase) + " can not be made: " + error.what());
        }
    }

    void FalloutScripts::activate(FalloutScript::FormId target, FalloutScript::FormId activator, bool runOnActivate)
    {
        runOnMainThread([this, target, activator, runOnActivate] {
            if (runOnActivate && mObjects.trigger(target, FalloutScript::BlockType::OnActivate, activator))
                return;
            const OFWorld::Ptr object = findReference(target);
            const OFWorld::Ptr actor = findReference(activator);
            if (!object.isEmpty() && !actor.isEmpty())
                OFBase::Environment::get().getLuaManager()->defaultActivation(object, actor);
        });
    }

    namespace
    {
        /// The id written as 0x and hexadecimal digits, with or without the "FormId:" that core.getFormId puts before
        /// them, or 0
        FalloutScript::FormId parseFormId(std::string_view name)
        {
            constexpr std::string_view prefix = "FormId:";
            if (name.starts_with(prefix))
                name.remove_prefix(prefix.size());
            if (name.size() <= 2 || name[0] != '0' || (name[1] != 'x' && name[1] != 'X'))
                return 0;
            char* end = nullptr;
            const std::string text(name.substr(2));
            const unsigned long long value = std::strtoull(text.c_str(), &end, 16);
            if (end == text.c_str() || *end != '\0' || value > 0xFFFFFFFFull)
                return 0;
            return static_cast<FalloutScript::FormId>(value);
        }
    }

    void FalloutScripts::buildNames() const
    {
        if (mNamesBuilt)
            return;
        mNamesBuilt = true;
        for (const ESM4::Quest& quest : mStore.get<ESM4::Quest>())
            mQuestNames.emplace(Misc::StringUtils::lowerCase(quest.mEditorId), quest.mId.toUint32());
        for (const ESM4::GlobalVariable& global : mStore.get<ESM4::GlobalVariable>())
            mGlobalNames.emplace(Misc::StringUtils::lowerCase(global.mEditorId), global.mId.toUint32());
    }

    FalloutScript::FormId FalloutScripts::findQuest(std::string_view name) const
    {
        if (const FalloutScript::FormId id = parseFormId(name))
            return mQuests.known(id) ? id : 0;
        buildNames();
        const auto found = mQuestNames.find(Misc::StringUtils::lowerCase(name));
        return found != mQuestNames.end() ? found->second : 0;
    }

    FalloutScript::FormId FalloutScripts::findGlobal(std::string_view name) const
    {
        if (const FalloutScript::FormId id = parseFormId(name))
            return mStore.get<ESM4::GlobalVariable>().search(refId(id)) != nullptr ? id : 0;
        buildNames();
        const auto found = mGlobalNames.find(Misc::StringUtils::lowerCase(name));
        return found != mGlobalNames.end() ? found->second : 0;
    }

    std::string FalloutScripts::editorId(FalloutScript::FormId id) const
    {
        if (const FalloutScript::QuestState* quest = mQuests.state(id))
            return quest->mRecord->mEditorId;
        if (const ESM4::GlobalVariable* global = mStore.get<ESM4::GlobalVariable>().search(refId(id)))
            return global->mEditorId;
        return {};
    }

    FalloutScripts::ActorValueText FalloutScripts::actorValueText(
        std::string_view editorId, std::string_view fallback) const
    {
        if (!mActorValueTextBuilt)
        {
            mActorValueTextBuilt = true;
            for (const ESM4::ActorValueInfo& info : mStore.get<ESM4::ActorValueInfo>())
                mActorValueText.emplace(
                    Misc::StringUtils::lowerCase(info.mEditorId), ActorValueText{ info.mFullName, info.mDescription });
        }
        ActorValueText text;
        const auto found = mActorValueText.find(Misc::StringUtils::lowerCase(editorId));
        if (found != mActorValueText.end())
            text = found->second;
        if (text.mName.empty())
            text.mName = fallback;
        return text;
    }

    std::vector<FalloutScripts::ChargenRace> FalloutScripts::playableRaces() const
    {
        constexpr std::uint32_t playableFlag = 0x1;
        constexpr std::uint32_t childFlag = 0x4;
        std::vector<ChargenRace> races;
        for (const ESM4::Race& race : mStore.get<ESM4::Race>())
        {
            if ((race.mRaceFlags & playableFlag) == 0 || (race.mRaceFlags & childFlag) != 0)
                continue;
            races.push_back({ numberOf(race.mId), race.mFullName.empty() ? race.mEditorId : race.mFullName });
        }
        return races;
    }

    std::vector<FalloutScripts::ChargenTrait> FalloutScripts::chargenTraits() const
    {
        std::vector<ChargenTrait> traits;
        if (!mChargen.hasTraits())
            return traits;
        for (const ESM4::Perk& perk : mStore.get<ESM4::Perk>())
            if (perk.mData.mTrait != 0 && !perk.mFullName.empty())
                traits.push_back({ numberOf(perk.mId), perk.mFullName, perk.mDescription });
        std::sort(traits.begin(), traits.end(), [](const ChargenTrait& left, const ChargenTrait& right) {
            return left.mName != right.mName ? left.mName < right.mName : left.mId < right.mId;
        });
        return traits;
    }

    bool FalloutScripts::playerTraits(FalloutScript::ActorTraits& traits)
    {
        return traitsOf(mPlayer, traits);
    }

    void FalloutScripts::beginChargenPage()
    {
        // A new character starts at 5 in each attribute, whatever its record says; one that has been through the screen
        // starts from what it has now (which scripts may have changed since)
        if (mChargen.page() != FalloutScript::ChargenPage::Special || mChargen.serial() == mChargenSeeded)
            return;
        mChargenSeeded = mChargen.serial();
        std::array<int, FalloutScript::Chargen::attributeCount> attributes;
        const bool made = mChargen.specialMade();
        for (int i = 0; i < FalloutScript::Chargen::attributeCount; ++i)
            attributes[static_cast<std::size_t>(i)] = made
                ? static_cast<int>(std::lround(mActorValues.base(mPlayer, FalloutScript::Chargen::firstAttribute + i)))
                : 5;
        mChargen.setAttributes(attributes);
    }

    void FalloutScripts::applyChargenSkills()
    {
        for (const FalloutScript::ChargenSkill& skill : mChargen.skills())
            mActorValues.setBase(mPlayer, skill.mNumber, mChargen.skillValue(skill.mNumber));
    }

    void FalloutScripts::finishChargenPage()
    {
        const std::optional<FalloutScript::ChargenPage> done = mChargen.finishPage();
        if (!done)
            return;
        switch (*done)
        {
            case FalloutScript::ChargenPage::Special:
                for (int i = 0; i < FalloutScript::Chargen::attributeCount; ++i)
                    mActorValues.setBase(mPlayer, FalloutScript::Chargen::firstAttribute + i,
                        mChargen.attribute(FalloutScript::Chargen::firstAttribute + i));
                // The skills of a character that is made come from the attributes. A screen that gives more points to
                // a character that has been made (a perk) leaves the skills as they are.
                if (!mChargen.lastFinishAdded())
                    applyChargenSkills();
                break;
            case FalloutScript::ChargenPage::Skills:
                if (!mChargen.lastFinishAdded())
                    applyChargenSkills();
                else
                {
                    // Only the skills the player tagged or let go of change, by the bonus of a tag
                    for (const int skill : mChargen.lastTagged())
                        mActorValues.setBase(
                            mPlayer, skill, mActorValues.base(mPlayer, skill) + FalloutScript::Chargen::tagBonus);
                    for (const int skill : mChargen.lastUntagged())
                        mActorValues.setBase(
                            mPlayer, skill, mActorValues.base(mPlayer, skill) - FalloutScript::Chargen::tagBonus);
                }
                break;
            case FalloutScript::ChargenPage::Traits:
            {
                // A trait is a perk the player has: the ones that were let go are taken back
                const std::vector<FalloutScript::FormId> picked = mChargen.traits();
                for (const FalloutScript::FormId trait : mChargen.givenTraits())
                    if (std::find(picked.begin(), picked.end(), trait) == picked.end())
                        mProgress.removePerk(mPlayer, trait);
                for (const FalloutScript::FormId trait : picked)
                    if (mProgress.perkRank(mPlayer, trait) == 0)
                        mProgress.addPerk(mPlayer, trait);
                mChargen.setGivenTraits(picked);
                break;
            }
            case FalloutScript::ChargenPage::Name:
            case FalloutScript::ChargenPage::Race:
                // They are kept by the making of the character, and everything that asks reads them there
                break;
        }
        beginChargenPage();
    }

    namespace
    {
        /// The globals of the game that are the clock of the world: GameHour, GameDaysPassed and TimeScale. The world
        /// keeps them under the names that Morrowind has for them. Null for any other global.
        const OFWorld::GlobalVariableName* worldClock(std::string_view editorId)
        {
            static constexpr OFWorld::GlobalVariableName hour = OFWorld::Globals::sGameHour;
            static constexpr OFWorld::GlobalVariableName days = OFWorld::Globals::sDaysPassed;
            static constexpr OFWorld::GlobalVariableName scale = OFWorld::Globals::sTimeScale;
            if (Misc::StringUtils::ciEqual(editorId, "GameHour"))
                return &hour;
            if (Misc::StringUtils::ciEqual(editorId, "GameDaysPassed"))
                return &days;
            if (Misc::StringUtils::ciEqual(editorId, "TimeScale"))
                return &scale;
            return nullptr;
        }
    }

    double FalloutScripts::gameHour()
    {
        const OFBase::World* world = OFBase::Environment::get().getWorld();
        return world != nullptr ? world->getGlobalFloat(OFWorld::Globals::sGameHour) : 0.0;
    }

    double FalloutScripts::gameDaysPassed()
    {
        const OFBase::World* world = OFBase::Environment::get().getWorld();
        return world != nullptr ? world->getGlobalFloat(OFWorld::Globals::sDaysPassed) : 0.0;
    }

    namespace
    {
        /// The games count the months from 0 (the GECK documents GameMonth as 0 to 11)
        constexpr int calendarMonthBase = 0;
        /// A new game of the world starts counting its days at 1
        constexpr double newGameDaysPassed = 1.0;

        /// 0 for GameYear, 1 for GameMonth, 2 for GameDay, -1 for any other global
        int datePartOf(std::string_view editorId)
        {
            if (Misc::StringUtils::ciEqual(editorId, "GameYear"))
                return 0;
            if (Misc::StringUtils::ciEqual(editorId, "GameMonth"))
                return 1;
            if (Misc::StringUtils::ciEqual(editorId, "GameDay"))
                return 2;
            return -1;
        }
    }

    bool FalloutScripts::dateNow(FalloutScript::GameDate& date)
    {
        if (!mCalendarStart)
        {
            CalendarStart start;
            bool year = false;
            bool month = false;
            bool day = false;
            for (const ESM4::GlobalVariable& global : mStore.get<ESM4::GlobalVariable>())
            {
                switch (datePartOf(global.mEditorId))
                {
                    case 0:
                        year = true;
                        start.mDate.mYear = static_cast<int>(global.mValue);
                        break;
                    case 1:
                        month = true;
                        start.mDate.mMonth = static_cast<int>(global.mValue);
                        break;
                    case 2:
                        day = true;
                        start.mDate.mDay = static_cast<int>(global.mValue);
                        break;
                    default:
                        break;
                }
            }
            // Files that do not have the three globals (or are not loaded yet) have no date, and are asked again
            if (!year || !month || !day)
                return false;
            mCalendarStart = start;
        }
        const OFBase::World* world = OFBase::Environment::get().getWorld();
        if (world == nullptr)
            return false;
        // The days of the world pass from the new game's day, whatever the record of GameDaysPassed starts at
        // (a script can have stored a number that is not finite in it, which counts as no days)
        double passed = world->getGlobalFloat(OFWorld::Globals::sDaysPassed) - newGameDaysPassed;
        if (!std::isfinite(passed))
            passed = 0;
        const FalloutScript::Calendar calendar(calendarMonthBase);
        date = calendar.dateOf(calendar.dayNumber(mCalendarStart->mDate)
            + static_cast<long long>(std::clamp(std::floor(passed), 0.0, 1.0e9)) + mCalendarShift);
        return true;
    }

    bool FalloutScripts::currentDate(FalloutScript::GameDate& date)
    {
        const std::lock_guard lock(mCalendarMutex);
        return dateNow(date);
    }

    bool FalloutScripts::setDate(int part, double value)
    {
        const std::lock_guard lock(mCalendarMutex);
        FalloutScript::GameDate now;
        if (!dateNow(now))
            return false;
        FalloutScript::GameDate wanted = now;
        // A number that is not one is left to zero, as for the other globals
        const int whole = std::isnan(value) ? 0 : static_cast<int>(std::clamp(std::floor(value), -1.0e6, 1.0e6));
        if (part == 0)
            wanted.mYear = whole;
        else if (part == 1)
            wanted.mMonth = whole;
        else
            wanted.mDay = whole;
        const FalloutScript::Calendar calendar(calendarMonthBase);
        // The date goes on from the one the script gave; a day that does not exist in its month is the nearest one
        mCalendarShift += calendar.dayNumber(wanted) - calendar.dayNumber(now);
        return true;
    }

    bool FalloutScripts::getGlobal(FalloutScript::FormId global, double& value)
    {
        const ESM4::GlobalVariable* record = mStore.get<ESM4::GlobalVariable>().search(refId(global));
        if (record == nullptr)
            return false;
        if (const OFWorld::GlobalVariableName* clock = worldClock(record->mEditorId))
        {
            if (const OFBase::World* world = OFBase::Environment::get().getWorld())
            {
                value = world->getGlobalFloat(*clock);
                return true;
            }
        }
        // The date moves with the days that pass; a script that sets it moves the date to the one it gives
        if (const int part = datePartOf(record->mEditorId); part >= 0)
        {
            FalloutScript::GameDate date;
            if (currentDate(date))
            {
                value = part == 0 ? date.mYear : part == 1 ? date.mMonth : date.mDay;
                return true;
            }
        }
        const auto found = mGlobals.find(global);
        value = found != mGlobals.end() ? found->second : fitToType(record->mType, record->mValue);
        return true;
    }

    bool FalloutScripts::setGlobal(FalloutScript::FormId global, double value)
    {
        const ESM4::GlobalVariable* record = mStore.get<ESM4::GlobalVariable>().search(refId(global));
        if (record == nullptr)
            return false;
        if (const OFWorld::GlobalVariableName* clock = worldClock(record->mEditorId))
        {
            if (OFBase::World* world = OFBase::Environment::get().getWorld())
            {
                world->setGlobalFloat(*clock, static_cast<float>(value));
                return true;
            }
        }
        if (const int part = datePartOf(record->mEditorId); part >= 0 && setDate(part, value))
            return true;
        mGlobals[global] = fitToType(record->mType, value);
        return true;
    }

    FalloutScript::Instance* FalloutScripts::findInstance(FalloutScript::FormId owner)
    {
        if (FalloutScript::Instance* quest = mQuests.instance(owner))
            return quest;
        return mObjects.instance(owner);
    }

    void FalloutScripts::log(std::string_view message)
    {
        Log(Debug::Warning) << "Script: " << message;
    }

    const ESM4::Script* FalloutScripts::findScript(FalloutScript::FormId id) const
    {
        return mStore.get<ESM4::Script>().search(refId(id));
    }

    void FalloutScripts::forEachQuest(const std::function<void(const ESM4::Quest&)>& function) const
    {
        for (const ESM4::Quest& quest : mStore.get<ESM4::Quest>())
            function(quest);
    }
}
