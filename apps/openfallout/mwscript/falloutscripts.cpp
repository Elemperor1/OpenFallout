#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <components/debug/debuglog.hpp>
#include <components/esm/refid.hpp>
#include <components/esm/util.hpp>
#include <components/esm4/loadachr.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadmesg.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/falloutpackages.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
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

        /// The id of the record as the store keys it, empty for ids that no record can have
        ESM::RefId refId(FalloutScript::FormId id)
        {
            const ESM::FormId form = ESM::FormId::fromUint32(id);
            return form.isZeroOrUnset() ? ESM::RefId() : ESM::RefId::formIdRefId(form);
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
        , mPlayer(player)
        , mMainThread(std::this_thread::get_id())
        , mRandom(std::random_device()())
    {
        FalloutScript::addQuestCommands(mInterpreter, mQuests);
        FalloutScript::addObjectCommands(
            mInterpreter, mObjects, *this, [this] { return std::uniform_int_distribution<int>(0, 99)(mRandom); });
        FalloutScript::addPackageCommands(mInterpreter, *this);
        FalloutScript::addPlacementCommands(mInterpreter, *this);
        FalloutScript::addMessageCommands(mInterpreter, *this);
        FalloutScript::addActorCommands(mInterpreter, mActors);
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

    void FalloutScripts::reset()
    {
        mGlobals.clear();
        mDisabled.clear();
        mPendingToggle.clear();
        mPendingLoad.clear();
        mPendingMoves.clear();
        mRelocations.clear();
        mReferences.clear();
        mScriptPackages.clear();
        mActors.clear();
        mObjects.clear();
        {
            const std::lock_guard lock(mDeferredMutex);
            mDeferred.clear();
            mDeferredDisabled.clear();
            mDeferredPlaces.clear();
        }
        mQuests.reset();
    }

    void FalloutScripts::update(float frametime, bool paused)
    {
        runPending();
        if (paused)
            return;
        dropObjectsThatLeft();
        mObjects.setSecondsPassed(frametime);
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

    namespace
    {
        /// The reference as scripts name it, 0 for an object that has no reference number of a plugin (those that are
        /// made while the game runs have a generated one, which has no 32 bit form)
        FalloutScript::FormId referenceOf(const OFWorld::Ptr& ptr)
        {
            const ESM::RefNum refNum = ptr.getCellRef().getRefNum();
            return refNum.hasContentFile() && refNum.mContentFile <= 0xfe ? refNum.toUint32() : 0;
        }
    }

    void FalloutScripts::cellLoaded(OFWorld::CellStore& cell)
    {
        // This runs while the scene is being changed, which scripts must not change in turn (enabling or disabling an
        // object takes it out of the scene or puts it in): what they do waits for the next frame
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
            const FalloutScript::FormId script = ptr.getClass().getFalloutScript(ptr);
            if (script != 0 && mObjects.add(reference, script) != nullptr)
                mPendingLoad.push_back(reference);
            return true;
        });
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
            const OFWorld::Ptr moved
                = OFBase::Environment::get().getWorldModel()->getPtr(ESM::FormId::fromUint32(reference));
            if (!moved.isEmpty() && moved.isInCell() && moved.getCell() != found->second.getCell())
                found->second = moved;
            return found->second;
        }
        // Objects made while the game runs are only known to the world
        const OFWorld::Ptr ptr = OFBase::Environment::get().getWorldModel()->getPtr(ESM::FormId::fromUint32(reference));
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

    void FalloutScripts::applyDisabled(FalloutScript::FormId reference, bool disabled)
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

    void FalloutScripts::tellPlayer(const std::string& text)
    {
        runOnMainThread([text] { OFBase::Environment::get().getWindowManager()->messageBox(text); });
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
        }
    }

    bool FalloutScripts::distance(FalloutScript::FormId first, FalloutScript::FormId second, double& result)
    {
        // Where the script has put the references counts, though the world has not done it yet
        Place a;
        Place b;
        if (!whereIs(first, a) || !whereIs(second, b))
            return false;
        // In the same cell, or in exterior cells of one worldspace
        if (a.mCell != b.mCell)
        {
            const OFWorld::WorldModel* model = OFBase::Environment::get().getWorldModel();
            const OFWorld::CellStore* cellA = model->findCell(a.mCell);
            const OFWorld::CellStore* cellB = model->findCell(b.mCell);
            if (cellA == nullptr || cellB == nullptr
                || !(cellA->isExterior() && cellB->isExterior()
                    && cellA->getCell()->getWorldSpace() == cellB->getCell()->getWorldSpace()))
                return false;
        }
        result = (a.mPosition - b.mPosition).length();
        return true;
    }

    void FalloutScripts::showMessage(FalloutScript::FormId message)
    {
        const ESM4::Message* record = mStore.get<ESM4::Message>().search(refId(message));
        if (record == nullptr)
        {
            log("ShowMessage: the message " + FalloutScript::hex(message) + " is not known");
            return;
        }
        if (record->mDescription.empty())
            return;
        // Buttons make a message a question to the player, which the game can not ask yet: the text is shown alone
        Log(Debug::Info) << "Message: " << record->mDescription;
        tellPlayer(record->mDescription);
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
    }

    bool FalloutScripts::baseOf(FalloutScript::FormId reference, ESM::FormId& base)
    {
        base = ESM::FormId();
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
        if (const ESM4::Npc* npc = mStore.get<ESM4::Npc>().search(base))
        {
            if (const ESM4::Npc* owner = templateOwner(mStore, npc, ESM4::Npc::Template_UseInventory))
                take(owner->mInventory);
        }
        else if (const ESM4::Creature* creature = mStore.get<ESM4::Creature>().search(base))
        {
            if (const ESM4::Creature* owner = templateOwner(mStore, creature, ESM4::Creature::Template_UseInventory))
                take(owner->mInventory);
        }
        else if (const ESM4::Container* container = mStore.get<ESM4::Container>().search(base))
            take(container->mInventory);
        return true;
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

    bool FalloutScripts::isDead(const OFWorld::Ptr& actor) const
    {
        const FalloutScript::FormId reference = referenceOf(actor);
        return reference != 0 && mActors.isDead(reference);
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

    FalloutScript::FormId FalloutScripts::cellOf(FalloutScript::FormId reference)
    {
        // Where the script has put the reference counts, though the world has not done it yet
        Place place;
        if (!whereIs(reference, place))
            return 0;
        const ESM::FormId* id = place.mCell.getIf<ESM::FormId>();
        return id != nullptr ? id->toUint32() : 0;
    }

    bool FalloutScripts::currentPlace(FalloutScript::FormId reference, Place& result)
    {
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
        OFBase::Environment::get().getLuaManager()->objectTeleported(moved);
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

    bool FalloutScripts::getGlobal(FalloutScript::FormId global, double& value)
    {
        const ESM4::GlobalVariable* record = mStore.get<ESM4::GlobalVariable>().search(refId(global));
        if (record == nullptr)
            return false;
        const auto found = mGlobals.find(global);
        value = found != mGlobals.end() ? found->second : fitToType(record->mType, record->mValue);
        return true;
    }

    bool FalloutScripts::setGlobal(FalloutScript::FormId global, double value)
    {
        const ESM4::GlobalVariable* record = mStore.get<ESM4::GlobalVariable>().search(refId(global));
        if (record == nullptr)
            return false;
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
