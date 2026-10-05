#include "apparatus.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadnpc.hpp>

#include "../mwworld/actionalchemy.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwgui/tooltips.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Apparatus::Apparatus()
        : OFWorld::RegisteredClass<Apparatus>(ESM::Apparatus::sRecordId)
    {
    }

    void Apparatus::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Apparatus::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Apparatus>(ptr);
    }

    std::string_view Apparatus::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Apparatus>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Apparatus::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Apparatus::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Apparatus>* ref = ptr.get<ESM::Apparatus>();

        return ref->mBase->mScript;
    }

    int Apparatus::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Apparatus>* ref = ptr.get<ESM::Apparatus>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Apparatus::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const auto sound = ESM::RefId::stringRefId("Item Apparatus Up");
        return sound;
    }

    const ESM::RefId& Apparatus::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const auto sound = ESM::RefId::stringRefId("Item Apparatus Down");
        return sound;
    }

    VFS::Path::NormalizedView Apparatus::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Apparatus>* ref = ptr.get<ESM::Apparatus>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Apparatus::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Apparatus>* ref = ptr.get<ESM::Apparatus>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;
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

    std::unique_ptr<OFWorld::Action> Apparatus::use(const OFWorld::Ptr& ptr, bool force) const
    {
        return std::make_unique<OFWorld::ActionAlchemy>(force);
    }

    OFWorld::Ptr Apparatus::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Apparatus>* ref = ptr.get<ESM::Apparatus>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    bool Apparatus::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Apparatus) != 0;
    }

    float Apparatus::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Apparatus>* ref = ptr.get<ESM::Apparatus>();
        return ref->mBase->mData.mWeight;
    }
}
