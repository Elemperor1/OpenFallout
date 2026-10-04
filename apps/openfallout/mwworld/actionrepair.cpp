#include "actionrepair.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwmechanics/actorutil.hpp"

namespace OFWorld
{
    ActionRepair::ActionRepair(const Ptr& item, bool force)
        : Action(false, item)
        , mForce(force)
    {
    }

    void ActionRepair::executeImp(const Ptr& actor)
    {
        if (actor != OFMechanics::getPlayer())
            return;

        if (!mForce && OFMechanics::isPlayerInCombat())
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sInventoryMessage2}");
            return;
        }

        OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Repair, getTarget());
    }
}
