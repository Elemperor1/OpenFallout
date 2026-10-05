#ifndef OPENFALLOUT_AICOMBAT_ACTION_H
#define OPENFALLOUT_AICOMBAT_ACTION_H

#include <memory>

#include "../mwworld/containerstore.hpp"
#include "../mwworld/ptr.hpp"

namespace OFMechanics
{
    class Action
    {
    public:
        virtual ~Action() {}
        virtual void prepare(const OFWorld::Ptr& actor) = 0;
        virtual float getCombatRange(bool& isRanged) const = 0;
        virtual float getActionCooldown() const { return 0.f; }
        virtual const ESM::Weapon* getWeapon() const { return nullptr; }
        virtual ESM::RefId getSpell() const { return {}; }
        virtual bool isAttackingOrSpell() const { return true; }
        virtual bool isFleeing() const { return false; }
    };

    class ActionFlee : public Action
    {
    public:
        ActionFlee() {}
        void prepare(const OFWorld::Ptr& actor) override {}
        float getCombatRange(bool& isRanged) const override { return 0.0f; }
        float getActionCooldown() const override { return 3.0f; }
        bool isAttackingOrSpell() const override { return false; }
        bool isFleeing() const override { return true; }
    };

    class ActionSpell : public Action
    {
    public:
        ActionSpell(const ESM::RefId& spellId)
            : mSpellId(spellId)
        {
        }
        ESM::RefId mSpellId;
        /// Sets the given spell as selected on the actor's spell list.
        void prepare(const OFWorld::Ptr& actor) override;

        float getCombatRange(bool& isRanged) const override;
        ESM::RefId getSpell() const override { return mSpellId; }
    };

    class ActionEnchantedItem : public Action
    {
    public:
        ActionEnchantedItem(const OFWorld::ContainerStoreIterator& item)
            : mItem(item)
        {
        }
        OFWorld::ContainerStoreIterator mItem;
        /// Sets the given item as selected enchanted item in the actor's InventoryStore.
        void prepare(const OFWorld::Ptr& actor) override;
        float getCombatRange(bool& isRanged) const override;

        /// Since this action has no animation, apply a small cool down for using it
        float getActionCooldown() const override { return 0.75f; }
    };

    class ActionPotion : public Action
    {
    public:
        ActionPotion(const OFWorld::Ptr& potion)
            : mPotion(potion)
        {
        }
        OFWorld::Ptr mPotion;
        /// Drinks the given potion.
        void prepare(const OFWorld::Ptr& actor) override;
        float getCombatRange(bool& isRanged) const override;
        bool isAttackingOrSpell() const override { return false; }

        /// Since this action has no animation, apply a small cool down for using it
        float getActionCooldown() const override { return 0.75f; }
    };

    class ActionWeapon : public Action
    {
    private:
        OFWorld::Ptr mAmmunition;
        OFWorld::Ptr mWeapon;

    public:
        /// \a weapon may be empty for hand-to-hand combat
        ActionWeapon(const OFWorld::Ptr& weapon, const OFWorld::Ptr& ammo = OFWorld::Ptr())
            : mAmmunition(ammo)
            , mWeapon(weapon)
        {
        }
        /// Equips the given weapon.
        void prepare(const OFWorld::Ptr& actor) override;
        float getCombatRange(bool& isRanged) const override;
        const ESM::Weapon* getWeapon() const override;
    };

    std::unique_ptr<Action> prepareNextAction(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);

    float getDistanceMinusHalfExtents(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy, bool minusZDist = false);
    float getMaxAttackDistance(const OFWorld::Ptr& actor);
    bool canFight(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);

    float vanillaRateFlee(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);
    bool makeFleeDecision(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy, float antiFleeRating);
}

#endif
