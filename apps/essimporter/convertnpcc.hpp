#ifndef OPENFALLOUT_ESSIMPORT_CONVERTNPCC_H
#define OPENFALLOUT_ESSIMPORT_CONVERTNPCC_H

#include "importnpcc.hpp"

#include <components/esm3/npcstate.hpp>

namespace ESSImport
{

    void convertNPCC(const NPCC& npcc, ESM::NpcState& npcState);

}

#endif
