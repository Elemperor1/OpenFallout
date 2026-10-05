#include "actionread.hpp"

#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadskil.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "class.hpp"
#include "esmstore.hpp"

namespace OFWorld
{
    ActionRead::ActionRead(const OFWorld::Ptr& object)
        : Action(false, object)
    {
    }

    void ActionRead::executeImp(const OFWorld::Ptr& actor)
    {
        const OFWorld::Ptr player = OFMechanics::getPlayer();
        if (actor != player && getTarget().getContainerStore() != nullptr)
            return;

        // Ensure we're not in combat
        if (OFMechanics::isPlayerInCombat()
            // Reading in combat is still allowed if the scroll/book is not in the player inventory yet
            // (since otherwise, there would be no way to pick it up)
            && getTarget().getContainerStore() == &player.getClass().getContainerStore(player))
        {
            OFBase::Environment::get().getWindowManager()->messageBox("#{sInventoryMessage4}");
            return;
        }

        LiveCellRef<ESM::Book>* ref = getTarget().get<ESM::Book>();

        if (ref->mBase->mData.mIsScroll)
            OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Scroll, getTarget());
        else
            OFBase::Environment::get().getWindowManager()->pushGuiMode(OFGui::GM_Book, getTarget());

        OFMechanics::NpcStats& npcStats = player.getClass().getNpcStats(player);

        // Skill gain from books
        const ESM::RefId& skill = ref->mBase->mData.mSkillId;
        if (!skill.empty() && !npcStats.hasBeenUsed(ref->mBase->mId))
        {
            OFBase::Environment::get().getLuaManager()->skillLevelUp(player, skill, "book");

            npcStats.flagAsUsed(ref->mBase->mId);
        }
    }
}
