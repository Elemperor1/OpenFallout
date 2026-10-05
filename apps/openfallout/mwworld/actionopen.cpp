#include "actionopen.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/disease.hpp"

namespace OFWorld
{
    ActionOpen::ActionOpen(const OFWorld::Ptr& container)
        : Action(false, container)
    {
    }

    void ActionOpen::executeImp(const OFWorld::Ptr& actor)
    {
        if (!OFBase::Environment::get().getWindowManager()->isAllowed(OFGui::GW_Inventory))
            return;

        if (actor != OFMechanics::getPlayer())
            return;

        if (!OFBase::Environment::get().getMechanicsManager()->onOpen(getTarget()))
            return;

        OFMechanics::diseaseContact(actor, getTarget());

        OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Container, getTarget());
    }
}
