#include "falloutscripts.hpp"
#include "falloutlaw.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <random>
#include <sstream>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadacti.hpp>
#include <components/esm4/loadalch.hpp>
#include <components/esm4/loadammo.hpp>
#include <components/esm4/loadarmo.hpp>
#include <components/esm4/loadbook.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadcont.hpp>
#include <components/esm4/loadcrea.hpp>
#include <components/esm4/loaddehy.hpp>
#include <components/esm4/loaddoor.hpp>
#include <components/esm4/loadfact.hpp>
#include <components/esm4/loadflst.hpp>
#include <components/esm4/loadfurn.hpp>
#include <components/esm4/loadhung.hpp>
#include <components/esm4/loadimod.hpp>
#include <components/esm4/loadingr.hpp>
#include <components/esm4/loadkeym.hpp>
#include <components/esm4/loadmesg.hpp>
#include <components/esm4/loadmisc.hpp>
#include <components/esm4/loadnote.hpp>
#include <components/esm4/loadnpc.hpp>
#include <components/esm4/loadrefr.hpp>
#include <components/esm4/loadslpd.hpp>
#include <components/esm4/loadtact.hpp>
#include <components/esm4/loadterm.hpp>
#include <components/esm4/loadweap.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/arguments.hpp>
#include <components/falloutscript/interaction.hpp>
#include <components/falloutscript/lockpick.hpp>
#include <components/falloutscript/messagetext.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/refdata.hpp"

namespace OFScript
{
    namespace
    {
        namespace Interaction = FalloutScript::Interaction;

        /// The actor value of the Lockpick skill
        constexpr int skillLockpick = 36;
        /// The actor value of the Science skill, which hacking goes by
        constexpr int skillScience = 40;

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

