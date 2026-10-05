#include "potion.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadnpc.hpp>

#include "../mwworld/actionapply.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwmechanics/alchemy.hpp"
#include "../mwmechanics/spellutil.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Potion::Potion()
        : OFWorld::RegisteredClass<Potion>(ESM::Potion::sRecordId)
    {
    }

    void Potion::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Potion::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Potion>(ptr);
    }

    std::string_view Potion::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Potion>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Potion::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Potion::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Potion>* ref = ptr.get<ESM::Potion>();

        return ref->mBase->mScript;
    }

    int Potion::getValue(const OFWorld::ConstPtr& ptr) const
    {
        return OFMechanics::getPotionValue(*ptr.get<ESM::Potion>()->mBase);
    }

    const ESM::RefId& Potion::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const auto sound = ESM::RefId::stringRefId("Item Potion Up");
        return sound;
    }

    const ESM::RefId& Potion::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const auto sound = ESM::RefId::stringRefId("Item Potion Down");
        return sound;
    }

    VFS::Path::NormalizedView Potion::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Potion>* ref = ptr.get<ESM::Potion>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Potion::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Potion>* ref = ptr.get<ESM::Potion>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;

        text += "\n#{sWeight}: " + OFGui::ToolTips::toString(ref->mBase->mData.mWeight);
        text += OFGui::ToolTips::getValueString(getValue(ptr), "#{sValue}");

        info.effects = OFGui::Widgets::MWEffectList::effectListFromESM(&ref->mBase->mEffects);

        // hide effects the player doesn't know about
        OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        for (size_t i = 0; i < info.effects.size(); ++i)
            info.effects[i].mKnown = OFMechanics::Alchemy::knownEffect(i, player);

        info.isPotion = true;

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }

        info.text = std::move(text);

        return info;
    }

    std::unique_ptr<OFWorld::Action> Potion::use(const OFWorld::Ptr& ptr, bool force) const
    {
        OFWorld::LiveCellRef<ESM::Potion>* ref = ptr.get<ESM::Potion>();

        auto action = std::make_unique<OFWorld::ActionApply>(ptr, ref->mBase->mId);

        action->setSound(ESM::RefId::stringRefId("Drink"));

        return action;
    }

    OFWorld::Ptr Potion::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Potion>* ref = ptr.get<ESM::Potion>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    bool Potion::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Potions) != 0;
    }

    float Potion::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Potion>* ref = ptr.get<ESM::Potion>();
        return ref->mBase->mData.mWeight;
    }
}
