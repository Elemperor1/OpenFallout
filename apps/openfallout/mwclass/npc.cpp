#include "npc.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <format>
#include <memory>
#include <stdexcept>

#include <components/misc/constants.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/rng.hpp>

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadbody.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadsoun.hpp>
#include <components/esm3/npcstate.hpp>
#include <components/settings/values.hpp>
#include <components/vfs/pathutil.hpp>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwlua/localscripts.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/aisetting.hpp"
#include "../mwmechanics/autocalcspell.hpp"
#include "../mwmechanics/combat.hpp"
#include "../mwmechanics/creaturecustomdataresetter.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/difficultyscaling.hpp"
#include "../mwmechanics/disease.hpp"
#include "../mwmechanics/inventory.hpp"
#include "../mwmechanics/movement.hpp"
#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/setbaseaisetting.hpp"
#include "../mwmechanics/spellcasting.hpp"
#include "../mwmechanics/weapontype.hpp"

#include "../mwworld/actionopen.hpp"
#include "../mwworld/actiontalk.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/localscripts.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwrender/npcanimation.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwgui/tooltips.hpp"

#include "nameorid.hpp"

namespace
{
    struct NpcParts
    {
        const ESM::RefId mSwimLeft = ESM::RefId::stringRefId("Swim Left");
        const ESM::RefId mSwimRight = ESM::RefId::stringRefId("Swim Right");
        const ESM::RefId mFootWaterLeft = ESM::RefId::stringRefId("FootWaterLeft");
        const ESM::RefId mFootWaterRight = ESM::RefId::stringRefId("FootWaterRight");
        const ESM::RefId mFootBareLeft = ESM::RefId::stringRefId("FootBareLeft");
        const ESM::RefId mFootBareRight = ESM::RefId::stringRefId("FootBareRight");
        const ESM::RefId mFootLightLeft = ESM::RefId::stringRefId("footLightLeft");
        const ESM::RefId mFootLightRight = ESM::RefId::stringRefId("footLightRight");
        const ESM::RefId mFootMediumRight = ESM::RefId::stringRefId("FootMedRight");
        const ESM::RefId mFootMediumLeft = ESM::RefId::stringRefId("FootMedLeft");
        const ESM::RefId mFootHeavyLeft = ESM::RefId::stringRefId("footHeavyLeft");
        const ESM::RefId mFootHeavyRight = ESM::RefId::stringRefId("footHeavyRight");
    };

    const NpcParts npcParts;

    bool isEven(double d)
    {
        double intPart;
        std::modf(d / 2.0, &intPart);
        return 2.0 * intPart == d;
    }

    float round_ieee_754(float f)
    {
        float i = std::floor(f);
        f -= i;
        if (f < 0.5)
            return i;
        if (f > 0.5)
            return i + 1.f;
        if (isEven(i))
            return i;
        return i + 1.f;
    }

    bool contains(const auto& array, ESM::RefId id)
    {
        return std::find(array.begin(), array.end(), id) != array.end();
    }

    void autoCalculateAttributes(const ESM::NPC* npc, const ESM::Race* race, OFMechanics::CreatureStats& creatureStats)
    {
        // race bonus
        bool male = (npc->mFlags & ESM::NPC::Female) == 0;

        const auto& attributes = OFBase::Environment::get().getESMStore()->get<ESM::Attribute>();
        int level = creatureStats.getLevel();
        for (const ESM::Attribute& attribute : attributes)
            creatureStats.setAttribute(
                attribute.mId, static_cast<float>(race->mData.getAttribute(attribute.mId, male)));

        // class bonus
        const ESM::Class* npcClass = OFBase::Environment::get().getESMStore()->get<ESM::Class>().find(npc->mClass);

        for (const ESM::RefId& id : npcClass->mData.mAttribute)
        {
            if (!id.empty())
                creatureStats.setAttribute(id, creatureStats.getAttribute(id).getBase() + 10);
        }

        // skill bonus
        for (const ESM::Attribute& attribute : attributes)
        {
            float modifierSum = 0;

            for (const ESM::Skill& skill : OFBase::Environment::get().getESMStore()->get<ESM::Skill>())
            {
                if (skill.mData.mAttribute != attribute.mId)
                    continue;

                // is this a minor or major skill?
                float add = 0.2f;
                if (contains(npcClass->mData.mMajorSkills, skill.mId))
                    add = 1.0;
                else if (contains(npcClass->mData.mMinorSkills, skill.mId))
                    add = 0.5;
                modifierSum += add;
            }
            creatureStats.setAttribute(attribute.mId,
                std::min(
                    round_ieee_754(creatureStats.getAttribute(attribute.mId).getBase() + (level - 1) * modifierSum),
                    100.f));
        }

        // initial health
        float strength = creatureStats.getAttribute(ESM::Attribute::Strength).getBase();
        float endurance = creatureStats.getAttribute(ESM::Attribute::Endurance).getBase();

        int multiplier = 3;

        if (npcClass->mData.mSpecialization == ESM::Class::Combat)
            multiplier += 2;
        else if (npcClass->mData.mSpecialization == ESM::Class::Stealth)
            multiplier += 1;

        if (std::find(npcClass->mData.mAttribute.begin(), npcClass->mData.mAttribute.end(), ESM::Attribute::Endurance)
            != npcClass->mData.mAttribute.end())
            multiplier += 1;

        creatureStats.setHealth(floor(0.5f * (strength + endurance)) + multiplier * (creatureStats.getLevel() - 1));
    }

