#ifndef OPENFALLOUT_COMPONENTS_CRASHCATCHER_WINDOWSCRASHDUMPPATHHELPERS_HPP
#define OPENFALLOUT_COMPONENTS_CRASHCATCHER_WINDOWSCRASHDUMPPATHHELPERS_HPP

#include <filesystem>

#include "windowscrashshm.hpp"

namespace Crash
{
    std::filesystem::path getCrashDumpPath(const CrashSHM& crashShm);

    std::filesystem::path getFreezeDumpPath(const CrashSHM& crashShm);
}

#endif
