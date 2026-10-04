#include "armor.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrace.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwlua/localscripts.hpp"

#include "../mwworld/actionequip.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/weapontype.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwgui/tooltips.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Armor::Armor()
        : OFWorld::RegisteredClass<Armor>(ESM::Armor::sRecordId)
    {
    }

    void Armor::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Armor::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Armor>(ptr);
    }

    std::string_view Armor::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Armor>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Armor::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    bool Armor::hasItemHealth(const OFWorld::ConstPtr& ptr) const
    {
        return true;
    }

    int Armor::getItemMaxHealth(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return ref->mBase->mData.mHealth;
    }

    ESM::RefId Armor::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return ref->mBase->mScript;
    }

    std::pair<std::vector<int>, bool> Armor::getEquipmentSlots(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        std::vector<int> slots;

        const int size = 11;

        static const int sMapping[size][2] = { { ESM::Armor::Helmet, OFWorld::InventoryStore::Slot_Helmet },
            { ESM::Armor::Cuirass, OFWorld::InventoryStore::Slot_Cuirass },
            { ESM::Armor::LPauldron, OFWorld::InventoryStore::Slot_LeftPauldron },
            { ESM::Armor::RPauldron, OFWorld::InventoryStore::Slot_RightPauldron },
            { ESM::Armor::Greaves, OFWorld::InventoryStore::Slot_Greaves },
            { ESM::Armor::Boots, OFWorld::InventoryStore::Slot_Boots },
            { ESM::Armor::LGauntlet, OFWorld::InventoryStore::Slot_LeftGauntlet },
            { ESM::Armor::RGauntlet, OFWorld::InventoryStore::Slot_RightGauntlet },
            { ESM::Armor::Shield, OFWorld::InventoryStore::Slot_CarriedLeft },
            { ESM::Armor::LBracer, OFWorld::InventoryStore::Slot_LeftGauntlet },
            { ESM::Armor::RBracer, OFWorld::InventoryStore::Slot_RightGauntlet } };

        for (int i = 0; i < size; ++i)
            if (sMapping[i][0] == ref->mBase->mData.mType)
            {
                slots.push_back(int(sMapping[i][1]));
                break;
            }

        return std::make_pair(slots, false);
    }

    ESM::RefId Armor::getEquipmentSkill(const OFWorld::ConstPtr& ptr, bool useLuaInterfaceIfAvailable) const
    {
        // We don't actually need an actor as such. We just need an object that has
        // lua scripts and the Combat interface.
        if (useLuaInterfaceIfAvailable)
        {
            // In this interface call, both objects are effectively const, so stripping Const from the ConstPtr is fine.
            OFWorld::Ptr mutablePtr(
                const_cast<OFWorld::LiveCellRefBase*>(ptr.mRef), const_cast<OFWorld::CellStore*>(ptr.mCell));
            auto res = OFLua::LocalScripts::callPlayerInterface<std::string>(
                "Combat", "getArmorSkill", OFLua::LObject(mutablePtr));
            if (res)
                return ESM::RefId::deserializeText(res.value());
        }

        // Fallback to the old engine implementation when actors don't have their scripts attached yet.

        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        std::string_view typeGmst;

        switch (ref->mBase->mData.mType)
        {
            case ESM::Armor::Helmet:
                typeGmst = "iHelmWeight";
                break;
            case ESM::Armor::Cuirass:
                typeGmst = "iCuirassWeight";
                break;
            case ESM::Armor::LPauldron:
            case ESM::Armor::RPauldron:
                typeGmst = "iPauldronWeight";
                break;
            case ESM::Armor::Greaves:
                typeGmst = "iGreavesWeight";
                break;
            case ESM::Armor::Boots:
                typeGmst = "iBootsWeight";
                break;
            case ESM::Armor::LGauntlet:
            case ESM::Armor::RGauntlet:
                typeGmst = "iGauntletWeight";
                break;
            case ESM::Armor::Shield:
                typeGmst = "iShieldWeight";
                break;
            case ESM::Armor::LBracer:
            case ESM::Armor::RBracer:
                typeGmst = "iGauntletWeight";
                break;
        }

        if (typeGmst.empty())
            return {};

        const OFWorld::Store<ESM::GameSetting>& gmst
            = OFBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

        float iWeight = floor(gmst.find(typeGmst)->mValue.getFloat());

        float epsilon = 0.0005f;

        if (ref->mBase->mData.mWeight <= iWeight * gmst.find("fLightMaxMod")->mValue.getFloat() + epsilon)
            return ESM::Skill::LightArmor;

        if (ref->mBase->mData.mWeight <= iWeight * gmst.find("fMedMaxMod")->mValue.getFloat() + epsilon)
            return ESM::Skill::MediumArmor;

        else
            return ESM::Skill::HeavyArmor;
    }

    int Armor::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Armor::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        const ESM::RefId es = getEquipmentSkill(ptr, false);
        static const ESM::RefId lightUp = ESM::RefId::stringRefId("Item Armor Light Up");
        static const ESM::RefId mediumUp = ESM::RefId::stringRefId("Item Armor Medium Up");
        static const ESM::RefId heavyUp = ESM::RefId::stringRefId("Item Armor Heavy Up");

        if (es == ESM::Skill::LightArmor)
            return lightUp;
        else if (es == ESM::Skill::MediumArmor)
            return mediumUp;
        else
            return heavyUp;
    }

    const ESM::RefId& Armor::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        const ESM::RefId es = getEquipmentSkill(ptr, false);
        static const ESM::RefId lightDown = ESM::RefId::stringRefId("Item Armor Light Down");
        static const ESM::RefId mediumDown = ESM::RefId::stringRefId("Item Armor Medium Down");
        static const ESM::RefId heavyDown = ESM::RefId::stringRefId("Item Armor Heavy Down");
        if (es == ESM::Skill::LightArmor)
            return lightDown;
        else if (es == ESM::Skill::MediumArmor)
            return mediumDown;
        else
            return heavyDown;
    }

    VFS::Path::NormalizedView Armor::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Armor::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;

        // get armor type string (light/medium/heavy)
        std::string typeText;
        if (ref->mBase->mData.mWeight == 0)
        {
            // no type
        }
        else
        {
            const ESM::RefId armorType = getEquipmentSkill(ptr, true);
            if (armorType == ESM::Skill::LightArmor)
                typeText = "#{sLight}";
            else if (armorType == ESM::Skill::MediumArmor)
                typeText = "#{sMedium}";
            else if (armorType == ESM::Skill::HeavyArmor)
                typeText = "#{sHeavy}";
            // For other skills, just subtitute the skill name
            // Normally you would never see this case, but modding allows getEquipmentSkill() to return any skill.
            else
                typeText = "#{sSkill" + armorType.toString() + "}";
        }

        text += "\n#{sArmorRating}: "
            + OFGui::ToolTips::toString(
                static_cast<int>(getSkillAdjustedArmorRating(ptr, OFMechanics::getPlayer(), true)));

        int remainingHealth = getItemHealth(ptr);
        text += "\n#{sCondition}: " + OFGui::ToolTips::toString(remainingHealth) + "/"
            + OFGui::ToolTips::toString(ref->mBase->mData.mHealth);

        if (!typeText.empty())
        {
            text += "\n#{sWeight}: " + OFGui::ToolTips::toString(ref->mBase->mData.mWeight) + " (";
            text += typeText;
            text += ')';
        }

        text += OFGui::ToolTips::getValueString(ref->mBase->mData.mValue, "#{sValue}");

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }

        info.enchant = ref->mBase->mEnchant;
        if (!info.enchant.empty())
            info.remainingEnchantCharge = static_cast<int>(ptr.getCellRef().getEnchantmentCharge());

        info.text = std::move(text);

        return info;
    }

    ESM::RefId Armor::getEnchantment(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return ref->mBase->mEnchant;
    }

    const ESM::RefId& Armor::applyEnchantment(
        const OFWorld::ConstPtr& ptr, const ESM::RefId& enchId, int enchCharge, const std::string& newName) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        ESM::Armor newItem = *ref->mBase;
        newItem.mId = ESM::RefId();
        newItem.mName = newName;
        newItem.mData.mEnchant = enchCharge;
        newItem.mEnchant = enchId;
        const ESM::Armor* record = OFBase::Environment::get().getESMStore()->insert(newItem);
        return record->mId;
    }

    float Armor::getSkillAdjustedArmorRating(
        const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& actor, bool useLuaInterfaceIfAvailable) const
    {
        if (useLuaInterfaceIfAvailable && actor == OFMechanics::getPlayer())
        {
            // In this interface call, both objects are effectively const, so stripping Const from the ConstPtr is fine.
            OFWorld::Ptr mutablePtr(
                const_cast<OFWorld::LiveCellRefBase*>(ptr.mRef), const_cast<OFWorld::CellStore*>(ptr.mCell));
            auto res = OFLua::LocalScripts::callPlayerInterface<float>(
                "Combat", "getSkillAdjustedArmorRating", OFLua::LObject(mutablePtr), OFLua::LObject(actor));
            if (res)
                return res.value();
        }

        // Fallback to the old engine implementation when actors don't have their scripts attached yet.

        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        const ESM::RefId armorSkillType = getEquipmentSkill(ptr, useLuaInterfaceIfAvailable);
        float armorSkill = actor.getClass().getSkill(actor, armorSkillType);

        int iBaseArmorSkill = OFBase::Environment::get()
                                  .getESMStore()
                                  ->get<ESM::GameSetting>()
                                  .find("iBaseArmorSkill")
                                  ->mValue.getInteger();

        if (ref->mBase->mData.mWeight == 0)
            return static_cast<float>(ref->mBase->mData.mArmor);
        else
            return ref->mBase->mData.mArmor * armorSkill / static_cast<float>(iBaseArmorSkill);
    }

    std::pair<int, std::string_view> Armor::canBeEquipped(const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const
    {
        const OFWorld::InventoryStore& invStore = npc.getClass().getInventoryStore(npc);

        if (getItemHealth(ptr) == 0)
            return { 0, "#{sInventoryMessage1}" };

        // slots that this item can be equipped in
        std::pair<std::vector<int>, bool> slots = getEquipmentSlots(ptr);

        if (slots.first.empty())
            return { 0, {} };

        if (npc.getClass().isNpc())
        {
            const ESM::RefId& npcRace = npc.get<ESM::NPC>()->mBase->mRace;

            // Beast races cannot equip shoes / boots, or full helms (head part vs hair part)
            const ESM::Race* race = OFBase::Environment::get().getESMStore()->get<ESM::Race>().find(npcRace);
            if (race->mData.mFlags & ESM::Race::Beast)
            {
                std::vector<ESM::PartReference> parts = ptr.get<ESM::Armor>()->mBase->mParts.mParts;

                for (std::vector<ESM::PartReference>::iterator itr = parts.begin(); itr != parts.end(); ++itr)
                {
                    if ((*itr).mPart == ESM::PRT_Head)
                        return { 0, "#{sNotifyMessage13}" };
                    if ((*itr).mPart == ESM::PRT_LFoot || (*itr).mPart == ESM::PRT_RFoot)
                        return { 0, "#{sNotifyMessage14}" };
                }
            }
        }

        for (std::vector<int>::const_iterator slot = slots.first.begin(); slot != slots.first.end(); ++slot)
        {
            // If equipping a shield, check if there's a twohanded weapon conflicting with it
            if (*slot == OFWorld::InventoryStore::Slot_CarriedLeft)
            {
                OFWorld::ConstContainerStoreIterator weapon
                    = invStore.getSlot(OFWorld::InventoryStore::Slot_CarriedRight);
                if (weapon != invStore.end() && weapon->getType() == ESM::Weapon::sRecordId)
                {
                    const OFWorld::LiveCellRef<ESM::Weapon>* ref = weapon->get<ESM::Weapon>();
                    if (OFMechanics::getWeaponType(ref->mBase->mData.mType)->mFlags & ESM::WeaponType::TwoHanded)
                        return { 3, {} };
                }

                return { 1, {} };
            }
        }
        return { 1, {} };
    }

    std::unique_ptr<OFWorld::Action> Armor::use(const OFWorld::Ptr& ptr, bool force) const
    {
        std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionEquip>(ptr, force);

        action->setSound(getUpSoundId(ptr));

        return action;
    }

    OFWorld::Ptr Armor::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    int Armor::getEnchantmentPoints(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();

        return ref->mBase->mData.mEnchant;
    }

    bool Armor::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Armor)
            || ((npcServices & ESM::NPC::MagicItems) && !getEnchantment(item).empty());
    }

    float Armor::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Armor>* ref = ptr.get<ESM::Armor>();
        return ref->mBase->mData.mWeight;
    }
}