    /**
     * @brief autoCalculateSkills
     *
     * Skills are calculated with following formulae ( http://www.uesp.net/wiki/Morrowind:NPCs#Skills ):
     *
     * Skills: (Level - 1) × (Majority Multiplier + Specialization Multiplier)
     *
     *         The Majority Multiplier is 1.0 for a Major or Minor Skill, or 0.1 for a Miscellaneous Skill.
     *
     *         The Specialization Multiplier is 0.5 for a Skill in the same Specialization as the class,
     *         zero for other Skills.
     *
     * and by adding class, race, specialization bonus.
     */
    void autoCalculateSkills(
        const ESM::NPC* npc, const ESM::Race* race, OFMechanics::NpcStats& npcStats, bool spellsInitialised)
    {
        const ESM::Class* npcClass = OFBase::Environment::get().getESMStore()->get<ESM::Class>().find(npc->mClass);

        unsigned int level = npcStats.getLevel();

        for (const auto& id : npcClass->mData.mMinorSkills)
        {
            if (!id.empty())
                npcStats.getSkill(id).setBase(npcStats.getSkill(id).getBase() + 10);
        }
        for (const auto& id : npcClass->mData.mMajorSkills)
        {
            if (!id.empty())
                npcStats.getSkill(id).setBase(npcStats.getSkill(id).getBase() + 25);
        }

        for (const ESM::Skill& skill : OFBase::Environment::get().getESMStore()->get<ESM::Skill>())
        {
            float majorMultiplier = 0.1f;
            float specMultiplier = 0.0f;

            int raceBonus = 0;
            int specBonus = 0;

            auto bonusIt = std::find_if(race->mData.mBonus.begin(), race->mData.mBonus.end(),
                [&](const auto& bonus) { return bonus.mSkill == skill.mId; });
            if (bonusIt != race->mData.mBonus.end())
                raceBonus = bonusIt->mBonus;

            // is this a minor or major skill?
            if (contains(npcClass->mData.mMinorSkills, skill.mId) || contains(npcClass->mData.mMajorSkills, skill.mId))
                majorMultiplier = 1.0f;

            // is this skill in the same Specialization as the class?
            if (skill.mData.mSpecialization == npcClass->mData.mSpecialization)
            {
                specMultiplier = 0.5f;
                specBonus = 5;
            }

            npcStats.getSkill(skill.mId).setBase(
                std::min(round_ieee_754(npcStats.getSkill(skill.mId).getBase() + 5 + raceBonus + specBonus
                             + (int(level) - 1) * (majorMultiplier + specMultiplier)),
                    100.f)); // Must gracefully handle level 0
        }

        if (!spellsInitialised)
        {
            std::vector<const ESM::Spell*> spells
                = OFMechanics::autoCalcNpcSpells(npcStats.getSkills(), npcStats.getAttributes(), race);
            npcStats.getSpells().addAutoCalc(spells);
        }
    }
}

namespace OFClass
{
    Npc::Npc()
        : OFWorld::RegisteredClass<Npc, Actor>(ESM::NPC::sRecordId)
    {
    }

    class NpcCustomData : public OFWorld::TypedCustomData<NpcCustomData>
    {
    public:
        OFMechanics::NpcStats mNpcStats;
        OFMechanics::Movement mMovement;
        OFWorld::InventoryStore mInventoryStore;

        NpcCustomData& asNpcCustomData() override { return *this; }
        const NpcCustomData& asNpcCustomData() const override { return *this; }
    };

    const Npc::GMST& Npc::getGmst()
    {
        static const GMST staticGmst = [] {
            GMST gmst;

            const OFWorld::Store<ESM::GameSetting>& store
                = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

            gmst.fMinWalkSpeed = store.find("fMinWalkSpeed");
            gmst.fMaxWalkSpeed = store.find("fMaxWalkSpeed");
            gmst.fEncumberedMoveEffect = store.find("fEncumberedMoveEffect");
            gmst.fSneakSpeedMultiplier = store.find("fSneakSpeedMultiplier");
            gmst.fAthleticsRunBonus = store.find("fAthleticsRunBonus");
            gmst.fBaseRunMultiplier = store.find("fBaseRunMultiplier");
            gmst.fMinFlySpeed = store.find("fMinFlySpeed");
            gmst.fMaxFlySpeed = store.find("fMaxFlySpeed");
            gmst.fSwimRunBase = store.find("fSwimRunBase");
            gmst.fSwimRunAthleticsMult = store.find("fSwimRunAthleticsMult");
            gmst.fJumpEncumbranceBase = store.find("fJumpEncumbranceBase");
            gmst.fJumpEncumbranceMultiplier = store.find("fJumpEncumbranceMultiplier");
            gmst.fJumpAcrobaticsBase = store.find("fJumpAcrobaticsBase");
            gmst.fJumpAcroMultiplier = store.find("fJumpAcroMultiplier");
            gmst.fJumpRunMultiplier = store.find("fJumpRunMultiplier");
            gmst.fWereWolfRunMult = store.find("fWereWolfRunMult");
            gmst.fKnockDownMult = store.find("fKnockDownMult");
            gmst.iKnockDownOddsMult = store.find("iKnockDownOddsMult");
            gmst.iKnockDownOddsBase = store.find("iKnockDownOddsBase");
            gmst.fCombatArmorMinMult = store.find("fCombatArmorMinMult");

            return gmst;
        }();
        return staticGmst;
    }

