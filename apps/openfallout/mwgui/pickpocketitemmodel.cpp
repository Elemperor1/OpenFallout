#include "pickpocketitemmodel.hpp"

#include <components/esm3/loadskil.hpp>
#include <components/misc/rng.hpp>

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/pickpocket.hpp"

#include "../mwworld/class.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

namespace OFGui
{

    PickpocketItemModel::PickpocketItemModel(
        const OFWorld::Ptr& actor, std::unique_ptr<ItemModel> sourceModel, bool hideItems)
        : mActor(actor)
        , mPickpocketDetected(false)
    {
        OFWorld::Ptr player = OFMechanics::getPlayer();
        mSourceModel = std::move(sourceModel);
        float chance = player.getClass().getSkill(player, ESM::Skill::Sneak);

        mSourceModel->update();
        // build list of items that player is unable to find when attempts to pickpocket.
        if (hideItems)
        {
            auto& prng = OFBase::Environment::get().getWorld()->getPrng();
            for (size_t i = 0; i < mSourceModel->getItemCount(); ++i)
            {
                if (Misc::Rng::roll0to99(prng) > chance)
                    mHiddenItems.push_back(mSourceModel->getItem(static_cast<ModelIndex>(i)));
            }
        }
    }

    bool PickpocketItemModel::allowedToUseItems() const
    {
        return false;
    }

    ItemStack PickpocketItemModel::getItem(ModelIndex index)
    {
        if (index < 0)
            throw std::runtime_error("Invalid index supplied");
        if (mItems.size() <= static_cast<size_t>(index))
            throw std::runtime_error("Item index out of range");
        return mItems[index];
    }

    size_t PickpocketItemModel::getItemCount()
    {
        return mItems.size();
    }

    void PickpocketItemModel::update()
    {
        mSourceModel->update();
        mItems.clear();
        for (size_t i = 0; i < mSourceModel->getItemCount(); ++i)
        {
            const ItemStack& item = mSourceModel->getItem(static_cast<ModelIndex>(i));

            // Bound items may not be stolen
            if (item.mFlags & ItemStack::Flag_Bound)
                continue;

            if (std::find(mHiddenItems.begin(), mHiddenItems.end(), item) == mHiddenItems.end()
                && item.mType != ItemStack::Type_Equipped)
                mItems.push_back(item);
        }
    }

    bool PickpocketItemModel::onDropItem(const OFWorld::Ptr& item, int count)
    {
        // don't allow "reverse pickpocket" (it will be handled by scripts after 1.0)
        return false;
    }

    void PickpocketItemModel::onClose()
    {
        // Make sure we were actually closed, rather than just temporarily hidden (e.g. console or main menu opened)
        if (OFBase::Environment::get().getWindowManager()->containsMode(GM_Container)
            // If it was already detected while taking an item, no need to check now
            || mPickpocketDetected)
            return;

        OFWorld::Ptr player = OFMechanics::getPlayer();
        OFMechanics::Pickpocket pickpocket(player, mActor);
        if (pickpocket.finish())
        {
            OFBase::Environment::get().getMechanicsManager()->commitCrime(
                player, mActor, OFBase::MechanicsManager::OT_Pickpocket, ESM::RefId(), 0, true);
            mPickpocketDetected = true;
            OFBase::Environment::get().getWindowManager()->removeGuiMode(OFGui::GM_Container);
        }
    }

    bool PickpocketItemModel::onTakeItem(const OFWorld::Ptr& item, int count)
    {
        if (mActor.getClass().getCreatureStats(mActor).getKnockedDown())
            return mSourceModel->onTakeItem(item, count);

        bool success = stealItem(item, count);
        if (success)
        {
            OFWorld::Ptr player = OFMechanics::getPlayer();
            OFBase::Environment::get().getMechanicsManager()->itemTaken(player, item, mActor, count, false);
        }

        return success;
    }

    bool PickpocketItemModel::stealItem(const OFWorld::Ptr& item, int count)
    {
        OFWorld::Ptr player = OFMechanics::getPlayer();
        OFMechanics::Pickpocket pickpocket(player, mActor);
        if (pickpocket.pick(item, count))
        {
            OFBase::Environment::get().getMechanicsManager()->commitCrime(
                player, mActor, OFBase::MechanicsManager::OT_Pickpocket, ESM::RefId(), 0, true);
            mPickpocketDetected = true;
            OFBase::Environment::get().getWindowManager()->removeGuiMode(OFGui::GM_Container);
            return false;
        }
        else
            player.getClass().skillUsageSucceeded(player, ESM::Skill::Sneak, ESM::Skill::Sneak_PickPocket);

        return true;
    }
}
