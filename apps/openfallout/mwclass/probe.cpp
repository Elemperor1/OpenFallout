#include "probe.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadprob.hpp>

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
    Probe::Probe()
        : OFWorld::RegisteredClass<Probe>(ESM::Probe::sRecordId)
    {
    }

    void Probe::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Probe::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Probe>(ptr);
    }

    std::string_view Probe::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Probe>(ptr);
    }
    std::unique_ptr<OFWorld::Action> Probe::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Probe::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();

        return ref->mBase->mScript;
    }

    std::pair<std::vector<int>, bool> Probe::getEquipmentSlots(const OFWorld::ConstPtr& ptr) const
    {
        std::vector<int> slots;

        slots.push_back(static_cast<int>(OFWorld::InventoryStore::Slot_CarriedRight));

        return std::make_pair(slots, false);
    }

    int Probe::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Probe::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const ESM::RefId sound = ESM::RefId::stringRefId("Item Probe Up");
        return sound;
    }

    const ESM::RefId& Probe::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const ESM::RefId sound = ESM::RefId::stringRefId("Item Probe Down");
        return sound;
    }

    VFS::Path::NormalizedView Probe::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Probe::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();

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

    std::unique_ptr<OFWorld::Action> Probe::use(const OFWorld::Ptr& ptr, bool force) const
    {
        std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionEquip>(ptr, force);

        action->setSound(getUpSoundId(ptr));

        return action;
    }

    OFWorld::Ptr Probe::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    std::pair<int, std::string_view> Probe::canBeEquipped(const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const
    {
        // Do not allow equip tools from inventory during attack
        if (OFBase::Environment::get().getMechanicsManager()->isAttackingOrSpell(npc)
            && !OFBase::Environment::get().getMechanicsManager()->isCastingSpell(npc)
            && OFBase::Environment::get().getWindowManager()->isGuiMode())
            return { 0, "#{sCantEquipWeapWarning}" };

        return { 1, {} };
    }

    bool Probe::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Probes) != 0;
    }

    int Probe::getItemMaxHealth(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();

        return ref->mBase->mData.mUses;
    }

    float Probe::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Probe>* ref = ptr.get<ESM::Probe>();
        return ref->mBase->mData.mWeight;
    }
}
