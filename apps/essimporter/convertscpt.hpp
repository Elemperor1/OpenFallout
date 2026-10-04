#ifndef OPENFALLOUT_ESSIMPORT_CONVERTSCPT_H
#define OPENFALLOUT_ESSIMPORT_CONVERTSCPT_H

#include <components/esm3/globalscript.hpp>

#include "importscpt.hpp"

namespace ESSImport
{

    void convertSCPT(const SCPT& scpt, ESM::GlobalScript& out);

}

#endif
