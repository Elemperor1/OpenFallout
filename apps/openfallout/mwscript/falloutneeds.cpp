#include "falloutscripts.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <components/debug/debuglog.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadspel.hpp>
#include <components/esm4/loadwrld.hpp>
#include <components/falloutscript/actorvalues.hpp>
#include <components/falloutscript/hardcore.hpp>
#include <components/falloutscript/restrictions.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/cell.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/datetimemanager.hpp"
#include "../mwworld/esmstore.hpp"

// The needs of the hardcore mode, and the one question that decides if the player may wait, sleep or travel: the host
// side of components/falloutscript/hardcore.* and restrictions.* (the rules are there, with the addresses of the
// executables they were read from; this gathers the facts they ask for)
namespace OFScript
{
    namespace
    {
        namespace Restrictions = FalloutScript::Restrictions;
        namespace AV = FalloutScript::ActorValue;

        /// The fraction of its height that has to be under the water for the player to count as under it (0x885520, the
        /// constant at 0x1084D20)
        constexpr float submergedShare = 0.875f;
        /// The conditions of the six parts the row of the display counts: head, torso, both arms, both legs
        constexpr int firstPartCondition = 25;
        constexpr int lastPartCondition = 30;
        /// The actor value of the radiation level
        constexpr int radiationLevel = 54;
        /// The actor values of the legs (left, right)
        constexpr int leftLegCondition = 29;
        constexpr int rightLegCondition = 30;
    }

    // ---- The hardcore mode ----

    void FalloutScripts::setHardcoreMode(bool on)
    {
        // The settings screen and the command are one thing (FalloutNV.exe 0x7D1400 and 0x5DE920 both end in 0x969E90): a
        // mode that is turned off takes the needs down to nothing
        if (!isNewVegas())
            return;
        if (mHardcoreMode.set(on ? 1 : 0, false))
            zeroNeeds();
    }

    void FalloutScripts::zeroNeeds()
    {
        mHardcore.reset(mPlayer);
    }

    void FalloutScripts::needNotice(const FalloutScript::HardcoreNeeds::Notice& notice)
    {
        // What the player is told when a need changes its stage (the callbacks 0x8C5610, 0x8C5890 and 0x8C5B10: the word of
        // the change, then the name of the stage or the word for being well)
        Log(Debug::Info) << "Hardcore: need " << notice.mNeed << (notice.mIncreased ? " went up" : " went down") << ", stage spell "
                         << std::hex << "0x" << notice.mStage << std::dec;
        for (const FalloutScript::HardcoreNeeds::NoticeLine& line : FalloutScript::HardcoreNeeds::noticeLines(notice))
        {
            std::string text = gameSettingText(line.mSetting);
            if (text.empty())
                continue;
            if (line.mStageName)
            {
                const ESM4::Spell* spell = mStore.get<ESM4::Spell>().search(ESM::FormId::fromUint32(notice.mStage));
                text += " " + (spell != nullptr ? spell->mFullName : std::string());
            }
            tellPlayer(text, line.mFace, 2.f);
        }
    }

    bool FalloutScripts::essentialByMode(FalloutScript::FormId actor)
    {
        // A companion cannot die unless the hardcore mode is on (FalloutNV.exe 0x87F3D0)
        return actor != 0 && actor != mPlayer
            && FalloutScript::companionIsEssential(mObjectState.teammate(actor), hardcoreMode());
    }

