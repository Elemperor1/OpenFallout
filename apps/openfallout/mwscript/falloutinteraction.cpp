#include "falloutscripts.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadterm.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/interaction.hpp>
#include <components/falloutscript/messagetext.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"

namespace OFScript
{
    namespace
    {
        namespace Interaction = FalloutScript::Interaction;

        /// The actor value of the Lockpick skill
        constexpr int skillLockpick = 36;

        /// The seconds after a lock opened before the object it belongs to is activated again: the screen of the lock
        /// is gone by then, and the screen of what opens (a container) does not meet it
        constexpr double reactivationDelay = 0.3;

        /// How often the markers near the player are looked for
        constexpr double discoveryInterval = 0.5;
    }

    std::string FalloutScripts::wordOf(std::string_view setting, std::string_view fallback)
    {
        std::string text = gameSettingText(setting);
        return text.empty() ? std::string(fallback) : text;
    }

    std::string FalloutScripts::refusalWord(const std::string& setting)
    {
        std::string text = gameSettingText(setting);
        // The files spell the setting for hostile characters without the e (sNoWaitHostilActorsNear)
        const std::size_t hostile = setting.find("HostileActors");
        if (text.empty() && hostile != std::string::npos)
        {
            std::string name = setting;
            name.erase(hostile + 6, 1);
            text = gameSettingText(name);
        }
        return text;
    }

    std::string FalloutScripts::terminalFormat(const std::string& text)
    {
        return FalloutScript::formatMessage(text, {}, [this](std::string_view name) { return messageToken(name); });
    }

    // ---- Locks ----

    bool FalloutScripts::lockStops(const OFWorld::Ptr& object, const OFWorld::Ptr& actor)
    {
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        if (object.isEmpty() || actor.isEmpty() || actor != world.getPlayerPtr())
            return false;
        if (object.getType() != ESM4::Door::sRecordId && object.getType() != ESM4::Container::sRecordId)
            return false;
        const FalloutScript::FormId reference = referenceOf(object);
        if (reference == 0 || !mReferenceState.isLocked(reference))
            return false;

        // The key of the lock opens it, and the activation goes on
        const ESM4::Reference* record = mStore.get<ESM4::Reference>().search(ESM::FormId::fromUint32(reference));
        if (record != nullptr && !record->mKey.isZeroOrUnset()
            && mActors.itemCount(mPlayer, record->mKey.toUint32()) > 0)
        {
            mReferenceState.unlock(reference);
            return false;
        }

        const Interaction::Setting setting
            = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
        const bool leveled = record != nullptr && (record->mLockFlags & 0x04) != 0;
        const int level
            = Interaction::lockLevel(mReferenceState.lockLevel(reference), leveled, mProgress.level(), setting);
        const Interaction::LockClass lockClass = Interaction::lockClass(level, setting);

        LockScreen screen;
        screen.mReference = reference;
        screen.mName = std::string(object.getClass().getName(object));
        screen.mLevel = level;
        screen.mKeyOnly = lockClass == Interaction::LockClass::Impossible;
        screen.mWord = wordOf(Interaction::lockClassWord(lockClass), screen.mKeyOnly ? "Requires Key" : "Locked");
        screen.mPins = mActors.itemCount(mPlayer, masterForm(sBobbyPin));
        screen.mSkill = mActorValues.current(mPlayer, skillLockpick);
        screen.mChance = screen.mKeyOnly ? 0.0 : Interaction::pickChance(screen.mSkill, level, setting);
        {
            const std::lock_guard lock(mInteractionMutex);
            screen.mSerial = ++mLockSerial;
            mLockScreen = screen;
        }
        return true;
    }

    std::optional<FalloutScripts::LockScreen> FalloutScripts::lockScreen()
    {
        const std::lock_guard lock(mInteractionMutex);
        if (mLockScreen)
            mLockScreen->mPins = mActors.itemCount(mPlayer, masterForm(sBobbyPin));
        return mLockScreen;
    }

    void FalloutScripts::closeLockScreen()
    {
        const std::lock_guard lock(mInteractionMutex);
        mLockScreen.reset();
    }

