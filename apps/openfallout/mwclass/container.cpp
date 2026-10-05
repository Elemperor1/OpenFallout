#include "container.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/containerstate.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadsoun.hpp>
#include <components/settings/values.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/actionharvest.hpp"
#include "../mwworld/actionopen.hpp"
#include "../mwworld/actiontrap.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/nullaction.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/animation.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwmechanics/inventory.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    ContainerCustomData::ContainerCustomData(const ESM::Container& container, OFWorld::CellStore* cell)
    {
        auto& prng = OFBase::Environment::get().getWorld()->getPrng();
        unsigned int seed = Misc::Rng::rollDice(std::numeric_limits<int>::max(), prng);
        // setting ownership not needed, since taking items from a container inherits the
        // container's owner automatically
        mStore.fillNonRandom(container.mInventory, ESM::RefId(), seed);
    }

    ContainerCustomData::ContainerCustomData(const ESM::InventoryState& inventory)
    {
        mStore.readState(inventory);
    }

    ContainerCustomData& ContainerCustomData::asContainerCustomData()
    {
        return *this;
    }
    const ContainerCustomData& ContainerCustomData::asContainerCustomData() const
    {
        return *this;
    }

    Container::Container()
        : OFWorld::RegisteredClass<Container>(ESM::Container::sRecordId)
    {
    }

    void Container::ensureCustomData(const OFWorld::Ptr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
            OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();

            // store
            ptr.getRefData().setCustomData(std::make_unique<ContainerCustomData>(*ref->mBase, ptr.getCell()));
            getContainerStore(ptr).setPtr(ptr);

            OFBase::Environment::get().getWorld()->addContainerScripts(ptr, ptr.getCell());
        }
    }

    bool Container::canBeHarvested(const OFWorld::ConstPtr& ptr) const
    {
        if (!Settings::game().mGraphicHerbalism)
            return false;
        const OFRender::Animation* animation = OFBase::Environment::get().getWorld()->getAnimation(ptr);
        if (animation == nullptr)
            return false;

        return animation->canBeHarvested();
    }

    void Container::respawn(const OFWorld::Ptr& ptr) const
    {
        OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();
        if (ref->mBase->mFlags & ESM::Container::Respawn)
        {
            // Container was not touched, there is no need to modify its content.
            if (ptr.getRefData().getCustomData() == nullptr)
                return;

            OFBase::Environment::get().getWorld()->removeContainerScripts(ptr);
            ptr.getRefData().setCustomData(nullptr);
        }
    }

    void Container::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    void Container::insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);
    }

    void Container::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, OFPhysics::CollisionType_World);
    }

    VFS::Path::NormalizedView Container::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Container>(ptr);
    }

    bool Container::useAnim() const
    {
        return true;
    }

    std::unique_ptr<OFWorld::Action> Container::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        if (!OFBase::Environment::get().getWindowManager()->isAllowed(OFGui::GW_Inventory))
            return std::make_unique<OFWorld::NullAction>();

        std::unique_ptr<OFWorld::Action> werewolfAction = getWerewolfRefusalAction(actor);
        if (werewolfAction)
            return werewolfAction;

        OFWorld::Ptr player = OFBase::Environment::get().getWorld()->getPlayerPtr();
        OFWorld::InventoryStore& invStore = player.getClass().getInventoryStore(player);

        bool isLocked = ptr.getCellRef().isLocked();
        bool isTrapped = !ptr.getCellRef().getTrap().empty();
        bool hasKey = false;
        std::string_view keyName;

        const ESM::RefId& keyId = ptr.getCellRef().getKey();
        if (!keyId.empty())
        {
            OFWorld::Ptr keyPtr = invStore.search(keyId);
            if (!keyPtr.isEmpty())
            {
                hasKey = true;
                keyName = keyPtr.getClass().getName(keyPtr);
            }
        }

        if (isLocked && hasKey)
        {
            OFBase::Environment::get().getWindowManager()->messageBox(std::string{ keyName } + " #{sKeyUsed}");
            ptr.getCellRef().unlock();
            // using a key disarms the trap
            if (isTrapped)
            {
                ptr.getCellRef().setTrap(ESM::RefId());
                OFBase::Environment::get().getSoundManager()->playSound3D(
                    ptr, ESM::RefId::stringRefId("Disarm Trap"), 1.0f, 1.0f);
                isTrapped = false;
            }
        }

        if (!isLocked || hasKey)
        {
            if (!isTrapped)
            {
                if (!canBeHarvested(ptr))
                    return std::make_unique<OFWorld::ActionOpen>(ptr);

                if (hasToolTip(ptr))
                    return std::make_unique<OFWorld::ActionHarvest>(ptr);

                return std::make_unique<OFWorld::FailedAction>(std::string_view{}, ptr);
            }
            else
            {
                // Activate trap
                std::unique_ptr<OFWorld::Action> action
                    = std::make_unique<OFWorld::ActionTrap>(ptr.getCellRef().getTrap(), ptr);
                action->setSound(ESM::RefId::stringRefId("Disarm Trap Fail"));
                return action;
            }
        }
        else
        {
            std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::FailedAction>(std::string_view{}, ptr);
            action->setSound(ESM::RefId::stringRefId("LockedChest"));
            return action;
        }
    }

    std::string_view Container::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Container>(ptr);
    }

    OFWorld::ContainerStore& Container::getContainerStore(const OFWorld::Ptr& ptr) const
    {
        ensureCustomData(ptr);
        return ptr.getRefData().getCustomData()->asContainerCustomData().mStore;
    }

    ESM::RefId Container::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();

        return ref->mBase->mScript;
    }

    bool Container::hasToolTip(const OFWorld::ConstPtr& ptr) const
    {
        if (const OFWorld::CustomData* data = ptr.getRefData().getCustomData())
        {
            if (!canBeHarvested(ptr))
                return true;
            const OFWorld::ContainerStore& store = data->asContainerCustomData().mStore;
            return !store.isResolved() || store.hasVisibleItems();
        }
        return true;
    }

    OFGui::ToolTipInfo Container::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name));

        std::string text;
        int lockLevel = ptr.getCellRef().getLockLevel();
        if (lockLevel)
        {
            if (ptr.getCellRef().isLocked())
                text += "\n#{sLockLevel}: " + OFGui::ToolTips::toString(lockLevel);
            else
                text += "\n#{sUnlocked}";
        }
        if (ptr.getCellRef().getTrap() != ESM::RefId())
            text += "\n#{sTrapped}";

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
            if (ptr.getCellRef().getRefId() == "stolen_goods")
                info.extra += "\nYou cannot use evidence chests";
        }

        info.text = std::move(text);

        return info;
    }

    float Container::getCapacity(const OFWorld::Ptr& ptr) const
    {
        OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();

        return ref->mBase->mWeight;
    }

    float Container::getEncumbrance(const OFWorld::Ptr& ptr) const
    {
        return getContainerStore(ptr).getWeight();
    }

    bool Container::canLock(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();
        return !(ref->mBase->mFlags & ESM::Container::Organic);
    }

    void Container::modifyBaseInventory(const ESM::RefId& containerId, const ESM::RefId& itemId, int amount) const
    {
        OFMechanics::modifyBaseInventory<ESM::Container>(containerId, itemId, amount);
    }

    OFWorld::Ptr Container::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Container>* ref = ptr.get<ESM::Container>();
        OFWorld::Ptr newPtr(cell.insert(ref), &cell);
        if (newPtr.getRefData().getCustomData())
        {
            OFBase::Environment::get().getWorldModel()->registerPtr(newPtr);
            getContainerStore(newPtr).setPtr(newPtr);
        }
        return newPtr;
    }

    void Container::readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const
    {
        if (!state.mHasCustomState)
            return;

        const ESM::ContainerState& containerState = state.asContainerState();
        ptr.getRefData().setCustomData(std::make_unique<ContainerCustomData>(containerState.mInventory));

        OFBase::Environment::get().getWorldModel()->registerPtr(ptr);
        getContainerStore(ptr).setPtr(ptr);
    }

    void Container::writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            state.mHasCustomState = false;
            return;
        }

        const ContainerCustomData& customData = ptr.getRefData().getCustomData()->asContainerCustomData();
        if (!customData.mStore.isResolved())
        {
            state.mHasCustomState = false;
            return;
        }

        ESM::ContainerState& containerState = state.asContainerState();
        customData.mStore.writeState(containerState.mInventory);
    }
}
