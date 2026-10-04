#include "mechanicsmanagerimp.hpp"

#include <cassert>

#include <osg/Stats>

#include <components/misc/rng.hpp>

#include <components/esm/records.hpp>
#include <components/esm/refid.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/stolenitems.hpp>

#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwsound/constants.hpp"

#include "actor.hpp"
#include "actors.hpp"
#include "actorutil.hpp"
#include "aicombat.hpp"
#include "aipursue.hpp"
#include "autocalcspell.hpp"
#include "combat.hpp"
#include "npcstats.hpp"
#include "spellutil.hpp"

namespace
{

    void getPersuasionRatings(
        const OFMechanics::NpcStats& stats, float& rating1, float& rating2, float& rating3, bool player)
    {
        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        float persTerm = stats.getAttribute(ESM::Attribute::Personality).getModified()
            / gmst.find("fPersonalityMod")->mValue.getFloat();
        float luckTerm
            = stats.getAttribute(ESM::Attribute::Luck).getModified() / gmst.find("fLuckMod")->mValue.getFloat();
        float repTerm = stats.getReputation() * gmst.find("fReputationMod")->mValue.getFloat();
        float fatigueTerm = stats.getFatigueTerm();
        float levelTerm = stats.getLevel() * gmst.find("fLevelMod")->mValue.getFloat();

        rating1 = (repTerm + luckTerm + persTerm + stats.getSkill(ESM::Skill::Speechcraft).getModified()) * fatigueTerm;

        if (player)
        {
            rating2 = rating1 + levelTerm;
            rating3 = (stats.getSkill(ESM::Skill::Mercantile).getModified() + luckTerm + persTerm) * fatigueTerm;
        }
        else
        {
            rating2
                = (levelTerm + repTerm + luckTerm + persTerm + stats.getSkill(ESM::Skill::Speechcraft).getModified())
                * fatigueTerm;
            rating3
                = (stats.getSkill(ESM::Skill::Mercantile).getModified() + repTerm + luckTerm + persTerm) * fatigueTerm;
        }
    }

    bool isOwned(const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, OFWorld::Ptr& victim)
    {
        const OFWorld::CellRef& cellref = target.getCellRef();

        const ESM::RefId& owner = cellref.getOwner();
        bool isOwned = !owner.empty() && owner != ESM::RefId::stringRefId("Player");

        const ESM::RefId& faction = cellref.getFaction();
        bool isFactionOwned = false;
        if (!faction.empty() && ptr.getClass().isNpc())
        {
            const std::map<ESM::RefId, int>& factions = ptr.getClass().getNpcStats(ptr).getFactionRanks();
            auto found = factions.find(faction);
            if (found == factions.end() || found->second < cellref.getFactionRank())
                isFactionOwned = true;
        }

        const std::string& globalVariable = cellref.getGlobalVariable();
        if (!globalVariable.empty() && OFBase::Environment::get().getWorld()->getGlobalInt(globalVariable))
        {
            isOwned = false;
            isFactionOwned = false;
        }

        if (!cellref.getOwner().empty())
            victim = OFBase::Environment::get().getWorld()->searchPtr(cellref.getOwner(), true, false);

        return isOwned || isFactionOwned;
    }
}

namespace OFMechanics
{
    void MechanicsManager::buildPlayer()
    {
        OFWorld::Ptr ptr = getPlayer();

        OFMechanics::CreatureStats& creatureStats = ptr.getClass().getCreatureStats(ptr);
        OFMechanics::NpcStats& npcStats = ptr.getClass().getNpcStats(ptr);

        npcStats.recalculateMagicka();

        const ESM::NPC* player = ptr.get<ESM::NPC>()->mBase;

        // reset
        creatureStats.setLevel(player->mNpdt.mLevel);
        creatureStats.getSpells().clear(true);
        creatureStats.getActiveSpells().clear(ptr);

        for (const auto& [skill, value] : player->mNpdt.mSkills)
            npcStats.getSkill(skill).setBase(value);

        for (const auto& [attribute, value] : player->mNpdt.mAttributes)
            npcStats.setAttribute(attribute, value);

        const OFWorld::ESMStore& esmStore = *OFBase::Environment::get().getESMStore();

        // race
        if (mRaceSelected)
        {
            const ESM::Race* race = esmStore.get<ESM::Race>().find(player->mRace);

            bool male = (player->mFlags & ESM::NPC::Female) == 0;

            for (const ESM::Attribute& attribute : esmStore.get<ESM::Attribute>())
                creatureStats.setAttribute(
                    attribute.mId, static_cast<float>(race->mData.getAttribute(attribute.mId, male)));

            for (const ESM::Skill& skill : esmStore.get<ESM::Skill>())
            {
                int bonus = 0;
                auto bonusIt = std::find_if(race->mData.mBonus.begin(), race->mData.mBonus.end(),
                    [&](const auto& v) { return v.mSkill == skill.mId; });
                if (bonusIt != race->mData.mBonus.end())
                    bonus = bonusIt->mBonus;

                npcStats.getSkill(skill.mId).setBase(5.f + bonus);
            }

            for (const ESM::RefId& power : race->mPowers.mList)
            {
                if (const ESM::Spell* spell = esmStore.get<ESM::Spell>().search(power))
                    creatureStats.getSpells().add(spell);
            }
        }

        // birthsign
        const ESM::RefId& signId = OFBase::Environment::get().getWorld()->getPlayer().getBirthSign();

        if (!signId.empty())
        {
            const ESM::BirthSign* sign = esmStore.get<ESM::BirthSign>().find(signId);

            for (const ESM::RefId& power : sign->mPowers.mList)
            {
                if (const ESM::Spell* spell = esmStore.get<ESM::Spell>().search(power))
                    creatureStats.getSpells().add(spell);
            }
        }

        // class
        if (mClassSelected)
        {
            const ESM::Class* playerClass = esmStore.get<ESM::Class>().find(player->mClass);

            for (const ESM::RefId& id : playerClass->mData.mAttribute)
            {
                if (!id.empty())
                    creatureStats.setAttribute(id, creatureStats.getAttribute(id).getBase() + 10);
            }

            for (const auto& id : playerClass->mData.mMinorSkills)
            {
                if (!id.empty())
                    npcStats.getSkill(id).setBase(npcStats.getSkill(id).getBase() + 10);
            }
            for (const auto& id : playerClass->mData.mMajorSkills)
            {
                if (!id.empty())
                    npcStats.getSkill(id).setBase(npcStats.getSkill(id).getBase() + 25);
            }

            for (const ESM::Skill& skill : esmStore.get<ESM::Skill>())
            {
                if (skill.mData.mSpecialization == playerClass->mData.mSpecialization)
                    npcStats.getSkill(skill.mId).setBase(npcStats.getSkill(skill.mId).getBase() + 5);
            }
        }

        // F_PCStart spells
        const ESM::Race* race = nullptr;
        if (mRaceSelected)
            race = esmStore.get<ESM::Race>().find(player->mRace);

        npcStats.updateHealth();

        std::vector<ESM::RefId> selectedSpells
            = autoCalcPlayerSpells(npcStats.getSkills(), npcStats.getAttributes(), race);

        for (const ESM::RefId& spell : selectedSpells)
            creatureStats.getSpells().add(spell);

        // forced update and current value adjustments
        mActors.updateActor(ptr, 0);

        for (int i = 0; i < 3; ++i)
        {
            DynamicStat<float> stat = creatureStats.getDynamic(i);
            stat.setCurrent(stat.getModified());
            creatureStats.setDynamic(i, stat);
        }

        // auto-equip again. we need this for when the race is changed to a beast race and shoes are no longer
        // equippable
        OFWorld::InventoryStore& invStore = ptr.getClass().getInventoryStore(ptr);
        for (int i = 0; i < OFWorld::InventoryStore::Slots; ++i)
            invStore.unequipAll();
        invStore.autoEquip();
    }

    MechanicsManager::MechanicsManager()
        : mUpdatePlayer(true)
        , mClassSelected(false)
        , mRaceSelected(false)
        , mAI(true)
    {
        // buildPlayer no longer here, needs to be done explicitly after all subsystems are up and running
    }

    void MechanicsManager::add(const OFWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            mActors.addActor(ptr);
        else
            mObjects.addObject(ptr);
    }

    void MechanicsManager::castSpell(const OFWorld::Ptr& ptr, const ESM::RefId& spellId, bool scriptedSpell)
    {
        if (ptr.getClass().isActor())
            mActors.castSpell(ptr, spellId, scriptedSpell);
    }

    void MechanicsManager::remove(const OFWorld::Ptr& ptr, bool keepActive)
    {
        if (ptr == OFBase::Environment::get().getWindowManager()->getWatchedActor())
            OFBase::Environment::get().getWindowManager()->watchActor(OFWorld::Ptr());
        mActors.removeActor(ptr, keepActive);
        mObjects.removeObject(ptr);
    }

    void MechanicsManager::updateCell(const OFWorld::Ptr& old, const OFWorld::Ptr& ptr)
    {
        if (old == OFBase::Environment::get().getWindowManager()->getWatchedActor())
            OFBase::Environment::get().getWindowManager()->watchActor(ptr);

        if (ptr.getClass().isActor())
            mActors.updateActor(old, ptr);
        else
            mObjects.updateObject(old, ptr);
    }

