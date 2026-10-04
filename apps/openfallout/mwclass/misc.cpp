#include "misc.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadnpc.hpp>

#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/actionsoulgem.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/manualref.hpp"
#include "../mwworld/nullaction.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Miscellaneous::Miscellaneous()
        : OFWorld::RegisteredClass<Miscellaneous>(ESM::Miscellaneous::sRecordId)
    {
    }

    bool Miscellaneous::isGold(const OFWorld::ConstPtr& ptr) const
    {
        return ptr.getCellRef().getRefId() == "gold_001" || ptr.getCellRef().getRefId() == "gold_005"
            || ptr.getCellRef().getRefId() == "gold_010" || ptr.getCellRef().getRefId() == "gold_025"
            || ptr.getCellRef().getRefId() == "gold_100";
    }

    void Miscellaneous::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    VFS::Path::NormalizedView Miscellaneous::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Miscellaneous>(ptr);
    }

    std::string_view Miscellaneous::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Miscellaneous>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Miscellaneous::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Miscellaneous::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();

        return ref->mBase->mScript;
    }

    int Miscellaneous::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();

        int value = ref->mBase->mData.mValue;
        if (isGold(ptr) && ptr.getCellRef().getCount() != 1)
            value = 1;

        if (!ptr.getCellRef().getSoul().empty())
        {
            const ESM::Creature* creature
                = OFBase::Environment::get().getESMStore()->get<ESM::Creature>().search(ref->mRef.getSoul());
            if (creature)
            {
                int soul = creature->mData.mSoul;
                if (Settings::game().mRebalanceSoulGemValues)
                {
                    // use the 'soul gem value rebalance' formula from the Morrowind Code Patch
                    double soulValue = 0.0001 * std::pow(soul, 3) + 2 * soul;

                    // for Azura's star add the unfilled value
                    if (ptr.getCellRef().getRefId() == "Misc_SoulGem_Azura")
                        value += static_cast<int>(soulValue);
                    else
                        value = static_cast<int>(soulValue);
                }
                else
                    value *= soul;
            }
        }

        return value;
    }

    const ESM::RefId& Miscellaneous::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const ESM::RefId soundGold = ESM::RefId::stringRefId("Item Gold Up");
        static const ESM::RefId soundMisc = ESM::RefId::stringRefId("Item Misc Up");
        if (isGold(ptr))
            return soundGold;

        return soundMisc;
    }

    const ESM::RefId& Miscellaneous::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const ESM::RefId soundGold = ESM::RefId::stringRefId("Item Gold Down");
        static const ESM::RefId soundMisc = ESM::RefId::stringRefId("Item Misc Down");
        if (isGold(ptr))
            return soundGold;
        return soundMisc;
    }

    VFS::Path::NormalizedView Miscellaneous::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();

        return ref->mBase->mIcon.getNormalized();
    }

    OFGui::ToolTipInfo Miscellaneous::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();

        OFGui::ToolTipInfo info;

        bool gold = isGold(ptr);
        if (gold)
            count *= getValue(ptr);

        std::string countString;
        if (!gold)
            countString = OFGui::ToolTips::getCountString(count);
        else // gold displays its count also if it's 1.
            countString = " (" + std::to_string(count) + ")";

        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count)
            + OFGui::ToolTips::getSoulString(ptr.getCellRef());
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;

        text += OFGui::ToolTips::getWeightString(ref->mBase->mData.mWeight, "#{sWeight}");
        if (!gold && !(ref->mBase->mData.mFlags & ESM::Miscellaneous::Key))
            text += OFGui::ToolTips::getValueString(getValue(ptr), "#{sValue}");

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }

        info.text = std::move(text);

        return info;
    }

    static OFWorld::Ptr createGold(OFWorld::CellStore& cell, int goldAmount)
    {
        std::string_view base = "gold_001";
        if (goldAmount >= 100)
            base = "gold_100";
        else if (goldAmount >= 25)
            base = "gold_025";
        else if (goldAmount >= 10)
            base = "gold_010";
        else if (goldAmount >= 5)
            base = "gold_005";

        const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();
        OFWorld::ManualRef newRef(store, ESM::RefId::stringRefId(base));
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = newRef.getPtr().get<ESM::Miscellaneous>();

        OFWorld::Ptr ptr(cell.insert(ref), &cell);
        ptr.getCellRef().setCount(goldAmount);
        return ptr;
    }

    OFWorld::Ptr Miscellaneous::copyToCell(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell, int count) const
    {
        OFWorld::Ptr newPtr;
        if (isGold(ptr))
        {
            newPtr = createGold(cell, getValue(ptr) * count);
            newPtr.getRefData() = ptr.getRefData();
        }
        else
        {
            const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();
            newPtr = OFWorld::Ptr(cell.insert(ref), &cell);
            newPtr.getCellRef().setCount(count);
        }
        newPtr.getCellRef().unsetRefNum();
        newPtr.getRefData().setLuaScripts(nullptr);
        OFBase::Environment::get().getWorldModel()->registerPtr(newPtr);
        return newPtr;
    }

    OFWorld::Ptr Miscellaneous::moveToCell(const OFWorld::Ptr& ptr, OFWorld::CellStore& cell) const
    {
        OFWorld::Ptr newPtr;
        if (isGold(ptr))
        {
            newPtr = createGold(cell, getValue(ptr) * ptr.getCellRef().getCount());
            newPtr.getRefData() = ptr.getRefData();
            newPtr.getCellRef().setRefNum(ptr.getCellRef().getRefNum());
        }
        else
        {
            const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();
            newPtr = OFWorld::Ptr(cell.insert(ref), &cell);
        }
        ptr.getRefData().setLuaScripts(nullptr);
        OFBase::Environment::get().getWorldModel()->registerPtr(newPtr);
        return newPtr;
    }

    std::unique_ptr<OFWorld::Action> Miscellaneous::use(const OFWorld::Ptr& ptr, bool force) const
    {
        if (isSoulGem(ptr))
            return std::make_unique<OFWorld::ActionSoulgem>(ptr);

        return std::make_unique<OFWorld::NullAction>();
    }

    bool Miscellaneous::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = item.get<ESM::Miscellaneous>();

        return !(ref->mBase->mData.mFlags & ESM::Miscellaneous::Key) && (npcServices & ESM::NPC::Misc) && !isGold(item);
    }

    float Miscellaneous::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();
        return ref->mBase->mData.mWeight;
    }

    bool Miscellaneous::isKey(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Miscellaneous>* ref = ptr.get<ESM::Miscellaneous>();
        return ref->mBase->mData.mFlags & ESM::Miscellaneous::Key;
    }

    bool Miscellaneous::isSoulGem(const OFWorld::ConstPtr& ptr) const
    {
        return ptr.getCellRef().getRefId().startsWith("misc_soulgem");
    }

}
