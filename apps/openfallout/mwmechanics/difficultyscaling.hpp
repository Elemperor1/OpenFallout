#ifndef OPENFALLOUT_MWMECHANICS_DIFFICULTYSCALING_H
#define OPENFALLOUT_MWMECHANICS_DIFFICULTYSCALING_H

namespace OFWorld
{
    class Ptr;
}

/// Scales damage dealt to an actor based on difficulty setting
float scaleDamage(float damage, const OFWorld::Ptr& attacker, const OFWorld::Ptr& victim);

#endif