    void MechanicsManager::drop(const OFWorld::CellStore* cellStore)
    {
        mActors.dropActors(cellStore, getPlayer());
        mObjects.dropObjects(cellStore);
    }

    void MechanicsManager::update(float duration, bool paused)
    {
        // Note: we should do it here since game mechanics and world updates use these values
        OFWorld::Ptr ptr = getPlayer();
        OFBase::WindowManager* winMgr = OFBase::Environment::get().getWindowManager();

        OFWorld::InventoryStore& inv = ptr.getClass().getInventoryStore(ptr);
        OFWorld::ContainerStoreIterator weapon = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);

        // Update the selected spell icon
        OFWorld::ContainerStoreIterator enchantItem = inv.getSelectedEnchantItem();
        if (enchantItem != inv.end())
            winMgr->setSelectedEnchantItem(*enchantItem);
        else
        {
            const ESM::RefId& spell = winMgr->getSelectedSpell();
            if (!spell.empty())
                winMgr->setSelectedSpell(spell, int(OFMechanics::getSpellSuccessChance(spell, ptr)));
            else
                winMgr->unsetSelectedSpell();
        }

        // Update the equipped weapon icon
        if (weapon == inv.end())
            winMgr->unsetSelectedWeapon();
        else
            winMgr->setSelectedWeapon(*weapon);

        if (mUpdatePlayer)
        {
            mUpdatePlayer = false;

            // HACK? The player has been changed, so a new Animation object may
            // have been made for them. Make sure they're properly updated.
            mActors.removeActor(ptr, true);
            mActors.addActor(ptr, true);
        }

