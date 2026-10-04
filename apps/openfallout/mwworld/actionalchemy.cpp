#include "actionalchemy.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

namespace OFWorld
{
    ActionAlchemy::ActionAlchemy(bool force)
        : Action(false)
        , mForce(force)
    {
    }

    void ActionAlchemy::executeImp(const Ptr& actor)
    {
        if (actor != OFMechanics::getPlayer())
            return;

        if (!mForce && OFMechanics::isPlayerInCombat())
        { // Ensure we're not in combat
            OFBase::Environment::get().getWindowManager()->messageBox("#{sInventoryMessage3}");
            return;
        }

        OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Alchemy);
    }
}
