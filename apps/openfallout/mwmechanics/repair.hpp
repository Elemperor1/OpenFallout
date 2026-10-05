#ifndef OPENFALLOUT_MWMECHANICS_REPAIR_H
#define OPENFALLOUT_MWMECHANICS_REPAIR_H

#include "../mwworld/ptr.hpp"

namespace OFMechanics
{

    class Repair
    {
    public:
        void setTool(const OFWorld::Ptr& tool) { mTool = tool; }
        OFWorld::Ptr getTool() { return mTool; }

        void repair(const OFWorld::Ptr& itemToRepair);

    private:
        OFWorld::Ptr mTool;
    };

}

#endif
