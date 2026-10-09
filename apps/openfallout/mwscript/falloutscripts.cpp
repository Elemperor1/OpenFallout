#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <components/debug/debuglog.hpp>
#include <components/esm/refid.hpp>
#include <components/esm4/loadglob.hpp>
#include <components/esm4/loadmesg.hpp>
#include <components/esm4/loadpack.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadscpt.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/falloutpackages.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
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
        , mPlayer(player)
        , mMainThread(std::this_thread::get_id())
        , mRandom(std::random_device()())
    {
        FalloutScript::addQuestCommands(mInterpreter, mQuests);
        FalloutScript::addObjectCommands(
            mInterpreter, mObjects, *this, [this] { return std::uniform_int_distribution<int>(0, 99)(mRandom); });
        FalloutScript::addPackageCommands(mInterpreter, *this);
        FalloutScript::addMessageCommands(mInterpreter, *this);
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
        mReferences.clear();
        mScriptPackages.clear();
        mObjects.clear();
        {
            const std::lock_guard lock(mDeferredMutex);
            mDeferred.clear();
            mDeferredDisabled.clear();
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
            // Every toggle that was asked for has been made now, and the state of the objects is theirs again
            if (mDeferred.empty())
                mDeferredDisabled.clear();
        }
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
            // Not if the cell has been unloaded since
            if (mReferences.count(reference) != 0)
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

    void FalloutScripts::dropObjectsThatLeft()
    {
        // An object that was moved to a cell that is not loaded is not in a loaded cell to be unloaded with it
        const OFWorld::Scene::CellStoreCollection& active
            = OFBase::Environment::get().getWorldScene()->getActiveCells();
        for (const FalloutScript::FormId reference : mObjects.updating())
        {
            const OFWorld::Ptr ptr = findReference(reference);
            if (ptr.isEmpty() || ptr.getCell() == nullptr || reference == mPlayer
                || active.find(const_cast<OFWorld::CellStore*>(ptr.getCell())) != active.end())
                continue;
            mObjects.setLoaded(reference, false);
            mReferences.erase(reference);
        }
    }

    bool FalloutScripts::distance(FalloutScript::FormId first, FalloutScript::FormId second, double& result)
    {
        const OFWorld::Ptr a = findReference(first);
        const OFWorld::Ptr b = findReference(second);
        if (a.isEmpty() || b.isEmpty())
            return false;
        // In the same cell, or in exterior cells of one worldspace
        const OFWorld::CellStore* cellA = a.getCell();
        const OFWorld::CellStore* cellB = b.getCell();
        if (cellA != cellB
            && !(cellA->isExterior() && cellB->isExterior()
                && cellA->getCell()->getWorldSpace() == cellB->getCell()->getWorldSpace()))
            return false;
        result = (a.getRefData().getPosition().asVec3() - b.getRefData().getPosition().asVec3()).length();
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

    void FalloutScripts::addScriptPackage(FalloutScript::FormId actor, FalloutScript::FormId package)
    {
        // A package that is among them already is the last again
        std::vector<FalloutScript::FormId>& packages = mScriptPackages[actor];
        packages.erase(std::remove(packages.begin(), packages.end(), package), packages.end());
        packages.push_back(package);
        evaluatePackage(actor);
    }

    void FalloutScripts::removeScriptPackage(FalloutScript::FormId actor)
    {
        const auto found = mScriptPackages.find(actor);
        if (found == mScriptPackages.end())
            return;
        found->second.pop_back();
        if (found->second.empty())
            mScriptPackages.erase(found);
        evaluatePackage(actor);
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
        const OFWorld::Ptr ptr = findReference(reference);
        if (ptr.isEmpty())
            return 0;
        const ESM::FormId* id = ptr.getCell()->getCell()->getId().getIf<ESM::FormId>();
        return id != nullptr ? id->toUint32() : 0;
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