    FalloutScripts::NeedsStatus FalloutScripts::needsStatus()
    {
        NeedsStatus status;
        status.mHardcore = hardcoreMode();
        std::array<double, 5> levels{};
        for (int need = 0; need < FalloutScript::HardcoreNeeds::needCount; ++need)
            levels[need] = mActorValues.current(mPlayer, FalloutScript::HardcoreNeeds::actorValueOf(need));
        levels[3] = mActorValues.current(mPlayer, radiationLevel);
        int crippled = 0;
        for (int part = firstPartCondition; part <= lastPartCondition; ++part)
            if (mActorValues.current(mPlayer, part) <= 0)
                ++crippled;
        status.mCrippledParts = crippled;
        for (int need = 0; need < 3; ++need)
        {
            status.mLevels[need] = levels[need];
            double most = 0;
            for (const FalloutScript::HardcoreNeeds::Stage& stage : hardcoreStages(need))
                most = std::max<double>(most, stage.mThreshold);
            if (most > 0)
                status.mMost[need] = most;
        }

        FalloutScript::NeedsDisplay::Thresholds thresholds;
        thresholds.mHalfAlpha = gameSetting("fHCHalfAlphaTextThreshold", 200);
        thresholds.mFullAlpha = gameSetting("fHCFullAlphaTextThreshold", 400);
        thresholds.mRed = gameSetting("fHCRedTextThreshold", 600);
        thresholds.mFlash = gameSetting("fHCFlashTextThreshold", 800);
        const double now = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch())
                               .count();
        const std::lock_guard lock(mNeedsDisplayMutex);
        mNeedsDisplay.setThresholds(thresholds);
        mNeedsDisplay.advance(now);
        // The three needs show only in the hardcore mode (their tiles are made clear otherwise); the radiation and the
        // limbs always do (0x7739F0)
        for (int tile = 0; tile < 3; ++tile)
            status.mTiles[tile] = status.mHardcore ? mNeedsDisplay.tile(levels[tile]) : FalloutScript::NeedsDisplay::Tile{ 1, 0 };
        status.mTiles[3] = mNeedsDisplay.tile(levels[3]);
        status.mTiles[4] = mNeedsDisplay.tile(mNeedsDisplay.limbLevel(crippled));
        return status;
    }

    // ---- May the player rest or travel? ----

    bool FalloutScripts::playerInRadiation()
    {
        // The executable keeps two flags for it (the radiation record of the manager, 0x5098E0); here the player is in
        // radiation while an effect that lasts is adding to its rads
        for (const FalloutScript::ItemUse::Active& active : mItemUse.active(mPlayer))
            if (active.mActorValue == radiationLevel && !active.mTemporary && active.mMagnitude < 0 && active.mRemaining > 0)
                return true;
        return false;
    }

    bool FalloutScripts::playerTrespassing()
    {
        // The flag of the player that the ownership rules set (0x8D1E70) is not kept by this engine: the work of the
        // crime rules, which call this when they have it
        return false;
    }

    bool FalloutScripts::playerAlarmSounding()
    {
        // The alarm of the crime rules (0x953C80: a process of kind 0x15 that is after the player)
        return false;
    }

    Restrictions::Facts FalloutScripts::restrictionFacts(Restrictions::Action action, FalloutScript::FormId bed)
    {
        Restrictions::Facts facts;
        OFBase::World& world = *OFBase::Environment::get().getWorld();
        const OFWorld::Ptr player = world.getPlayerPtr();
        facts.mHostilesNear = hostilesNearPlayer();
        facts.mTakingHealthDamage = playerTakingHealthDamage();
        facts.mTrespassing = playerTrespassing();
        facts.mAlarmSounding = playerAlarmSounding();
        facts.mAirborne = playerInAir();
        facts.mInRadiation = playerInRadiation();
        if (!player.isEmpty() && player.isInCell())
            facts.mUnderwater = world.isUnderwater(player, submergedShare);

        const bool outside = inOpenWorld(mPlayer);
        const FalloutScript::FormId cell = parentCell(mPlayer);
        if (action != Restrictions::Action::FastTravel)
        {
            // A place can forbid waiting by a flag of its record: the cell of an interior, the worldspace outside
            if (outside)
            {
                const ESM4::World* space
                    = mStore.get<ESM4::World>().search(ESM::FormId::fromUint32(worldspaceOf(mPlayer)));
                facts.mNoWaitPlace = space != nullptr && (space->mFlags & ESM4::Rec_CanNotWait) != 0;
            }
            else if (cell != 0)
            {
                const ESM4::Cell* record = mStore.get<ESM4::Cell>().search(ESM::FormId::fromUint32(cell));
                facts.mNoWaitPlace = record != nullptr && (record->mFlags & ESM4::Rec_CanNotWait) != 0;
            }
            // The second switch of EnableFastTravel is the control of waiting that New Vegas reads (0x944360)
            facts.mWaitSwitchedOff = isNewVegas() && !mMapMarkers.restAllowed();
            if (action == Restrictions::Action::Sleep && bed != 0)
            {
                // What a script made of the bed counts, so the owner is not the one the record names
                const FalloutScript::FormId owner = mObjectState.owner(bed);
                facts.mOwnedBed = owner != 0 && owner != playerBase();
            }
        }
        else
        {
            facts.mScriptBlocked = !mMapMarkers.fastTravelAllowed();
            const PipboyStats stats = pipboyStats();
            facts.mOverencumbered = stats.mWeight > stats.mMaxWeight;
            facts.mTravelWhileOverencumbered
                = isNewVegas() && mPerks.apply(FalloutScript::EntryPoint::travelOverencumbered, mPlayer, 0.0) >= 1.0;
            facts.mSpent = mActorValues.current(mPlayer, AV::fatigue) < 1 || mActorValues.current(mPlayer, AV::health) < 1;
            // The place the records forbid to travel from (0x544520): an interior unless the flag "can travel from here" of
            // its cell is set (the flag the plugin code names CELL_NoTravel); outside, a cell that has the flag, or else a
            // worldspace that has the flag that forbids it
            if (outside)
            {
                const ESM4::Cell* record = cell != 0 ? mStore.get<ESM4::Cell>().search(ESM::FormId::fromUint32(cell)) : nullptr;
                const ESM4::World* space
                    = mStore.get<ESM4::World>().search(ESM::FormId::fromUint32(worldspaceOf(mPlayer)));
                facts.mTravelForbidden = (record != nullptr && (record->mCellFlags & ESM4::CELL_NoTravel) != 0)
                    || (space != nullptr && (space->mWorldFlags & ESM4::World::WLD_NoFastTravel) != 0);
            }
            else if (cell != 0)
            {
                const ESM4::Cell* record = mStore.get<ESM4::Cell>().search(ESM::FormId::fromUint32(cell));
                facts.mTravelForbidden = record != nullptr && (record->mCellFlags & ESM4::CELL_NoTravel) == 0;
            }
        }
        return facts;
    }

    double FalloutScripts::travelSpeed()
    {
        int legs = 0;
        if (mActorValues.current(mPlayer, leftLegCondition) <= 0)
            ++legs;
        if (mActorValues.current(mPlayer, rightLegCondition) <= 0)
            ++legs;
        return Restrictions::travelSpeed(mActorValues.current(mPlayer, AV::speedMult), legs,
            gameSetting("fMoveBaseSpeed", 77), gameSetting("fMoveRunMult", 4),
            gameSetting("fMoveOneCrippledLegSpeedMult", 0.85), gameSetting("fMoveTwoCrippledLegsSpeedMult", 0.75));
    }

    // ---- The commands of the menu of the rest ----

    void FalloutScripts::showSleepWaitMenu(bool sleeping, bool checked)
    {
        // With the check the answer is the one of the key that waits: its refusals, and the menu of waiting
        // (FalloutNV.exe 0x969FA0); without it the menu opens as it was asked for (0x7054F0)
        if (checked)
            requestRest(false, 0);
        else
            openRestMenu(sleeping, 0);
    }

    double FalloutScripts::playerSleepHours()
    {
        const std::lock_guard lock(mInteractionMutex);
        if (!mRest.mActive)
            return 0.0;
        return std::max(0.0, mRest.mTotal - mRest.mDone);
    }
}
