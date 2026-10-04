#include "ingredient.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadnpc.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/actioneat.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/nullaction.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Ingredient::Ingredient()
        : OFWorld::RegisteredClass<Ingredient>(ESM::Ingredient::sRecordId)
    {
    }

    void Ingredient::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Ingredient::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Ingredient>(ptr);
    }

    std::string_view Ingredient::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Ingredient>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Ingredient::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Ingredient::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Ingredient>* ref = ptr.get<ESM::Ingredient>();

        return ref->mBase->mScript;
    }

    int Ingredient::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Ingredient>* ref = ptr.get<ESM::Ingredient>();

        return ref->mBase->mData.mValue;
    }

    std::unique_ptr<OFWorld::Action> Ingredient::use(const OFWorld::Ptr& ptr, bool force) const
    {
        if (ptr.get<ESM::Ingredient>()->mBase->mData.mEffectID[0].empty())
            return std::make_unique<OFWorld::NullAction>();
        std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionEat>(ptr);

        action->setSound(ESM::RefId::stringRefId("Swallow"));

        return action;
    }

    const ESM::RefId& Ingredient::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static auto sound = ESM::RefId::stringRefId("Item Ingredient Up");
        return sound;
    }

    const ESM::RefId& Ingredient::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static auto sound = ESM::RefId::stringRefId("Item Ingredient Down");
        return sound;
    }

    VFS::Path::NormalizedView Ingredient::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Ingredient>* ref = ptr.get<ESM::Ingredient>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Ingredient::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Ingredient>* ref = ptr.get<ESM::Ingredient>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;

        text += OFGui::ToolTips::getWeightString(ref->mBase->mData.mWeight, "#{sWeight}");
        text += OFGui::ToolTips::getValueString(ref->mBase->mData.mValue, "#{sValue}");

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }

        OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        float alchemySkill = player.getClass().getSkill(player, ESM::Skill::Alchemy);

        static const float fWortChanceValue = OFBase::Environment::get()
                                                  .getESMStore()
                                                  ->get<ESM::GameSetting>()
                                                  .find("fWortChanceValue")
                                                  ->mValue.getFloat();

        OFGui::Widgets::SpellEffectList list;
        for (int i = 0; i < 4; ++i)
        {
            if (ref->mBase->mData.mEffectID[i].empty())
                continue;
            OFGui::Widgets::SpellEffectParams params;
            params.mEffectID = ref->mBase->mData.mEffectID[i];
            params.mAttribute = ref->mBase->mData.mAttributes[i];
            params.mSkill = ref->mBase->mData.mSkills[i];
            params.mKnown = alchemySkill >= fWortChanceValue * (i + 1);

            list.push_back(params);
        }
        info.effects = std::move(list);

        info.text = std::move(text);
        info.isIngredient = true;

        return info;
    }

    OFWorld::Ptr Ingredient::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Ingredient>* ref = ptr.get<ESM::Ingredient>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    bool Ingredient::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Ingredients) != 0;
    }

    float Ingredient::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Ingredient>* ref = ptr.get<ESM::Ingredient>();
        return ref->mBase->mData.mWeight;
    }
}
