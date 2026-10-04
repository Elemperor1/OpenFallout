#ifndef OPENFALLOUT_ESSIMPORT_KLST_H
#define OPENFALLOUT_ESSIMPORT_KLST_H

#include <cstdint>
#include <map>

#include <components/esm/refid.hpp>

namespace ESM
{
    class ESMReader;
}

namespace ESSImport
{

    /// Kill Stats
    struct KLST
    {
        void load(ESM::ESMReader& esm);

        std::map<ESM::RefId, int32_t> mKillCounter;

        int32_t mWerewolfKills;
    };

}

#endif
