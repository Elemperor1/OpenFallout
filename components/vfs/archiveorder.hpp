#ifndef OPENFALLOUT_COMPONENTS_VFS_ARCHIVEORDER_H
#define OPENFALLOUT_COMPONENTS_VFS_ARCHIVEORDER_H

#include <string>
#include <vector>

namespace Files
{
    class Collections;
    class GameIni;
}

namespace VFS
{
    // Which BSA archives the games open and which of them is looked in first. The games open the archives of the
    // sArchiveList of the ini file, then (New Vegas) Update.bsa, then for every plugin that has a master the archives
    // whose names start with the name of the plugin. They keep the archives in one list and look for a file in the
    // archives from the front of the list, the first one that has it wins. Fallout 3 appends every archive to the end
    // of the list, so the first one opened wins. New Vegas ranks an archive by its name (see orderArchives).

    enum class ArchiveRules
    {
        Fallout3,
        NewVegas,
    };

    /// The [Archive] settings of the ini file that matter
    struct ArchiveSettings
    {
        /// bUseArchives. When 0 the games open no archive.
        bool mUseArchives = true;

        /// bInvalidateOlderFiles. When 1 a loose file wins over an archive that has a file of the same path, when 0
        /// the archive wins.
        bool mInvalidateOlderFiles = true;

        /// sArchiveList: the archives the ini file lists, in its order
        std::vector<std::string> mArchiveList;

        /// sInvalidationFile
        std::string mInvalidationFile = "ArchiveInvalidation.txt";

        /// Reads the settings. Where the ini files do not name an archive list, the one of the retail game is used (the
        /// games' own fall-back is a list of three archives of which one is Oblivion's).
        static ArchiveSettings read(const Files::GameIni& ini, ArchiveRules rules);
    };

    /// The archives in the order the games open them: the ones of the ini file's list that exist, then the explicit
    /// ones, then Update.bsa (New Vegas, if there is one), then the ones of the plugins (\a pluginArchives, in the
    /// order of their plugins). A name once, whatever its case. The archives that the settings list and Update.bsa are
    /// left out when the settings turn archives off; the explicit ones stay, as they were asked for.
    std::vector<std::string> archivesToOpen(const ArchiveSettings& settings, ArchiveRules rules,
        const std::vector<std::string>& explicitArchives, const std::vector<std::string>& pluginArchives,
        const Files::Collections& collections);

    /// The archives in the order the games look in them, the first one that has a file first. \a opened is the order
    /// they were opened in. Fallout 3 keeps that order. New Vegas ranks an archive by the first of these that its
    /// name contains (case counts): Fallo 0, DeadM 1, Hones 2, Lones 3, OldWo 4, Updat 5, none 6. A rank 0 archive goes
    /// to the end. Any other goes in front of the first archive in the list that is Fallo or DeadM, or Hones when the
    /// rank is above 1, Lones above 2, OldWo above 3, Updat above 3, and to the front of the list if there is none:
    /// the archives of the later DLC are looked in before those of the earlier ones, Update.bsa between them, and the
    /// archives of mods (rank 6) before all of them.
    std::vector<std::string> orderArchives(const std::vector<std::string>& opened, ArchiveRules rules);
}

#endif
