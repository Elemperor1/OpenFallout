#ifndef OPENFALLOUT_ESMTOOL_ARGUMENTS_H
#define OPENFALLOUT_ESMTOOL_ARGUMENTS_H

#include <filesystem>
#include <optional>
#include <vector>

#include <components/esm/format.hpp>

namespace EsmTool
{
    struct Arguments
    {
        std::optional<ESM::Format> mRawFormat;
        bool quiet_given = false;
        bool loadcells_given = false;
        bool plain_given = false;
        bool failed_given = false;

        std::string mode;
        std::string encoding;
        std::filesystem::path filename;
        std::filesystem::path outname;
        // Every file named on the command line, survey mode reads all of them
        std::vector<std::filesystem::path> inputFiles;

        std::vector<std::string> types;
        std::string name;
    };
}

#endif
