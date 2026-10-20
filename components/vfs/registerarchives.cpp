#include "registerarchives.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>

#include <components/vfs/archiveinvalidation.hpp>
#include <components/vfs/bsaarchive.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <components/vfs/manager.hpp>

namespace VFS
{
    namespace
    {
        void addArchives(VFS::Manager* vfs, const Files::Collections& collections,
            const std::vector<std::string>& archives, const ToUTF8::StatelessUtf8Encoder* encoder,
            const ArchiveOptions& options)
        {
            std::function<bool(std::string_view)> directoryExists;
            if (options.mInvalidation != nullptr && !options.mInvalidation->empty())
            {
                // The Data folder of the games is the data directories together
                auto cache = std::make_shared<std::map<std::string, bool, std::less<>>>();
                directoryExists = [cache, dataDirs = collections.getPaths()](std::string_view directory) {
                    const auto found = cache->find(directory);
                    if (found != cache->end())
                        return found->second;
                    bool exists = directory.empty();
                    for (const auto& dataDir : dataDirs)
                        exists = exists
                            || std::filesystem::is_directory(dataDir / Files::pathFromUnicodeString(directory));
                    cache->emplace(std::string(directory), exists);
                    return exists;
                };
            }

            for (std::vector<std::string>::const_iterator archive = archives.begin(); archive != archives.end();
                 ++archive)
            {
                if (collections.doesExist(*archive))
                {
                    // Last BSA has the highest priority
                    const auto archivePath = collections.getPath(*archive);
                    Log(Debug::Info) << "Adding BSA archive " << archivePath;
                    std::unique_ptr<Archive> bsa = makeBsaArchive(archivePath, encoder);
                    if (directoryExists)
                        bsa = std::make_unique<InvalidatedArchive>(
                            std::move(bsa), options.mInvalidation, directoryExists);
                    vfs->addArchive(std::move(bsa));
                }
                else
                {
                    throw std::runtime_error("Archive '" + *archive + "' not found");
                }
            }
        }

        void addLooseFiles(VFS::Manager* vfs, const Files::PathContainer& dataDirs)
        {
            std::set<std::filesystem::path> seen;
            for (const auto& dataDir : dataDirs)
            {
                if (seen.insert(dataDir).second)
                {
                    Log(Debug::Info) << "Adding data directory " << dataDir;
                    // Last data dir has the highest priority
                    vfs->addArchive(std::make_unique<FileSystemArchive>(dataDir));
                }
                else
                    Log(Debug::Info) << "Ignoring duplicate data directory " << dataDir;
            }
        }
    }

    void registerArchives(VFS::Manager* vfs, const Files::Collections& collections,
        const std::vector<std::string>& archives, bool useLooseFiles, const ToUTF8::StatelessUtf8Encoder* encoder,
        const ArchiveOptions& options)
    {
        // What is registered last wins
        if (useLooseFiles && options.mArchivesWin)
            addLooseFiles(vfs, collections.getPaths());
        addArchives(vfs, collections, archives, encoder, options);
        if (useLooseFiles && !options.mArchivesWin)
            addLooseFiles(vfs, collections.getPaths());

        vfs->buildIndex();
    }

}
