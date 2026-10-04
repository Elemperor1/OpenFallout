#include "repair.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadrepa.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/actionrepair.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Repair::Repair()
        : OFWorld::RegisteredClass<Repair>(ESM::Repair::sRecordId)
    {
    }

    void Repair::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Repair::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Repair>(ptr);
    }

    std::string_view Repair::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Repair>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Repair::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Repair::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();

        return ref->mBase->mScript;
    }

    int Repair::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Repair::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static auto val = ESM::RefId::stringRefId("Item Repair Up");
        return val;
    }

    const ESM::RefId& Repair::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static auto val = ESM::RefId::stringRefId("Item Repair Down");
        return val;
    }

    VFS::Path::NormalizedView Repair::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();

        return ref->mBase->mIcon.getNormalized();
    }

    bool Repair::hasItemHealth(const OFWorld::ConstPtr& ptr) const
    {
        return true;
    }

    int Repair::getItemMaxHealth(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();

        return ref->mBase->mData.mUses;
    }

    OFGui::ToolTipInfo Repair::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();

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

    OFWorld::Ptr Repair::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    std::unique_ptr<OFWorld::Action> Repair::use(const OFWorld::Ptr& ptr, bool force) const
    {
        return std::make_unique<OFWorld::ActionRepair>(ptr, force);
    }

    bool Repair::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::RepairItem) != 0;
    }

    float Repair::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Repair>* ref = ptr.get<ESM::Repair>();
        return ref->mBase->mData.mWeight;
    }
}
