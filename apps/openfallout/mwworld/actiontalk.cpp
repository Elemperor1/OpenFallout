#include "actiontalk.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwscript/falloutscripts.hpp"

#include <components/esm/defs.hpp>

namespace OFWorld
{
    ActionTalk::ActionTalk(const Ptr& actor)
        : Action(false, actor)
    {
    }

    void ActionTalk::executeImp(const Ptr& actor)
    {
        if (actor != OFMechanics::getPlayer())
            return;
        // A character of Fallout talks with the dialogue of the games it is from
        if (ESM::isESM4Rec(static_cast<ESM::RecNameInts>(getTarget().getType())))
            OFBase::Environment::get().getFalloutScripts()->talkTo(getTarget());
        else
            OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Dialogue, getTarget());
    }
}
