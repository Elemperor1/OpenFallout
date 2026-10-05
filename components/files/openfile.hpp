#ifndef OPENFALLOUT_COMPONENTS_FILES_OPENFILE_H
#define OPENFALLOUT_COMPONENTS_FILES_OPENFILE_H

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>

namespace Files
{
    std::unique_ptr<std::ifstream> openBinaryInputFileStream(const std::filesystem::path& path);
}

#endif
