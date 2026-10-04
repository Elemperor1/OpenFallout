#include "book.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadsoun.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/actionread.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwmechanics/npcstats.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Book::Book()
        : OFWorld::RegisteredClass<Book>(ESM::Book::sRecordId)
    {
    }

    void Book::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Book::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Book>(ptr);
    }

    std::string_view Book::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Book>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Book::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        std::unique_ptr<OFWorld::Action> werewolfAction = getWerewolfRefusalAction(actor);
        if (werewolfAction)
            return werewolfAction;

        return std::make_unique<OFWorld::ActionRead>(ptr);
    }

    ESM::RefId Book::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        return ref->mBase->mScript;
    }

    int Book::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Book::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static auto var = ESM::RefId::stringRefId("Item Book Up");
        return var;
    }

    const ESM::RefId& Book::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static auto var = ESM::RefId::stringRefId("Item Book Down");
        return var;
    }

    VFS::Path::NormalizedView Book::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Book::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

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

        info.enchant = ref->mBase->mEnchant;

        info.text = std::move(text);

        return info;
    }

    ESM::RefId Book::getEnchantment(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        return ref->mBase->mEnchant;
    }

    const ESM::RefId& Book::applyEnchantment(
        const OFWorld::ConstPtr& ptr, const ESM::RefId& enchId, int enchCharge, const std::string& newName) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        ESM::Book newItem = *ref->mBase;
        newItem.mId = ESM::RefId();
        newItem.mName = newName;
        newItem.mData.mIsScroll = 1;
        newItem.mData.mEnchant = enchCharge;
        newItem.mEnchant = enchId;
        const ESM::Book* record = OFBase::Environment::get().getESMStore()->insert(newItem);
        return record->mId;
    }

    std::unique_ptr<OFWorld::Action> Book::use(const OFWorld::Ptr& ptr, bool force) const
    {
        return std::make_unique<OFWorld::ActionRead>(ptr);
    }

    OFWorld::Ptr Book::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    int Book::getEnchantmentPoints(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();

        return ref->mBase->mData.mEnchant;
    }

    bool Book::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Books)
            || ((npcServices & ESM::NPC::MagicItems) && !getEnchantment(item).empty());
    }

    float Book::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Book>* ref = ptr.get<ESM::Book>();
        return ref->mBase->mData.mWeight;
    }
}