        mActors.update(duration, paused);
        mObjects.update(duration, paused);
    }

    void MechanicsManager::processChangedSettings(const Settings::CategorySettingVector& changed)
    {
        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "Game" && it->second == "actors processing range")
            {
                int state = OFBase::Environment::get().getStateManager()->getState();
                if (state != OFBase::StateManager::State_Running)
                    continue;

                // Update mechanics for new processing range immediately
                update(0.f, false);
            }
        }
    }

    void MechanicsManager::notifyDied(const OFWorld::Ptr& actor)
    {
        mActors.notifyDied(actor);
    }

    bool MechanicsManager::isActorDetected(const OFWorld::Ptr& actor, const OFWorld::Ptr& observer)
    {
        return mActors.isActorDetected(actor, observer);
    }

    bool MechanicsManager::isAttackPreparing(const OFWorld::Ptr& ptr)
    {
        return mActors.isAttackPreparing(ptr);
    }

    bool MechanicsManager::isRunning(const OFWorld::Ptr& ptr)
    {
        CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);
        if (!stats.getStance(OFMechanics::CreatureStats::Stance_Run))
            return false;

        if (mActors.isRunning(ptr))
            return true;

        OFBase::World* world = OFBase::Environment::get().getWorld();
        return !world->isOnGround(ptr) && !world->isSwimming(ptr) && !world->isFlying(ptr);
    }

    bool MechanicsManager::isSneaking(const OFWorld::Ptr& ptr)
    {
        CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);
        if (!stats.getStance(OFMechanics::CreatureStats::Stance_Sneak))
            return false;

        if (mActors.isSneaking(ptr))
            return true;

        OFBase::World* world = OFBase::Environment::get().getWorld();
        return !world->isOnGround(ptr) && !world->isSwimming(ptr) && !world->isFlying(ptr);
    }

    void MechanicsManager::rest(double hours, bool sleep)
    {
        if (sleep)
            OFBase::Environment::get().getWorld()->rest(hours);

        mActors.rest(hours, sleep);
    }

    void MechanicsManager::restoreDynamicStats(const OFWorld::Ptr& actor, double hours, bool sleep)
    {
        mActors.restoreDynamicStats(actor, hours, sleep);
    }

    int MechanicsManager::getHoursToRest() const
    {
        return mActors.getHoursToRest(getPlayer());
    }

    void MechanicsManager::setPlayerName(const std::string& name)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();

        ESM::NPC player = *world->getPlayerPtr().get<ESM::NPC>()->mBase;
        player.mName = name;

        world->getStore().insert(player);

        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerRace(
        const ESM::RefId& race, bool male, const ESM::RefId& head, const ESM::RefId& hair)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();

        ESM::NPC player = *world->getPlayerPtr().get<ESM::NPC>()->mBase;

        if (player.mRace != race || player.mHead != head || player.mHair != hair || player.isMale() != male)
        {
            player.mRace = race;
            player.mHead = head;
            player.mHair = hair;
            player.setIsMale(male);

            world->getStore().insert(player);
            world->renderPlayer();
        }

        mRaceSelected = true;
        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerBirthsign(const ESM::RefId& id)
    {
        OFBase::Environment::get().getWorld()->getPlayer().setBirthSign(id);
        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerClass(const ESM::RefId& id)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();

        ESM::NPC player = *world->getPlayerPtr().get<ESM::NPC>()->mBase;
        player.mClass = id;

        world->getStore().insert(player);

        mClassSelected = true;
        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerClass(const ESM::Class& value)
    {
        OFBase::World* world = OFBase::Environment::get().getWorld();

        const ESM::Class* ptr = world->getStore().insert(value);

        ESM::NPC player = *world->getPlayerPtr().get<ESM::NPC>()->mBase;
        player.mClass = ptr->mId;

        world->getStore().insert(player);

        mClassSelected = true;
        buildPlayer();
        mUpdatePlayer = true;
    }

    int MechanicsManager::getDerivedDisposition(const OFWorld::Ptr& ptr, bool clamp)
    {
        const OFMechanics::NpcStats& npcStats = ptr.getClass().getNpcStats(ptr);
        float x = static_cast<float>(npcStats.getBaseDisposition() + npcStats.getCrimeDispositionModifier());

        OFWorld::LiveCellRef<ESM::NPC>* npc = ptr.get<ESM::NPC>();
        OFWorld::Ptr playerPtr = getPlayer();
        OFWorld::LiveCellRef<ESM::NPC>* player = playerPtr.get<ESM::NPC>();
        const OFMechanics::NpcStats& playerStats = playerPtr.getClass().getNpcStats(playerPtr);

        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        static const float fDispRaceMod = gmst.find("fDispRaceMod")->mValue.getFloat();
        if (npc->mBase->mRace == player->mBase->mRace)
            x += fDispRaceMod;

        static const float fDispPersonalityMult = gmst.find("fDispPersonalityMult")->mValue.getFloat();
        static const float fDispPersonalityBase = gmst.find("fDispPersonalityBase")->mValue.getFloat();
        x += fDispPersonalityMult
            * (playerStats.getAttribute(ESM::Attribute::Personality).getModified() - fDispPersonalityBase);

        float reaction = 0;
        int rank = 0;
        const ESM::RefId& npcFaction = ptr.getClass().getPrimaryFaction(ptr);

        if (playerStats.getFactionRanks().find(npcFaction) != playerStats.getFactionRanks().end())
        {
            if (!playerStats.getExpelled(npcFaction))
            {
                // faction reaction towards itself. yes, that exists
                reaction = static_cast<float>(
                    OFBase::Environment::get().getDialogueManager()->getFactionReaction(npcFaction, npcFaction));

                rank = playerStats.getFactionRanks().find(npcFaction)->second;
            }
        }
        else if (!npcFaction.empty())
        {
            auto playerFactionIt = playerStats.getFactionRanks().begin();
            for (; playerFactionIt != playerStats.getFactionRanks().end(); ++playerFactionIt)
            {
                const ESM::RefId& itFaction = playerFactionIt->first;

                // Ignore the faction, if a player was expelled from it.
                if (playerStats.getExpelled(itFaction))
                    continue;

                int itReaction
                    = OFBase::Environment::get().getDialogueManager()->getFactionReaction(npcFaction, itFaction);
                if (playerFactionIt == playerStats.getFactionRanks().begin() || itReaction < reaction)
                {
                    reaction = static_cast<float>(itReaction);
                    rank = playerFactionIt->second;
                }
            }
        }
        else
        {
            reaction = 0;
            rank = 0;
        }

        static const float fDispFactionRankMult = gmst.find("fDispFactionRankMult")->mValue.getFloat();
        static const float fDispFactionRankBase = gmst.find("fDispFactionRankBase")->mValue.getFloat();
        static const float fDispFactionMod = gmst.find("fDispFactionMod")->mValue.getFloat();
        x += (fDispFactionRankMult * rank + fDispFactionRankBase) * fDispFactionMod * reaction;

        static const float fDispCrimeMod = gmst.find("fDispCrimeMod")->mValue.getFloat();
        static const float fDispDiseaseMod = gmst.find("fDispDiseaseMod")->mValue.getFloat();
        x -= fDispCrimeMod * playerStats.getBounty();
        if (playerStats.hasCommonDisease() || playerStats.hasBlightDisease())
            x += fDispDiseaseMod;

        static const float fDispWeaponDrawn = gmst.find("fDispWeaponDrawn")->mValue.getFloat();
        if (playerStats.getDrawState() == OFMechanics::DrawState::Weapon)
            x += fDispWeaponDrawn;

        x += ptr.getClass()
                 .getCreatureStats(ptr)
                 .getMagicEffects()
                 .getOrDefault(ESM::MagicEffect::Charm)
                 .getMagnitude();

        if (clamp)
            return std::clamp(static_cast<int>(x), 0, 100); //, normally clamped to [0..100] when used
        return static_cast<int>(x);
    }

    int MechanicsManager::getBarterOffer(const OFWorld::Ptr& ptr, int basePrice, bool buying)
    {
        // Make sure zero base price items/services can't be bought/sold for 1 gold
        // and return the intended base price for creature merchants
        if (basePrice == 0 || ptr.getType() == ESM::Creature::sRecordId)
            return basePrice;

        const OFMechanics::NpcStats& sellerStats = ptr.getClass().getNpcStats(ptr);

        OFWorld::Ptr playerPtr = getPlayer();
        const OFMechanics::NpcStats& playerStats = playerPtr.getClass().getNpcStats(playerPtr);

        // I suppose the temporary disposition change (second param to getDerivedDisposition()) _has_ to be considered
        // here, otherwise one would get different prices when exiting and re-entering the dialogue window...
        int clampedDisposition = getDerivedDisposition(ptr);
        float a = std::min(playerPtr.getClass().getSkill(playerPtr, ESM::Skill::Mercantile), 100.f);
        float b = std::min(0.1f * playerStats.getAttribute(ESM::Attribute::Luck).getModified(), 10.f);
        float c = std::min(0.2f * playerStats.getAttribute(ESM::Attribute::Personality).getModified(), 10.f);
        float d = std::min(ptr.getClass().getSkill(ptr, ESM::Skill::Mercantile), 100.f);
        float e = std::min(0.1f * sellerStats.getAttribute(ESM::Attribute::Luck).getModified(), 10.f);
        float f = std::min(0.2f * sellerStats.getAttribute(ESM::Attribute::Personality).getModified(), 10.f);
        float pcTerm = (clampedDisposition - 50 + a + b + c) * playerStats.getFatigueTerm();
        float npcTerm = (d + e + f) * sellerStats.getFatigueTerm();
        float buyTerm = 0.01f * (100 - 0.5f * (pcTerm - npcTerm));
        float sellTerm = 0.01f * (50 - 0.5f * (npcTerm - pcTerm));
        int offerPrice = int(basePrice * (buying ? buyTerm : sellTerm));
        return std::max(1, offerPrice);
    }

    int MechanicsManager::countDeaths(const ESM::RefId& id) const
    {
        return mActors.countDeaths(id);
    }

    void MechanicsManager::getPersuasionDispositionChange(
        const OFWorld::Ptr& npc, PersuasionType type, bool& success, int& tempChange, int& permChange)
    {
        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        OFMechanics::NpcStats& npcStats = npc.getClass().getNpcStats(npc);

        OFWorld::Ptr playerPtr = getPlayer();
        const OFMechanics::NpcStats& playerStats = playerPtr.getClass().getNpcStats(playerPtr);

        float npcRating1, npcRating2, npcRating3;
        getPersuasionRatings(npcStats, npcRating1, npcRating2, npcRating3, false);

        float playerRating1, playerRating2, playerRating3;
        getPersuasionRatings(playerStats, playerRating1, playerRating2, playerRating3, true);

        const int currentDisposition = getDerivedDisposition(npc);

        float d = 1 - 0.02f * abs(currentDisposition - 50);
        float target1 = d * (playerRating1 - npcRating1 + 50);
        float target2 = d * (playerRating2 - npcRating2 + 50);

        float bribeMod;
        if (type == PT_Bribe10)
            bribeMod = gmst.find("fBribe10Mod")->mValue.getFloat();
        else if (type == PT_Bribe100)
            bribeMod = gmst.find("fBribe100Mod")->mValue.getFloat();
        else
            bribeMod = gmst.find("fBribe1000Mod")->mValue.getFloat();

        float target3 = d * (playerRating3 - npcRating3 + 50) + bribeMod;

        const float iPerMinChance = gmst.find("iPerMinChance")->mValue.getFloat();
        const float iPerMinChange = gmst.find("iPerMinChange")->mValue.getFloat();
        const float fPerDieRollMult = gmst.find("fPerDieRollMult")->mValue.getFloat();
        const float fPerTempMult = gmst.find("fPerTempMult")->mValue.getFloat();

        float x = 0;
        float y = 0;

        auto& prng = OFBase::Environment::get().getWorld()->getPrng();
        int roll = Misc::Rng::roll0to99(prng);

        if (type == PT_Admire)
        {
            target1 = std::max(iPerMinChance, target1);
            success = (roll <= target1);
            float c = floor(fPerDieRollMult * (target1 - roll));
            x = success ? std::max(iPerMinChange, c) : c;
        }
        else if (type == PT_Intimidate)
        {
            target2 = std::max(iPerMinChance, target2);

            success = (roll <= target2);

            float r;
            if (roll != target2)
                r = floor(target2 - roll);
            else
                r = 1;

            if (roll <= target2)
            {
                float s = floor(r * fPerDieRollMult * fPerTempMult);

                const int flee = npcStats.getAiSetting(OFMechanics::AiSetting::Flee).getBase();
                const int fight = npcStats.getAiSetting(OFMechanics::AiSetting::Fight).getBase();
                npcStats.setAiSetting(
                    OFMechanics::AiSetting::Flee, std::clamp(flee + int(std::max(iPerMinChange, s)), 0, 100));
                npcStats.setAiSetting(
                    OFMechanics::AiSetting::Fight, std::clamp(fight + int(std::min(-iPerMinChange, -s)), 0, 100));
            }

            float c = -std::abs(floor(r * fPerDieRollMult));
            if (success)
            {
                if (std::abs(c) < iPerMinChange)
                {
                    // Deviating from Morrowind here: it doesn't increase disposition on marginal wins,
                    // which seems to be a bug (MCP fixes it too).
                    // Original logic: x = 0, y = -iPerMinChange
                    x = iPerMinChange;
                    y = x; // This goes unused.
                }
                else
                {
                    x = -floor(c * fPerTempMult);
                    y = c;
                }
            }
            else
            {
                x = floor(c * fPerTempMult);
                y = c;
            }
        }
        else if (type == PT_Taunt)
        {
            target1 = std::max(iPerMinChance, target1);
            success = (roll <= target1);

            float c = std::abs(floor(target1 - roll));

            if (success)
            {
                float s = c * fPerDieRollMult * fPerTempMult;
                const int flee = npcStats.getAiSetting(AiSetting::Flee).getBase();
                const int fight = npcStats.getAiSetting(AiSetting::Fight).getBase();
                npcStats.setAiSetting(
                    AiSetting::Flee, std::clamp(flee + std::min(-int(iPerMinChange), int(-s)), 0, 100));
                npcStats.setAiSetting(
                    AiSetting::Fight, std::clamp(fight + std::max(int(iPerMinChange), int(s)), 0, 100));
            }
            x = floor(-c * fPerDieRollMult);

            if (success && std::abs(x) < iPerMinChange)
                x = -iPerMinChange;
        }
        else // Bribe
        {
            target3 = std::max(iPerMinChance, target3);
            success = (roll <= target3);
            float c = floor((target3 - roll) * fPerDieRollMult);

            x = success ? std::max(iPerMinChange, c) : c;
        }

        if (type == PT_Intimidate)
        {
            tempChange = int(x);
            if (currentDisposition + tempChange > 100)
                tempChange = 100 - currentDisposition;
            else if (currentDisposition + tempChange < 0)
                tempChange = -currentDisposition;
            permChange = success ? -int(tempChange / fPerTempMult) : int(y);
        }
        else
        {
            tempChange = int(x * fPerTempMult);
            if (currentDisposition + tempChange > 100)
                tempChange = 100 - currentDisposition;
            else if (currentDisposition + tempChange < 0)
                tempChange = -currentDisposition;
            permChange = int(tempChange / fPerTempMult);
        }
    }

    void MechanicsManager::forceStateUpdate(const OFWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            mActors.forceStateUpdate(ptr);
    }

    bool MechanicsManager::playAnimationGroup(
        const OFWorld::Ptr& ptr, std::string_view groupName, int mode, uint32_t number, bool scripted)
    {
        if (ptr.getClass().isActor())
            return mActors.playAnimationGroup(ptr, groupName, mode, number, scripted);
        else
            return mObjects.playAnimationGroup(ptr, groupName, mode, number, scripted);
    }
    bool MechanicsManager::playAnimationGroupLua(const OFWorld::Ptr& ptr, std::string_view groupName, uint32_t loops,
        float speed, std::string_view startKey, std::string_view stopKey, bool forceLoop)
    {
        if (ptr.getClass().isActor())
            return mActors.playAnimationGroupLua(ptr, groupName, loops, speed, startKey, stopKey, forceLoop);
        else
            return mObjects.playAnimationGroupLua(ptr, groupName, loops, speed, startKey, stopKey, forceLoop);
    }
    void MechanicsManager::enableLuaAnimations(const OFWorld::Ptr& ptr, bool enable)
    {
        if (ptr.getClass().isActor())
            mActors.enableLuaAnimations(ptr, enable);
        else
            mObjects.enableLuaAnimations(ptr, enable);
    }
    void MechanicsManager::skipAnimation(const OFWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            mActors.skipAnimation(ptr);
        else
            mObjects.skipAnimation(ptr);
    }

    bool MechanicsManager::checkAnimationPlaying(const OFWorld::Ptr& ptr, std::string_view groupName)
    {
        if (ptr.getClass().isActor())
            return mActors.checkAnimationPlaying(ptr, groupName);
        else
            return false;
    }

    bool MechanicsManager::checkScriptedAnimationPlaying(const OFWorld::Ptr& ptr) const
    {
        if (ptr.getClass().isActor())
            return mActors.checkScriptedAnimationPlaying(ptr);

        return false;
    }

    bool MechanicsManager::onOpen(const OFWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            return true;
        else
            return mObjects.onOpen(ptr);
    }

    void MechanicsManager::onClose(const OFWorld::Ptr& ptr)
    {
        if (!ptr.getClass().isActor())
            mObjects.onClose(ptr);
    }

    void MechanicsManager::persistAnimationStates()
    {
        mActors.persistAnimationStates();
        mObjects.persistAnimationStates();
    }

    void MechanicsManager::clearAnimationQueue(const OFWorld::Ptr& ptr, bool clearScripted)
    {
        if (ptr.getClass().isActor())
            mActors.clearAnimationQueue(ptr, clearScripted);
        else
            mObjects.clearAnimationQueue(ptr, clearScripted);
    }

    void MechanicsManager::updateMagicEffects(const OFWorld::Ptr& ptr)
    {
        mActors.updateMagicEffects(ptr);
    }

    bool MechanicsManager::toggleAI()
    {
        mAI = !mAI;
        return mAI;
    }

    bool MechanicsManager::isAIActive()
    {
        return mAI;
    }

    void MechanicsManager::playerLoaded()
    {
        mUpdatePlayer = true;
        mClassSelected = true;
        mRaceSelected = true;
    }

    namespace
    {
        std::set<ESM::RefId> makeBoundItemIdCache()
        {
            std::set<ESM::RefId> boundItemIDCache;

            // Build a list of known bound item ID's
            const OFWorld::Store<ESM::GameSetting>& gameSettings
                = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

            for (const ESM::GameSetting& currentSetting : gameSettings)
            {
                // Don't bother checking this GMST if it's not a sMagicBound* one.
                if (!currentSetting.mId.startsWith("smagicbound"))
                    continue;

                // All sMagicBound* GMST's should be of type string
                std::string currentGMSTValue = currentSetting.mValue.getString();
                Misc::StringUtils::lowerCaseInPlace(currentGMSTValue);

                boundItemIDCache.insert(ESM::RefId::stringRefId(currentGMSTValue));
            }

            return boundItemIDCache;
        }
    }

    bool MechanicsManager::isBoundItem(const OFWorld::Ptr& item)
    {
        static const std::set<ESM::RefId> boundItemIdCache = makeBoundItemIdCache();

        return boundItemIdCache.find(item.getCellRef().getRefId()) != boundItemIdCache.end();
    }

    bool MechanicsManager::isAllowedToUse(const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, OFWorld::Ptr& victim)
    {
        if (target.isEmpty() || target == ptr)
            return true;

        const OFWorld::CellRef& cellref = target.getCellRef();
        // there is no harm to use unlocked doors
        if (target.getClass().isDoor() && !cellref.isLocked() && cellref.getTrap().empty())
        {
            return true;
        }

        if (!target.getClass().hasToolTip(target))
            return true;

        // TODO: implement a better check to check if target is owned bed
        if (target.getClass().isActivator() && !target.getClass().getScript(target).startsWith("Bed"))
            return true;

        if (target.getClass().isNpc())
        {
            if (target.getClass().getCreatureStats(target).isDead())
                return true;

            if (target.getClass().getCreatureStats(target).getAiSequence().isInCombat())
                return true;

            // check if a player tries to pickpocket a target NPC
            if (target.getClass().getCreatureStats(target).getKnockedDown() || isSneaking(ptr))
                return false;

            return true;
        }

        if (isOwned(ptr, target, victim))
            return false;

        // A special case for evidence chest - we should not allow to take items even if it is technically permitted
        return !(cellref.getRefId() == "stolen_goods");
    }

    bool MechanicsManager::sleepInBed(const OFWorld::Ptr& ptr, const OFWorld::Ptr& bed)
    {
        if (ptr.getClass().getNpcStats(ptr).isWerewolf())
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sWerewolfRefusal}");
            return true;
        }

        if (OFBase::Environment::get().getWorld()->getPlayer().enemiesNearby())
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage2}");
            return true;
        }

        OFWorld::Ptr victim;
        if (isAllowedToUse(ptr, bed, victim))
            return false;

        if (commitCrime(ptr, victim, OT_SleepingInOwnedBed, bed.getCellRef().getFaction()))
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage64}");
            return true;
        }
        else
            return false;
    }

    void MechanicsManager::unlockAttempted(const OFWorld::Ptr& ptr, const OFWorld::Ptr& item)
    {
        OFWorld::Ptr victim;
        if (isOwned(ptr, item, victim))
        {
            // Note that attempting to unlock something that has ever been locked is a crime even if it's already
            // unlocked. Likewise, it's illegal to unlock something that has a trap but isn't otherwise locked.
            const auto& cellref = item.getCellRef();
            if (cellref.getLockLevel() || cellref.isLocked() || !cellref.getTrap().empty())
                commitCrime(ptr, victim, OT_Trespassing, item.getCellRef().getFaction());
        }
    }

    std::vector<std::pair<ESM::RefId, int>> MechanicsManager::getStolenItemOwners(const ESM::RefId& itemid)
    {
        std::vector<std::pair<ESM::RefId, int>> result;
        StolenItemsMap::const_iterator it = mStolenItems.find(itemid);
        if (it == mStolenItems.end())
            return result;
        else
        {
            const OwnerMap& owners = it->second;
            for (OwnerMap::const_iterator ownerIt = owners.begin(); ownerIt != owners.end(); ++ownerIt)
                result.emplace_back(ownerIt->first.first, ownerIt->second);
            return result;
        }
    }

    bool MechanicsManager::isItemStolenFrom(const ESM::RefId& itemid, const OFWorld::Ptr& ptr)
    {
        StolenItemsMap::const_iterator it = mStolenItems.find(itemid);
        if (it == mStolenItems.end())
            return false;

        const OwnerMap& owners = it->second;
        const ESM::RefId& ownerid = ptr.getCellRef().getRefId();
        OwnerMap::const_iterator ownerFound = owners.find(std::make_pair(ownerid, false));
        if (ownerFound != owners.end())
            return true;

        const ESM::RefId& factionid = ptr.getClass().getPrimaryFaction(ptr);
        if (!factionid.empty())
        {
            OwnerMap::const_iterator factionOwnerFound = owners.find(std::make_pair(factionid, true));
            return factionOwnerFound != owners.end();
        }

        return false;
    }

    void MechanicsManager::confiscateStolenItemToOwner(
        const OFWorld::Ptr& player, const OFWorld::Ptr& item, const OFWorld::Ptr& victim, int count)
    {
        if (player != getPlayer())
            return;

        const ESM::RefId& itemId = item.getCellRef().getRefId();

        StolenItemsMap::iterator stolenIt = mStolenItems.find(itemId);
        if (stolenIt == mStolenItems.end())
            return;

        Owner owner;
        owner.first = victim.getCellRef().getRefId();
        owner.second = false;

        const ESM::RefId& victimFaction = victim.getClass().getPrimaryFaction(victim);
        if (!victimFaction.empty() && item.getCellRef().getFaction() == victimFaction) // Is the item faction-owned?
        {
            owner.first = victimFaction;
            owner.second = true;
        }

        // decrease count of stolen items
        int toRemove = std::min(count, mStolenItems[itemId][owner]);
        mStolenItems[itemId][owner] -= toRemove;
        if (mStolenItems[itemId][owner] == 0)
        {
            // erase owner from stolen items owners
            OwnerMap& owners = stolenIt->second;
            OwnerMap::iterator ownersIt = owners.find(owner);
            if (ownersIt != owners.end())
                owners.erase(ownersIt);
        }

        OFWorld::ContainerStore& store = player.getClass().getContainerStore(player);

        // move items from player to owner and report about theft
        victim.getClass().getContainerStore(victim).add(item, toRemove);
        store.remove(item, toRemove);
        commitCrime(
            player, victim, OT_Theft, item.getCellRef().getFaction(), item.getClass().getValue(item) * toRemove);
    }

    void MechanicsManager::confiscateStolenItems(const OFWorld::Ptr& player, const OFWorld::Ptr& targetContainer)
    {
        OFWorld::ContainerStore& store = player.getClass().getContainerStore(player);
        OFWorld::ContainerStore& containerStore = targetContainer.getClass().getContainerStore(targetContainer);
        for (OFWorld::ContainerStoreIterator it = store.begin(); it != store.end(); ++it)
        {
            StolenItemsMap::iterator stolenIt = mStolenItems.find(it->getCellRef().getRefId());
            if (stolenIt == mStolenItems.end())
                continue;
            OwnerMap& owners = stolenIt->second;
            int itemCount = it->getCellRef().getCount();
            for (OwnerMap::iterator ownerIt = owners.begin(); ownerIt != owners.end();)
            {
                int toRemove = std::min(itemCount, ownerIt->second);
                itemCount -= toRemove;
                ownerIt->second -= toRemove;
                if (ownerIt->second == 0)
                    owners.erase(ownerIt++);
                else
                    ++ownerIt;
            }

            int toMove = it->getCellRef().getCount() - itemCount;

            containerStore.add(*it, toMove);
            store.remove(*it, toMove);
        }
        // TODO: unhardcode the locklevel
        targetContainer.getCellRef().lock(50);
    }

    void MechanicsManager::itemTaken(
        const OFWorld::Ptr& ptr, const OFWorld::Ptr& item, const OFWorld::Ptr& container, int count, bool alarm)
    {
        if (ptr != getPlayer())
            return;

        OFWorld::Ptr victim;

        bool isAllowed = true;
        const OFWorld::CellRef* ownerCellRef = &item.getCellRef();
        if (!container.isEmpty())
        {
            // Inherit the owner of the container
            ownerCellRef = &container.getCellRef();
            isAllowed = isAllowedToUse(ptr, container, victim);
        }
        else
        {
            isAllowed = isAllowedToUse(ptr, item, victim);
        }

        if (isAllowed)
            return;

        Owner owner;
        owner.second = false;
        if (!container.isEmpty() && container.getClass().isActor())
        {
            // "container" is an actor inventory, so just take actor's ID
            owner.first = ownerCellRef->getRefId();
        }
        else
        {
            owner.first = ownerCellRef->getOwner();
            if (owner.first.empty())
            {
                owner.first = ownerCellRef->getFaction();
                owner.second = true;
            }
        }

        const bool isGold = item.getClass().isGold(item);
        if (!isGold)
        {
            if (victim.isEmpty()
                || (victim.getClass().isActor() && victim.getCellRef().getCount() > 0
                    && !victim.getClass().getCreatureStats(victim).isDead()))
                mStolenItems[item.getCellRef().getRefId()][owner] += count;
        }
        if (alarm)
        {
            int value = count;
            if (!isGold)
                value *= item.getClass().getValue(item);
            commitCrime(ptr, victim, OT_Theft, ownerCellRef->getFaction(), value);
        }
    }

    bool MechanicsManager::commitCrime(const OFWorld::Ptr& player, const OFWorld::Ptr& victim, OffenseType type,
        const ESM::RefId& factionId, int arg, bool victimAware)
    {
        // NOTE: victim may be empty

        // Only player can commit crime
        if (player != getPlayer())
            return false;

        if (type == OT_Assault)
            victimAware = true;

        // Find all the actors within the alarm radius
        std::vector<OFWorld::Ptr> neighbors;

        osg::Vec3f from(player.getRefData().getPosition().asVec3());
        const OFWorld::ESMStore& esmStore = *OFBase::Environment::get().getESMStore();
        float radius = esmStore.get<ESM::GameSetting>().find("fAlarmRadius")->mValue.getFloat();

        mActors.getObjectsInRange(from, radius, neighbors);

        // victim should be considered even beyond alarm radius
        if (!victim.isEmpty() && (from - victim.getRefData().getPosition().asVec3()).length2() > radius * radius)
            neighbors.push_back(victim);

        // get the player's followers / allies (works recursively) that will not report crimes
        std::set<OFWorld::Ptr> playerFollowers;
        getActorsSidingWith(player, playerFollowers);

        // Did anyone see it?
        bool crimeSeen = false;
        for (const OFWorld::Ptr& neighbor : neighbors)
        {
            if (!canReportCrime(neighbor, victim, playerFollowers))
                continue;

            if ((neighbor == victim && victimAware)
                // Murder crime can be reported even if no one saw it (hearing is enough, I guess).
                // TODO: Add mod support for stealth executions!
                || (type == OT_Murder && neighbor != victim)
                || (OFBase::Environment::get().getWorld()->getLOS(player, neighbor)
                    && awarenessCheck(player, neighbor)))
            {
                // NPC will complain about theft even if he will do nothing about it
                if (type == OT_Theft || type == OT_Pickpocket)
                    OFBase::Environment::get().getDialogueManager()->say(neighbor, ESM::RefId::stringRefId("thief"));

                crimeSeen = true;
            }
        }

        if (crimeSeen)
            reportCrime(player, victim, type, factionId, arg);
        else if (type == OT_Assault && !victim.isEmpty())
        {
            bool reported = false;
            if (victim.getClass().isClass(victim, "guard")
                && !victim.getClass().getCreatureStats(victim).getAiSequence().hasPackage(AiPackageTypeId::Pursue))
                reported = reportCrime(player, victim, type, ESM::RefId(), arg);

            // TODO: combat should be started with an "unaware" flag, which makes the victim flee?
            if (!reported)
                startCombat(victim, player, &playerFollowers);
        }
        return crimeSeen;
    }

    bool MechanicsManager::canReportCrime(
        const OFWorld::Ptr& actor, const OFWorld::Ptr& victim, std::set<OFWorld::Ptr>& playerFollowers)
    {
        if (actor == getPlayer() || !actor.getClass().isNpc() || actor.getClass().getCreatureStats(actor).isDead())
            return false;

        if (actor.getClass().getCreatureStats(actor).getAiSequence().isInCombat(victim))
            return false;

        // Unconsious actor can not report about crime and should not become hostile
        if (actor.getClass().getCreatureStats(actor).getKnockedDown())
            return false;

        // Player's followers should not attack player, or try to arrest him
        if (actor.getClass().getCreatureStats(actor).getAiSequence().hasPackage(AiPackageTypeId::Follow))
        {
            if (playerFollowers.find(actor) != playerFollowers.end())
                return false;
        }

        return true;
    }

    bool MechanicsManager::reportCrime(
        const OFWorld::Ptr& player, const OFWorld::Ptr& victim, OffenseType type, const ESM::RefId& factionId, int arg)
    {
        const OFWorld::Store<ESM::GameSetting>& store
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        if (type == OT_Murder && !victim.isEmpty())
            victim.getClass().getCreatureStats(victim).notifyMurder();

        // Bounty and disposition penalty for each type of crime
        int bounty;
        float disp, dispVictim;
        if (type == OT_Trespassing || type == OT_SleepingInOwnedBed)
        {
            bounty = store.find("iCrimeTresspass")->mValue.getInteger();
            disp = dispVictim = store.find("iDispTresspass")->mValue.getFloat();
        }
        else if (type == OT_Pickpocket)
        {
            bounty = store.find("iCrimePickPocket")->mValue.getInteger();
            disp = dispVictim = store.find("fDispPickPocketMod")->mValue.getFloat();
        }
        else if (type == OT_Assault)
        {
            bounty = store.find("iCrimeAttack")->mValue.getInteger();
            disp = store.find("iDispAttackMod")->mValue.getFloat();
            dispVictim = store.find("fDispAttacking")->mValue.getFloat();
        }
        else if (type == OT_Murder)
        {
            bounty = store.find("iCrimeKilling")->mValue.getInteger();
            disp = dispVictim = store.find("iDispKilling")->mValue.getFloat();
        }
        else if (type == OT_Theft)
        {
            bounty = static_cast<int>(arg * store.find("fCrimeStealing")->mValue.getFloat());
            bounty = std::max(1, bounty); // Minimum bounty of 1, in case items with zero value are stolen
            disp = dispVictim = store.find("fDispStealing")->mValue.getFloat() * arg;
        }
        else
        {
            bounty = arg;
            disp = dispVictim = 0.f;
        }

        // Make surrounding actors within alarm distance respond to the crime
        std::vector<OFWorld::Ptr> neighbors;

        const OFWorld::ESMStore& esmStore = *OFBase::Environment::get().getESMStore();

        osg::Vec3f from(player.getRefData().getPosition().asVec3());
        float radius = esmStore.get<ESM::GameSetting>().find("fAlarmRadius")->mValue.getFloat();

        mActors.getObjectsInRange(from, radius, neighbors);

        // victim should be considered even beyond alarm radius
        if (!victim.isEmpty() && (from - victim.getRefData().getPosition().asVec3()).length2() > radius * radius)
            neighbors.push_back(victim);

        int id = OFBase::Environment::get().getWorld()->getPlayer().getNewCrimeId();

        // What amount of provocation did this crime generate?
        // Controls whether witnesses will engage combat with the criminal.
        int fight = 0, fightVictim = 0;
        if (type == OT_Trespassing || type == OT_SleepingInOwnedBed)
            fight = fightVictim = esmStore.get<ESM::GameSetting>().find("iFightTrespass")->mValue.getInteger();
        else if (type == OT_Pickpocket)
        {
            fight = esmStore.get<ESM::GameSetting>().find("iFightPickpocket")->mValue.getInteger();
            fightVictim = esmStore.get<ESM::GameSetting>().find("iFightPickpocket")->mValue.getInteger()
                * 4; // *4 according to research wiki
        }
        else if (type == OT_Assault)
        {
            fight = esmStore.get<ESM::GameSetting>().find("iFightAttacking")->mValue.getInteger();
            fightVictim = esmStore.get<ESM::GameSetting>().find("iFightAttack")->mValue.getInteger();
        }
        else if (type == OT_Murder)
            fight = fightVictim = esmStore.get<ESM::GameSetting>().find("iFightKilling")->mValue.getInteger();
        else if (type == OT_Theft)
            fight = fightVictim = esmStore.get<ESM::GameSetting>().find("fFightStealing")->mValue.getInteger() * arg;

        bool reported = false;

        std::set<OFWorld::Ptr> playerFollowers;
        getActorsSidingWith(player, playerFollowers);

        // Tell everyone (including the original reporter) in alarm range
        for (const OFWorld::Ptr& actor : neighbors)
        {
            if (!canReportCrime(actor, victim, playerFollowers))
                continue;

            // Will the witness report the crime?
            if (actor.getClass().getCreatureStats(actor).getAiSetting(AiSetting::Alarm).getBase() >= 100)
            {
                reported = true;

                if (type == OT_Trespassing)
                    OFBase::Environment::get().getDialogueManager()->say(actor, ESM::RefId::stringRefId("intruder"));
            }
        }

        for (const OFWorld::Ptr& actor : neighbors)
        {
            if (!canReportCrime(actor, victim, playerFollowers))
                continue;

            NpcStats& observerStats = actor.getClass().getNpcStats(actor);

            int alarm = observerStats.getAiSetting(AiSetting::Alarm).getBase();
            float alarmTerm = 0.01f * alarm;

            bool isActorVictim = actor == victim;
            float dispTerm = isActorVictim ? dispVictim : disp;

            bool isActorGuard = actor.getClass().isClass(actor, "guard");

            int currentDisposition = getDerivedDisposition(actor);

            bool isPermanent = false;
            bool applyOnlyIfHostile = false;
            int dispositionModifier = 0;
            // Murdering and trespassing seem to do not affect disposition
            if (type == OT_Theft)
            {
                dispositionModifier = static_cast<int>(dispTerm * alarmTerm);
            }
            else if (type == OT_Pickpocket)
            {
                if (alarm >= 100 && isActorGuard)
                    dispositionModifier = static_cast<int>(dispTerm);
                else if (isActorVictim && isActorGuard)
                {
                    isPermanent = true;
                    dispositionModifier = static_cast<int>(dispTerm * alarmTerm);
                }
                else if (isActorVictim)
                {
                    isPermanent = true;
                    dispositionModifier = static_cast<int>(dispTerm);
                }
            }
            else if (type == OT_Assault)
            {
                if (isActorVictim && !isActorGuard)
                {
                    isPermanent = true;
                    dispositionModifier = static_cast<int>(dispTerm);
                }
                else if (alarm >= 100)
                    dispositionModifier = static_cast<int>(dispTerm);
                else if (isActorVictim && isActorGuard)
                {
                    isPermanent = true;
                    dispositionModifier = static_cast<int>(dispTerm * alarmTerm);
                }
                else
                {
                    applyOnlyIfHostile = true;
                    dispositionModifier = static_cast<int>(dispTerm * alarmTerm);
                }
            }

            bool setCrimeId = false;
            if (isPermanent && dispositionModifier != 0 && !applyOnlyIfHostile)
            {
                setCrimeId = true;
                dispositionModifier = std::clamp(dispositionModifier, -currentDisposition, 100 - currentDisposition);
                int baseDisposition = observerStats.getBaseDisposition();
                observerStats.setBaseDisposition(baseDisposition + dispositionModifier);
            }
            else if (dispositionModifier != 0 && !applyOnlyIfHostile)
            {
                setCrimeId = true;
                dispositionModifier = std::clamp(dispositionModifier, -currentDisposition, 100 - currentDisposition);
                observerStats.modCrimeDispositionModifier(dispositionModifier);
            }

            if (isActorGuard && alarm >= 100)
            {
                // Mark as Alarmed for dialogue
                observerStats.setAlarmed(true);

                setCrimeId = true;

                if (!observerStats.getAiSequence().isInPursuit())
                {
                    observerStats.getAiSequence().stack(AiPursue(player), actor);
                }
            }
            else
            {
                // If Alarm is 0, treat it like 100 to calculate a Fight modifier for a victim of pickpocketing.
                // Observers which do not try to arrest player do not care about pickpocketing at all.
                if (type == OT_Pickpocket && isActorVictim && alarmTerm == 0.0)
                    alarmTerm = 1.0;
                else if (type == OT_Pickpocket && !isActorVictim)
                    alarmTerm = 0.0;

                float fightTerm = static_cast<float>(isActorVictim ? fightVictim : fight);
                fightTerm += getFightDispositionBias(dispTerm);
                fightTerm += getFightDistanceBias(actor, player);
                fightTerm *= alarmTerm;

                const int observerFightRating = observerStats.getAiSetting(AiSetting::Fight).getBase();
                if (observerFightRating + fightTerm > 100)
                    fightTerm = static_cast<float>(100 - observerFightRating);
                fightTerm = std::max(0.f, fightTerm);

                if (observerFightRating + fightTerm >= 100)
                {
                    if (dispositionModifier != 0 && applyOnlyIfHostile)
                    {
                        dispositionModifier
                            = std::clamp(dispositionModifier, -currentDisposition, 100 - currentDisposition);
                        observerStats.modCrimeDispositionModifier(dispositionModifier);
                    }

                    startCombat(actor, player, &playerFollowers);
                    observerStats.setHitAttemptActor(player.getCellRef().getRefNum());

                    // Apply aggression value to the base Fight rating, so that the actor can continue fighting
                    // after a Calm spell wears off
                    observerStats.setAiSetting(AiSetting::Fight, observerFightRating + static_cast<int>(fightTerm));

                    setCrimeId = true;

                    // Mark as Alarmed for dialogue
                    observerStats.setAlarmed(true);
                }
            }

            // Set the crime ID, which we will use to calm down participants
            // once the bounty has been paid and restore their disposition to player character.
            if (setCrimeId)
                observerStats.setCrimeId(id);
        }

        if (reported)
        {
            player.getClass().getNpcStats(player).setBounty(
                std::max(0, player.getClass().getNpcStats(player).getBounty() + bounty));

            // If committing a crime against a faction member, expell from the faction
            if (!victim.isEmpty() && victim.getClass().isNpc())
            {
                const ESM::RefId& factionID = victim.getClass().getPrimaryFaction(victim);

                const std::map<ESM::RefId, int>& playerRanks = player.getClass().getNpcStats(player).getFactionRanks();
                if (playerRanks.find(factionID) != playerRanks.end())
                {
                    player.getClass().getNpcStats(player).expell(factionID, true);
                }
            }
            else if (!factionId.empty())
            {
                const std::map<ESM::RefId, int>& playerRanks = player.getClass().getNpcStats(player).getFactionRanks();
                if (playerRanks.find(factionId) != playerRanks.end())
                {
                    player.getClass().getNpcStats(player).expell(factionId, true);
                }
            }

            if (type == OT_Assault && !victim.isEmpty()
                && !victim.getClass().getCreatureStats(victim).getAiSequence().isInCombat(player)
                && victim.getClass().isNpc())
            {
                // Attacker is in combat with us, but we are not in combat with the attacker yet. Time to fight back.
                // Note: accidental or collateral damage attacks are ignored.
                if (!victim.getClass().getCreatureStats(victim).getAiSequence().isInPursuit())
                    startCombat(victim, player, &playerFollowers);

                // Set the crime ID, which we will use to calm down participants
                // once the bounty has been paid.
                victim.getClass().getNpcStats(victim).setCrimeId(id);
            }
        }

        return reported;
    }

    bool MechanicsManager::actorAttacked(const OFWorld::Ptr& target, const OFWorld::Ptr& attacker)
    {
        const OFWorld::Ptr& player = getPlayer();
        if (target == player || !attacker.getClass().isActor())
            return false;

        OFMechanics::CreatureStats& statsTarget = target.getClass().getCreatureStats(target);
        AiSequence& seq = statsTarget.getAiSequence();
        const bool inCombat = seq.isInCombat(attacker);

        if (canCommitCrimeAgainst(target, attacker))
            commitCrime(attacker, target, OFBase::MechanicsManager::OT_Assault);

        if (!attacker.isEmpty()
            && (attacker.getClass().getCreatureStats(attacker).getAiSequence().isInCombat(target) || attacker == player)
            && !inCombat)
        {
            // Attacker is in combat with us, but we are not in combat with the attacker yet. Time to fight back.
            // Note: accidental or collateral damage attacks are ignored.
            if (!target.getClass().getCreatureStats(target).getAiSequence().isInPursuit())
            {
                // If an actor has OnPCHitMe declared in his script, his Fight = 0 and the attacker is player,
                // he will attack the player only if we will force him (e.g. via StartCombat console command)
                bool peaceful = false;
                const ESM::RefId& script = target.getClass().getScript(target);
                if (!script.empty() && target.getRefData().getLocals().hasVar(script, "onpchitme")
                    && attacker == player)
                {
                    const int fight
                        = target.getClass().getCreatureStats(target).getAiSetting(AiSetting::Fight).getModified();
                    peaceful = (fight == 0);
                }

                if (!peaceful)
                {
                    SidingCache cachedAllies{ mActors, false };
                    const std::set<OFWorld::Ptr>& attackerAllies = cachedAllies.getActorsSidingWith(attacker);
                    startCombat(target, attacker, &attackerAllies);
                    // Force friendly actors into combat to prevent infighting between followers
                    for (const auto& follower : cachedAllies.getActorsSidingWith(target))
                    {
                        if (follower != attacker && follower != player)
                            startCombat(follower, attacker, &attackerAllies);
                    }
                }
            }
        }

        return true;
    }

    bool MechanicsManager::canCommitCrimeAgainst(const OFWorld::Ptr& target, const OFWorld::Ptr& attacker)
    {
        const OFWorld::Class& cls = target.getClass();
        const OFMechanics::CreatureStats& stats = cls.getCreatureStats(target);
        const OFMechanics::AiSequence& seq = stats.getAiSequence();
        return cls.isNpc() && !attacker.isEmpty() && !isAggressive(target, attacker) && !seq.isEngagedWithActor()
            && !stats.getAiSequence().isInPursuit() && !cls.getNpcStats(target).isWerewolf()
            && stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Vampirism).getMagnitude() <= 0;
    }

    void MechanicsManager::actorKilled(const OFWorld::Ptr& victim, const OFWorld::Ptr& attacker)
    {
        if (attacker.isEmpty() || victim.isEmpty())
            return;

        if (victim == attacker)
            return; // known to happen

        if (!victim.getClass().isNpc())
            return; // TODO: implement animal rights

        const OFMechanics::NpcStats& victimStats = victim.getClass().getNpcStats(victim);
        const OFWorld::Ptr& player = getPlayer();
        bool canCommit = attacker == player && canCommitCrimeAgainst(victim, attacker);

        // For now we report only about crimes of player and player's followers
        if (attacker != player)
        {
            std::set<OFWorld::Ptr> playerFollowers;
            getActorsSidingWith(player, playerFollowers);
            if (playerFollowers.find(attacker) == playerFollowers.end())
                return;
        }

        if (!canCommit && victimStats.getCrimeId() == -1)
            return;

        // Simple check for who attacked first: if the player attacked first, a crimeId should be set
        // Doesn't handle possible edge case where no one reported the assault, but in such a case,
        // for bystanders it is not possible to tell who attacked first, anyway.
        commitCrime(player, victim, OFBase::MechanicsManager::OT_Murder);
    }

    bool MechanicsManager::awarenessCheck(const OFWorld::Ptr& ptr, const OFWorld::Ptr& observer, bool useCache)
    {
        if (observer.getClass().getCreatureStats(observer).isDead() || !observer.getRefData().isEnabled())
            return false;

        const OFWorld::Store<ESM::GameSetting>& store
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);

        float sneakTerm = 0;
        if (isSneaking(ptr))
        {
            static float fSneakSkillMult = store.find("fSneakSkillMult")->mValue.getFloat();
            static float fSneakBootMult = store.find("fSneakBootMult")->mValue.getFloat();
            float sneak = static_cast<float>(ptr.getClass().getSkill(ptr, ESM::Skill::Sneak));
            float agility = stats.getAttribute(ESM::Attribute::Agility).getModified();
            float luck = stats.getAttribute(ESM::Attribute::Luck).getModified();
            float bootWeight = 0;
            if (ptr.getClass().isNpc() && OFBase::Environment::get().getWorld()->isOnGround(ptr))
            {
                const OFWorld::InventoryStore& inv = ptr.getClass().getInventoryStore(ptr);
                OFWorld::ConstContainerStoreIterator it = inv.getSlot(OFWorld::InventoryStore::Slot_Boots);
                if (it != inv.end())
                    bootWeight = it->getClass().getWeight(*it);
            }
            sneakTerm = fSneakSkillMult * sneak + 0.2f * agility + 0.1f * luck + bootWeight * fSneakBootMult;
        }

        static float fSneakDistBase = store.find("fSneakDistanceBase")->mValue.getFloat();
        static float fSneakDistMult = store.find("fSneakDistanceMultiplier")->mValue.getFloat();

        osg::Vec3f pos1(ptr.getRefData().getPosition().asVec3());
        osg::Vec3f pos2(observer.getRefData().getPosition().asVec3());
        float distTerm = fSneakDistBase + fSneakDistMult * (pos1 - pos2).length();

        float chameleon = stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Chameleon).getMagnitude();
        float invisibility = stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Invisibility).getMagnitude();
        float x = sneakTerm * distTerm * stats.getFatigueTerm() + chameleon;
        if (invisibility > 0.f)
            x += 100.f;

        CreatureStats& observerStats = observer.getClass().getCreatureStats(observer);
        float obsAgility = observerStats.getAttribute(ESM::Attribute::Agility).getModified();
        float obsLuck = observerStats.getAttribute(ESM::Attribute::Luck).getModified();
        float obsBlind = observerStats.getMagicEffects().getOrDefault(ESM::MagicEffect::Blind).getMagnitude();
        float obsSneak = observer.getClass().getSkill(observer, ESM::Skill::Sneak);

        float obsTerm = obsSneak + 0.2f * obsAgility + 0.1f * obsLuck - obsBlind;

        // is ptr behind the observer?
        static float fSneakNoViewMult = store.find("fSneakNoViewMult")->mValue.getFloat();
        static float fSneakViewMult = store.find("fSneakViewMult")->mValue.getFloat();
        float y = 0;
        osg::Vec3f vec = pos1 - pos2;
        if (observer.getRefData().getBaseNode())
        {
            osg::Vec3f observerDir = (observer.getRefData().getBaseNode()->getAttitude() * osg::Vec3f(0, 1, 0));

            float angleRadians = std::acos(observerDir * vec / (observerDir.length() * vec.length()));
            if (angleRadians > osg::DegreesToRadians(90.f))
                y = obsTerm * observerStats.getFatigueTerm() * fSneakNoViewMult;
            else
                y = obsTerm * observerStats.getFatigueTerm() * fSneakViewMult;
        }

        float target = x - y;
        if (useCache)
            return observerStats.getAwarenessRoll() >= target;
        auto& prng = OFBase::Environment::get().getWorld()->getPrng();
        return Misc::Rng::roll0to99(prng) >= target;
    }

    void MechanicsManager::startCombat(
        const OFWorld::Ptr& ptr, const OFWorld::Ptr& target, const std::set<OFWorld::Ptr>* targetAllies)
    {
        CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);

        // Don't add duplicate packages nor add packages to dead actors.
        if (stats.isDead() || stats.getAiSequence().isInCombat(target))
            return;

        // The target is somehow the same as the actor. Early-out.
        if (ptr == target)
        {
            // We don't care about dialogue filters since the target is invalid.
            // We still want to play the combat taunt.
            OFBase::Environment::get().getDialogueManager()->say(ptr, ESM::RefId::stringRefId("attack"));
            if (!stats.getAiSequence().isInCombat())
                stats.resetFriendlyHits();
            return;
        }

        const bool inCombat = stats.getAiSequence().isInCombat();
        bool shout = !inCombat;
        if (inCombat)
        {
            const auto isInCombatWithOneOf = [&](const auto& allies) {
                for (const OFWorld::Ptr& ally : allies)
                {
                    if (stats.getAiSequence().isInCombat(ally))
                        return true;
                }
                return false;
            };
            if (targetAllies)
                shout = !isInCombatWithOneOf(*targetAllies);
            else
            {
                shout = stats.getAiSequence().isInCombat(target);
                if (!shout)
                {
                    std::set<OFWorld::Ptr> sidingActors;
                    getActorsSidingWith(target, sidingActors);
                    shout = !isInCombatWithOneOf(sidingActors);
                }
            }
        }
        stats.getAiSequence().stack(OFMechanics::AiCombat(target), ptr);
        if (target == getPlayer())
        {
            // if guard starts combat with player, guards pursuing player should do the same
            if (ptr.getClass().isClass(ptr, "Guard"))
            {
                const ESM::RefNum playerNum = target.getCellRef().getRefNum();
                // Stops guard from ending combat if player is unreachable
                stats.setHitAttemptActor(playerNum);
                for (const Actor& actor : mActors)
                {
                    if (actor.isInvalid())
                        continue;
                    if (actor.getPtr().getClass().isClass(actor.getPtr(), "Guard"))
                    {
                        OFMechanics::AiSequence& aiSeq
                            = actor.getPtr().getClass().getCreatureStats(actor.getPtr()).getAiSequence();
                        if (aiSeq.getTypeId() == OFMechanics::AiPackageTypeId::Pursue)
                        {
                            aiSeq.stopPursuit();
                            aiSeq.stack(OFMechanics::AiCombat(target), ptr);
                            // Stops guard from ending combat if player is unreachable
                            actor.getPtr().getClass().getCreatureStats(actor.getPtr()).setHitAttemptActor(playerNum);
                        }
                    }
                }
            }
        }

        // Must be done after the target is set up, so that CreatureTargetted dialogue filter works properly
        if (shout)
            OFBase::Environment::get().getDialogueManager()->say(ptr, ESM::RefId::stringRefId("attack"));
    }

    void MechanicsManager::stopCombat(const OFWorld::Ptr& actor)
    {
        mActors.stopCombat(actor);
    }

    void MechanicsManager::getObjectsInRange(
        const osg::Vec3f& position, float radius, std::vector<OFWorld::Ptr>& objects)
    {
        mActors.getObjectsInRange(position, radius, objects);
        mObjects.getObjectsInRange(position, radius, objects);
    }

    void MechanicsManager::getActorsInRange(
        const osg::Vec3f& position, float radius, std::vector<OFWorld::Ptr>& objects)
    {
        mActors.getObjectsInRange(position, radius, objects);
    }

    bool MechanicsManager::isAnyActorInRange(const osg::Vec3f& position, float radius)
    {
        return mActors.isAnyObjectInRange(position, radius);
    }

    std::vector<OFWorld::Ptr> MechanicsManager::getActorsSidingWith(const OFWorld::Ptr& actor)
    {
        return mActors.getActorsSidingWith(actor);
    }

    std::vector<OFWorld::Ptr> MechanicsManager::getActorsFollowing(const OFWorld::Ptr& actor)
    {
        return mActors.getActorsFollowing(actor);
    }

    std::vector<int> MechanicsManager::getActorsFollowingIndices(const OFWorld::Ptr& actor)
    {
        return mActors.getActorsFollowingIndices(actor);
    }

    std::map<int, OFWorld::Ptr> MechanicsManager::getActorsFollowingByIndex(const OFWorld::Ptr& actor)
    {
        return mActors.getActorsFollowingByIndex(actor);
    }

    std::vector<OFWorld::Ptr> MechanicsManager::getActorsFighting(const OFWorld::Ptr& actor)
    {
        return mActors.getActorsFighting(actor);
    }

    std::vector<OFWorld::Ptr> MechanicsManager::getEnemiesNearby(const OFWorld::Ptr& actor)
    {
        return mActors.getEnemiesNearby(actor);
    }

    void MechanicsManager::getActorsFollowing(const OFWorld::Ptr& actor, std::set<OFWorld::Ptr>& out)
    {
        mActors.getActorsFollowing(actor, out);
    }

    void MechanicsManager::getActorsSidingWith(const OFWorld::Ptr& actor, std::set<OFWorld::Ptr>& out)
    {
        mActors.getActorsSidingWith(actor, out);
    }

    size_t MechanicsManager::countSavedGameRecords() const
    {
        return 1 // Death counter
            + 1; // Stolen items
    }

    void MechanicsManager::write(ESM::ESMWriter& writer, Loading::Listener& listener) const
    {
        mActors.write(writer, listener);

        ESM::StolenItems items;
        items.mStolenItems = mStolenItems;
        writer.startRecord(ESM::REC_STLN);
        items.write(writer);
        writer.endRecord(ESM::REC_STLN);
    }

    void MechanicsManager::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        if (type == ESM::REC_STLN)
        {
            ESM::StolenItems items;
            items.load(reader);
            mStolenItems = items.mStolenItems;
        }
        else
            mActors.readRecord(reader, type);
    }

    void MechanicsManager::clear()
    {
        mActors.clear();
        mStolenItems.clear();
        mClassSelected = false;
        mRaceSelected = false;
    }

    void MechanicsManager::resurrect(const OFWorld::Ptr& ptr)
    {
        CreatureStats& stats = ptr.getClass().getCreatureStats(ptr);
        if (stats.isDead())
        {
            stats.resurrect();
            mActors.resurrect(ptr);
        }
    }

    bool MechanicsManager::isCastingSpell(const OFWorld::Ptr& ptr) const
    {
        return mActors.isCastingSpell(ptr);
    }

    bool MechanicsManager::isReadyToBlock(const OFWorld::Ptr& ptr) const
    {
        return mActors.isReadyToBlock(ptr);
    }

    bool MechanicsManager::isAttackingOrSpell(const OFWorld::Ptr& ptr) const
    {
        return mActors.isAttackingOrSpell(ptr);
    }

    void MechanicsManager::setWerewolf(const OFWorld::Ptr& actor, bool werewolf)
    {
        OFMechanics::NpcStats& npcStats = actor.getClass().getNpcStats(actor);

        // The actor does not have to change state
        if (npcStats.isWerewolf() == werewolf)
            return;

        OFWorld::Player* player = &OFBase::Environment::get().getWorld()->getPlayer();

        // Werewolfs can not cast spells, so we need to unset the prepared spell if there is one.
        if (npcStats.getDrawState() == OFMechanics::DrawState::Spell)
            npcStats.setDrawState(OFMechanics::DrawState::Nothing);

        npcStats.setWerewolf(werewolf);

        OFWorld::InventoryStore& inv = actor.getClass().getInventoryStore(actor);

        if (werewolf)
        {
            inv.unequipAll();
            inv.equip(OFWorld::InventoryStore::Slot_Robe,
                inv.ContainerStore::add(ESM::RefId::stringRefId("werewolfrobe"), 1));
        }
        else
        {
            inv.unequipSlot(OFWorld::InventoryStore::Slot_Robe);
            inv.ContainerStore::remove(ESM::RefId::stringRefId("werewolfrobe"), 1);
        }

        if (actor == player->getPlayer())
        {
            OFBase::Environment::get().getWorld()->reattachPlayerCamera();

            // Update the GUI only when called on the player
            OFBase::WindowManager* windowManager = OFBase::Environment::get().getWindowManager();

            // Transforming removes all temporary effects
            actor.getClass().getCreatureStats(actor).getActiveSpells().purge(
                [](const auto& params) { return params.hasFlag(ESM::ActiveSpells::Flag_Temporary); }, actor);
            mActors.updateActor(actor, 0.f);

            if (werewolf)
            {
                player->saveStats();
                player->setWerewolfStats();
                windowManager->forceHide(OFGui::GW_Inventory);
                windowManager->forceHide(OFGui::GW_Magic);
            }
            else
            {
                player->restoreStats();
                windowManager->unsetForceHide(OFGui::GW_Inventory);
                windowManager->unsetForceHide(OFGui::GW_Magic);
            }

            windowManager->setWerewolfOverlay(werewolf);

            // Witnesses of the player's transformation will make them a globally known werewolf
            std::vector<OFWorld::Ptr> neighbors;
            const OFWorld::Store<ESM::GameSetting>& gmst
                = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
            getActorsInRange(
                actor.getRefData().getPosition().asVec3(), gmst.find("fAlarmRadius")->mValue.getFloat(), neighbors);

            bool detected = false, reported = false;
            for (const OFWorld::Ptr& neighbor : neighbors)
            {
                if (neighbor == actor || !neighbor.getClass().isNpc())
                    continue;

                if (OFBase::Environment::get().getWorld()->getLOS(neighbor, actor) && awarenessCheck(actor, neighbor))
                {
                    detected = true;
                    if (neighbor.getClass()
                            .getCreatureStats(neighbor)
                            .getAiSetting(OFMechanics::AiSetting::Alarm)
                            .getModified()
                        > 0)
                    {
                        reported = true;
                        break;
                    }
                }
            }

            if (detected)
            {
                windowManager->messageBox("#{sWerewolfAlarmMessage}");
                OFBase::Environment::get().getWorld()->setGlobalInt(OFWorld::Globals::sPCKnownWerewolf, 1);

                if (reported)
                {
                    npcStats.setBounty(
                        std::max(0, npcStats.getBounty() + gmst.find("iWereWolfBounty")->mValue.getInteger()));
                }
            }
        }
    }

    void MechanicsManager::applyWerewolfAcrobatics(const OFWorld::Ptr& actor)
    {
        const ESM::Skill* acrobatics
            = OFBase::Environment::get().getESMStore()->get<ESM::Skill>().find(ESM::Skill::Acrobatics);
        OFMechanics::NpcStats& stats = actor.getClass().getNpcStats(actor);
        auto& skill = stats.getSkill(acrobatics->mId);
        skill.setBase(skill.getBase(), true);
        skill.setModifier(acrobatics->mWerewolfValue - skill.getBase());
    }

    void MechanicsManager::cleanupSummonedCreature(ESM::RefNum creature)
    {
        mActors.cleanupSummonedCreature(creature);
    }

    void MechanicsManager::reportStats(unsigned int frameNumber, osg::Stats& stats) const
    {
        stats.setAttribute(frameNumber, "Mechanics Actors", static_cast<double>(mActors.size()));
        stats.setAttribute(frameNumber, "Mechanics Objects", static_cast<double>(mObjects.size()));
    }

    int MechanicsManager::getGreetingTimer(const OFWorld::Ptr& ptr) const
    {
        return mActors.getGreetingTimer(ptr);
    }

    float MechanicsManager::getAngleToPlayer(const OFWorld::Ptr& ptr) const
    {
        return mActors.getAngleToPlayer(ptr);
    }

    GreetingState MechanicsManager::getGreetingState(const OFWorld::Ptr& ptr) const
    {
        return mActors.getGreetingState(ptr);
    }

    void MechanicsManager::fastForwardAi() const
    {
        mActors.fastForwardAi();
    }
}
