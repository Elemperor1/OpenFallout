#ifndef OPENFALLOUT_MWMECHANICS_CREATURECUSTOMDATARESETTER_H
#define OPENFALLOUT_MWMECHANICS_CREATURECUSTOMDATARESETTER_H

#include "../mwworld/ptr.hpp"

namespace OFMechanics
{
    struct CreatureCustomDataResetter
    {
        OFWorld::Ptr mPtr;

        ~CreatureCustomDataResetter()
        {
            if (!mPtr.isEmpty())
                mPtr.getRefData().setCustomData({});
        }
    };
}

#endif
