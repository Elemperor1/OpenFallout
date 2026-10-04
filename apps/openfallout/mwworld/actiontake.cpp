#include "actiontake.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwgui/inventorywindow.hpp"

#include "class.hpp"
#include "containerstore.hpp"

namespace OFWorld
{
    ActionTake::ActionTake(const OFWorld::Ptr& object)
        : Action(true, object)
    {
    }

    void ActionTake::executeImp(const Ptr& actor)
    {
        // When in GUI mode, we should use drag and drop
        if (actor == OFBase::Environment::get().getWorld()->getPlayerPtr())
        {
            OFGui::GuiMode mode = OFBase::Environment::get().getWindowManager()->getMode();
            if (mode == OFGui::GM_Inventory || mode == OFGui::GM_Container)
            {
                OFBase::Environment::get().getWindowManager()->getInventoryWindow()->pickUpObject(getTarget());
                return;
            }
        }

        int count = getTarget().getCellRef().getCount();
        if (getTarget().getClass().isGold(getTarget()))
            count *= getTarget().getClass().getValue(getTarget());

        OFBase::Environment::get().getMechanicsManager()->itemTaken(actor, getTarget(), OFWorld::Ptr(), count);
        OFWorld::Ptr newitem = *actor.getClass().getContainerStore(actor).add(getTarget(), count);
        OFBase::Environment::get().getWorld()->deleteObject(getTarget());
        setTarget(newitem);
    }
}
