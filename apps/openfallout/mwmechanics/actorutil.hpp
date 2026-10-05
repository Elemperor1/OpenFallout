#ifndef OPENFALLOUT_MWMECHANICS_ACTORUTIL_H
#define OPENFALLOUT_MWMECHANICS_ACTORUTIL_H

namespace OFWorld
{
    class Ptr;
}

namespace OFMechanics
{
    OFWorld::Ptr getPlayer();
    bool isPlayerInCombat();
    bool canActorMoveByZAxis(const OFWorld::Ptr& actor);
    bool hasWaterWalking(const OFWorld::Ptr& actor);
    bool isTargetMagicallyHidden(const OFWorld::Ptr& actor);
}

#endif
