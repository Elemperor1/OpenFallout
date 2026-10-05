#ifndef OPENFALLOUT_MECHANICS_PICKPOCKET_H
#define OPENFALLOUT_MECHANICS_PICKPOCKET_H

#include "../mwworld/ptr.hpp"

namespace OFMechanics
{

    class Pickpocket
    {
    public:
        Pickpocket(const OFWorld::Ptr& thief, const OFWorld::Ptr& victim);

        /// Steal some items
        /// @return Was the thief detected?
        bool pick(const OFWorld::Ptr& item, int count);
        /// End the pickpocketing process
        /// @return Was the thief detected?
        bool finish();

    private:
        bool getDetected(float valueTerm);
        float getChanceModifier(const OFWorld::Ptr& ptr, float add = 0);
        OFWorld::Ptr mThief;
        OFWorld::Ptr mVictim;
    };

}

#endif
