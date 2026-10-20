#ifndef OPENFALLOUT_COMPONENTS_VFS_ARCHIVEINVALIDATION_H
#define OPENFALLOUT_COMPONENTS_VFS_ARCHIVEINVALIDATION_H

#include "archive.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>

namespace VFS
{
    /// The ArchiveInvalidation.txt of the games (the file the ini file's sInvalidationFile names), which says which
    /// files of the archives the games must not use. A line that has a backslash is a directory: no file that is
    /// directly in that directory of an archive counts. Any other line is a file name: a file of that name does not
    /// count in any directory of an archive for which the Data folder has the directory too, even where it has no loose
    /// file of that name. An archive file that does not count is not found in that archive; the games look on in the
    /// next archive and then at the loose files.
    class ArchiveInvalidation
    {
    public:
        /// Reads the text of the file. Lines end at a line feed or a carriage return, are cut at 259 characters and are
        /// compared without regard to case or to the kind of slash.
        static ArchiveInvalidation parse(std::string_view text);

        bool empty() const { return mDirectories.empty() && mFileNames.empty(); }

        /// Whether the file at the normalised path does not count. \a directoryExists says whether the Data folder has
        /// a directory (a normalised path without the file name).
        bool invalidates(
            std::string_view normalizedPath, const std::function<bool(std::string_view)>& directoryExists) const;

    private:
        std::unordered_set<std::string> mDirectories;
        std::unordered_set<std::string> mFileNames;
    };

    /// An archive without the files that an ArchiveInvalidation does not count
    class InvalidatedArchive : public Archive
    {
    public:
        InvalidatedArchive(std::unique_ptr<Archive> archive, std::shared_ptr<const ArchiveInvalidation> invalidation,
            std::function<bool(std::string_view)> directoryExists);

        void listResources(FileMap& out) override;

        bool contains(Path::NormalizedView file) const override;

        std::string getDescription() const override { return mArchive->getDescription(); }

    private:
        std::unique_ptr<Archive> mArchive;
        std::shared_ptr<const ArchiveInvalidation> mInvalidation;
        std::function<bool(std::string_view)> mDirectoryExists;
    };
}

#endif
