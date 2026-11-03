#include "falloutscripts.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <components/debug/debuglog.hpp>
#include <components/misc/strings/algorithm.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"

namespace OFScript
{
    namespace
    {
        namespace AV = FalloutScript::ActorValue;

        /// The variable of a script by its name, in any case; -1 for a script that has none of that name or no script
        /// at all (the accessor of the executable, FalloutNV.exe 0x8C1740, answers -1 for a variable it does not find)
        double namedVariable(FalloutScript::Instance* instance, std::string_view name)
        {
            if (instance == nullptr)
                return -1.0;
            const std::vector<FalloutScript::Variable>& variables = instance->script().variables();
            for (std::size_t i = 1; i < variables.size(); ++i)
                if (Misc::StringUtils::ciEqual(variables[i].mName, name))
                    return instance->variable(static_cast<std::uint32_t>(i));
            return -1.0;
        }

        /// Writes the variable (0x8C16C0); false when the script has none of that name
        bool setNamedVariable(FalloutScript::Instance* instance, std::string_view name, double value)
        {
            if (instance == nullptr)
                return false;
            const std::vector<FalloutScript::Variable>& variables = instance->script().variables();
            for (std::size_t i = 1; i < variables.size(); ++i)
                if (Misc::StringUtils::ciEqual(variables[i].mName, name))
                    return instance->setVariable(static_cast<std::uint32_t>(i), value);
            return false;
        }

        /// The stimpak of the games, the one the wheel gives (an aid item of this editor id)
        constexpr std::string_view stimpakId = "Stimpak";
    }

    bool FalloutScripts::isCompanion(const OFWorld::Ptr& actor)
    {
        // Fallout 3 has no wheel: its companions are talked to
        if (mGame != FalloutScript::Game::NewVegas)
            return false;
        const FalloutScript::FormId reference = referenceOf(actor);
        if (reference == 0 || reference == mPlayer)
            return false;
        return mObjectState.teammate(reference) && !mActors.isDead(reference);
    }

    bool FalloutScripts::openCompanionWheel(const OFWorld::Ptr& actor)
    {
        const FalloutScript::FormId reference = referenceOf(actor);
        if (reference == 0 || reference == mPlayer)
            return false;
        CompanionWheel wheel;
        wheel.mActor = reference;
        wheel.mName = std::string(actor.getClass().getName(actor));
        // The wheel reads four variables of the companion's script when it opens and keeps nothing of its own: what the
        // commands set is what the scripts of the topics wrote (FalloutNV.exe 0x754DE0)
        FalloutScript::Instance* instance = mObjects.instance(reference);
        wheel.mAggressive = namedVariable(instance, "FollowerSwitchAggressive") == 1.0;
        wheel.mStaysFar = namedVariable(instance, "IsFollowingLong") == 1.0;
        wheel.mRanged = namedVariable(instance, "CombatStyleRanged") == 1.0;
        const double waiting = namedVariable(instance, "Waiting");
        wheel.mFollowing = waiting == 0.0;
        wheel.mWaiting = waiting != 0.0;
        if (OFBase::InputManager* input = OFBase::Environment::get().getInputManager())
            wheel.mPad = input->joystickLastUsed();
        {
            const std::lock_guard lock(mCompanionMutex);
            wheel.mSerial = ++mCompanionSerial;
            mCompanion = wheel;
        }
        Log(Debug::Info) << "Companion wheel: opened for " << std::hex << reference << std::dec
                         << " aggressive=" << wheel.mAggressive << " far=" << wheel.mStaysFar
                         << " ranged=" << wheel.mRanged << " following=" << wheel.mFollowing
                         << " script=" << (instance != nullptr);
        return true;
    }

    std::optional<FalloutScripts::CompanionWheel> FalloutScripts::companionWheel()
    {
        const std::lock_guard lock(mCompanionMutex);
        return mCompanion;
    }

