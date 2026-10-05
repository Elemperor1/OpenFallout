#ifndef MWMECHANICS_RECHARGE_H
#define MWMECHANICS_RECHARGE_H

#include "../mwworld/ptr.hpp"

namespace OFMechanics
{

    bool rechargeItem(const OFWorld::Ptr& item, const float maxCharge, const float duration);

    bool rechargeItem(const OFWorld::Ptr& item, const OFWorld::Ptr& gem);

}

#endif
