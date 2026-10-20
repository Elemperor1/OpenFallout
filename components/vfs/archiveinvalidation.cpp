#include "archiveinvalidation.hpp"

#include "file.hpp"
#include "pathutil.hpp"

#include <algorithm>

namespace VFS
{
    namespace
    {
        constexpr std::size_t sMaxLine = 0x104 - 1;

        std::string_view trim(std::string_view text)
        {
            const char* spaces = " \t";
            const std::size_t first = text.find_first_not_of(spaces);
            if (first == std::string_view::npos)
                return {};
            return text.substr(first, text.find_last_not_of(spaces) - first + 1);
        }
    }

    ArchiveInvalidation ArchiveInvalidation::parse(std::string_view text)
    {
        // A byte order mark of UTF-8 starts some files
        if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0)
            text.remove_prefix(3);

        ArchiveInvalidation result;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find_first_of("\r\n", start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view line = trim(text.substr(start, end - start));
            start = end + 1;
            if (line.empty())
                continue;
            line = line.substr(0, sMaxLine);

            if (line.find_first_of("\\/") == std::string_view::npos)
            {
                result.mFileNames.insert(Path::normalizeFilename(line));
                continue;
            }
            std::string directory = Path::normalizeFilename(line);
            while (!directory.empty() && directory.back() == Path::separator)
                directory.pop_back();
            result.mDirectories.insert(std::move(directory));
        }
        return result;
    }

    bool ArchiveInvalidation::invalidates(
        std::string_view normalizedPath, const std::function<bool(std::string_view)>& directoryExists) const
    {
        const std::size_t slash = normalizedPath.find_last_of(Path::separator);
        const std::string_view directory
            = slash == std::string_view::npos ? std::string_view() : normalizedPath.substr(0, slash);
        const std::string_view file
            = slash == std::string_view::npos ? normalizedPath : normalizedPath.substr(slash + 1);

        if (!mDirectories.empty() && mDirectories.contains(std::string(directory)))
            return true;
        return !mFileNames.empty() && mFileNames.contains(std::string(file)) && directoryExists(directory);
    }

    InvalidatedArchive::InvalidatedArchive(std::unique_ptr<Archive> archive,
        std::shared_ptr<const ArchiveInvalidation> invalidation, std::function<bool(std::string_view)> directoryExists)
        : mArchive(std::move(archive))
        , mInvalidation(std::move(invalidation))
        , mDirectoryExists(std::move(directoryExists))
    {
    }

    void InvalidatedArchive::listResources(FileMap& out)
    {
        FileMap all;
        mArchive->listResources(all);
        for (const auto& [path, file] : all)
            if (!mInvalidation->invalidates(path.value(), mDirectoryExists))
                out[path] = file;
    }

    bool InvalidatedArchive::contains(Path::NormalizedView file) const
    {
        return mArchive->contains(file) && !mInvalidation->invalidates(file.value(), mDirectoryExists);
    }
}