    void Npc::ensureCustomData(const OFWorld::Ptr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
            auto tempData = std::make_unique<NpcCustomData>();
            NpcCustomData* data = tempData.get();
            OFMechanics::CreatureCustomDataResetter resetter{ ptr };
            ptr.getRefData().setCustomData(std::move(tempData));

            OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();

            const bool autoCalc = ref->mBase->mFlags & ESM::NPC::Autocalc;
            const bool spellsInitialised = data->mNpcStats.getSpells().setSpells(ref->mBase->mId, autoCalc);

            const ESM::Race* race = OFBase::Environment::get().getESMStore()->get<ESM::Race>().find(ref->mBase->mRace);
            // creature stats
            data->mNpcStats.setLevel(ref->mBase->mNpdt.mLevel);
            data->mNpcStats.setBaseDisposition(ref->mBase->mNpdt.mDisposition);
            data->mNpcStats.setReputation(ref->mBase->mNpdt.mReputation);
            if (!autoCalc)
            {
                for (const auto& [skill, value] : ref->mBase->mNpdt.mSkills)
                    data->mNpcStats.getSkill(skill).setBase(value);

                for (const auto& [attribute, value] : ref->mBase->mNpdt.mAttributes)
                    data->mNpcStats.setAttribute(attribute, value);

                data->mNpcStats.setHealth(ref->mBase->mNpdt.mHealth);
                data->mNpcStats.setMagicka(ref->mBase->mNpdt.mMana);
                data->mNpcStats.setFatigue(ref->mBase->mNpdt.mFatigue);
            }
            else
            {
                for (int i = 0; i < 3; ++i)
                    data->mNpcStats.setDynamic(i, 10);

                autoCalculateAttributes(ref->mBase, race, data->mNpcStats);
                autoCalculateSkills(ref->mBase, race, data->mNpcStats, spellsInitialised);
            }

            // Persistent actors with 0 health do not play death animation
            if (data->mNpcStats.isDead())
                data->mNpcStats.setDeathAnimationFinished(isPersistent(ptr));

            // race powers
            data->mNpcStats.getSpells().addAllToInstance(race->mPowers.mList);

            if (!ref->mBase->mFaction.empty())
            {
                static const int iAutoRepFacMod = OFBase::Environment::get()
                                                      .getESMStore()
                                                      ->get<ESM::GameSetting>()
                                                      .find("iAutoRepFacMod")
                                                      ->mValue.getInteger();
                static const int iAutoRepLevMod = OFBase::Environment::get()
                                                      .getESMStore()
                                                      ->get<ESM::GameSetting>()
                                                      .find("iAutoRepLevMod")
                                                      ->mValue.getInteger();
                int rank = ref->mBase->getFactionRank();

                data->mNpcStats.setReputation(
                    iAutoRepFacMod * (rank + 1) + iAutoRepLevMod * (data->mNpcStats.getLevel() - 1));
            }

            data->mNpcStats.getAiSequence().fill(ref->mBase->mAiPackage);

            data->mNpcStats.setAiSetting(OFMechanics::AiSetting::Hello, ref->mBase->mAiData.mHello);
            data->mNpcStats.setAiSetting(OFMechanics::AiSetting::Fight, ref->mBase->mAiData.mFight);
            data->mNpcStats.setAiSetting(OFMechanics::AiSetting::Flee, ref->mBase->mAiData.mFlee);
            data->mNpcStats.setAiSetting(OFMechanics::AiSetting::Alarm, ref->mBase->mAiData.mAlarm);

            // spells
            if (!spellsInitialised)
                data->mNpcStats.getSpells().addAllToInstance(ref->mBase->mSpells.mList);

            data->mNpcStats.setGoldPool(ref->mBase->mNpdt.mGold);

            // store
            resetter.mPtr = {};
            if (autoCalc)
                data->mNpcStats.recalculateMagicka();

            // inventory
            // setting ownership is used to make the NPC auto-equip his initial equipment only, and not bartered items
            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
            OFWorld::InventoryStore& inventory = getInventoryStore(ptr);
            inventory.setPtr(ptr);
            inventory.fill(ref->mBase->mInventory, ptr.getCellRef().getRefId(), prng);
            inventory.autoEquip();
        }
    }

    void Npc::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        renderingInterface.getObjects().insertNPC(ptr);
    }

    bool Npc::isPersistent(const OFWorld::ConstPtr& actor) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = actor.get<ESM::NPC>();
        return (ref->mBase->mRecordFlags & ESM::FLAG_Persistent) != 0;
    }

    VFS::Path::NormalizedView Npc::getModel(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();
        const VFS::Path::NormalizedView model = [&]() -> VFS::Path::NormalizedView {
            const ESM::Race* race = OFBase::Environment::get().getESMStore()->get<ESM::Race>().find(ref->mBase->mRace);
            if (race->mData.mFlags & ESM::Race::Beast)
                return Settings::models().mBaseanimkna.get();
            return Settings::models().mBaseanim.get();
        }();
        // Base animations should be in the meshes dir
        constexpr VFS::Path::NormalizedView prefix("meshes/");
        if (!model.value().starts_with(prefix.value()))
            throw std::runtime_error(std::format("NPC {} model path does not start with \"{}\": {}",
                ref->mRef.getRefId().toDebugString(), prefix.value(), model.value()));
        return VFS::Path::NormalizedView(model.value().substr(prefix.value().size()).data());
    }

    VFS::Path::Normalized Npc::getCorrectedModel(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();

        const ESM::Race* race = OFBase::Environment::get().getESMStore()->get<ESM::Race>().find(ref->mBase->mRace);
        if (race->mData.mFlags & ESM::Race::Beast)
            return Settings::models().mBaseanimkna.get();

        return Settings::models().mBaseanim.get();
    }

    void Npc::getModelsToPreload(const OFWorld::ConstPtr& ptr, std::vector<VFS::Path::NormalizedView>& models) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* npc = ptr.get<ESM::NPC>();
        const auto& esmStore = OFBase::Environment::get().getESMStore();
        models.push_back(getModel(ptr));

        if (!npc->mBase->mModel.empty())
            models.push_back(npc->mBase->mModel.getNormalized());

        if (!npc->mBase->mHead.empty())
        {
            const ESM::BodyPart* head = esmStore->get<ESM::BodyPart>().search(npc->mBase->mHead);
            if (head)
                models.push_back(head->mModel.getNormalized());
        }
        if (!npc->mBase->mHair.empty())
        {
            const ESM::BodyPart* hair = esmStore->get<ESM::BodyPart>().search(npc->mBase->mHair);
            if (hair)
                models.push_back(hair->mModel.getNormalized());
        }

        bool female = (npc->mBase->mFlags & ESM::NPC::Female);

        const OFWorld::CustomData* customData = ptr.getRefData().getCustomData();
        if (customData)
        {
            const OFWorld::InventoryStore& invStore = customData->asNpcCustomData().mInventoryStore;
            for (int slot = 0; slot < OFWorld::InventoryStore::Slots; ++slot)
            {
                OFWorld::ConstContainerStoreIterator equipped = invStore.getSlot(slot);
                if (equipped != invStore.end())
                {
                    const auto addParts = [&](const std::vector<ESM::PartReference>& parts) {
                        for (const ESM::PartReference& partRef : parts)
                        {
                            const ESM::RefId& partname
                                = (female && !partRef.mFemale.empty()) || (!female && partRef.mMale.empty())
                                ? partRef.mFemale
                                : partRef.mMale;

                            const ESM::BodyPart* part = esmStore->get<ESM::BodyPart>().search(partname);
                            if (part && !part->mModel.empty())
                                models.push_back(part->mModel.getNormalized());
                        }
                    };
                    if (equipped->getType() == ESM::Clothing::sRecordId)
                    {
                        const ESM::Clothing* clothes = equipped->get<ESM::Clothing>()->mBase;
                        addParts(clothes->mParts.mParts);
                    }
                    else if (equipped->getType() == ESM::Armor::sRecordId)
                    {
                        const ESM::Armor* armor = equipped->get<ESM::Armor>()->mBase;
                        addParts(armor->mParts.mParts);
                    }
                    else
                    {
                        const VFS::Path::NormalizedView model = equipped->getClass().getModel(*equipped);
                        if (!model.empty())
                            models.push_back(model);
                    }
                }
            }
        }

        // preload body parts
        if (const ESM::Race* race = esmStore->get<ESM::Race>().search(npc->mBase->mRace))
        {
            const std::vector<const ESM::BodyPart*>& parts
                = OFRender::NpcAnimation::getBodyParts(race->mId, female, false, false);
            for (const ESM::BodyPart* part : parts)
            {
                if (part && !part->mModel.empty())
                    models.push_back(part->mModel.getNormalized());
            }
        }
    }

    std::string_view Npc::getName(const OFWorld::ConstPtr& ptr) const
    {
        if (ptr.getRefData().getCustomData()
            && ptr.getRefData().getCustomData()->asNpcCustomData().mNpcStats.isWerewolf())
        {
            const OFWorld::Store<ESM::GameSetting>& store
                = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

            return store.find("sWerewolfPopup")->mValue.getString();
        }

        return getNameOrId<ESM::NPC>(ptr);
    }

    OFMechanics::CreatureStats& Npc::getCreatureStats(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);

        return ptr.getRefData().getCustomData()->asNpcCustomData().mNpcStats;
    }

    OFMechanics::NpcStats& Npc::getNpcStats(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);

        return ptr.getRefData().getCustomData()->asNpcCustomData().mNpcStats;
    }

    bool Npc::evaluateHit(const OFWorld::Ptr& ptr, OFWorld::Ptr& victim, osg::Vec3f& hitPosition) const
    {
        victim = OFWorld::Ptr();
        hitPosition = osg::Vec3f();

        // Get the weapon used (if hand-to-hand, weapon = inv.end())
        OFWorld::InventoryStore& inv = getInventoryStore(ptr);
        OFWorld::ContainerStoreIterator weaponslot = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
        OFWorld::Ptr weapon;
        if (weaponslot != inv.end() && weaponslot->getType() == ESM::Weapon::sRecordId)
            weapon = *weaponslot;

        OFBase::World* world = OFBase::Environment::get().getWorld();

        const float dist = OFMechanics::getMeleeWeaponReach(ptr, weapon);
        const std::pair<OFWorld::Ptr, osg::Vec3f> result = OFMechanics::getHitContact(ptr, dist);
        if (result.first.isEmpty()) // Didn't hit anything
            return true;

        // Note that earlier we returned true in spite of an apparent failure to hit anything alive.
        // This is because hitting nothing is not a "miss" and should be handled as such character controller-side.
        victim = result.first;
        hitPosition = result.second;

        ESM::RefId weapskill = ESM::Skill::HandToHand;
        if (!weapon.isEmpty())
            weapskill = weapon.getClass().getEquipmentSkill(weapon);

        float hitchance = OFMechanics::getHitChance(ptr, victim, static_cast<int>(getSkill(ptr, weapskill)));

        return Misc::Rng::roll0to99(world->getPrng()) < hitchance;
    }

    void Npc::hit(const OFWorld::Ptr& ptr, float attackStrength, float attackWindUp, int type,
        const OFWorld::Ptr& victim, const osg::Vec3f& hitPosition, bool success) const
    {
        OFWorld::InventoryStore& inv = getInventoryStore(ptr);
        OFWorld::ContainerStoreIterator weaponslot = inv.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
        OFWorld::Ptr weapon;
        if (weaponslot != inv.end() && weaponslot->getType() == ESM::Weapon::sRecordId)
            weapon = *weaponslot;

        OFMechanics::applyFatigueLoss(ptr, weapon, attackStrength);

        if (victim.isEmpty()) // Didn't hit anything
            return;

        const OFWorld::Class& othercls = victim.getClass();
        OFMechanics::CreatureStats& otherstats = othercls.getCreatureStats(victim);
        if (otherstats.isDead()) // Can't hit dead actors
            return;

        if (!OFMechanics::isInMeleeReach(ptr, victim, OFMechanics::getMeleeWeaponReach(ptr, weapon)))
            return;

        if (ptr == OFMechanics::getPlayer())
            OFBase::Environment::get().getWindowManager()->setEnemy(victim);

        float damage = 0.0f;
        if (!success)
        {
            OFBase::Environment::get().getLuaManager()->onHit(ptr, victim, weapon, OFWorld::Ptr(), type, attackStrength,
                attackWindUp, damage, false, hitPosition, false, OFMechanics::DamageSourceType::Melee);
            OFMechanics::reduceWeaponCondition(damage, false, weapon, ptr);
            OFMechanics::resistNormalWeapon(victim, ptr, weapon, damage);
            return;
        }

        bool healthdmg;
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
            }
            OFMechanics::adjustWeaponDamage(damage, weapon, ptr);
            OFMechanics::reduceWeaponCondition(damage, true, weapon, ptr);
            OFMechanics::resistNormalWeapon(victim, ptr, weapon, damage);
            OFMechanics::applyWerewolfDamageMult(victim, weapon, damage);
            healthdmg = true;
        }
        else
        {
            OFMechanics::getHandToHandDamage(ptr, victim, damage, healthdmg, attackStrength);
        }

        OFBase::World* world = OFBase::Environment::get().getWorld();
        const OFWorld::Store<ESM::GameSetting>& store = world->getStore().get<ESM::GameSetting>();

        if (ptr == OFMechanics::getPlayer())
        {
            ESM::RefId weapskill = ESM::Skill::HandToHand;
            if (!weapon.isEmpty())
                weapskill = weapon.getClass().getEquipmentSkill(weapon);
            skillUsageSucceeded(ptr, weapskill, ESM::Skill::Weapon_SuccessfulHit);

            const OFMechanics::AiSequence& seq = victim.getClass().getCreatureStats(victim).getAiSequence();

            bool unaware
                = !seq.isInCombat() && !OFBase::Environment::get().getMechanicsManager()->awarenessCheck(ptr, victim);
            if (unaware)
            {
                damage *= store.find("fCombatCriticalStrikeMult")->mValue.getFloat();
                OFBase::Environment::get().getWindowManager()->messageBox("#{sTargetCriticalStrike}");
                if (healthdmg)
                {
                    OFBase::Environment::get().getSoundManager()->playSound3D(
                        victim, ESM::RefId::stringRefId("critical damage"), 1.0f, 1.0f);
                }
            }
        }

        if (othercls.getCreatureStats(victim).getKnockedDown())
            damage *= store.find("fCombatKODamageMult")->mValue.getFloat();

        // Apply "On hit" enchanted weapons
        OFMechanics::applyOnStrikeEnchantment(ptr, victim, weapon, hitPosition);

        OFMechanics::applyElementalShields(ptr, victim);

        if (OFMechanics::blockMeleeAttack(ptr, victim, weapon, damage, attackStrength))
            damage = 0;

        if (victim == OFMechanics::getPlayer() && OFBase::Environment::get().getWorld()->getGodModeState())
            damage = 0;

        OFMechanics::diseaseContact(victim, ptr);

        OFBase::Environment::get().getLuaManager()->onHit(ptr, victim, weapon, OFWorld::Ptr(), type, attackStrength,
            attackWindUp, damage, healthdmg, hitPosition, true, OFMechanics::DamageSourceType::Melee);
    }

    void Npc::onHit(const OFWorld::Ptr& ptr, const std::map<std::string, float>& damages, ESM::RefId object,
        const OFWorld::Ptr& attacker, bool successful, const OFMechanics::DamageSourceType sourceType) const
    {
        OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        bool wasDead = stats.isDead();

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
            const ESM::RefId& script = getScript(ptr);
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

        if (ptr == OFMechanics::getPlayer() && OFBase::Environment::get().getWorld()->getGodModeState())
            return;

        bool hasDamage = false;
        bool hasHealthDamage = false;
        float healthDamage = 0.f;
        for (auto& [stat, damage] : damages)
        {
            if (damage < 0.001f)
                continue;
            hasDamage = true;

            if (stat == "health")
            {
                hasHealthDamage = true;
                healthDamage = damage;
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

        if (hasDamage && !attacker.isEmpty())
        {
            // 'ptr' is losing health. Play a 'hit' voiced dialog entry if not already saying
            // something, alert the character controller, scripts, etc.
            const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();
            int chance = store.get<ESM::GameSetting>().find("iVoiceHitOdds")->mValue.getInteger();
            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
            if (Misc::Rng::roll0to99(prng) < chance)
                OFBase::Environment::get().getDialogueManager()->say(ptr, ESM::RefId::stringRefId("hit"));
        }

        if (hasHealthDamage && healthDamage > 0.0f)
        {
            if (ptr == OFMechanics::getPlayer())
                OFBase::Environment::get().getWindowManager()->activateHitOverlay();
        }

        if (!wasDead && getCreatureStats(ptr).isDead())
        {
            // NPC was killed
            if (!attacker.isEmpty() && attacker.getClass().isNpc()
                && attacker.getClass().getNpcStats(attacker).isWerewolf())
            {
                attacker.getClass().getNpcStats(attacker).addWerewolfKill();
            }

            OFBase::Environment::get().getMechanicsManager()->actorKilled(ptr, attacker);
        }
    }

    std::unique_ptr<OFWorld::Action> Npc::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        // player got activated by another NPC
        if (ptr == OFMechanics::getPlayer())
            return std::make_unique<OFWorld::ActionTalk>(actor);

        // Werewolfs can't activate NPCs
        std::unique_ptr<OFWorld::Action> werewolfAction = getWerewolfRefusalAction(actor);
        if (werewolfAction)
            return werewolfAction;

        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        const OFMechanics::AiSequence& aiSequence = stats.getAiSequence();
        const bool isPursuing = aiSequence.isInPursuit() && actor == OFMechanics::getPlayer();
        const bool inCombatWithActor = aiSequence.isInCombat(actor) || isPursuing;

        if (stats.isDead())
        {
            // by default user can loot non-fighting actors during death animation
            if (Settings::game().mCanLootDuringDeathAnimation)
                return std::make_unique<OFWorld::ActionOpen>(ptr);

            // otherwise wait until death animation
            if (stats.isDeathAnimationFinished())
                return std::make_unique<OFWorld::ActionOpen>(ptr);
        }
        else
        {
            const bool allowStealingFromKO
                = Settings::game().mAlwaysAllowStealingFromKnockedOutActors || !inCombatWithActor;
            if (stats.getKnockedDown() && allowStealingFromKO)
                return std::make_unique<OFWorld::ActionOpen>(ptr);

            const bool allowStealingWhileSneaking = !inCombatWithActor;
            if (OFBase::Environment::get().getMechanicsManager()->isSneaking(actor) && allowStealingWhileSneaking)
                return std::make_unique<OFWorld::ActionOpen>(ptr);

            const bool allowTalking = !inCombatWithActor && !getNpcStats(ptr).isWerewolf();
            if (allowTalking)
                return std::make_unique<OFWorld::ActionTalk>(ptr);
        }

        if (inCombatWithActor)
            return std::make_unique<OFWorld::FailedAction>("#{sActorInCombat}");

        return std::make_unique<OFWorld::FailedAction>();
    }

    OFWorld::ContainerStore& Npc::getContainerStore(const OFWorld::Ptr& ptr) const
    {
        return getInventoryStore(ptr);
    }

    OFWorld::InventoryStore& Npc::getInventoryStore(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return ptr.getRefData().getCustomData()->asNpcCustomData().mInventoryStore;
    }

    ESM::RefId Npc::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();

        return ref->mBase->mScript;
    }

    float Npc::getMaxSpeed(const OFWorld::Ptr& ptr) const
    {
        // TODO: This function is called several times per frame for each NPC.
        // It would be better to calculate it only once per frame for each NPC and save the result in CreatureStats.
        const OFMechanics::NpcStats& stats = getNpcStats(ptr);
        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;

        const OFBase::World* world = OFBase::Environment::get().getWorld();
        const GMST& gmst = getGmst();

        const OFMechanics::MagicEffects& mageffects = stats.getMagicEffects();

        const float normalizedEncumbrance = getNormalizedEncumbrance(ptr);
        const bool running = OFBase::Environment::get().getMechanicsManager()->isRunning(ptr);

        float moveSpeed;
        if (normalizedEncumbrance > 1.0f)
            moveSpeed = 0.0f;
        else if (mageffects.getOrDefault(ESM::MagicEffect::Levitate).getMagnitude() > 0 && world->isLevitationEnabled())
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
        else if (running && !OFBase::Environment::get().getMechanicsManager()->isSneaking(ptr))
            moveSpeed = getRunSpeed(ptr);
        else
            moveSpeed = getWalkSpeed(ptr);

        if (stats.isWerewolf() && running && stats.getDrawState() == OFMechanics::DrawState::Nothing)
            moveSpeed *= gmst.fWereWolfRunMult->mValue.getFloat();

        return moveSpeed;
    }

    float Npc::getJump(const OFWorld::Ptr& ptr) const
    {
        const float normalizedEncumbrance = getNormalizedEncumbrance(ptr);
        if (normalizedEncumbrance > 1.0f)
            return 0.f;

        const OFMechanics::NpcStats& stats = getNpcStats(ptr);
        if (stats.isParalyzed() || stats.getKnockedDown() || stats.isDead())
            return 0.f;

        const GMST& gmst = getGmst();
        const OFMechanics::MagicEffects& mageffects = stats.getMagicEffects();
        const float encumbranceTerm = gmst.fJumpEncumbranceBase->mValue.getFloat()
            + gmst.fJumpEncumbranceMultiplier->mValue.getFloat() * (1.0f - normalizedEncumbrance);

        float a = getSkill(ptr, ESM::Skill::Acrobatics);
        float b = 0.0f;
        if (a > 50.0f)
        {
            b = a - 50.0f;
            a = 50.0f;
        }

        float x = gmst.fJumpAcrobaticsBase->mValue.getFloat()
            + std::pow(a / 15.0f, gmst.fJumpAcroMultiplier->mValue.getFloat());
        x += 3.0f * b * gmst.fJumpAcroMultiplier->mValue.getFloat();
        x += mageffects.getOrDefault(ESM::MagicEffect::Jump).getMagnitude() * 64;
        x *= encumbranceTerm;

        if (stats.getStance(OFMechanics::CreatureStats::Stance_Run))
            x *= gmst.fJumpRunMultiplier->mValue.getFloat();
        x *= stats.getFatigueTerm();
        x -= -Constants::GravityConst * Constants::UnitsPerMeter;
        x /= 3.0f;

        return x;
    }

    OFMechanics::Movement& Npc::getMovementSettings(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);

        return ptr.getRefData().getCustomData()->asNpcCustomData().mMovement;
    }

    bool Npc::isEssential(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();

        return (ref->mBase->mFlags & ESM::NPC::Essential) != 0;
    }

    bool Npc::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        if (!ptr.getRefData().getCustomData() || OFBase::Environment::get().getWindowManager()->isGuiMode())
            return true;

        const NpcCustomData& customData = ptr.getRefData().getCustomData()->asNpcCustomData();

        if (customData.mNpcStats.isDead() && customData.mNpcStats.isDeathAnimationFinished())
            return true;

        const OFMechanics::AiSequence& aiSeq = customData.mNpcStats.getAiSequence();
        if (!aiSeq.isInCombat() || aiSeq.isFleeing())
            return true;

        if (Settings::game().mAlwaysAllowStealingFromKnockedOutActors && customData.mNpcStats.getKnockedDown())
            return true;

        return false;
    }

    OFGui::ToolTipInfo Npc::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();

        bool fullHelp = OFBase::Environment::get().getWindowManager()->getFullHelp();
        OFGui::ToolTipInfo info;

        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name));
        if (fullHelp && !ref->mBase->mName.empty() && ptr.getRefData().getCustomData()
            && ptr.getRefData().getCustomData()->asNpcCustomData().mNpcStats.isWerewolf())
        {
            info.caption += " (";
            info.caption += MyGUI::TextIterator::toTagsString(ref->mBase->mName);
            info.caption += ")";
        }

        if (fullHelp)
            info.extra = OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");

        return info;
    }

    float Npc::getCapacity(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::CreatureStats& stats = getCreatureStats(ptr);
        static const float fEncumbranceStrMult = OFBase::Environment::get()
                                                     .getESMStore()
                                                     ->get<ESM::GameSetting>()
                                                     .find("fEncumbranceStrMult")
                                                     ->mValue.getFloat();
        return stats.getAttribute(ESM::Attribute::Strength).getModified() * fEncumbranceStrMult;
    }

    float Npc::getEncumbrance(const OFWorld::Ptr& ptr) const
    {
        // According to UESP, inventory weight is ignored in werewolf form. Does that include
        // feather and burden effects?
        return getNpcStats(ptr).isWerewolf() ? 0.0f : Actor::getEncumbrance(ptr);
    }

    void Npc::skillUsageSucceeded(const OFWorld::Ptr& ptr, ESM::RefId skill, int usageType, float extraFactor) const
    {
        OFBase::Environment::get().getLuaManager()->skillUse(ptr, skill, usageType, extraFactor);
    }

    float Npc::getArmorRating(const OFWorld::Ptr& ptr, bool useLuaInterfaceIfAvailable) const
    {
        if (useLuaInterfaceIfAvailable && ptr == OFMechanics::getPlayer())
        {
            auto res = OFLua::LocalScripts::callPlayerInterface<float>("Combat", "getArmorRating");
            if (res)
                return res.value();
        }

        // Fallback to the old engine implementation when actors don't have their scripts attached yet.

        const OFWorld::Store<ESM::GameSetting>& store
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        OFMechanics::NpcStats& stats = getNpcStats(ptr);
        const OFWorld::InventoryStore& invStore = getInventoryStore(ptr);

        float fUnarmoredBase1 = store.find("fUnarmoredBase1")->mValue.getFloat();
        float fUnarmoredBase2 = store.find("fUnarmoredBase2")->mValue.getFloat();
        float unarmoredSkill = getSkill(ptr, ESM::Skill::Unarmored);

        float ratings[OFWorld::InventoryStore::Slots];
        for (int i = 0; i < OFWorld::InventoryStore::Slots; i++)
        {
            OFWorld::ConstContainerStoreIterator it = invStore.getSlot(i);
            if (it == invStore.end() || it->getType() != ESM::Armor::sRecordId)
            {
                // unarmored
                ratings[i] = (fUnarmoredBase1 * unarmoredSkill) * (fUnarmoredBase2 * unarmoredSkill);
            }
            else
            {
                ratings[i] = it->getClass().getSkillAdjustedArmorRating(*it, ptr);

                // Take in account armor condition
                const bool hasHealth = it->getClass().hasItemHealth(*it);
                if (hasHealth)
                {
                    ratings[i] *= it->getClass().getItemNormalizedHealth(*it);
                }
            }
        }

        float shield = stats.getMagicEffects().getOrDefault(ESM::MagicEffect::Shield).getMagnitude();

        return ratings[OFWorld::InventoryStore::Slot_Cuirass] * 0.3f
            + (ratings[OFWorld::InventoryStore::Slot_CarriedLeft] + ratings[OFWorld::InventoryStore::Slot_Helmet]
                  + ratings[OFWorld::InventoryStore::Slot_Greaves] + ratings[OFWorld::InventoryStore::Slot_Boots]
                  + ratings[OFWorld::InventoryStore::Slot_LeftPauldron]
                  + ratings[OFWorld::InventoryStore::Slot_RightPauldron])
            * 0.1f
            + (ratings[OFWorld::InventoryStore::Slot_LeftGauntlet]
                  + ratings[OFWorld::InventoryStore::Slot_RightGauntlet])
            * 0.05f
            + shield;
    }

    void Npc::adjustScale(const OFWorld::ConstPtr& ptr, osg::Vec3f& scale, bool rendering) const
    {
        if (!rendering)
            return; // collision meshes are not scaled based on race height
                    // having the same collision extents for all races makes the environments easier to test

        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();

        const ESM::Race* race = OFBase::Environment::get().getESMStore()->get<ESM::Race>().find(ref->mBase->mRace);

        // Race weight should not affect 1st-person meshes, otherwise it will change hand proportions and can break
        // aiming.
        if (ptr == OFMechanics::getPlayer() && ptr.isInCell() && OFBase::Environment::get().getWorld()->isFirstPerson())
        {
            if (ref->mBase->isMale())
                scale *= race->mData.mMaleHeight;
            else
                scale *= race->mData.mFemaleHeight;

            return;
        }

        if (ref->mBase->isMale())
        {
            scale.x() *= race->mData.mMaleWeight;
            scale.y() *= race->mData.mMaleWeight;
            scale.z() *= race->mData.mMaleHeight;
        }
        else
        {
            scale.x() *= race->mData.mFemaleWeight;
            scale.y() *= race->mData.mFemaleWeight;
            scale.z() *= race->mData.mFemaleHeight;
        }
    }

    int Npc::getServices(const OFWorld::ConstPtr& actor) const
    {
        const ESM::NPC* npc = actor.get<ESM::NPC>()->mBase;
        if (npc->mFlags & ESM::NPC::Autocalc)
        {
            const ESM::Class* npcClass = OFBase::Environment::get().getESMStore()->get<ESM::Class>().find(npc->mClass);
            return npcClass->mData.mServices;
        }
        return npc->mAiData.mServices;
    }

    ESM::RefId Npc::getSoundIdFromSndGen(const OFWorld::Ptr& ptr, std::string_view name) const
    {
        if (name == "left" || name == "right")
        {
            OFBase::World* world = OFBase::Environment::get().getWorld();
            if (world->isFlying(ptr))
                return ESM::RefId();
            osg::Vec3f pos(ptr.getRefData().getPosition().asVec3());
            if (world->isSwimming(ptr))
                return (name == "left") ? npcParts.mSwimLeft : npcParts.mSwimRight;
            if (world->isUnderwater(ptr.getCell(), pos) || world->isWalkingOnWater(ptr))
                return (name == "left") ? npcParts.mFootWaterLeft : npcParts.mFootWaterRight;
            if (world->isOnGround(ptr))
            {
                const OFWorld::InventoryStore& inv = Npc::getInventoryStore(ptr);
                OFWorld::ConstContainerStoreIterator boots = inv.getSlot(OFWorld::InventoryStore::Slot_Boots);
                if (boots == inv.end() || boots->getType() != ESM::Armor::sRecordId)
                    return (name == "left") ? npcParts.mFootBareLeft : npcParts.mFootBareRight;

                ESM::RefId skill = boots->getClass().getEquipmentSkill(*boots);
                if (skill == ESM::Skill::LightArmor)
                    return (name == "left") ? npcParts.mFootLightLeft : npcParts.mFootLightRight;
                else if (skill == ESM::Skill::MediumArmor)
                    return (name == "left") ? npcParts.mFootMediumLeft : npcParts.mFootMediumRight;
                else if (skill == ESM::Skill::HeavyArmor)
                    return (name == "left") ? npcParts.mFootHeavyLeft : npcParts.mFootHeavyRight;
            }
            return ESM::RefId();
        }

        // Morrowind ignores land soundgen for NPCs
        if (name == "land")
            return ESM::RefId();
        if (name == "swimleft")
            return npcParts.mSwimLeft;
        if (name == "swimright")
            return npcParts.mSwimRight;
        // TODO: I have no idea what these are supposed to do for NPCs since they use
        // voiced dialog for various conditions like health loss and combat taunts. Maybe
        // only for biped creatures?

        if (name == "moan")
            return ESM::RefId();
        if (name == "roar")
            return ESM::RefId();
        if (name == "scream")
            return ESM::RefId();

        throw std::runtime_error("Unexpected soundgen type: " + std::string(name));
    }

    OFWorld::Ptr Npc::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();
        OFWorld::Ptr newPtr(cell.insert(ref), &cell);
        if (newPtr.getRefData().getCustomData())
        {
            OFBase::Environment::get().getWorldModel()->registerPtr(newPtr);
            newPtr.getClass().getContainerStore(newPtr).setPtr(newPtr);
        }
        return newPtr;
    }

    float Npc::getSkill(const OFWorld::Ptr& ptr, ESM::RefId id) const
    {
        return getNpcStats(ptr).getSkill(id).getModified();
    }

    void Npc::readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const
    {
        if (!state.mHasCustomState)
            return;

        const ESM::NpcState& npcState = state.asNpcState();

        if (!ptr.getRefData().getCustomData())
        {
            if (npcState.mCreatureStats.mMissingACDT)
                ensureCustomData(ptr);
            else
            {
                // Create a CustomData, but don't fill it from ESM records (not needed)
                auto data = std::make_unique<NpcCustomData>();
                OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
                data->mInventoryStore.setPtr(ptr);
                ptr.getRefData().setCustomData(std::move(data));
            }
        }

        NpcCustomData& customData = ptr.getRefData().getCustomData()->asNpcCustomData();

        customData.mInventoryStore.readState(npcState.mInventory);
        const ESM::NPC* base = ptr.get<ESM::NPC>()->mBase;
        const bool autoCalc = base->mFlags & ESM::NPC::Autocalc;
        const bool spellsInitialised = customData.mNpcStats.getSpells().setSpells(base->mId, autoCalc);
        if (!spellsInitialised && autoCalc)
        {
            customData.mNpcStats.setLevel(base->mNpdt.mLevel);
            const ESM::Race* race = OFBase::Environment::get().getESMStore()->get<ESM::Race>().find(base->mRace);
            autoCalculateAttributes(base, race, customData.mNpcStats);
            autoCalculateSkills(base, race, customData.mNpcStats, spellsInitialised);
            customData.mNpcStats.getSpells().addAllToInstance(race->mPowers.mList);
        }
        customData.mNpcStats.readState(npcState.mNpcStats);
        if (spellsInitialised)
            customData.mNpcStats.getSpells().clear();
        customData.mNpcStats.readState(npcState.mCreatureStats);
    }

    void Npc::writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            state.mHasCustomState = false;
            return;
        }

        const NpcCustomData& customData = ptr.getRefData().getCustomData()->asNpcCustomData();
        if (ptr.getCellRef().getCount() <= 0
            && (!(ptr.get<ESM::NPC>()->mBase->mFlags & ESM::NPC::Respawn) || !customData.mNpcStats.isDead()))
        {
            state.mHasCustomState = false;
            return;
        }

        ESM::NpcState& npcState = state.asNpcState();
        customData.mInventoryStore.writeState(npcState.mInventory);
        customData.mNpcStats.writeState(npcState.mNpcStats);
        customData.mNpcStats.writeState(npcState.mCreatureStats);
    }

    int Npc::getBaseGold(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();
        return ref->mBase->mNpdt.mGold;
    }

    bool Npc::isClass(const OFWorld::ConstPtr& ptr, std::string_view className) const
    {
        return ptr.get<ESM::NPC>()->mBase->mClass == className;
    }

    bool Npc::canSwim(const OFWorld::ConstPtr& ptr) const
    {
        return true;
    }

    bool Npc::canWalk(const OFWorld::ConstPtr& ptr) const
    {
        return true;
    }

    void Npc::respawn(const OFWorld::Ptr& ptr) const
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

        if (ptr.get<ESM::NPC>()->mBase->mFlags & ESM::NPC::Respawn
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

    int Npc::getBaseFightRating(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();
        return ref->mBase->mAiData.mFight;
    }

    bool Npc::isBipedal(const OFWorld::ConstPtr& ptr) const
    {
        return true;
    }

    ESM::RefId Npc::getPrimaryFaction(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();
        return ref->mBase->mFaction;
    }

    int Npc::getPrimaryFactionRank(const OFWorld::ConstPtr& ptr) const
    {
        const ESM::RefId& factionID = ptr.getClass().getPrimaryFaction(ptr);
        if (factionID.empty())
            return -1;

        // Search in the NPC data first
        if (const OFWorld::CustomData* data = ptr.getRefData().getCustomData())
        {
            int rank = data->asNpcCustomData().mNpcStats.getFactionRank(factionID);
            if (rank >= 0)
                return rank;
        }

        // Use base NPC record as a fallback
        const OFWorld::LiveCellRef<ESM::NPC>* ref = ptr.get<ESM::NPC>();
        return ref->mBase->getFactionRank();
    }

    void Npc::setBaseAISetting(const ESM::RefId& id, OFMechanics::AiSetting setting, int value) const
    {
        OFMechanics::setBaseAISetting<ESM::NPC>(id, setting, static_cast<unsigned char>(value));
    }

    void Npc::modifyBaseInventory(const ESM::RefId& actorId, const ESM::RefId& itemId, int amount) const
    {
        OFMechanics::modifyBaseInventory<ESM::NPC>(actorId, itemId, amount);
    }

    float Npc::getWalkSpeed(const OFWorld::Ptr& ptr) const
    {
        const GMST& gmst = getGmst();
        const OFMechanics::NpcStats& stats = getNpcStats(ptr);
        const float normalizedEncumbrance = getNormalizedEncumbrance(ptr);
        const bool sneaking = OFBase::Environment::get().getMechanicsManager()->isSneaking(ptr);

        float walkSpeed = gmst.fMinWalkSpeed->mValue.getFloat()
            + 0.01f * stats.getAttribute(ESM::Attribute::Speed).getModified()
                * (gmst.fMaxWalkSpeed->mValue.getFloat() - gmst.fMinWalkSpeed->mValue.getFloat());
        walkSpeed *= 1.0f - gmst.fEncumberedMoveEffect->mValue.getFloat() * normalizedEncumbrance;
        walkSpeed = std::max(0.0f, walkSpeed);
        if (sneaking)
            walkSpeed *= gmst.fSneakSpeedMultiplier->mValue.getFloat();

        return walkSpeed;
    }

    float Npc::getRunSpeed(const OFWorld::Ptr& ptr) const
    {
        const GMST& gmst = getGmst();
        return getWalkSpeed(ptr)
            * (0.01f * getSkill(ptr, ESM::Skill::Athletics) * gmst.fAthleticsRunBonus->mValue.getFloat()
                + gmst.fBaseRunMultiplier->mValue.getFloat());
    }

    float Npc::getSwimSpeed(const OFWorld::Ptr& ptr) const
    {
        const OFMechanics::MagicEffects& effects = getNpcStats(ptr).getMagicEffects();
        const bool running = OFBase::Environment::get().getMechanicsManager()->isRunning(ptr);
        return getSwimSpeedImpl(ptr, getGmst(), effects, running ? getRunSpeed(ptr) : getWalkSpeed(ptr));
    }
}