        // A lock that forcing broke stays broken
        if (mLockBroken.count(reference) > 0)
        {
            showNotice(wordOf("sLockBroken", "The lock is broken."), 2.0);
            return true;
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
        screen.mClass = static_cast<int>(lockClass);
        const FalloutScript::Lockpick::Rules rules;
        screen.mGate = rules.mSkillGate[static_cast<std::size_t>(std::clamp(screen.mClass, 0, 5))];
        screen.mForce
            = screen.mKeyOnly ? 0.0 : FalloutScript::Lockpick::forceChance(screen.mSkill, screen.mClass, rules);
        // A character below the skill of the class does not get the screen of the lock, only a box that says so
        if (!screen.mKeyOnly && !FalloutScript::Lockpick::mayTry(screen.mSkill, screen.mClass, rules))
        {
            std::string text = wordOf("sLockpickSkillTooLow", "You need a Lockpick skill of %.0f to try this lock.");
            const std::string number = std::to_string(static_cast<int>(screen.mGate));
            const std::size_t at = text.find('%');
            if (at != std::string::npos)
            {
                std::size_t end = at + 1;
                while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) || text[end] == '.'))
                    ++end;
                if (end < text.size())
                    text.replace(at, end - at + 1, number);
            }
            else
                text += " " + number;
            showNotice(text, 0);
            return true;
        }
        {
            const std::lock_guard lock(mInteractionMutex);
            screen.mSerial = ++mLockSerial;
            mLockScreen = screen;
        }
        return true;
    }

    bool FalloutScripts::hudInfo(const OFWorld::Ptr& target, HudInfo& result)
    {
        if (target.isEmpty() || !target.isInCell())
            return false;
        const std::uint32_t type = target.getType();
        const ESM::RefId base = target.getCellRef().getRefId();
        bool item = false;
        // Whether the line of weight and value is under the name: the executable's builder (FalloutNV.exe 0x775A00, Mac
        // round 12AO B4) shows it for armor, miscellaneous things, aid and item mods, for a weapon or ammunition only
        // when it is playable, and for books, ingredients and keys not at all (the name line alone)
        bool row = false;
        std::string verb;
        // The things the player takes: their weight and value are in the record, and the word is Take (a book that lies
        // in the world has that word too: the executable shows the word of Take for the activation of a readable item)
        if (const ESM4::Weapon* record = mStore.get<ESM4::Weapon>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mData.value;
            item = true;
            row = !record->mHasGameData || (record->mGame.mFlags1 & ESM4::Weapon::GameData::NonPlayable) == 0;
        }
        else if (const ESM4::Armor* record = mStore.get<ESM4::Armor>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mData.value;
            item = true;
            row = true;
        }
        else if (const ESM4::Ammunition* record = mStore.get<ESM4::Ammunition>().search(base))
        {
            result.mWeight = FalloutScript::carriedAmmoWeight(hardcoreMode(), record->mData.mWeight);
            result.mValue = record->mData.mValue;
            item = true;
            // The second bit of the flags of an ammunition record is the one that keeps it from the player
            row = (record->mData.mFlags & 0x02) == 0;
        }
        else if (const ESM4::Potion* record = mStore.get<ESM4::Potion>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mItem.value;
            item = true;
            row = true;
        }
        else if (const ESM4::MiscItem* record = mStore.get<ESM4::MiscItem>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mData.value;
            item = true;
            row = true;
        }
        else if (const ESM4::Book* record = mStore.get<ESM4::Book>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mData.value;
            item = true;
        }
        else if (const ESM4::Ingredient* record = mStore.get<ESM4::Ingredient>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mData.value;
            item = true;
        }
        else if (const ESM4::ItemMod* record = mStore.get<ESM4::ItemMod>().search(base))
        {
            result.mWeight = record->mData.mWeight;
            result.mValue = record->mData.mValue;
            item = true;
            row = true;
        }
        else if (const ESM4::Key* record = mStore.get<ESM4::Key>().search(base))
        {
            result.mWeight = record->mData.weight;
            result.mValue = record->mData.value;
            item = true;
        }
        if (item)
            verb = "sTargetTypeTake";
        else if (type == ESM4::Container::sRecordId)
            verb = "sTargetTypeOpen";
        else if (type == ESM4::Door::sRecordId)
            verb = "sTargetTypeOpenDoor";
        else if (type == ESM4::Activator::sRecordId || type == ESM4::TalkingActivator::sRecordId
            || type == ESM4::Terminal::sRecordId)
            verb = "sTargetTypeActivate";
        else if (type == ESM4::Furniture::sRecordId)
        {
            // Sitting and sleeping are the words for furniture whose markers say so (the activation types 3 and 5 of
            // the executable's table, FalloutNV.exe 0x11D5160; that they follow the kind of the marker is a reading of
            // the port's, the executable's function that tells was not read): a marker that the record has switched on
            // and that is of a sleeping kind makes it a bed, of a sitting kind a seat
            verb = "sTargetTypeActivate";
            const std::uint32_t flags = target.get<ESM4::Furniture>()->mBase->mActiveMarkerFlags;
            const std::vector<FalloutScript::FurnitureEntry> entries = furnitureEntries(target);
            bool sleeps = false;
            bool sits = false;
            for (std::size_t i = 0; i < entries.size(); ++i)
            {
                if (!FalloutScript::furnitureEntryEnabled(flags, i))
                    continue;
                const std::uint8_t kind = entries[i].mRef;
                sleeps = sleeps || (kind >= 1 && kind <= 6);
                sits = sits || (kind >= 10 && kind <= 20);
            }
            if (sleeps)
                verb = "sTargetTypeSleep";
            else if (sits)
                verb = "sTargetTypeSit";
        }
        else if (type == ESM4::Npc::sRecordId || type == ESM4::Creature::sRecordId)
        {
            const FalloutScript::FormId actor = scriptReferenceOf(target);
            if (actor == 0 || actor == mPlayer)
                return false;
            if (mActors.isDead(actor))
                verb = "sSearch";
            else if (type == ESM4::Npc::sRecordId && mCombat.target(actor) != mPlayer)
            {
                // A character that does not fight the player is talked to, or picked from when the player sneaks (the
                // executable's activation type 1 for a living character while the player sneaks, FalloutNV.exe
                // 0x579280 and 0x7772AC, Mac round 12BA G1)
                const OFWorld::Ptr body = OFBase::Environment::get().getWorld()->getPlayerPtr();
                const bool sneaking
                    = !body.isEmpty() && OFBase::Environment::get().getMechanicsManager()->isSneaking(body);
                verb = sneaking ? "sPickpocket" : "sTargetTypeTalk";
            }
        }
        else
            return false;

        result.mName = std::string(target.getClass().getName(target));
        // A thing with no name has no text (the executable's name line is blank, and the games show nothing)
        if (result.mName.empty())
            return false;
        result.mVerb = verb;
        result.mStats = row;
        if (item)
        {
            result.mCount = std::max(1, std::abs(target.getCellRef().getCount()));
            // What belongs to someone else is stolen, not taken: the owner of the reference is a character that is not
            // the player, or a faction the player is not in. The executable's test (FalloutNV.exe 0x579690) is longer
            // and was not read to its end: the owner of the cell is not asked here
            const FalloutScript::FormId reference = referenceOf(target);
            const FalloutScript::FormId owner = reference != 0 ? mObjectState.owner(reference) : 0;
            if (owner != 0 && owner != playerBase())
            {
                const bool faction = mStore.get<ESM4::Faction>().search(ESM::FormId::fromUint32(owner)) != nullptr;
                if (!faction || mActors.factionRank(mPlayer, owner) < 0)
                    result.mVerb = "sSteal";
            }
        }

        if (type == ESM4::Door::sRecordId || type == ESM4::Container::sRecordId)
        {
            const FalloutScript::FormId reference = referenceOf(target);
            if (reference != 0)
            {
                result.mBroken = mLockBroken.count(reference) > 0;
                const ESM4::Reference* record
                    = mStore.get<ESM4::Reference>().search(ESM::FormId::fromUint32(reference));
                // A door the reference marks as inaccessible says so, and has no verb (FalloutNV.exe 0x57B460, Mac
                // round 12AN B7: the flag 0x100 of the reference of a door)
                if (type == ESM4::Door::sRecordId && record != nullptr && (record->mFlags & ESM4::Rec_NoAccess) != 0)
                {
                    result.mInaccessible = true;
                    result.mVerb.clear();
                }
                if (mReferenceState.isLocked(reference))
                {
                    result.mLocked = true;
                    result.mHasKey = record != nullptr && !record->mKey.isZeroOrUnset()
                        && mActors.itemCount(mPlayer, record->mKey.toUint32()) > 0;
                    const Interaction::Setting setting
                        = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
                    const bool leveled = record != nullptr && (record->mLockFlags & 0x04) != 0;
                    const int level = Interaction::lockLevel(
                        mReferenceState.lockLevel(reference), leveled, mProgress.level(), setting);
                    const Interaction::LockClass lockClass = Interaction::lockClass(level, setting);
                    result.mLockWord = Interaction::lockClassWord(lockClass);
                    result.mKeyOnly = lockClass == Interaction::LockClass::Impossible;
                }
                if (type == ESM4::Door::sRecordId)
                {
                    // A door the player cannot open is picked; one that stands open is closed (FalloutNV.exe
                    // 0x7772AC, Mac round 12BA G1)
                    settleDoor(target, reference);
                    if (result.mInaccessible)
                        result.mVerb.clear();
                    else if (result.mLocked && !result.mHasKey)
                        result.mVerb = "sPick";
                    else if (!result.mLocked && mOpenStates.restsOpen(reference))
                        result.mVerb = "sCloseButton";
                }
            }
        }

        // The key (or button) of the activation is named as the player has it set
        if (const auto key = messageToken("-sUActnGrab"))
            result.mKeyText = *key;
        if (OFBase::InputManager* input = OFBase::Environment::get().getInputManager())
            result.mPad = input->joystickLastUsed();
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

    // ---- The help manual ----

    std::vector<FalloutScripts::HelpPage> FalloutScripts::helpManual(bool pad)
    {
        std::vector<HelpPage> pages;
        const std::string wanted = pad ? "helpmanualxbox" : "helpmanual";
        for (const ESM4::FormIdList& list : mStore.get<ESM4::FormIdList>())
        {
            if (Misc::StringUtils::lowerCase(list.mEditorId) != wanted)
                continue;
            for (const ESM::FormId& entry : list.mObjects)
            {
                const ESM4::Message* message = mStore.get<ESM4::Message>().search(entry);
                if (message == nullptr)
                    continue;
                HelpPage page;
                page.mId = message->mEditorId;
                page.mTitle = message->mFullName;
                page.mText = message->mDescription;
                pages.push_back(std::move(page));
            }
            break;
        }
        return pages;
    }

    std::optional<FalloutScripts::HelpPage> FalloutScripts::helpMessage(std::string_view editorId)
    {
        const std::string wanted = Misc::StringUtils::lowerCase(editorId);
        for (const ESM4::Message& message : mStore.get<ESM4::Message>())
        {
            if (Misc::StringUtils::lowerCase(message.mEditorId) != wanted)
                continue;
            HelpPage page;
            page.mId = message.mEditorId;
            page.mTitle = message.mFullName;
            page.mText = message.mDescription;
            return page;
        }
        return std::nullopt;
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

        lockOutcome("opened");
        tellPlayer("Lock picked.");
        return "opened";
    }

    std::string FalloutScripts::lockOutcome(const std::string& kind)
    {
        LockScreen screen;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mLockScreen)
                return std::string();
            screen = *mLockScreen;
        }
        if (kind == "outOfPicks")
        {
            // The player has no pick (to begin with, or the last has broken): the screen stays, the pin cannot be used
            showNotice(wordOf("sOutOfLockpicks", "You are out of lockpicks."), 2.0);
            return std::string();
        }
        if (kind == "pickBroke")
        {
            // Playing the break takes one pin, whatever the lock
            mActors.removeItem(mPlayer, masterForm(sBobbyPin), 1);
            return std::to_string(mActors.itemCount(mPlayer, masterForm(sBobbyPin)));
        }
        if (kind == "forceFailed")
        {
            // Forcing a lock and failing breaks it for good, and the screen closes (no pin is used)
            mLockBroken.insert(screen.mReference);
            showNotice(wordOf("sLockBroken", "The lock is broken."), 2.0);
            closeLockScreen();
            return "broken";
        }
        if (kind != "opened")
            return std::string();

        const Interaction::Setting setting
            = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
        mReferenceState.unlock(screen.mReference);
        // A lock that belongs to someone is a theft that the owner may see (0x78EB50)
        if (mLaw)
            mLaw->tamperedWith(screen.mReference);
        // The experience is given once for a lock; opening it again (it was locked anew) gives none
        if (mLockExperienceGiven.insert(screen.mReference).second)
        {
            if (const char* experience
                = Interaction::lockClassExperience(Interaction::lockClass(screen.mLevel, setting)))
            {
                static constexpr std::array<int, 5> defaults{ 20, 30, 40, 50, 60 };
                const int category = std::clamp(screen.mClass, 0, 4);
                const int reward
                    = static_cast<int>(gameSetting(experience, defaults[static_cast<std::size_t>(category)]));
                if (reward > 0)
                    rewardExperience(reward);
            }
        }
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
        const Interaction::Setting setting
            = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
        TerminalState state;
        state.mReference = reference;
        state.mPath.push_back(base.toUint32());
        state.mHackLevel = Interaction::hackLevel(
            record->mDifficulty, (record->mTerminalFlags & 0x01) != 0, mProgress.level(), setting);
        state.mHackParity = static_cast<int>(base.toUint32() & 1);
        if (closed && !record->mPasswordNote.isZeroOrUnset()
            && mActors.itemCount(mPlayer, record->mPasswordNote.toUint32()) > 0)
        {
            mReferenceState.unlock(reference);
            tellPlayer("Password accepted.");
        }
        else if (closed)
        {
            int lockouts = 0;
            {
                const std::lock_guard lock(mInteractionMutex);
                const auto found = mTerminalLockouts.find(reference);
                if (found != mTerminalLockouts.end())
                    lockouts = found->second;
            }
            // A terminal that locked the player out stays shut; once it is a second try for a perk that lets one
            // (FalloutNV.exe 0x501990: the byte of the terminal, and the entry point 20 of the perks). A terminal of
            // difficulty 5 asks for a key, and has no game of hacking (0x966c60 answers 2 for it too)
            const bool again = mPerks.apply(FalloutScript::EntryPoint::ignoreLockedTerminal, mPlayer, 0.0) != 0.0;
            if (record->mDifficulty > 4 || lockouts >= 2 || (lockouts == 1 && !again))
                state.mLockedOut = true;
            else
            {
                // The Science that the level asks for (FalloutNV.exe 0x501760); a player with less does not see the
                // game at all. Fallout3.exe has no such setting (round 12T: the five are not in it), so only New Vegas
                // asks
                const double skill = mActorValues.current(mPlayer, skillScience);
                const double needed = FalloutScript::Hacking::minimumScience(state.mHackLevel, setting);
                if (mGame == FalloutScript::Game::NewVegas && skill < needed)
                {
                    tellPlayer(FalloutScript::formatMessage(
                        gameSettingText("sHackIneligible"), { std::floor(needed), std::floor(skill) }));
                    return true;
                }
                state.mPuzzle = newPuzzle(state.mHackLevel, state.mHackParity);
            }
        }
        {
            const std::lock_guard lock(mInteractionMutex);
            if (mTerminal)
                leaveTerminal();
            state.mSerial = ++mTerminalSerial;
            mTerminal = std::move(state);
        }
        return true;
    }

    double FalloutScripts::hackGate()
    {
        const std::lock_guard lock(mInteractionMutex);
        return mHackGate;
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
        FalloutScript::FormId base = 0;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mTerminal)
                return std::nullopt;
            const TerminalState& state = *mTerminal;
            screen.mTerminal = state.mReference;
            screen.mSerial = state.mSerial;
            // The Back line is in the menus below the first
            screen.mCanGoBack = state.mPath.size() > 1;
            menu = state.mPath.back();
            base = state.mPath.front();
            screen.mResult = state.mResult;
            screen.mResultSerial = state.mResultSerial;
            screen.mNote = state.mNote;
            screen.mLockedOut = state.mLockedOut;
            screen.mHackLevel = state.mHackLevel;
            screen.mIntroStars
                = static_cast<int>(FalloutScript::Hacking::wordLength(state.mHackLevel, state.mHackParity));
            screen.mGate = mHackGate;
            if (state.mLockedOut)
                screen.mHacking = true;
            if (state.mPuzzle)
            {
                screen.mHacking = true;
                screen.mSolved = state.mPuzzle->solved();
                screen.mWords = state.mPuzzle->words();
                screen.mGuesses = state.mGuesses;
                screen.mAttempts = state.mPuzzle->attemptsLeft();
                screen.mMaxAttempts = state.mPuzzle->maxAttempts();
                screen.mDump = state.mPuzzle->dump();
                screen.mAddress = static_cast<int>(state.mPuzzle->address());
                screen.mWordsLeft = static_cast<int>(state.mPuzzle->wordsLeft());
            }
        }
        // A terminal whose record is not marked as unlocked tells the player that a password is asked for (the
        // introduction of the screen is typed at every visit, also after the terminal was hacked: 0x501ae0)
        if (const ESM4::Terminal* root = mStore.get<ESM4::Terminal>().search(ESM::FormId::fromUint32(base)))
            screen.mIntro = (root->mTerminalFlags & 0x02) == 0;
        const ESM4::Terminal* record = mStore.get<ESM4::Terminal>().search(ESM::FormId::fromUint32(menu));
        if (record == nullptr)
            return screen;
        screen.mName = record->mFullName;
        if (screen.mHacking)
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
            if (!mTerminal || mTerminal->mPuzzle || mTerminal->mLockedOut || !mTerminal->mNote.empty())
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

        // The result script of the line runs first, then what the line shows: its result goes in the line under the
        // menu, the note in the zone of the screen (0x757f70)
        mObjects.runResult(item.mScript, terminal);
        std::string note;
        if (!item.mDisplayNote.isZeroOrUnset())
        {
            const ESM4::Note* noteRecord = mStore.get<ESM4::Note>().search(item.mDisplayNote);
            // A line adds its note when its flag says so, and not twice
            if ((item.mFlags & 0x01) != 0 && mActors.itemCount(mPlayer, item.mDisplayNote.toUint32()) == 0)
            {
                mActors.addItem(mPlayer, item.mDisplayNote.toUint32(), 1);
                std::string told = gameSettingText("sComputersAddedNote");
                if (told.empty())
                    told = "Note added: %s";
                const std::size_t place = told.find("%s");
                if (place != std::string::npos)
                    told.replace(place, 2, noteRecord != nullptr ? noteRecord->mFullName : std::string());
                tellPlayer(told);
            }
            // Only a note of text is shown by the terminal; a sound, an image and a voice have other screens
            if (noteRecord != nullptr && noteRecord->mType == 1)
                note = terminalFormat(noteRecord->mText);
        }

        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || mTerminal->mReference != terminal)
            return;
        if (!item.mSubMenu.isZeroOrUnset() && mStore.get<ESM4::Terminal>().search(item.mSubMenu) != nullptr)
        {
            mTerminal->mPath.push_back(item.mSubMenu.toUint32());
            mTerminal->mResult.clear();
        }
        else if (!item.mResultText.empty())
        {
            mTerminal->mResult = terminalFormat(item.mResultText);
            ++mTerminal->mResultSerial;
        }
        mTerminal->mNote = std::move(note);
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
        else if (!state.mNote.empty())
        {
            // The note was read: the menu comes back
            state.mNote.clear();
            state.mSerial = ++mTerminalSerial;
        }
        else if (state.mPath.size() > 1)
        {
            state.mPath.pop_back();
            state.mResult.clear();
            state.mSerial = ++mTerminalSerial;
        }
        else
            leaveTerminal();
    }

    void FalloutScripts::leaveTerminal()
    {
        // The next board waits for a time from the closing of the screen of the last one
        if (mTerminal && (mTerminal->mPuzzle || mTerminal->mLockedOut))
            mHackGate = gameSetting("iHackingRetryMilliseconds", 10000) / 1000.0;
        mTerminal.reset();
    }

    std::unique_ptr<FalloutScript::Hacking::Puzzle> FalloutScripts::newPuzzle(int level, int parity)
    {
        const Interaction::Setting setting
            = [this](std::string_view name, double fallback) { return gameSetting(name, fallback); };
        std::uint32_t seed = 0;
        {
            const std::lock_guard lock(mCombatRandomMutex);
            seed = static_cast<std::uint32_t>(mCombatRandom());
        }
        // The Science of the player and what the perks add to it for hacking
        const double science = mActorValues.current(mPlayer, skillScience)
            + mPerks.apply(FalloutScript::EntryPoint::hackingModifier, mPlayer, 0.0);
        return std::make_unique<FalloutScript::Hacking::Puzzle>(
            seed, level, parity, science, setting, hackingDictionary());
    }

    const FalloutScript::Hacking::Dictionary& FalloutScripts::hackingDictionary()
    {
        const std::lock_guard lock(mHackingDictionaryMutex);
        if (!mHackingDictionary)
        {
            std::string text;
            if (const Resource::ResourceSystem* resources = OFBase::Environment::get().getResourceSystem())
            {
                Files::IStreamPtr stream = resources->getVFS()->find(VFS::Path::Normalized("menus/falloutdict.txt"));
                if (stream && stream->peek() != std::istream::traits_type::eof())
                {
                    std::ostringstream bytes;
                    bytes << stream->rdbuf();
                    text = bytes.str();
                }
            }
            mHackingDictionary = FalloutScript::Hacking::Dictionary::parse(text);
            // An install without the file plays with the project's own words
            Log(Debug::Info) << "Hacking dictionary: "
                             << (mHackingDictionary->empty() ? "built in" : "the game's file");
        }
        return *mHackingDictionary;
    }

    void FalloutScripts::hackGuessed(TerminalState& state, bool correct)
    {
        state.mSerial = ++mTerminalSerial;
        if (correct)
        {
            mReferenceState.unlock(state.mReference);
            // A terminal that belongs to someone costs the karma of a theft (0x501310)
            if (mLaw)
                mLaw->tamperedWith(state.mReference, true);
            const int level = state.mHackLevel;
            if (const char* experience = Interaction::hackClassExperience(level))
            {
                const int reward = static_cast<int>(gameSetting(experience, 0));
                if (reward > 0)
                    rewardExperience(reward);
            }
        }
        else if (state.mPuzzle->lockedOut())
        {
            // The terminal is shut: the next visit finds it so (a second try only for a perk), and the screen closes
            ++mTerminalLockouts[state.mReference];
            state.mLockedOut = true;
        }
    }

    void FalloutScripts::hackFinish()
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || !mTerminal->mPuzzle || !mTerminal->mPuzzle->solved())
            return;
        mTerminal->mPuzzle.reset();
        mTerminal->mGuesses.clear();
        mTerminal->mSerial = ++mTerminalSerial;
    }

    int FalloutScripts::hackGuess(int word)
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || !mTerminal->mPuzzle || mTerminal->mLockedOut || word < 0)
            return -1;
        TerminalState& state = *mTerminal;
        FalloutScript::Hacking::Puzzle& puzzle = *state.mPuzzle;
        const FalloutScript::Hacking::Puzzle::Guess guess = puzzle.guess(static_cast<std::size_t>(word));
        if (guess.mIgnored)
            return -1;
        state.mGuesses.push_back({ puzzle.words()[static_cast<std::size_t>(word)], guess.mLikeness });
        hackGuessed(state, guess.mCorrect);
        if (guess.mCorrect)
        {
            // The screen of widgets has nothing to show for it
            state.mPuzzle.reset();
            state.mGuesses.clear();
        }
        return guess.mLikeness;
    }

    namespace
    {
        const char* unitKind(FalloutScript::Hacking::Puzzle::Unit::Kind kind)
        {
            switch (kind)
            {
                case FalloutScript::Hacking::Puzzle::Unit::Kind::Word:
                    return "word";
                case FalloutScript::Hacking::Puzzle::Unit::Kind::Brackets:
                    return "brackets";
                default:
                    return "character";
            }
        }
    }

    std::optional<FalloutScripts::HackUnit> FalloutScripts::hackUnit(int position)
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || !mTerminal->mPuzzle || position < 0
            || static_cast<std::size_t>(position) >= mTerminal->mPuzzle->dump().size())
            return std::nullopt;
        const FalloutScript::Hacking::Puzzle::Unit unit
            = mTerminal->mPuzzle->unitAt(static_cast<std::size_t>(position));
        return HackUnit{ unitKind(unit.mKind), static_cast<int>(unit.mFirst), static_cast<int>(unit.mLength) };
    }

    FalloutScripts::HackPick FalloutScripts::hackPick(int position)
    {
        HackPick result;
        const std::lock_guard lock(mInteractionMutex);
        if (!mTerminal || !mTerminal->mPuzzle || mTerminal->mLockedOut || position < 0)
            return result;
        TerminalState& state = *mTerminal;
        FalloutScript::Hacking::Puzzle& puzzle = *state.mPuzzle;
        using Effect = FalloutScript::Hacking::Puzzle::Pick::Effect;
        const FalloutScript::Hacking::Puzzle::Pick pick = puzzle.pick(static_cast<std::size_t>(position));
        if (pick.mIgnored)
            return result;
        result.mUnit
            = { unitKind(pick.mUnit.mKind), static_cast<int>(pick.mUnit.mFirst), static_cast<int>(pick.mUnit.mLength) };
        result.mText = pick.mText;
        result.mIgnored = false;
        result.mCorrect = pick.mCorrect;
        result.mLikeness = pick.mLikeness;
        result.mAttemptsLeft = pick.mAttemptsLeft;
        switch (pick.mEffect)
        {
            case Effect::Denied:
                result.mEffect = "denied";
                break;
            case Effect::Removed:
                result.mEffect = "removed";
                break;
            case Effect::Replenished:
                result.mEffect = "replenished";
                break;
            default:
                break;
        }
        // A word is a guess, and so is a character that is no part of a word or a pair of brackets (0x767060)
        if (pick.mUnit.mKind != FalloutScript::Hacking::Puzzle::Unit::Kind::Brackets)
        {
            state.mGuesses.push_back({ pick.mText, pick.mLikeness });
            result.mLockedOut = !pick.mCorrect && puzzle.lockedOut();
            hackGuessed(state, pick.mCorrect);
        }
        else if (pick.mEffect != Effect::None && pick.mEffect != Effect::Denied)
            state.mSerial = ++mTerminalSerial;
        return result;
    }

    void FalloutScripts::closeTerminal()
    {
        const std::lock_guard lock(mInteractionMutex);
        leaveTerminal();
    }

    // ---- Waiting and sleeping ----

    bool FalloutScripts::hostilesNearPlayer()
    {
        namespace Restrictions = FalloutScript::Restrictions;
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return false;
        // The reach of the check: the executables look as far as fHostileActorInteriorDistance (2000) in an interior and
        // fHostileActorExteriorDistance (3000) outside, both absent from the master files (FalloutNV.exe 0x9764A0)
        const double reach = Restrictions::hostileReach(!player.getCell()->isExterior(),
            gameSetting("fHostileActorExteriorDistance", 3000), gameSetting("fHostileActorInteriorDistance", 2000));
        const osg::Vec3f here = player.getRefData().getPosition().asVec3();
        // The characters that are after the player (the executable also counts those that its factions and aggression
        // make enemies: the combat state here lists the ones that went for it)
        std::vector<Restrictions::Candidate> candidates;
        for (const FalloutScript::FormId attacker : mCombat.attackersOf(mPlayer))
        {
            const OFWorld::Ptr enemy = findReference(attacker);
            if (enemy.isEmpty() || !enemy.isInCell() || !sharesSpace(enemy.getCell(), player.getCell()))
                continue;
            Restrictions::Candidate candidate;
            candidate.mHostile = true;
            candidate.mDead = mActors.isDead(attacker);
            // One that was removed or disabled is no threat, whatever it was doing
            candidate.mDisabled = !enemy.getRefData().isEnabled() || enemy.getCellRef().getCount() <= 0;
            candidate.mDistance = (enemy.getRefData().getPosition().asVec3() - here).length();
            candidates.push_back(candidate);
        }
        return Restrictions::hostileNear(candidates, reach);
    }

    bool FalloutScripts::playerTakingHealthDamage()
    {
        // The executables look for a running effect that changes health and is harmful, whatever its size
        for (const FalloutScript::ItemUse::Active& active : mItemUse.active(mPlayer))
            if (active.mActorValue == FalloutScript::ActorValue::health && active.mMagnitude < 0 && active.mRemaining > 0)
                return true;
        return false;
    }

    void FalloutScripts::wakePlayer(double hours)
    {
        // Only a sleep that is going on is cut short (M13 executable reads, WakeUpPC): what is asked is no less than none
        // and replaces the hours that are left, and none ends the sleep where it is
        const std::lock_guard lock(mInteractionMutex);
        if (!mRest.mActive || !mRest.mSleeping)
            return;
        const double left = std::isnan(hours) ? 0.0 : std::max(hours, 0.0);
        if (left <= 0)
            mRest.mFinish = true;
        else
            mRest.mTotal = mRest.mDone + left;
    }

    void FalloutScripts::setPlayerSleepHours(double hours)
    {
        // The hours the script gives are the ones that are left, with no limit of the menu on them (the command stores
        // them as they are; a number below one leaves none for the hourly count to pass)
        const std::lock_guard lock(mInteractionMutex);
        if (!mRest.mActive)
            return;
        const double left = std::isnan(hours) ? 0.0 : hours;
        // (a fraction of an hour above one counts as a whole one when the count reaches it: what the hourly check does
        // with a fraction is not traced)
        if (left < 1.0)
            mRest.mFinish = true;
        else
            mRest.mTotal = mRest.mDone + left;
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
        namespace Restrictions = FalloutScript::Restrictions;
        const OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        if (player.isEmpty() || !player.isInCell())
            return "You cannot rest now.";
        const Restrictions::Action action = sleeping ? Restrictions::Action::Sleep : Restrictions::Action::Wait;
        const char* refusal = Restrictions::refusal(action, restrictionFacts(action, bed));
        if (refusal == nullptr)
            return std::string();
        const std::string text = refusalWord(refusal);
        log(std::string("Rest refused: ") + refusal + (text.empty() ? " (the files have no text for it)" : ""));
        return text.empty() ? std::string(sleeping ? "You cannot sleep now." : "You cannot wait now.") : text;
    }

    void FalloutScripts::requestRest(bool sleeping, FalloutScript::FormId bed)
    {
        const std::string refusal = restRefusalText(sleeping, bed);
        if (!refusal.empty())
        {
            tellPlayer(refusal, 2);
            return;
        }
        openRestMenu(sleeping, bed);
    }

    void FalloutScripts::openRestMenu(bool sleeping, FalloutScript::FormId bed)
    {
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
            tellPlayer(refusal, 2);
            return refusal;
        }
        if (sleeping && bed != 0)
        {
            // The game does not lay the player on the bed (M13 executable reads: the activation of a bed opens the menu
            // where the player stands, with no walk to the entry, no lying pose and no getting up): the player is on
            // the bed all the time it sleeps only as far as GetSleeping and the questions about the furniture of a
            // character are asked
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
        // Whole hours, as the menu of the games gives (a part of an hour would pass as a whole one)
        mRest.mTotal = std::floor(std::clamp(hours, 1.0, 24.0) + 1e-9);
        mRest.mDone = 0;
        mRest.mTickSeconds = 0;
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
            entry.mWorldspace = marker.mWorldspace;
            entry.mType = marker.mType;
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

    FalloutScripts::MapInfo FalloutScripts::mapInfo()
    {
        MapInfo info;
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        if (!player.isEmpty() && player.isInCell())
        {
            info.mPosition = player.getRefData().getPosition().asVec3();
            constexpr double degreesPerRadian = 180.0 / 3.14159265358979323846;
            info.mHeading = std::fmod(
                std::fmod(player.getRefData().getPosition().rot[2] * degreesPerRadian, 360.0) + 360.0, 360.0);
            info.mOutside = inOpenWorld(mPlayer);
            info.mLocation = std::string(world.getCellName(player.getCell()));
            if (info.mOutside)
            {
                info.mWorldspace = worldspaceOf(mPlayer);
                if (info.mWorldspace != 0)
                    mLastOpenSpace = info.mWorldspace;
            }
        }
        // Inside a building the map is the one of the world outside: the last the player stood in, and the main world
        // of the game when it has not been outside yet (the first place that the player had found was a world of an
        // add-on in the first run)
        if (info.mWorldspace == 0)
        {
            placedMarkers();
            info.mWorldspace = mLastOpenSpace != 0 ? mLastOpenSpace : mMainSpace;
        }
        if (info.mWorldspace != 0)
        {
            const ESM4::World* space = mStore.get<ESM4::World>().search(ESM::FormId::fromUint32(info.mWorldspace));
            // Said once for each world space the page asks about: a page with no picture and no markers is either a
            // space the plugins do not hold or one whose record has no map
            if (info.mWorldspace != mMapSpaceTold)
            {
                mMapSpaceTold = info.mWorldspace;
                Log(Debug::Info) << "Map of the world space " << FalloutScript::hex(info.mWorldspace) << ": "
                                 << (space == nullptr ? "no record" : "record found") << ", name of "
                                 << (space != nullptr ? space->mFullName.size() : 0) << " characters, picture of "
                                 << (space != nullptr ? space->mMapFile.size() : 0) << " characters, "
                                 << (space != nullptr ? space->mMap.width : 0) << "x"
                                 << (space != nullptr ? space->mMap.height : 0) << " units, outside "
                                 << (info.mOutside ? "yes" : "no");
            }
            if (space != nullptr)
            {
                info.mWorldName = space->mFullName;
                info.mMapFile = space->mMapFile;
                info.mWidth = space->mMap.width;
                info.mHeight = space->mMap.height;
                info.mNorthWestX = space->mMap.NWcellX;
                info.mNorthWestY = space->mMap.NWcellY;
                info.mSouthEastX = space->mMap.SEcellX;
                info.mSouthEastY = space->mMap.SEcellY;
            }
        }
        info.mHasDate = currentDate(info.mDate);
        info.mHour = gameHour();
        return info;
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

        namespace Restrictions = FalloutScript::Restrictions;
        Restrictions::Facts facts = restrictionFacts(Restrictions::Action::FastTravel, 0);
        facts.mUndiscovered = mMapMarkers.state(marker) != 2;
        if (const char* refusal = Restrictions::refusal(Restrictions::Action::FastTravel, facts))
        {
            std::string text = refusalWord(refusal);
            log(std::string("Travel refused: ") + refusal + (text.empty() ? " (the files have no text for it)" : ""));
            if (text.empty())
                text = "You cannot travel now.";
            tellPlayer(text);
            return text;
        }

        // The time the trip takes (FalloutNV.exe 0x93CDF0): the length of the way over the speed of the player, in hours of
        // the game at the time scale, cut to whole hours. The executable measures the way on the navigation mesh, which is
        // not loaded for places so far; the straight line is what is used here (a way is never shorter), and a trip from
        // a place that is not in the space of the marker has no length, as one the executable can not measure has none.
        double distance = 0;
        if (inOpenWorld(mPlayer) && worldspaceOf(mPlayer) == target->mWorldspace)
        {
            const osg::Vec3f away = target->mPosition - player.getRefData().getPosition().asVec3();
            distance = std::sqrt(away.x() * away.x() + away.y() * away.y());
        }
        const double timeScale = std::max(static_cast<double>(world.getTimeManager()->getGameTimeScale()), 1.0);
        const int hours = Restrictions::travelHours(distance, travelSpeed(), timeScale);
        log("Travel: " + std::to_string(hours) + " hours for " + std::to_string(static_cast<long long>(distance))
            + " units");
        runOnMainThread([this, marker, hours, timeScale] {
            namespace AV = FalloutScript::ActorValue;
            OFBase::World* world = OFBase::Environment::get().getWorld();
            // Each hour of the trip goes by on its own: the time moves, what runs by the seconds of the game ages by the
            // hour, and the player heals for two seconds of its regeneration (0x93CDF0 and 0x88B510)
            const double seconds = 3600.0 / timeScale;
            for (int hour = 0; hour < hours; ++hour)
            {
                world->advanceTime(1.0);
                // The needs of the hardcore mode count the hour before the effects of the stages it reaches run in it
                updateHardcore(false);
                if (!mActors.isDead(mPlayer))
                    mActorValues.restore(mPlayer, AV::health,
                        FalloutScript::DerivedStats::regeneration(
                            mActorValues.current(mPlayer, AV::healRate), Restrictions::travelHealingSeconds));
                mItemUse.update(seconds);
                mMagic.update(seconds);
                // The needs or an effect can have taken the last of the health: the trip ends there, with no more
                // hours and no move (as the rest ends)
                if (mActors.isDead(mPlayer))
                    return;
            }
            // A little above the marker, which stands about where a character would. IsPlayerMovingIntoNewSpace is on
            // for the whole trip (the executable's worker of the fast travel sets it and clears it).
            playerMoveBegins();
            moveTo(mPlayer, marker, { 0.0, 0.0, 48.0 });
            playerMoveEnds();
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

        // The wait before another board is laid out
        {
            const std::lock_guard lock(mInteractionMutex);
            if (mHackGate > 0)
                mHackGate = std::max(0.0, mHackGate - frametime);
        }

        updateRest(frametime);
        discoverMarkers(frametime);
    }

    std::vector<FalloutScript::HardcoreNeeds::Stage> FalloutScripts::hardcoreStages(int need)
    {
        std::vector<FalloutScript::HardcoreNeeds::Stage> stages;
        const auto add = [&stages](const auto& records) {
            for (const auto& record : records)
                stages.push_back({ static_cast<int>(record.mData.mTriggerThreshold), record.mData.mActorEffect });
        };
        switch (need)
        {
            case FalloutScript::HardcoreNeeds::Dehydration:
                add(mStore.get<ESM4::DehydrationStage>());
                break;
            case FalloutScript::HardcoreNeeds::Hunger:
                add(mStore.get<ESM4::HungerStage>());
                break;
            default:
                add(mStore.get<ESM4::SleepDeprivationStage>());
                break;
        }
        return stages;
    }

    void FalloutScripts::updateHardcore(bool sleeping)
    {
        if (!hardcoreOn())
        {
            // Turned off (a script chose the casual mode): the stages of the needs come off the player
            if (mHardcoreOn.exchange(false))
                mHardcore.end(mPlayer);
            return;
        }
        if (!mHardcoreOn.exchange(true))
        {
            // The settings are intervals in game minutes at the time scale of 60 (the executables' defaults are what
            // the master file of New Vegas says)
            mHardcore.setRates(gameSetting("fHCDehydrationRate", 10), gameSetting("fHCStarvationRate", 25),
                gameSetting("fHCSleepDeprivationRate", 50));
        }
        // The updater of the executable does nothing while the control of fighting is taken away (0x969C30 asks the
        // control byte of the player for its bit 4), and the time it left out is counted when it is given back
        if ((mControlsOff.load() & (1u << static_cast<unsigned>(FalloutScript::PlayerControl::Fighting))) != 0)
            return;
        OFBase::World* world = OFBase::Environment::get().getWorld();
        if (world == nullptr || mActors.isDead(mPlayer))
            return;
        if (world->getGodModeState())
        {
            // A player that cannot be hurt is not worn down by the needs either (the last stage takes all the health):
            // they stop counting, and count again from where the god mode ends
            mHardcore.clear();
            return;
        }
        const double minutes = std::floor(gameDaysPassed()) * 24.0 * 60.0 + gameHour() * 60.0;
        mHardcore.update(mPlayer, minutes, world->getTimeManager()->getGameTimeScale(), sleeping);
    }

    void FalloutScripts::updateRest(float frametime)
    {
        namespace AV = FalloutScript::ActorValue;
        bool sleeping = false;
        FalloutScript::FormId bed = 0;
        bool finish = false;
        bool fadeOut = false;
        bool hour = false;
        {
            const std::lock_guard lock(mInteractionMutex);
            if (!mRest.mActive)
                return;
            sleeping = mRest.mSleeping;
            bed = mRest.mBed;
            finish = mRest.mFinish;
            if (sleeping && !mRest.mFaded)
            {
                // The screen goes dark as the sleep begins (waiting leaves it as it is), and stays dark until the end
                mRest.mFaded = true;
                fadeOut = true;
            }
            // One game hour passes for each second of the frames; what is left of the second after the hour is dropped
            mRest.mTickSeconds += frametime;
            if (mRest.mTickSeconds >= sRestSecondsPerHour)
            {
                mRest.mTickSeconds = 0;
                hour = !finish;
            }
        }
        if (fadeOut)
        {
            runOnMainThread([] {
                if (OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager())
                    windows->fadeScreenOut(sRestFadeSeconds);
            });
        }
        std::string stopped;
        bool died = false;
        // Characters that come for the player end it
        if (!finish && hostilesNearPlayer())
        {
            stopped = restRefusalText(sleeping, 0);
            if (stopped.empty())
                stopped = "You were interrupted.";
            finish = true;
            hour = false;
        }
        else if (hour)
        {
            const auto world = OFBase::Environment::get().getWorld();
            world->advanceTime(1.0);
            {
                // The hour has gone by before the scripts of the effects run in it, so the hours that one of them asks
                // for (SetPCSleepHours, WakeUpPC) are the ones after it
                const std::lock_guard lock(mInteractionMutex);
                mRest.mDone += 1.0;
            }
            // The needs of the hardcore mode count the hour that went by, as one slept when this is a sleep (the hour
            // that ends the rest included: it is not left to the frames after it, which count awake time)
            updateHardcore(sleeping);
            // The game runs what it simulates by the seconds of its own clock, which is 3600 over the time scale for
            // each hour that goes by (12 hours of rest is 1440 of them at the scale of 30, the lifetime of the effect
            // of a good night's sleep in the reads)
            const double timeScale = std::max(static_cast<double>(world->getTimeManager()->getGameTimeScale()), 1.0);
            const double seconds = 3600.0 / timeScale;
            // The hour heals as the time of any frame does: by the heal rate of the player over 3600 for each of those
            // seconds (FalloutNV.exe 0x88B510, item 17 of m13-exe-reads.md)
            if (!mActors.isDead(mPlayer))
                mActorValues.restore(mPlayer, AV::health,
                    FalloutScript::DerivedStats::regeneration(mActorValues.current(mPlayer, AV::healRate), seconds));
            // The effects that are running age by the hour as well. Both halves of an effect go on together, what it
            // does to the actor values (the item use: boosts that end, points over time) and the record that it is
            // running (the magic), as the frames of the game do when it is not paused
            mItemUse.update(seconds);
            // (the scripts of those effects run for the seconds of the hour, which GetSecondsPassed tells them)
            mMagic.update(seconds);
            // Each hour of sleep lowers the sleep deprivation of the hardcore mode
            if (sleeping && hardcoreOn())
                mHardcore.sleepHour(mPlayer, gameSetting("fHCSleepRestorationMod", 60));
            // An effect that ran out can have taken the last of the health: the rest ends, with nothing given back
            died = mActors.isDead(mPlayer);
            if (died)
                finish = true;
        }

        double done = 0;
        bool completed = false;
        {
            const std::lock_guard lock(mInteractionMutex);
            done = mRest.mDone;
            // A script that ran in the hour (that of a running effect) can have asked to wake up, or left no hours:
            // the rest ends here, and is not a completed sleep
            if (mRest.mFinish)
                finish = true;
            if (!finish && mRest.mDone + 1e-9 >= mRest.mTotal)
            {
                finish = true;
                completed = true;
            }
        }
        if (!finish)
            return;

        // The last hour of a sleep that was not stopped gives back all the health and fatigue and the condition of
        // every part of the body (the executables skip it in the hardcore mode of New Vegas)
        if (sleeping && completed && !hardcoreOn())
        {
            constexpr double all = 1.0e9;
            mActorValues.restore(mPlayer, AV::health, all);
            mActorValues.restore(mPlayer, AV::fatigue, all);
            for (int part = AV::firstCondition; part <= AV::lastCondition; ++part)
                mActorValues.restore(mPlayer, part, all);
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
        if (stopped.empty() && !died && hours > 0)
            tellPlayer(std::string(sleeping ? "You slept " : "You waited ") + std::to_string(hours)
                + (hours == 1 ? " hour." : " hours."));
        bool faded = false;
        {
            const std::lock_guard lock(mInteractionMutex);
            faded = mRest.mFaded;
            mRest.mFaded = false;
        }
        if (faded)
        {
            // Whatever ended the sleep, the screen gets light again
            runOnMainThread([] {
                if (OFBase::WindowManager* windows = OFBase::Environment::get().getWindowManager())
                    windows->fadeScreenIn(sRestFadeSeconds);
            });
        }
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
        if (space != 0)
            mLastOpenSpace = space;
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
                markerDiscovered(marker.mId);
                noteChallengeMarker(marker.mId);
                const int reward = static_cast<int>(gameSetting("iXPRewardDiscoverMapMarker", 10));
                if (reward > 0)
                    rewardExperience(reward);
            }
        }
    }
}
