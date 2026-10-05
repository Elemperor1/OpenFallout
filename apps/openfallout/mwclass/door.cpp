#include "door.hpp"

#include <MyGUI_TextIterator.h>
#include <MyGUI_UString.h>

#include <components/esm3/doorstate.hpp>
#include <components/esm3/loaddoor.hpp>
#include <components/esm3/loadmgef.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/actiondoor.hpp"
#include "../mwworld/actionteleport.hpp"
#include "../mwworld/actiontrap.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/containerstore.hpp"
#include "../mwworld/customdata.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwgui/tooltips.hpp"

#include "../mwrender/animation.hpp"
#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "classmodel.hpp"
#include "nameorid.hpp"

namespace OFClass
{
    class DoorCustomData : public OFWorld::TypedCustomData<DoorCustomData>
    {
    public:
        OFWorld::DoorState mDoorState = OFWorld::DoorState::Idle;

        DoorCustomData& asDoorCustomData() override { return *this; }
        const DoorCustomData& asDoorCustomData() const override { return *this; }
    };

    Door::Door()
        : OFWorld::RegisteredClass<Door>(ESM::Door::sRecordId)
    {
    }

    void Door::insertObjectRendering(
        const OFWorld::Ptr& ptr, const std::string& model, OFRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
            ptr.getRefData().getBaseNode()->setNodeMask(OFRender::Mask_Static);
        }
    }

    void Door::insertObject(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);

        // Resume the door's opening/closing animation if it wasn't finished
        if (ptr.getRefData().getCustomData())
        {
            const DoorCustomData& customData = ptr.getRefData().getCustomData()->asDoorCustomData();
            if (customData.mDoorState != OFWorld::DoorState::Idle)
            {
                OFBase::Environment::get().getWorld()->activateDoor(ptr, customData.mDoorState);
            }
        }
    }

    void Door::insertObjectPhysics(const OFWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation,
        OFPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, VFS::Path::toNormalized(model), rotation, OFPhysics::CollisionType_Door);
    }

    bool Door::isDoor() const
    {
        return true;
    }

    bool Door::useAnim() const
    {
        return true;
    }

    VFS::Path::NormalizedView Door::getModel(const OFWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM::Door>(ptr);
    }

    std::string_view Door::getName(const OFWorld::ConstPtr& ptr) const
    {
        return getNameOrId<ESM::Door>(ptr);
    }

    std::unique_ptr<OFWorld::Action> Door::activate(const OFWorld::Ptr& ptr, const OFWorld::Ptr& actor) const
    {
        OFWorld::LiveCellRef<ESM::Door>* ref = ptr.get<ESM::Door>();

        const ESM::RefId& openSound = ref->mBase->mOpenSound;
        const ESM::RefId& closeSound = ref->mBase->mCloseSound;
        const ESM::RefId lockedSound = ESM::RefId::stringRefId("LockedDoor");

        // FIXME: If NPC activate teleporting door, it can lead to crash due to iterator invalidation in the Actors
        // update. Make such activation a no-op for now, like how it is in the vanilla game.
        if (actor != OFMechanics::getPlayer() && ptr.getCellRef().getTeleport())
        {
            std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::FailedAction>(std::string_view{}, ptr);
            action->setSound(lockedSound);
            return action;
        }

        // make door glow if player activates it with telekinesis
        if (actor == OFMechanics::getPlayer()
            && OFBase::Environment::get().getWorld()->getDistanceToFocusObject()
                > OFBase::Environment::get().getWorld()->getMaxActivationDistance())
        {
            OFRender::Animation* animation = OFBase::Environment::get().getWorld()->getAnimation(ptr);
            if (animation)
            {
                const OFWorld::ESMStore& store = *OFBase::Environment::get().getESMStore();
                const ESM::MagicEffect* effect = store.get<ESM::MagicEffect>().find(ESM::MagicEffect::Telekinesis);

                animation->addSpellCastGlow(
                    effect->getColor(), 1); // 1 second glow to match the time taken for a door opening or closing
            }
        }

        OFWorld::ContainerStore& invStore = actor.getClass().getContainerStore(actor);

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
            if (actor == OFMechanics::getPlayer())
                OFBase::Environment::get().getWindowManager()->messageBox(std::string{ keyName } + " #{sKeyUsed}");
            ptr.getCellRef().unlock(); // Call the function here. because that makes sense.
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
            if (isTrapped)
            {
                // Trap activation
                std::unique_ptr<OFWorld::Action> action
                    = std::make_unique<OFWorld::ActionTrap>(ptr.getCellRef().getTrap(), ptr);
                action->setSound(ESM::RefId::stringRefId("Disarm Trap Fail"));
                return action;
            }

            if (ptr.getCellRef().getTeleport())
            {
                if (actor == OFMechanics::getPlayer()
                    && OFBase::Environment::get().getWorld()->getDistanceToFocusObject()
                        > OFBase::Environment::get().getWorld()->getMaxActivationDistance())
                {
                    // player activated teleport door with telekinesis
                    return std::make_unique<OFWorld::FailedAction>();
                }
                else
                {
                    std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionTeleport>(
                        ptr.getCellRef().getDestCell(), ptr.getCellRef().getDoorDest(), true);
                    action->setSound(openSound);
                    return action;
                }
            }
            else
            {
                // animated door
                std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::ActionDoor>(ptr);
                const auto doorState = getDoorState(ptr);
                bool opening = true;
                float doorRot = ptr.getRefData().getPosition().rot[2] - ptr.getCellRef().getPosition().rot[2];
                if (doorState == OFWorld::DoorState::Opening)
                    opening = false;
                if (doorState == OFWorld::DoorState::Idle && doorRot != 0)
                    opening = false;

                if (opening)
                {
                    OFBase::Environment::get().getSoundManager()->fadeOutSound3D(ptr, closeSound, 0.5f);
                    // Doors rotate at 90 degrees per second, so start the sound at
                    // where it would be at the current rotation.
                    float offset = doorRot / (osg::PIf * 0.5f);
                    action->setSoundOffset(offset);
                    action->setSound(openSound);
                }
                else
                {
                    OFBase::Environment::get().getSoundManager()->fadeOutSound3D(ptr, openSound, 0.5f);
                    float offset = 1.0f - doorRot / (osg::PIf * 0.5f);
                    action->setSoundOffset(std::max(offset, 0.0f));
                    action->setSound(closeSound);
                }

                return action;
            }
        }
        else
        {
            // locked, and we can't open.
            std::unique_ptr<OFWorld::Action> action = std::make_unique<OFWorld::FailedAction>(std::string_view{}, ptr);
            action->setSound(lockedSound);
            return action;
        }
    }

    bool Door::canLock(const OFWorld::ConstPtr& ptr) const
    {
        return true;
    }

    bool Door::allowTelekinesis(const OFWorld::ConstPtr& ptr) const
    {
        if (ptr.getCellRef().getTeleport() && !ptr.getCellRef().isLocked() && ptr.getCellRef().getTrap().empty())
            return false;
        else
            return true;
    }

    ESM::RefId Door::getScript(const OFWorld::ConstPtr& ptr) const
    {
        const OFWorld::LiveCellRef<ESM::Door>* ref = ptr.get<ESM::Door>();

        return ref->mBase->mScript;
    }

    OFGui::ToolTipInfo Door::getToolTipInfo(const OFWorld::ConstPtr& ptr, int count) const
    {
        const OFWorld::LiveCellRef<ESM::Door>* ref = ptr.get<ESM::Door>();

        OFGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MyGUI::UString(name));

        std::string text;

        if (ptr.getCellRef().getTeleport())
        {
            std::string destination = getDestination(*ref);
            if (!destination.empty())
            {
                text += "\n#{sTo}";
                text += "\n" + destination;
            }
        }

        int lockLevel = ptr.getCellRef().getLockLevel();
        if (lockLevel)
        {
            if (ptr.getCellRef().isLocked())
                text += "\n#{sLockLevel}: " + OFGui::ToolTips::toString(lockLevel);
            else
                text += "\n#{sUnlocked}";
        }
        if (!ptr.getCellRef().getTrap().empty())
            text += "\n#{sTrapped}";

        if (OFBase::Environment::get().getWindowManager()->getFullHelp())
        {
            info.extra += OFGui::ToolTips::getCellRefString(ptr.getCellRef());
            info.extra += OFGui::ToolTips::getMiscString(ref->mBase->mScript.getRefIdString(), "Script");
        }
        info.text = std::move(text);

        return info;
    }

    std::string Door::getDestination(const OFWorld::LiveCellRef<ESM::Door>& door)
    {
        const OFWorld::CellStore* cell = OFBase::Environment::get().getWorldModel()->findCell(door.mRef.getDestCell());
        if (cell == nullptr)
            return {};
        std::string_view dest = OFBase::Environment::get().getWorld()->getCellName(cell);
        return "#{sCell=" + std::string{ dest } + "}";
    }

    OFWorld::Ptr Door::copyToCellImpl(const OFWorld::ConstPtr& ptr, OFWorld::CellStore& cell) const
    {
        const OFWorld::LiveCellRef<ESM::Door>* ref = ptr.get<ESM::Door>();

        return OFWorld::Ptr(cell.insert(ref), &cell);
    }

    void Door::ensureCustomData(const OFWorld::Ptr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            ptr.getRefData().setCustomData(std::make_unique<DoorCustomData>());
        }
    }

    OFWorld::DoorState Door::getDoorState(const OFWorld::ConstPtr& ptr) const
    {
        if (!ptr.getRefData().getCustomData())
            return OFWorld::DoorState::Idle;
        const DoorCustomData& customData = ptr.getRefData().getCustomData()->asDoorCustomData();
        return customData.mDoorState;
    }

    void Door::setDoorState(const OFWorld::Ptr& ptr, OFWorld::DoorState state) const
    {
        if (ptr.getCellRef().getTeleport())
            throw std::runtime_error("load doors can't be moved");

        ensureCustomData(ptr);
        DoorCustomData& customData = ptr.getRefData().getCustomData()->asDoorCustomData();
        customData.mDoorState = state;
    }

    void Door::readAdditionalState(const OFWorld::Ptr& ptr, const ESM::ObjectState& state) const
    {
        if (!state.mHasCustomState)
            return;

        ensureCustomData(ptr);
        DoorCustomData& customData = ptr.getRefData().getCustomData()->asDoorCustomData();
        const ESM::DoorState& doorState = state.asDoorState();
        customData.mDoorState = OFWorld::DoorState(doorState.mDoorState);
    }

    void Door::writeAdditionalState(const OFWorld::ConstPtr& ptr, ESM::ObjectState& state) const
    {
        if (!ptr.getRefData().getCustomData())
        {
            state.mHasCustomState = false;
            return;
        }

        const DoorCustomData& customData = ptr.getRefData().getCustomData()->asDoorCustomData();
        ESM::DoorState& doorState = state.asDoorState();
        doorState.mDoorState = int(customData.mDoorState);
    }

}
