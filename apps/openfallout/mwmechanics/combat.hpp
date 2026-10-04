#ifndef OPENFALLOUT_MECHANICS_COMBAT_H
#define OPENFALLOUT_MECHANICS_COMBAT_H

#include <utility>

namespace osg
{
    class Vec3f;
}

namespace OFWorld
{
    class Ptr;
}

namespace OFMechanics
{

    bool applyOnStrikeEnchantment(const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim, const OFWorld::Ptr& object,
        const osg::Vec3f& hitPosition, const bool fromProjectile = false);

    /// @return can we block the attack?
    bool blockMeleeAttack(const OFWorld::Ptr& attacker, const OFWorld::Ptr& blocker, const OFWorld::Ptr& weapon,
        float damage, float attackStrength);

    /// @return does normal weapon resistance and weakness apply to the weapon?
    bool isNormalWeapon(const OFWorld::Ptr& weapon);

    void resistNormalWeapon(
        const OFWorld::Ptr& actor, const OFWorld::Ptr& attacker, const OFWorld::Ptr& weapon, float& damage);

    void applyWerewolfDamageMult(const OFWorld::Ptr& actor, const OFWorld::Ptr& weapon, float& damage);

    /// @note for a thrown weapon, \a weapon == \a projectile, for bows/crossbows, \a projectile is the arrow/bolt
    /// @note \a victim may be empty (e.g. for a hit on terrain), a non-actor (environment objects) or an actor
    void projectileHit(const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim, OFWorld::Ptr weapon,
        const OFWorld::Ptr& projectile, const osg::Vec3f& hitPosition, float attackStrength, float attackWindUp);

    /// Get the chance (in percent) for \a attacker to successfully hit \a victim with a given weapon skill value
    float getHitChance(const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim, int skillValue);

    /// Applies damage to attacker based on the victim's elemental shields.
    void applyElementalShields(const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim);

    /// @param damage Unmitigated weapon damage of the attack
    /// @param hit Was the attack successful?
    /// @param weapon The weapon used.
    /// @note if the weapon is unequipped as result of condition damage, a new Ptr will be assigned to \a weapon.
    void reduceWeaponCondition(float damage, bool hit, OFWorld::Ptr& weapon, const OFWorld::Ptr& attacker);

    /// Adjust weapon damage based on its condition. A used weapon will be less effective.
    void adjustWeaponDamage(float& damage, const OFWorld::Ptr& weapon, const OFWorld::Ptr& attacker);

    void getHandToHandDamage(
        const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim, float& damage, bool& healthdmg, float attackStrength);

    /// Apply the fatigue loss incurred by attacking with the given weapon (weapon may be empty = hand-to-hand)
    void applyFatigueLoss(const OFWorld::Ptr& attacker, const OFWorld::Ptr& weapon, float attackStrength);

    int getFightTerm(const OFWorld::Ptr& actor, const OFWorld::Ptr& target);
    float getFightDispositionBias(float disposition);
    float getFightDistanceBias(const OFWorld::Ptr& actor1, const OFWorld::Ptr& actor2);
    float getAggroDistance(const OFWorld::Ptr& actor, const osg::Vec3f& lhs, const osg::Vec3f& rhs);
    bool isAggressionCapable(const OFWorld::Ptr& actor);
    bool isAggressive(const OFWorld::Ptr& actor, const OFWorld::Ptr& target);

    // Cursed distance calculation used for combat proximity and hit checks in Morrowind
    float getDistanceToBounds(const OFWorld::Ptr& actor, const OFWorld::Ptr& target);

    float getMeleeWeaponReach(const OFWorld::Ptr& actor, const OFWorld::Ptr& weapon);

    bool isInMeleeReach(const OFWorld::Ptr& actor, const OFWorld::Ptr& target, const float reach);

    // Similarly cursed hit target selection
    std::pair<OFWorld::Ptr, osg::Vec3f> getHitContact(const OFWorld::Ptr& actor, float reach);

    bool friendlyHit(const OFWorld::Ptr& attacker, const OFWorld::Ptr& target, bool complain);
}

#endif
