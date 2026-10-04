#ifndef OPENFALLOUT_ESMTOOL_TES4_H
#define OPENFALLOUT_ESMTOOL_TES4_H

#include <fstream>
#include <iosfwd>
#include <memory>

namespace EsmTool
{
    struct Arguments;

    int loadTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream);

    int censusTes4(const Arguments& info, std::unique_ptr<std::ifstream>&& stream);
}

#endif
