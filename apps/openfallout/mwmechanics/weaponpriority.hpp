#ifndef OPENFALLOUT_WEAPON_PRIORITY_H
#define OPENFALLOUT_WEAPON_PRIORITY_H

#include <components/esm/refid.hpp>

namespace OFWorld
{
    class Ptr;
}

namespace OFMechanics
{
    float rateWeapon(const OFWorld::Ptr& item, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy,
        ESM::RefId typeId = {}, float arrowRating = 0.f, float boltRating = 0.f);

    float rateAmmo(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy, OFWorld::Ptr& bestAmmo, ESM::RefId ammoType);
    float rateAmmo(const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy, ESM::RefId ammoType);

    float vanillaRateWeaponAndAmmo(
        const OFWorld::Ptr& weapon, const OFWorld::Ptr& ammo, const OFWorld::Ptr& actor, const OFWorld::Ptr& enemy);
}

#endif
