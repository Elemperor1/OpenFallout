#include "actionsoulgem.hpp"

#include <components/debug/debuglog.hpp>
#include <components/esm3/loadcrea.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/esmstore.hpp"

namespace OFWorld
{

    ActionSoulgem::ActionSoulgem(const Ptr& object)
        : Action(false, object)
    {
    }

    void ActionSoulgem::executeImp(const Ptr& actor)
    {
        if (actor != OFMechanics::getPlayer())
            return;

        if (OFMechanics::isPlayerInCombat())
        { // Ensure we're not in combat
            OFBase::Environment::get().getWindowManager()->messageBox("#{sInventoryMessage5}");
            return;
        }

        const auto& target = getTarget();
        const ESM::RefId& targetSoul = target.getCellRef().getSoul();

        if (targetSoul.empty())
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage32}");
            return;
        }

        if (!OFBase::Environment::get().getESMStore()->get<ESM::Creature>().search(targetSoul))
        {
            Log(Debug::Warning) << "Soul '" << targetSoul << "' not found (item: '" << target.getCellRef().getRefId()
                                << "')";
            return;
        }

        OFBase::Environment::get().getWindowManager()->showSoulgemDialog(target);
    }

}