    std::string FalloutScripts::pickLock()
    {
        LockScreen screen;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mLockScreen)
                return std::string();
            screen = *mLockScreen;
        }
        if (screen.mKeyOnly)
        {
            tellPlayer("It requires a key.");
            return "keyonly";
        }
        if (mActors.itemCount(mPlayer, masterForm(sBobbyPin)) <= 0)
        {
            tellPlayer("You have no bobby pins.");
            return "nopins";
        }
        bool opens = false;
        {
            const std::lock_guard lock(mCombatRandomMutex);
            opens = std::uniform_real_distribution<double>(0.0, 1.0)(mCombatRandom) < screen.mChance;
        }
        if (!opens)
        {
            // A pin that did not open the lock is spent
            mActors.removeItem(mPlayer, masterForm(sBobbyPin), 1);
            tellPlayer("Your bobby pin broke.");
            return "broke";
        }

        const Interaction::Setting setting
            = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
        mReferenceState.unlock(screen.mReference);
        if (const char* experience = Interaction::lockClassExperience(Interaction::lockClass(screen.mLevel, setting)))
        {
            const int reward = static_cast<int>(gameSetting(experience, 0));
            if (reward > 0)
                rewardExperience(reward);
        }
        tellPlayer("Lock picked.");
        {
            const std::lock_guard lock(mInteractionMutex);
            mLockScreen.reset();
            mReactivate = Reactivation{ screen.mReference, reactivationDelay };
        }
        return "opened";
    }

    // ---- Terminals ----

    bool FalloutScripts::openTerminal(const OFWorld::Ptr& object, const OFWorld::Ptr& actor)
    {
        if (object.isEmpty() || actor.isEmpty() || actor != OFBase::Environment::get().getWorld()->getPlayerPtr()
            || object.getType() != ESM4::Terminal::sRecordId)
            return false;
        const FalloutScript::FormId reference = referenceOf(object);
        ESM::FormId base;
        if (reference == 0 || !baseOf(reference, base))
            return false;
        const ESM4::Terminal* record = mStore.get<ESM4::Terminal>().search(base);
        if (record == nullptr)
            return false;

        // A terminal is closed until it is hacked or its password is carried, unless its record says it is not
        const bool closed = mReferenceState.isLocked(reference);
        TerminalState state;
        state.mReference = reference;
        state.mPath.push_back(base.toUint32());
        if (closed && !record->mPasswordNote.isZeroOrUnset()
            && mActors.itemCount(mPlayer, record->mPasswordNote.toUint32()) > 0)
        {
            mReferenceState.unlock(reference);
            tellPlayer("Password accepted.");
        }
        else if (closed)
        {
            const Interaction::Setting setting
                = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
            state.mHackLevel = Interaction::hackLevel(
                record->mDifficulty, (record->mTerminalFlags & 0x01) != 0, mProgress.level(), setting);
            std::uint32_t seed = 0;
            {
                const std::lock_guard lock(mCombatRandomMutex);
                seed = static_cast<std::uint32_t>(mCombatRandom());
            }
            state.mPuzzle = std::make_unique<FalloutScript::Hacking::Puzzle>(seed, state.mHackLevel, setting);
        }
        {
            const std::lock_guard lock(mInteractionMutex);
            // The time to wait does not end by leaving the screen and coming back
            const auto cooling = mTerminalCooldowns.find(reference);
            if (cooling != mTerminalCooldowns.end())
            {
                if (state.mPuzzle)
                    state.mLockout = cooling->second;
                mTerminalCooldowns.erase(cooling);
            }
            if (mTerminal)
                leaveTerminal();
            state.mSerial = ++mTerminalSerial;
            mTerminal = std::move(state);
        }
        return true;
    }

    int FalloutScripts::terminalSerial()
    {
        const std::lock_guard lock(mInteractionMutex);
        return mTerminal ? mTerminal->mSerial : 0;
    }

    std::optional<FalloutScripts::TerminalScreen> FalloutScripts::terminalScreen()
    {
        TerminalScreen screen;
        FalloutScript::FormId menu = 0;
        bool showingResult = false;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mTerminal)
                return std::nullopt;
            const TerminalState& state = *mTerminal;
            screen.mTerminal = state.mReference;
            screen.mSerial = state.mSerial;
            screen.mCanGoBack = state.mShowingResult || state.mPath.size() > 1;
            menu = state.mPath.back();
            showingResult = state.mShowingResult;
            screen.mBody = state.mResult;
            if (state.mPuzzle)
            {
                screen.mHacking = true;
                screen.mHackLevel = state.mHackLevel;
                screen.mWords = state.mPuzzle->words();
                screen.mGuesses = state.mGuesses;
                screen.mAttempts = state.mPuzzle->attemptsLeft();
                screen.mLockout = state.mLockout;
            }
        }
        const ESM4::Terminal* record = mStore.get<ESM4::Terminal>().search(ESM::FormId::fromUint32(menu));
        if (record == nullptr)
            return screen;
        screen.mName = record->mFullName;
        if (screen.mHacking)
            return screen;
        if (showingResult)
            return screen;
        screen.mBody = terminalFormat(record->mText);
        FalloutScript::ConditionContext context;
        context.mSubject = mPlayer;
        if (!mConditions.holds(record->mConditions, context))
            return screen;
        for (std::size_t i = 0; i < record->mMenuItems.size(); ++i)
        {
            const ESM4::Terminal::MenuItem& item = record->mMenuItems[i];
            if (item.mText.empty() || !mConditions.holds(item.mConditions, context))
                continue;
            screen.mLines.push_back({ static_cast<int>(i), terminalFormat(item.mText) });
        }
        return screen;
    }

    void FalloutScripts::terminalChoose(int index)
    {
        FalloutScript::FormId menu = 0;
        FalloutScript::FormId terminal = 0;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mTerminal || mTerminal->mPuzzle || mTerminal->mShowingResult)
                return;
            menu = mTerminal->mPath.back();
            terminal = mTerminal->mReference;
        }
        const ESM4::Terminal* record = mStore.get<ESM4::Terminal>().search(ESM::FormId::fromUint32(menu));
        if (record == nullptr || index < 0 || index >= static_cast<int>(record->mMenuItems.size()))
            return;
        const ESM4::Terminal::MenuItem& item = record->mMenuItems[static_cast<std::size_t>(index)];
        FalloutScript::ConditionContext context;
        context.mSubject = mPlayer;
        // Only a line that the screen shows is one to choose: it has text, and the conditions of the menu and of the
        // line hold
        if (item.mText.empty() || !mConditions.holds(record->mConditions, context)
            || !mConditions.holds(item.mConditions, context))
            return;

        // The result script of the line runs first, then the note, then what the line shows
        mObjects.runResult(item.mScript, terminal);
        if (!item.mDisplayNote.isZeroOrUnset() && (item.mFlags & 0x01) != 0)
        {
            mActors.addItem(mPlayer, item.mDisplayNote.toUint32(), 1);
            tellPlayer("Note added.");
        }

        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || mTerminal->mReference != terminal)
            return;
        if (!item.mSubMenu.isZeroOrUnset() && mStore.get<ESM4::Terminal>().search(item.mSubMenu) != nullptr)
            mTerminal->mPath.push_back(item.mSubMenu.toUint32());
        else if (!item.mResultText.empty())
        {
            mTerminal->mResult = terminalFormat(item.mResultText);
            mTerminal->mShowingResult = true;
        }
        mTerminal->mSerial = ++mTerminalSerial;
    }

    void FalloutScripts::terminalBack()
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal)
            return;
        TerminalState& state = *mTerminal;
        if (state.mPuzzle)
            leaveTerminal();
        else if (state.mShowingResult)
        {
            state.mShowingResult = false;
            state.mResult.clear();
            state.mSerial = ++mTerminalSerial;
        }
        else if (state.mPath.size() > 1)
        {
            state.mPath.pop_back();
            state.mSerial = ++mTerminalSerial;
        }
        else
            leaveTerminal();
    }

    void FalloutScripts::leaveTerminal()
    {
        if (mTerminal && mTerminal->mPuzzle && mTerminal->mLockout > 0)
            mTerminalCooldowns[mTerminal->mReference] = mTerminal->mLockout;
        mTerminal.reset();
    }

    int FalloutScripts::hackGuess(int word)
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || !mTerminal->mPuzzle || mTerminal->mLockout > 0 || word < 0)
            return -1;
        TerminalState& state = *mTerminal;
        FalloutScript::Hacking::Puzzle& puzzle = *state.mPuzzle;
        const FalloutScript::Hacking::Puzzle::Guess guess = puzzle.guess(static_cast<std::size_t>(word));
        if (guess.mIgnored)
            return -1;
        state.mGuesses.push_back({ puzzle.words()[static_cast<std::size_t>(word)], guess.mLikeness });
        state.mSerial = ++mTerminalSerial;
        if (guess.mCorrect)
        {
            mReferenceState.unlock(state.mReference);
            state.mPuzzle.reset();
            state.mGuesses.clear();
            tellPlayer("Password accepted.");
        }
        else if (puzzle.lockedOut())
        {
            state.mLockout = gameSetting("iHackingRetryMilliseconds", 10000) / 1000.0;
            tellPlayer("The terminal is locked.");
        }
        return guess.mLikeness;
    }

    void FalloutScripts::closeTerminal()
    {
        const std::lock_guard lock(mInteractionMutex);
        leaveTerminal();
    }

    // ---- Waiting and sleeping ----

    bool FalloutScripts::hostilesNearPlayer()
    {
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return false;
        for (const FalloutScript::FormId attacker : mCombat.attackersOf(mPlayer))
        {
            if (mActors.isDead(attacker))
                continue;
            // One that was removed or disabled is no threat, whatever it was doing
            const OFWorld::Ptr enemy = findReference(attacker);
            if (!enemy.isEmpty() && enemy.isInCell() && sharesSpace(enemy.getCell(), player.getCell())
                && enemy.getRefData().isEnabled() && enemy.getCellRef().getCount() > 0)
                return true;
        }
        return false;
    }

    bool FalloutScripts::playerInAir()
    {
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        // The test the mechanics use for a fall: isFlying is about the ability to fly, not about touching the ground
        return !player.isEmpty() && !world.isOnGround(player) && !world.isSwimming(player) && !world.isFlying(player);
    }

    std::string FalloutScripts::restRefusalText(bool sleeping, FalloutScript::FormId bed)
    {
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return "You cannot rest now.";
        if (!mMapMarkers.restAllowed())
            return "You cannot rest here.";
        Interaction::RestConditions conditions;
        conditions.mSleeping = sleeping;
        conditions.mHostilesNear = hostilesNearPlayer();
        conditions.mUnderwater = world.isSwimming(player);
        conditions.mInAir = playerInAir();
        if (sleeping && bed != 0)
        {
            // What a script made of the bed counts, so the owner is not the one the record names
            const FalloutScript::FormId owner = mObjectState.owner(bed);
            conditions.mOwnedBed = owner != 0 && owner != playerBase();
        }
        const std::optional<std::string> refusal = Interaction::restRefusal(conditions);
        if (!refusal)
            return std::string();
        const std::string text = refusalWord(*refusal);
        log("Rest refused: " + *refusal + (text.empty() ? " (the files have no text for it)" : ""));
        return text.empty() ? std::string(sleeping ? "You cannot sleep now." : "You cannot wait now.") : text;
    }

    void FalloutScripts::requestRest(bool sleeping, FalloutScript::FormId bed)
    {
        const std::string refusal = restRefusalText(sleeping, bed);
        if (!refusal.empty())
        {
            tellPlayer(refusal);
            return;
        }
        const std::lock_guard lock(mInteractionMutex);
        if (mRest.mOpen || mRest.mActive)
            return;
        const int serial = mRest.mSerial + 1;
        const int ended = mRest.mEnded;
        mRest = RestState();
        mRest.mOpen = true;
        mRest.mSleeping = sleeping;
        mRest.mBed = bed;
        mRest.mSerial = serial;
        mRest.mEnded = ended;
    }

    std::optional<FalloutScripts::RestScreen> FalloutScripts::restScreen()
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mRest.mOpen)
            return std::nullopt;
        return RestScreen{ mRest.mSleeping, mRest.mBed, mRest.mSerial };
    }

    std::string FalloutScripts::startRest(double hours)
    {
        bool sleeping = false;
        FalloutScript::FormId bed = 0;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mRest.mOpen || mRest.mActive)
                return "You cannot rest now.";
            sleeping = mRest.mSleeping;
            bed = mRest.mBed;
        }
        const std::string refusal = restRefusalText(sleeping, bed);
        if (!refusal.empty())
        {
            tellPlayer(refusal);
            return refusal;
        }
        if (sleeping && bed != 0)
        {
            // The player lies on the bed all the time it sleeps, as far as GetSleeping and the questions about the
            // furniture of a character are asked
            const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
            const OFWorld::Ptr furniture = findReference(bed);
            if (!player.isEmpty() && !furniture.isEmpty())
            {
                const std::lock_guard furnitureLock(mFurnitureMutex);
                FurnitureUse& use = mFurniture[mPlayer];
                use.mFurniture = bed;
                use.mPlace = player.getRefData().getPosition().asVec3();
                use.mFurniturePlace = furniture.getRefData().getPosition().asVec3();
                use.mEntry = -1;
                use.mKind = 1;
            }
        }
        const std::lock_guard lock(mInteractionMutex);
        mRest.mActive = true;
        mRest.mTotal = std::clamp(hours, 1.0, 24.0);
        mRest.mDone = 0;
        mRest.mStopped.clear();
        mPlayerSleeping = sleeping;
        return std::string();
    }

    FalloutScripts::RestProgress FalloutScripts::restProgress()
    {
        const std::lock_guard lock(mInteractionMutex);
        RestProgress progress;
        progress.mActive = mRest.mActive;
        progress.mSleeping = mRest.mSleeping;
        progress.mDone = mRest.mDone;
        progress.mTotal = mRest.mTotal;
        progress.mStopped = mRest.mStopped;
        progress.mEnded = mRest.mEnded;
        return progress;
    }

    void FalloutScripts::cancelRest()
    {
        const std::lock_guard lock(mInteractionMutex);
        if (mRest.mActive)
        {
            // What has passed stays; the sleep ends where it was stopped
            mRest.mTotal = mRest.mDone;
            mRest.mFinish = true;
        }
        else
            mRest.mOpen = false;
    }

    // ---- The map ----

    std::vector<FalloutScripts::MapEntry> FalloutScripts::mapEntries()
    {
        std::vector<MapEntry> entries;
        const FalloutScript::FormId space = worldspaceOf(mPlayer);
        osg::Vec3f here;
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        const bool placed = !player.isEmpty() && player.isInCell();
        if (placed)
            here = player.getRefData().getPosition().asVec3();
        const bool outside = placed && inOpenWorld(mPlayer);
        for (const PlacedMarker& marker : placedMarkers())
        {
            const int state = mMapMarkers.state(marker.mId);
            if (state == 0)
                continue;
            MapEntry entry;
            entry.mId = marker.mId;
            entry.mName = marker.mName;
            entry.mPosition = marker.mPosition;
            entry.mTravel = state == 2;
            entry.mDistance = -1;
            if (outside && marker.mWorldspace == space)
            {
                const osg::Vec3f away = marker.mPosition - here;
                entry.mDistance = std::sqrt(away.x() * away.x() + away.y() * away.y());
            }
            entries.push_back(std::move(entry));
        }
        std::sort(entries.begin(), entries.end(), [](const MapEntry& a, const MapEntry& b) {
            if (a.mName != b.mName)
                return a.mName < b.mName;
            return a.mId < b.mId;
        });
        return entries;
    }

    std::string FalloutScripts::travelTo(FalloutScript::FormId marker)
    {
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        const PlacedMarker* target = nullptr;
        for (const PlacedMarker& placed : placedMarkers())
            if (placed.mId == marker)
                target = &placed;
        if (target == nullptr || player.isEmpty() || !player.isInCell())
            return "You cannot travel there.";

        Interaction::TravelConditions conditions;
        conditions.mUndiscovered = mMapMarkers.state(marker) != 2;
        conditions.mHostilesNear = hostilesNearPlayer();
        conditions.mScriptBlocked = !mMapMarkers.fastTravelAllowed();
        const PipboyStats stats = pipboyStats();
        conditions.mOverencumbered = stats.mWeight > stats.mMaxWeight;
        conditions.mInAir = playerInAir();
        // The place the records forbid to travel from: an interior unless its flag "can travel from here" is set (the
        // flag the plugin code names CELL_NoTravel), a worldspace that has the flag that forbids it. The executable
        // reads the same bit of the cells outside too, which is not done here.
        const bool outside = inOpenWorld(mPlayer);
        const FalloutScript::FormId cell = parentCell(mPlayer);
        if (outside)
        {
            const ESM4::World* space = mStore.get<ESM4::World>().search(ESM::FormId::fromUint32(worldspaceOf(mPlayer)));
            conditions.mCellForbids = space != nullptr && (space->mWorldFlags & ESM4::World::WLD_NoFastTravel) != 0;
        }
        else if (cell != 0)
        {
            const ESM4::Cell* record = mStore.get<ESM4::Cell>().search(ESM::FormId::fromUint32(cell));
            conditions.mCellForbids = record != nullptr && (record->mCellFlags & ESM4::CELL_NoTravel) == 0;
        }
        if (const std::optional<std::string> refusal = Interaction::travelRefusal(conditions))
        {
            std::string text = refusalWord(*refusal);
            log("Travel refused: " + *refusal + (text.empty() ? " (the files have no text for it)" : ""));
            if (text.empty())
                text = "You cannot travel now.";
            tellPlayer(text);
            return text;
        }

        // The time the trip takes: from where the player stands when that is in the space of the marker, else the
        // trip is as long as the longest one
        double distance = 20000;
        if (outside && worldspaceOf(mPlayer) == target->mWorldspace)
        {
            const osg::Vec3f away = target->mPosition - player.getRefData().getPosition().asVec3();
            distance = std::sqrt(away.x() * away.x() + away.y() * away.y());
        }
        const double hours = Interaction::travelHours(distance);
        runOnMainThread([this, marker, hours] {
            OFBase::Environment::get().getWorld()->advanceTime(hours);
            // A little above the marker, which stands about where a character would
            moveTo(mPlayer, marker, { 0.0, 0.0, 48.0 });
        });
        return std::string();
    }

    // ---- What moves on its own ----

    void FalloutScripts::updateInteraction(float frametime)
    {
        // A lock that opened activates its object again once its screen is gone
        FalloutScript::FormId reactivate = 0;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (mReactivate)
            {
                mReactivate->mDelay -= frametime;
                if (mReactivate->mDelay <= 0)
                {
                    reactivate = mReactivate->mReference;
                    mReactivate.reset();
                }
            }
        }
        if (reactivate != 0)
        {
            const OFWorld::Ptr object = findReference(reactivate);
            const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
            if (!object.isEmpty() && !player.isEmpty())
                OFBase::Environment::get().getLuaManager()->defaultActivation(object, player);
        }

        // A terminal that was locked up tries again once the time is over, with other words
        {
            const std::lock_guard lock(mInteractionMutex);
            if (mTerminal && mTerminal->mPuzzle && mTerminal->mLockout > 0)
            {
                const int before = static_cast<int>(std::ceil(mTerminal->mLockout));
                mTerminal->mLockout -= frametime;
                if (mTerminal->mLockout <= 0)
                {
                    mTerminal->mLockout = 0;
                    const Interaction::Setting setting
                        = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
                    std::uint32_t seed = 0;
                    {
                        const std::lock_guard randomLock(mCombatRandomMutex);
                        seed = static_cast<std::uint32_t>(mCombatRandom());
                    }
                    mTerminal->mPuzzle
                        = std::make_unique<FalloutScript::Hacking::Puzzle>(seed, mTerminal->mHackLevel, setting);
                    mTerminal->mGuesses.clear();
                    mTerminal->mSerial = ++mTerminalSerial;
                }
                else if (static_cast<int>(std::ceil(mTerminal->mLockout)) != before)
                    mTerminal->mSerial = ++mTerminalSerial;
            }
        }

        {
            const std::lock_guard lock(mInteractionMutex);
            for (auto cooling = mTerminalCooldowns.begin(); cooling != mTerminalCooldowns.end();)
            {
                cooling->second -= frametime;
                if (cooling->second <= 0)
                    cooling = mTerminalCooldowns.erase(cooling);
                else
                    ++cooling;
            }
        }

        updateRest(frametime);
        discoverMarkers(frametime);
    }

    void FalloutScripts::updateRest(float frametime)
    {
        bool sleeping = false;
        FalloutScript::FormId bed = 0;
        double step = 0;
        bool finish = false;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mRest.mActive)
                return;
            sleeping = mRest.mSleeping;
            bed = mRest.mBed;
            finish = mRest.mFinish;
            step = std::min(mRest.mTotal - mRest.mDone, static_cast<double>(frametime) * sRestHoursPerSecond);
        }
        std::string stopped;
        // Characters that come for the player end it
        if (!finish && hostilesNearPlayer())
        {
            stopped = restRefusalText(sleeping, 0);
            if (stopped.empty())
                stopped = "You were interrupted.";
            finish = true;
        }
        else if (!finish && step > 0)
            OFBase::Environment::get().getWorld()->advanceTime(step);

        double done = 0;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!finish)
                mRest.mDone += step;
            done = mRest.mDone;
            if (mRest.mDone + 1e-9 >= mRest.mTotal)
                finish = true;
        }
        if (!finish)
            return;

        // Sleep heals: the stand-in is a full recovery in eight hours of it
        if (sleeping && done > 0)
        {
            namespace AV = FalloutScript::ActorValue;
            const double most = mActorValues.permanent(mPlayer, AV::health);
            mActorValues.restore(mPlayer, AV::health, most * done / 8.0);
        }
        mPlayerSleeping = false;
        if (sleeping)
        {
            // The player gets up from the bed
            const std::lock_guard furnitureLock(mFurnitureMutex);
            const auto found = mFurniture.find(mPlayer);
            if (found != mFurniture.end() && found->second.mFurniture == bed)
                mFurniture.erase(found);
        }
        const int hours = static_cast<int>(std::floor(done + 1e-9));
        if (stopped.empty() && hours > 0)
            tellPlayer(std::string(sleeping ? "You slept " : "You waited ") + std::to_string(hours)
                + (hours == 1 ? " hour." : " hours."));
        const std::lock_guard lock(mInteractionMutex);
        mRest.mActive = false;
        mRest.mOpen = false;
        mRest.mFinish = false;
        mRest.mStopped = stopped;
        ++mRest.mEnded;
    }

    bool FalloutScripts::inOpenWorld(FalloutScript::FormId reference)
    {
        Place place;
        if (!whereIs(reference, place))
            return false;
        const OFWorld::CellStore* cell = cellAt(place);
        return cell != nullptr && cell->isExterior();
    }

    void FalloutScripts::discoverMarkers(float frametime)
    {
        mDiscoveryTimer += frametime;
        if (mDiscoveryTimer < discoveryInterval)
            return;
        mDiscoveryTimer = 0;
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell() || !inOpenWorld(mPlayer))
            return;
        const FalloutScript::FormId space = worldspaceOf(mPlayer);
        const osg::Vec3f here = player.getRefData().getPosition().asVec3();
        const double reach = gameSetting("iMapMarkerRevealDistance", 1000);
        for (const PlacedMarker& marker : placedMarkers())
        {
            if (marker.mWorldspace != space || mMapMarkers.state(marker.mId) == 2)
                continue;
            const osg::Vec3f away = marker.mPosition - here;
            if (std::sqrt(away.x() * away.x() + away.y() * away.y()) > reach)
                continue;
            // Walking up to a place finds it (one that a script only showed on the map too), and lets the player travel
            // back to it
            if (mMapMarkers.show(marker.mId, true))
            {
                markerShown(marker.mId);
                const int reward = static_cast<int>(gameSetting("iXPRewardDiscoverMapMarker", 10));
                if (reward > 0)
                    rewardExperience(reward);
            }
        }
    }
}
