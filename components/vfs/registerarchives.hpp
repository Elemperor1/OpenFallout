#ifndef OPENFALLOUT_COMPONENTS_VFS_REGISTER_ARCHIVES_H
#define OPENFALLOUT_COMPONENTS_VFS_REGISTER_ARCHIVES_H

#include <components/files/collections.hpp>

#include <memory>

namespace ToUTF8
{
    class StatelessUtf8Encoder;
}

namespace VFS
{
    class Manager;
    class ArchiveInvalidation;

    /// How the archives and the loose files of the data directories go together
    struct ArchiveOptions
    {
        /// A file of an archive wins over a loose file of the same path. The games do this when bInvalidateOlderFiles
        /// of their ini file is 0; with 1, their default, a loose file wins.
        bool mArchivesWin = false;

        /// Files of the archives that do not count (the games' ArchiveInvalidation.txt), or none
        std::shared_ptr<const ArchiveInvalidation> mInvalidation;
    };

    /// @brief Register BSA and file system archives based on the given OpenMW configuration. The archives are given in
    /// the order of increasing priority.
    void registerArchives(VFS::Manager* vfs, const Files::Collections& collections,
        const std::vector<std::string>& archives, bool useLooseFiles, const ToUTF8::StatelessUtf8Encoder* encoder,
        const ArchiveOptions& options = {});
}

#endif
