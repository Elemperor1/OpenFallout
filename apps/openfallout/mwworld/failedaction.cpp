#include "failedaction.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

namespace OFWorld
{
    FailedAction::FailedAction(std::string_view msg, const Ptr& target)
        : Action(false, target)
        , mMessage(msg)
    {
    }

    void FailedAction::executeImp(const Ptr& actor)
    {
        if (actor == OFMechanics::getPlayer() && !mMessage.empty())
            OFBase::Environment::get().getWindowManager()->messageBox(mMessage);
    }
}
