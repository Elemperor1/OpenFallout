#include "actiontalk.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

namespace OFWorld
{
    ActionTalk::ActionTalk(const Ptr& actor)
        : Action(false, actor)
    {
    }

    void ActionTalk::executeImp(const Ptr& actor)
    {
        if (actor == OFMechanics::getPlayer())
            OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Dialogue, getTarget());
    }
}
