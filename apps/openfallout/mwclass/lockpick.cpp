#include "lockpick.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadnpc.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/actionequip.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Lockpick::Lockpick()
        : OFWorld::RegisteredClass<Lockpick>(ESM::Lockpick::sRecordId)
    {
    }

    void Lockpick::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Lockpick::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Lockpick>(ptr);
    }

    std::string_view Lockpick::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Lockpick>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Lockpick::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Lockpick::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();

        return ref->mBase->mScript;
    }

    std::pair<std::vector<int>, bool> Lockpick::getEquipmentSlots(const OFWorld::ConstPtr& ptr) const
    {
        std::vector<int> slots;

        slots.push_back(static_cast<int>(OFWorld::InventoryStore::Slot_CarriedRight));

        return std::make_pair(slots, false);
    }

    int Lockpick::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Lockpick::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static ESM::RefId sound = ESM::RefId::stringRefId("Item Lockpick Up");
        return sound;
    }

    const ESM::RefId& Lockpick::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static ESM::RefId sound = ESM::RefId::stringRefId("Item Lockpick Down");
        return sound;
    }

    VFS::Path::NormalizedView Lockpick::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Lockpick::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;

        int remainingUses = getItemHealth(ptr);

        text += "\n#{sUses}: " + OFGui::ToolTips::toString(remainingUses);
        text += "\n#{sQuality}: " + OFGui::ToolTips::toString(ref->mBase->mData.mQuality);
        text += OFGui::ToolTips::getWeightString(ref->mBase->mData.mWeight, "#{sWeight}");
        text += OFGui::ToolTips::getValueString(ref->mBase->mData.mValue, "#{sValue}");

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }

        info.text = std::move(text);

        return info;
    }

    std::unique_ptr<OFWorld::Action> Lockpick::use(const OFWorld::Ptr& ptr, bool force) const
    {
        std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionEquip>(ptr, force);

        action->setSound(getUpSoundId(ptr));

        return action;
    }

    OFWorld::Ptr Lockpick::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    std::pair<int, std::string_view> Lockpick::canBeEquipped(
        const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const
    {
        // Do not allow equip tools from inventory during attack
        if (OFBase::Environment::get().getMechanicsManager()->isAttackingOrSpell(npc)
            && !OFBase::Environment::get().getMechanicsManager()->isCastingSpell(npc)
            && OFBase::Environment::get().getWindowManager()->isGuiMode())
            return { 0, "#{sCantEquipWeapWarning}" };

        return { 1, {} };
    }

    bool Lockpick::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Picks) != 0;
    }

    int Lockpick::getItemMaxHealth(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();

        return ref->mBase->mData.mUses;
    }

    float Lockpick::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Lockpick>* ref = ptr.get<ESM::Lockpick>();
        return ref->mBase->mData.mWeight;
    }
}
