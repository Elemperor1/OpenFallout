#include "creature.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/creaturestate.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadsndg.hpp>
#include <components/esm3/loadsoun.hpp>
#include <components/misc/rng.hpp>
#include <components/settings/values.hpp>

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/aisetting.hpp"
#include "../mwmechanics/combat.hpp"
#include "../mwmechanics/creaturecustomdataresetter.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/difficultyscaling.hpp"
#include "../mwmechanics/disease.hpp"
#include "../mwmechanics/inventory.hpp"
#include "../mwmechanics/magiceffects.hpp"
#include "../mwmechanics/movement.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/setbaseaisetting.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwlua/localscripts.hpp"

#include "../mwworld/actionopen.hpp"
#include "../mwworld/actiontalk.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/localscripts.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwgui/tooltips.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace
{
    bool isFlagBitSet(const OFWorld::ConstPtr& ptr, ESM::Creature::Flags bitMask)
    {
        return (ptr.get<ESM::Creature>()->mBase->mFlags & bitMask) != 0;
    }
}

namespace OFClass
{

    class CreatureCustomData : public OFWorld::TypedCustomData<CreatureCustomData>
    {
    public:
        OFMechanics::CreatureStats mCreatureStats;
        std::unique_ptr<OFWorld::ContainerStore> mContainerStore; // may be InventoryStore for some creatures
        OFMechanics::Movement mMovement;

        CreatureCustomData() = default;
        CreatureCustomData(const CreatureCustomData& other);
        CreatureCustomData(CreatureCustomData&& other) = default;

        CreatureCustomData& asCreatureCustomData() override { return *this; }
        const CreatureCustomData& asCreatureCustomData() const override { return *this; }
    };

    CreatureCustomData::CreatureCustomData(const CreatureCustomData& other)
        : mCreatureStats(other.mCreatureStats)
        , mContainerStore(other.mContainerStore->clone())
        , mMovement(other.mMovement)
    {
    }

    Creature::Creature()
        : OFWorld::RegisteredClass<Creature, Actor>(ESM::Creature::sRecordId)
    {
    }

    const Creature::GMST& Creature::getGmst()
    {
        static const GMST staticGmst = [] {
            GMST gmst;

            const OFWorld::Store<ESM::GameSetting>& store
                = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

            gmst.fMinWalkSpeedCreature = store.find("fMinWalkSpeedCreature");
            gmst.fMaxWalkSpeedCreature = store.find("fMaxWalkSpeedCreature");
            gmst.fEncumberedMoveEffect = store.find("fEncumberedMoveEffect");
            gmst.fSneakSpeedMultiplier = store.find("fSneakSpeedMultiplier");
            gmst.fAthleticsRunBonus = store.find("fAthleticsRunBonus");
            gmst.fBaseRunMultiplier = store.find("fBaseRunMultiplier");
            gmst.fMinFlySpeed = store.find("fMinFlySpeed");
            gmst.fMaxFlySpeed = store.find("fMaxFlySpeed");
            gmst.fSwimRunBase = store.find("fSwimRunBase");
            gmst.fSwimRunAthleticsMult = store.find("fSwimRunAthleticsMult");
            gmst.fKnockDownMult = store.find("fKnockDownMult");
            gmst.iKnockDownOddsMult = store.find("iKnockDownOddsMult");
            gmst.iKnockDownOddsBase = store.find("iKnockDownOddsBase");

            return gmst;
        }();
        return staticGmst;
    }