    FalloutScripts::CompanionStatus FalloutScripts::companionStatus()
    {
        CompanionStatus status;
        FalloutScript::FormId companion = 0;
        {
            const std::lock_guard lock(mCompanionMutex);
            if (!mCompanion)
                return status;
            companion = mCompanion->mActor;
        }
        std::vector<std::pair<FalloutScript::FormId, int>> held;
        if (mActors.items(mPlayer, held))
            for (const auto& [item, count] : held)
                if (Misc::StringUtils::ciEqual(editorId(item), stimpakId))
                    status.mStimpaks += count;
        status.mHealth = mActorValues.current(companion, AV::health);
        status.mMaxHealth = mActorValues.permanent(companion, AV::health);
        status.mCapacity = mActorValues.permanent(companion, AV::carryWeight);
        std::vector<std::pair<FalloutScript::FormId, int>> carried;
        if (mActors.items(companion, carried))
        {
            for (const auto& [item, count] : carried)
            {
                FalloutScript::ItemInfo info;
                if (itemInfo(item, info))
                    status.mCarried += info.mWeight * count;
            }
        }
        if (const FalloutScript::FormId weapon = mEquipment.weapon(companion); weapon != 0)
            status.mWeapon = itemName(weapon);
        return status;
    }

    bool FalloutScripts::companionSay(std::string_view topic)
    {
        FalloutScript::FormId companion = 0;
        {
            const std::lock_guard lock(mCompanionMutex);
            if (!mCompanion)
                return false;
            companion = mCompanion->mActor;
        }
        FalloutScript::FormId found = 0;
        {
            const std::lock_guard lock(mDialogueMutex);
            found = mDialogue.findTopic(std::string(topic));
        }
        if (found == 0)
        {
            Log(Debug::Info) << "Companion wheel: the topic " << topic << " is not in the files";
            return false;
        }
        // The companion answers the command with the lines of the topic; the results of the response (the scripts
        // that set its variables) run when the response is over
        return sayTopic(companion, found, 0, false, false);
    }

    void FalloutScripts::companionSetAggressive(bool aggressive)
    {
        FalloutScript::FormId companion = 0;
        {
            const std::lock_guard lock(mCompanionMutex);
            if (!mCompanion)
                return;
            companion = mCompanion->mActor;
        }
        runOnMainThread([this, companion, aggressive]() {
            setNamedVariable(mObjects.instance(companion), "FollowerSwitchAggressive", aggressive ? 1.0 : 0.0);
        });
    }

    bool FalloutScripts::companionStimpak()
    {
        FalloutScript::FormId companion = 0;
        {
            const std::lock_guard lock(mCompanionMutex);
            if (!mCompanion)
                return false;
            companion = mCompanion->mActor;
        }
        FalloutScript::FormId stimpak = 0;
        std::vector<std::pair<FalloutScript::FormId, int>> held;
        if (mActors.items(mPlayer, held))
        {
            for (const auto& [item, count] : held)
            {
                if (count > 0 && Misc::StringUtils::ciEqual(editorId(item), stimpakId))
                {
                    stimpak = item;
                    break;
                }
            }
        }
        if (stimpak == 0)
            return false;
        // The stimpak is the player's and is used on the companion: it goes to its things and is used there
        if (transferItems(mPlayer, companion, stimpak, 1) <= 0)
            return false;
        const FalloutScript::ItemUse::Result result = useItem(companion, stimpak);
        Log(Debug::Info) << "Companion wheel: a stimpak for " << std::hex << companion << std::dec << ", outcome "
                         << static_cast<int>(result.mOutcome);
        if (result.mOutcome != FalloutScript::ItemUse::Outcome::Consumed)
        {
            // Nothing was used up: the player keeps the stimpak
            transferItems(companion, mPlayer, stimpak, 1);
            return false;
        }
        return true;
    }

    void FalloutScripts::companionTalk()
    {
        FalloutScript::FormId companion = 0;
        {
            const std::lock_guard lock(mCompanionMutex);
            if (!mCompanion)
                return;
            companion = mCompanion->mActor;
            mCompanion.reset();
        }
        runOnMainThread([this, companion]() {
            const OFWorld::Ptr ptr = findReference(companion);
            if (!ptr.isEmpty())
                talkTo(ptr);
        });
    }

    void FalloutScripts::closeCompanionWheel()
    {
        const std::lock_guard lock(mCompanionMutex);
        mCompanion.reset();
    }
}
