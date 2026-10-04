#include "light.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/objectstate.hpp>
#include <components/esm4/loadligh.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/actionequip.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/nullaction.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    Light::Light()
        : OFWorld::RegisteredClass<Light>(ESM::Light::sRecordId)
    {
    }

    void Light::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        // Insert even if model is empty, so that the light is added
        renderingInterface.getObjects().insertModel(ptr, model, !(ref->mBase->mData.mFlags & ESM::Light::OffDefault));
    }

    void Light::insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();
        assert(ref->mBase != nullptr);

        insertObjectPhysics(ptr, model, rotation, physics);

        if (!ref->mBase->mSound.empty() && !(ref->mBase->mData.mFlags & ESM::Light::OffDefault))
            OFBase::Environment::get().getSoundManager()->playSound3D(
                ptr, ref->mBase->mSound, 1.0, 1.0, OFSound::Type::Sfx, OFSound::PlayMode::Loop);
    }

    void Light::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        // TODO: add option somewhere to enable collision for placeable objects
        if ((ptr.get<ESM::Light>()->mBase->mData.mFlags & ESM::Light::Carry) == 0)
            physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, OFPhysics::CollisionType_World);
    }

    bool Light::useAnim() const
    {
        return true;
    }

    VFS::Path::NormalizedView Light::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Light>(ptr);
    }

    std::string_view Light::getName(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        if (ref->mBase->mModel.empty())
            return {};
        return getNameOrId<ESM::Light>(ptr);
    }

    bool Light::isItem(const OFWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM::Light>()->mBase->mData.mFlags & ESM::Light::Carry;
    }

    std::unique_ptr<OFWorld::Action> Light::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        if (!OFBase::Environment::get().getWindowManager()->isAllowed(OFGui::GW_Inventory))
            return std::make_unique<OFWorld::NullAction>();

        OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();
        if (!(ref->mBase->mData.mFlags & ESM::Light::Carry))
            return std::make_unique<OFWorld::FailedAction>();

        return defaultItemActivate(ptr, actor);
    }

    ESM::RefId Light::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        return ref->mBase->mScript;
    }

    std::pair<std::vector<int>, bool> Light::getEquipmentSlots(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        std::vector<int> slots;

        if (ref->mBase->mData.mFlags & ESM::Light::Carry)
            slots.push_back(int(OFWorld::InventoryStore::Slot_CarriedLeft));

        return std::make_pair(slots, false);
    }

    int Light::getValue(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        return ref->mBase->mData.mValue;
    }

    const ESM::RefId& Light::getUpSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const auto sound = ESM::RefId::stringRefId("Item Misc Up");
        return sound;
    }

    const ESM::RefId& Light::getDownSoundId(const OFWorld::ConstPtr& ptr) const
    {
        static const auto sound = ESM::RefId::stringRefId("Item Misc Down");
        return sound;
    }

    VFS::Path::NormalizedView Light::getInventoryIcon(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        return ref->mBase->mIcon.getNormalized();
    }

    bool Light::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        return showsInInventory(ptr);
    }

    OFGui::ToolTipInfo Light::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name)) + OFGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon.getOriginal();

        std::string text;

        // Don't show duration for infinite light sources.
        if (Settings::game().mShowEffectDuration && ptr.getClass().getRemainingUsageTime(ptr) != -1)
            text += OFGui::ToolTips::getDurationString(ptr.getClass().getRemainingUsageTime(ptr), "\n#{sDuration}");

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

    bool Light::showsInInventory(const OFWorld::ConstPtr& ptr) const
    {
        const ESM::Light* light = ptr.get<ESM::Light>()->mBase;

        if (!(light->mData.mFlags & ESM::Light::Carry))
            return false;

        return Class::showsInInventory(ptr);
    }

    std::unique_ptr<OFWorld::Action> Light::use(const OFWorld::Ptr& ptr, bool force) const
    {
        std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionEquip>(ptr, force);

        action->setSound(getUpSoundId(ptr));

        return action;
    }

    void Light::setRemainingUsageTime(const OFWorld::Ptr& ptr, float duration) const
    {
        ptr.getCellRef().setChargeFloat(duration);
    }

    float Light::getRemainingUsageTime(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();
        if (ptr.getCellRef().getCharge() == -1)
            return static_cast<float>(ref->mBase->mData.mTime);
        else
            return ptr.getCellRef().getChargeFloat();
    }

    OFWorld::Ptr Light::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    bool Light::canSell(const OFWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Lights) != 0;
    }

    float Light::getWeight(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();
        return ref->mBase->mData.mWeight;
    }

    std::pair<int, std::string_view> Light::canBeEquipped(const OFWorld::ConstPtr& ptr, const OFWorld::Ptr& npc) const
    {
        const OFWorld::LiveCellRef<ESM::Light>* ref = ptr.get<ESM::Light>();
        if (!(ref->mBase->mData.mFlags & ESM::Light::Carry))
            return { 0, {} };

        return { 1, {} };
    }

    ESM::RefId Light::getSound(const OFWorld::ConstPtr& ptr) const
    {
        return ptr.get<ESM::Light>()->mBase->mSound;
    }

}