    void Creature::ensureCustomData(const OFWorld::Ptr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
            auto tempData = std::make_unique<CreatureCustomData>();
            CreatureCustomData* data = tempData.get();
            OFMechanics::CreatureCustomDataResetter resetter{ ptr };
            ptr.getRefData().setCustomData(std::move(tempData));

            OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();

            // creature stats
            for (const auto& [attribute, value] : ref->mBase->mData.mAttributes)
                data->mCreatureStats.setAttribute(attribute, static_cast<float>(value));
            data->mCreatureStats.setHealth(static_cast<float>(ref->mBase->mData.mHealth));
            data->mCreatureStats.setMagicka(static_cast<float>(ref->mBase->mData.mMana));
            data->mCreatureStats.setFatigue(static_cast<float>(ref->mBase->mData.mFatigue));

            data->mCreatureStats.setLevel(ref->mBase->mData.mLevel);

            data->mCreatureStats.getAiSequence().fill(ref->mBase->mAiPackage);

            data->mCreatureStats.setAiSetting(OFMechanics::AiSetting::Hello, ref->mBase->mAiData.mHello);
            data->mCreatureStats.setAiSetting(OFMechanics::AiSetting::Fight, ref->mBase->mAiData.mFight);
            data->mCreatureStats.setAiSetting(OFMechanics::AiSetting::Flee, ref->mBase->mAiData.mFlee);
            data->mCreatureStats.setAiSetting(OFMechanics::AiSetting::Alarm, ref->mBase->mAiData.mAlarm);

            // Persistent actors with 0 health do not play death animation
            if (data->mCreatureStats.isDead())
                data->mCreatureStats.setDeathAnimationFinished(isPersistent(ptr));

            // spells
            bool spellsInitialised = data->mCreatureStats.getSpells().setSpells(ref->mBase->mId);
            if (!spellsInitialised)
                data->mCreatureStats.getSpells().addAllToInstance(ref->mBase->mSpells.mList);

            // inventory
            bool hasInventory = hasInventoryStore(ptr);
            if (hasInventory)
                data->mContainerStore = std::make_unique<OFWorld::InventoryStore>();
            else
                data->mContainerStore = std::make_unique<OFWorld::ContainerStore>();
            data->mContainerStore->setPtr(ptr);

            data->mCreatureStats.setGoldPool(ref->mBase->mData.mGold);

            resetter.mPtr = {};

            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
            getContainerStore(ptr).fill(ref->mBase->mInventory, ptr.getCellRef().getRefId(), prng);

            if (hasInventory)
                getInventoryStore(ptr).autoEquip();
        }
    }

    void Creature::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        OFRender::Objects& objects = renderingInterface.getObjects();
        objects.insertCreature(ptr, model, hasInventoryStore(ptr));
    }

    VFS::Path::NormalizedView Creature::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Creature>(ptr);
    }

    void Creature::getModelsToPreload(
        const OFWorld::ConstPtr& ptr, std::vector<VFS::Path::NormalizedView>& models) const
    {
        VFS::Path::NormalizedView model = getModel(ptr);
        if (!model.empty())
            models.push_back(model);

        const OFWorld::CustomData* customData = ptr.getRefData().getCustomData();
        if (customData && hasInventoryStore(ptr))
        {
            const auto& invStore
                = static_cast<const OFWorld::InventoryStore&>(*customData->asCreatureCustomData().mContainerStore);
            for (int slot = 0; slot < OFWorld::InventoryStore::Slots; ++slot)
            {
                OFWorld::ConstContainerStoreIterator equipped = invStore.getSlot(slot);
                if (equipped != invStore.end())
                {
                    model = equipped->getClass().getModel(*equipped);
                    if (!model.empty())
                        models.push_back(model);
                }
            }
        }
    }

    std::string_view Creature::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Creature>(ptr);
    }

    OFMechanics::CreatureStats& Creature::getCreatureStats(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);

        return ptr.getRefData().getCustomData()->asCreatureCustomData().mCreatureStats;
    }

    bool Creature::evaluateHit(const OFWorld::Ptr& ptr, OFWorld::Ptr& victim, osg::Vec3f& hitPosition) const
    {
        victim = OFWorld::Ptr();
        hitPosition = osg::Vec3f();

        // Get the weapon used (if hand-to-hand, weapon = inv.end())
        OFWorld::Ptr weapon;
        if (hasInventoryStore(ptr))
        {
            OFWorld::InventoryStore& inv = getInventoryStore(ptr);
            OFWorld::ContainerStoreIterator weaponslot = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
            if (weaponslot != inv.end() && weaponslot->getType() == ESM::Weapon::sRecordId)
                weapon = *weaponslot;
        }

        OFBase::World* world = OFBase::Environment::get().getWorld();

        const float dist = OFMechanics::getMeleeWeaponReach(ptr, weapon);
        const std::pair<OFWorld::Ptr, osg::Vec3f> result = OFMechanics::getHitContact(ptr, dist);
        if (result.first.isEmpty()) // Didn't hit anything
            return true;

        // Note that earlier we returned true in spite of an apparent failure to hit anything alive.
        // This is because hitting nothing is not a "miss" and should be handled as such character controller-side.
        victim = result.first;
        hitPosition = result.second;

        float hitchance = OFMechanics::getHitChance(ptr, victim, ptr.get<ESM::Creature>()->mBase->mData.mCombat);
        return Misc::Rng::roll0to99(world->getPrng()) < hitchance;
    }

    void Creature::hit(const OFWorld::Ptr& ptr, float attackStrength, float attackWindUp, int type,
        const OFWorld::Ptr& victim, const osg::Vec3f& hitPosition, bool success) const
    {
        OFMechanics::CreatureStats& stats = getCreatureStats(ptr);

        if (stats.getDrawState() != OFMechanics::DrawState::Weapon)
            return;

        OFWorld::Ptr weapon;
        if (hasInventoryStore(ptr))
        {
            OFWorld::InventoryStore& inv = getInventoryStore(ptr);
            OFWorld::ContainerStoreIterator weaponslot = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
            if (weaponslot != inv.end() && weaponslot->getType() == ESM::Weapon::sRecordId)
                weapon = *weaponslot;
        }

        OFMechanics::applyFatigueLoss(ptr, weapon, attackStrength);

        if (victim.isEmpty())
            return; // Didn't hit anything

        const OFWorld::Class& othercls = victim.getClass();
        OFMechanics::CreatureStats& otherstats = othercls.getCreatureStats(victim);
        if (otherstats.isDead()) // Can't hit dead actors
            return;

        if (!OFMechanics::isInMeleeReach(ptr, victim, OFMechanics::getMeleeWeaponReach(ptr, weapon)))
            return;

        if (!success)
        {
            OFBase::Environment::get().getLuaManager()->onHit(ptr, victim, weapon, OFWorld::Ptr(), type, attackStrength,
                attackWindUp, 0.0f, false, hitPosition, false, OFMechanics::DamageSourceType::Melee);
            OFMechanics::reduceWeaponCondition(0.f, false, weapon, ptr);
            return;
        }

        OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();
        int min, max;
        switch (type)
        {
            case 0:
                min = ref->mBase->mData.mAttack[0];
                max = ref->mBase->mData.mAttack[1];
                break;
            case 1:
                min = ref->mBase->mData.mAttack[2];
                max = ref->mBase->mData.mAttack[3];
                break;
            case 2:
            default:
                min = ref->mBase->mData.mAttack[4];
                max = ref->mBase->mData.mAttack[5];
                break;
        }

        float damage = min + (max - min) * attackStrength;
        bool healthdmg = true;
        if (!weapon.isEmpty())
        {
            const unsigned char* attack = nullptr;
            if (type == ESM::Weapon::AT_Chop)
                attack = weapon.get<ESM::Weapon>()->mBase->mData.mChop.data();
            else if (type == ESM::Weapon::AT_Slash)
                attack = weapon.get<ESM::Weapon>()->mBase->mData.mSlash.data();
            else if (type == ESM::Weapon::AT_Thrust)
                attack = weapon.get<ESM::Weapon>()->mBase->mData.mThrust.data();
            if (attack)
            {
                damage = attack[0] + ((attack[1] - attack[0]) * attackStrength);
                OFMechanics::adjustWeaponDamage(damage, weapon, ptr);
                OFMechanics::reduceWeaponCondition(damage, true, weapon, ptr);
                OFMechanics::resistNormalWeapon(victim, ptr, weapon, damage);
            }

            // Apply "On hit" enchanted weapons
            OFMechanics::applyOnStrikeEnchantment(ptr, victim, weapon, hitPosition);
        }
        else if (isBipedal(ptr))
        {
            OFMechanics::getHandToHandDamage(ptr, victim, damage, healthdmg, attackStrength);
        }

        OFMechanics::applyElementalShields(ptr, victim);

        if (OFMechanics::blockMeleeAttack(ptr, victim, weapon, damage, attackStrength))
            damage = 0;

        OFMechanics::diseaseContact(victim, ptr);

        OFBase::Environment::get().getLuaManager()->onHit(ptr, victim, weapon, OFWorld::Ptr(), type, attackStrength,
            attackWindUp, damage, healthdmg, hitPosition, true, OFMechanics::DamageSourceType::Melee);
    }

    void Creature::onHit(const OFWorld::Ptr& ptr, const std::map<std::string, float>& damages, ESM::RefId object,
        const OFWorld::Ptr& attacker, bool successful, const OFMechanics::DamageSourceType sourceType) const
    {
        OFMechanics::CreatureStats& stats = getCreatureStats(ptr);

        // Self defense
        bool setOnPcHitMe = true;

        // NOTE: 'object' and/or 'attacker' may be empty.
        if (!attacker.isEmpty() && attacker.getClass().isActor() && !stats.getAiSequence().isInCombat(attacker))
        {
            stats.setAttacked(true);

            bool complain = sourceType == OFMechanics::DamageSourceType::Melee;
            bool supportFriendlyFire = sourceType != OFMechanics::DamageSourceType::Ranged;
            if (supportFriendlyFire && OFMechanics::friendlyHit(attacker, ptr, complain))
                setOnPcHitMe = false;
            else
                setOnPcHitMe = OFBase::Environment::get().getMechanicsManager()->actorAttacked(ptr, attacker);
        }

        // Attacker and target store each other as hitattemptactor if they have no one stored yet
        if (!attacker.isEmpty() && attacker.getClass().isActor())
        {
            OFMechanics::CreatureStats& statsAttacker = attacker.getClass().getCreatureStats(attacker);
            // First handle the attacked actor
            if (!stats.getHitAttemptActor().isSet()
                && (statsAttacker.getAiSequence().isInCombat(ptr) || attacker == OFMechanics::getPlayer()))
                stats.setHitAttemptActor(attacker.getCellRef().getRefNum());

            // Next handle the attacking actor
            if (!statsAttacker.getHitAttemptActor().isSet()
                && (statsAttacker.getAiSequence().isInCombat(ptr) || attacker == OFMechanics::getPlayer()))
                statsAttacker.setHitAttemptActor(ptr.getCellRef().getRefNum());
        }

        if (!object.empty())
            stats.setLastHitAttemptObject(object);

        if (setOnPcHitMe && !attacker.isEmpty() && attacker == OFMechanics::getPlayer())
        {
            const ESM::RefId& script = ptr.get<ESM::Creature>()->mBase->mScript;
            /* Set the OnPCHitMe script variable. The script is responsible for clearing it. */
            if (!script.empty())
                ptr.getRefData().getLocals().setVarByInt(script, "onpchitme", 1);
        }

        if (!successful)
        {
            // Missed
            return;
        }

        if (!object.empty())
            stats.setLastHitObject(object);

        for (auto& [stat, damage] : damages)
        {
            if (damage < 0.001f)
                continue;

            if (stat == "health")
            {
                OFMechanics::DynamicStat<float> health(getCreatureStats(ptr).getHealth());
                health.setCurrent(health.getCurrent() - damage);
                stats.setHealth(health);
            }
            else if (stat == "fatigue")
            {
                OFMechanics::DynamicStat<float> fatigue(getCreatureStats(ptr).getFatigue());
                fatigue.setCurrent(fatigue.getCurrent() - damage, true);
                stats.setFatigue(fatigue);
            }
            else if (stat == "magicka")
            {
                OFMechanics::DynamicStat<float> magicka(getCreatureStats(ptr).getMagicka());
                magicka.setCurrent(magicka.getCurrent() - damage);
                stats.setMagicka(magicka);
            }
        }
    }

    std::unique_ptr<OFWorld::Action> Creature::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        std::unique_ptr<OFWorld::Action> werewolfAction = getWerewolfRefusalAction(actor);
        if (werewolfAction)
            return werewolfAction;

        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        if (stats.isDead())
        {
            // by default user can loot non-fighting actors during death animation
            if (Settings::game().mCanLootDuringDeathAnimation)
                return std::make_unique<OFWorld::ActionOpen>(ptr);

            // otherwise wait until death animation
            if (stats.isDeathAnimationFinished())
                return std::make_unique<OFWorld::ActionOpen>(ptr);
        }
        else if (!stats.getKnockedDown())
            return std::make_unique<OFWorld::ActionTalk>(ptr);

        // Tribunal and some mod companions oddly enough must use open action as fallback
        if (!getScript(ptr).empty() && ptr.getRefData().getLocals().getIntVar(getScript(ptr), "companion"))
            return std::make_unique<OFWorld::ActionOpen>(ptr);

        return std::make_unique<OFWorld::FailedAction>();
    }

    OFWorld::ContainerStore& Creature::getContainerStore(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return *ptr.getRefData().getCustomData()->asCreatureCustomData().mContainerStore;
    }

    OFWorld::InventoryStore& Creature::getInventoryStore(const OFWorld::Ptr& ptr) const
    {
        if (hasInventoryStore(ptr))
            return static_cast<OFWorld::InventoryStore&>(getContainerStore(ptr));
        else
            throw std::runtime_error("this creature has no inventory store");
    }

    bool Creature::hasInventoryStore(const OFWorld::ConstPtr& ptr) const
    {
        return isFlagBitSet(ptr, ESM::Creature::Weapon);
    }

    ESM::RefId Creature::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();

        return ref->mBase->mScript;
    }

    bool Creature::isEssential(const OFWorld::ConstPtr& ptr) const
    {
        return isFlagBitSet(ptr, ESM::Creature::Essential);
    }

    float Creature::getMaxSpeed(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);

        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;

        const GMST& gmst = getGmst();

        const OFBase::World* world = OFBase::Environment::get().getWorld();
        const OFMechanics::MagicEffects& mageffects = stats.getMagicEffects();
        const float normalizedEncumbrance = getNormalizedEncumbrance(ptr);

        float moveSpeed;

        if (normalizedEncumbrance > 1.0f)
            moveSpeed = 0.0f;
        else if (canFly(ptr)
            || (mageffects.getOrDefault(ESM::MagicEffect::Levitate).getMagnitude() > 0 && world->isLevitationEnabled()))
        {
            float flySpeed = 0.01f
                * (stats.getAttribute(ESM::Attribute::Speed).getModified()
                    + mageffects.getOrDefault(ESM::MagicEffect::Levitate).getMagnitude());
            flySpeed = gmst.fMinFlySpeed->mValue.getFloat()
                + flySpeed * (gmst.fMaxFlySpeed->mValue.getFloat() - gmst.fMinFlySpeed->mValue.getFloat());
            flySpeed *= 1.0f - gmst.fEncumberedMoveEffect->mValue.getFloat() * normalizedEncumbrance;
            flySpeed = std::max(0.0f, flySpeed);
            moveSpeed = flySpeed;
        }
        else if (world->isSwimming(ptr))
            moveSpeed = getSwimSpeed(ptr);
        else
            moveSpeed = getWalkSpeed(ptr);

        return moveSpeed;
    }

    OFMechanics::Movement& Creature::getMovementSettings(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);

        return ptr.getRefData().getCustomData()->asCreatureCustomData().mMovement;
    }

    bool Creature::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        if (!ptr.getRefData().getCustomData() || OFBase::Environment::get().getWindowManager()->isGuiMode())
            return true;

        const CreatureCustomData& customData = ptr.getRefData().getCustomData()->asCreatureCustomData();

        if (customData.mCreatureStats.isDead() && customData.mCreatureStats.isDeathAnimationFinished())
            return true;

        const OFMechanics::AiSequence& aiSeq = customData.mCreatureStats.getAiSequence();
        return !aiSeq.isInCombat() || aiSeq.isFleeing();
    }

    OFGui::ToolTipInfo Creature::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name));

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");

        return info;
    }

    float Creature::getArmorRating(const OFWorld::Ptr& ptr, bool useLuaInterfaceIfAvailable) const
    {
        // Equipment armor rating is deliberately ignored.
        return getCreatureStats(ptr).getMagicEffects().getOrDefault(ESM::MagicEffect::Shield).getMagnitude();
    }

    float Creature::getCapacity(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        return stats.getAttribute(ESM::Attribute::Strength).getModified() * 5;
    }

    int Creature::getServices(const OFWorld::ConstPtr& actor) const
    {
        return actor.get<ESM::Creature>()->mBase->mAiData.mServices;
    }

    bool Creature::isPersistent(const OFWorld::ConstPtr& actor) const
    {
        const OFWorld::LiveCellRef<ESM::Creature>* ref = actor.get<ESM::Creature>();
        return (ref->mBase->mRecordFlags & ESM::FLAG_Persistent) != 0;
    }

    ESM::RefId Creature::getSoundIdFromSndGen(const OFWorld::Ptr& ptr, std::string_view name) const
    {
        int type = getSndGenTypeFromName(ptr, name);
        if (type < 0)
            return ESM::RefId();

        std::vector<const ESM::SoundGenerator*> sounds;
        std::vector<const ESM::SoundGenerator*> fallbacksounds;

        OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();

        const ESM::RefId& ourId = (ref->mBase->mOriginal.empty()) ? ptr.getCellRef().getRefId() : ref->mBase->mOriginal;

        const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();
        auto sound = store.get<ESM::SoundGenerator>().begin();
        while (sound != store.get<ESM::SoundGenerator>().end())
        {
            if (type == sound->mType && !sound->mCreature.empty() && ourId == sound->mCreature)
                sounds.push_back(&*sound);
            if (type == sound->mType && sound->mCreature.empty())
                fallbacksounds.push_back(&*sound);
            ++sound;
        }

        if (sounds.empty())
        {
            const VFS::Path::NormalizedView model = getModel(ptr);
            if (!model.empty())
            {
                for (const ESM::Creature& creature : store.get<ESM::Creature>())
                {
                    if (creature.mId != ourId && creature.mOriginal != ourId && !creature.mModel.empty()
                        && model == creature.mModel.getNormalized())
                    {
                        const ESM::RefId& fallbackId = !creature.mOriginal.empty() ? creature.mOriginal : creature.mId;
                        sound = store.get<ESM::SoundGenerator>().begin();
                        while (sound != store.get<ESM::SoundGenerator>().end())
                        {
                            if (type == sound->mType && !sound->mCreature.empty() && fallbackId == sound->mCreature)
                                sounds.push_back(&*sound);
                            ++sound;
                        }
                        break;
                    }
                }
            }
        }

        auto& prng = OFBase::Environment::get().getWorld()->getPrng();
        if (!sounds.empty())
            return sounds[Misc::Rng::rollDice(sounds.size(), prng)]->mSound;
        if (!fallbacksounds.empty())
            return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;

        return ESM::RefId();
    }

    OFWorld::Ptr Creature::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();
        OFWorld::Ptr newPtr(cell.insert(ref), &cell);
        if (newPtr.getRefData().getCustomData())
        {
            OFBase::Environment::get().getWorldModel()->registerPtr(newPtr);
            newPtr.getClass().getContainerStore(newPtr).setPtr(newPtr);
        }
        return newPtr;
    }

    bool Creature::isBipedal(const OFWorld::ConstPtr& ptr) const
    {
        return isFlagBitSet(ptr, ESM::Creature::Bipedal);
    }

    bool Creature::canFly(const OFWorld::ConstPtr& ptr) const
    {
        return isFlagBitSet(ptr, ESM::Creature::Flies);
    }

    bool Creature::canSwim(const OFWorld::ConstPtr& ptr) const
    {
        return isFlagBitSet(ptr, static_cast<ESM::Creature::Flags>(ESM::Creature::Swims | ESM::Creature::Bipedal));
    }

    bool Creature::canWalk(const OFWorld::ConstPtr& ptr) const
    {
        return isFlagBitSet(ptr, static_cast<ESM::Creature::Flags>(ESM::Creature::Walks | ESM::Creature::Bipedal));
    }

    int Creature::getSndGenTypeFromName(const OFWorld::Ptr& ptr, std::string_view name)
    {
        if (name == "left")
        {
            OFBase::World* world = OFBase::Environment::get().getWorld();
            if (world->isFlying(ptr))
                return -1;
            osg::Vec3f pos(ptr.getRefData().getPosition().asVec3());
            if (world->isUnderwater(ptr.getCell(), pos) || world->isWalkingOnWater(ptr))
                return ESM::SoundGenerator::SwimLeft;
            if (world->isOnGround(ptr))
                return ESM::SoundGenerator::LeftFoot;
            return -1;
        }
        if (name == "right")
        {
            OFBase::World* world = OFBase::Environment::get().getWorld();
            if (world->isFlying(ptr))
                return -1;
            osg::Vec3f pos(ptr.getRefData().getPosition().asVec3());
            if (world->isUnderwater(ptr.getCell(), pos) || world->isWalkingOnWater(ptr))
                return ESM::SoundGenerator::SwimRight;
            if (world->isOnGround(ptr))
                return ESM::SoundGenerator::RightFoot;
            return -1;
        }
        if (name == "swimleft")
            return ESM::SoundGenerator::SwimLeft;
        if (name == "swimright")
            return ESM::SoundGenerator::SwimRight;
        if (name == "moan")
            return ESM::SoundGenerator::Moan;
        if (name == "roar")
            return ESM::SoundGenerator::Roar;
        if (name == "scream")
            return ESM::SoundGenerator::Scream;
        if (name == "land")
            return ESM::SoundGenerator::Land;

        throw std::runtime_error("Unexpected soundgen type: " + std::string(name));
    }

    float Creature::getSkill(const OFWorld::Ptr& ptr, ESM::RefId id) const
    {
        OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();

        const ESM::Skill* skillRecord = OFBase::Environment::get().getESMStore()->get<ESM::Skill>().find(id);

        switch (skillRecord->mData.mSpecialization)
        {
            case ESM::Class::Combat:
                return static_cast<float>(ref->mBase->mData.mCombat);
            case ESM::Class::Magic:
                return static_cast<float>(ref->mBase->mData.mMagic);
            case ESM::Class::Stealth:
                return static_cast<float>(ref->mBase->mData.mStealth);
            default:
                throw std::runtime_error("invalid specialisation");
        }
    }

    void Creature::readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const
    {
        if (!state.mHasCustomState)
            return;

        const ESM::CreatureState& creatureState = state.asCreatureState();

        if (!ptr.getRefData().getCustomData())
        {
            if (creatureState.mCreatureStats.mMissingACDT)
                ensureCustomData(ptr);
            else
            {
                // Create a CustomData, but don't fill it from ESM records (not needed)
                auto data = std::make_unique<CreatureCustomData>();

                if (hasInventoryStore(ptr))
                    data->mContainerStore = std::make_unique<OFWorld::InventoryStore>();
                else
                    data->mContainerStore = std::make_unique<OFWorld::ContainerStore>();

                OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
                data->mContainerStore->setPtr(ptr);

                ptr.getRefData().setCustomData(std::move(data));
            }
        }

        CreatureCustomData& customData = ptr.getRefData().getCustomData()->asCreatureCustomData();

        customData.mContainerStore->readState(creatureState.mInventory);
        bool spellsInitialised = customData.mCreatureStats.getSpells().setSpells(ptr.get<ESM::Creature>()->mBase->mId);
        if (spellsInitialised)
            customData.mCreatureStats.getSpells().clear();
        customData.mCreatureStats.readState(creatureState.mCreatureStats);
    }

    void Creature::writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            state.mHasCustomState = false;
            return;
        }

        const CreatureCustomData& customData = ptr.getRefData().getCustomData()->asCreatureCustomData();
        if (ptr.getCellRef().getCount() <= 0
            && (!isFlagBitSet(ptr, ESM::Creature::Respawn) || !customData.mCreatureStats.isDead()))
        {
            state.mHasCustomState = false;
            return;
        }

        ESM::CreatureState& creatureState = state.asCreatureState();
        customData.mContainerStore->writeState(creatureState.mInventory);
        customData.mCreatureStats.writeState(creatureState.mCreatureStats);
    }

    int Creature::getBaseGold(const OFWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM::Creature>()->mBase->mData.mGold;
    }

    void Creature::respawn(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::CreatureStats& creatureStats = getCreatureStats(ptr);
        if (ptr.getCellRef().getCount() > 0 && !creatureStats.isDead())
            return;

        if (!creatureStats.isDeathAnimationFinished())
            return;

        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        static const float fCorpseRespawnDelay = gmst.find("fCorpseRespawnDelay")->mValue.getFloat();
        static const float fCorpseClearDelay = gmst.find("fCorpseClearDelay")->mValue.getFloat();

        float delay
            = ptr.getCellRef().getCount() == 0 ? fCorpseClearDelay : std::min(fCorpseRespawnDelay, fCorpseClearDelay);

        if (isFlagBitSet(ptr, ESM::Creature::Respawn)
            && creatureStats.getTimeOfDeath() + delay <= OFBase::Environment::get().getWorld()->getTimeStamp())
        {
            if (ptr.getCellRef().hasContentFile())
            {
                if (ptr.getCellRef().getCount() == 0)
                {
                    ptr.getCellRef().setCount(1);
                    const ESM::RefId& script = getScript(ptr);
                    if (!script.empty())
                        OFBase::Environment::get().getWorld()->getLocalScripts().add(script, ptr);
                }

                OFBase::Environment::get().getWorld()->removeContainerScripts(ptr);
                OFBase::Environment::get().getWindowManager()->onDeleteCustomData(ptr);
                ptr.getRefData().setCustomData(nullptr);

                // Reset to original position
                OFBase::Environment::get().getWorld()->moveObject(
                    ptr, ptr.getCell()->getOriginCell(ptr), ptr.getCellRef().getPosition().asVec3());
                OFBase::Environment::get().getWorld()->rotateObject(
                    ptr, ptr.getCellRef().getPosition().asRotationVec3(), OFBase::RotationFlag_none);
            }
        }
    }

    int Creature::getBaseFightRating(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();
        return ref->mBase->mAiData.mFight;
    }

    void Creature::adjustScale(const OFWorld::ConstPtr& ptr, osg::Vec3f& scale, bool /* rendering */) const
    {
        const OFWorld::LiveCellRef<ESM::Creature>* ref = ptr.get<ESM::Creature>();
        scale *= ref->mBase->mScale;
    }

    void Creature::setBaseAISetting(const ESM::RefId& id, OFMechanics::AiSetting setting, int value) const
    {
        OFMechanics::setBaseAISetting<ESM::Creature>(id, setting, static_cast<unsigned char>(value));
    }

    void Creature::modifyBaseInventory(const ESM::RefId& actorId, const ESM::RefId& itemId, int amount) const
    {
        OFMechanics::modifyBaseInventory<ESM::Creature>(actorId, itemId, amount);
    }

    float Creature::getWalkSpeed(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        const GMST& gmst = getGmst();

        return gmst.fMinWalkSpeedCreature->mValue.getFloat()
            + 0.01f * stats.getAttribute(ESM::Attribute::Speed).getModified()
            * (gmst.fMaxWalkSpeedCreature->mValue.getFloat() - gmst.fMinWalkSpeedCreature->mValue.getFloat());
    }

    float Creature::getRunSpeed(const OFWorld::Ptr& ptr) const
    {
        return getWalkSpeed(ptr);
    }

    float Creature::getSwimSpeed(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        const OFMechanics::MagicEffects& mageffects = stats.getMagicEffects();

        return getSwimSpeedImpl(ptr, getGmst(), mageffects, getWalkSpeed(ptr));
    }
}
