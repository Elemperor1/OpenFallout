#include "archiveorder.hpp"

#include <components/debug/debuglog.hpp>
#include <components/files/collections.hpp>
#include <components/files/gameini.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <unordered_set>

namespace VFS
{
    namespace
    {
        constexpr std::array<std::string_view, 6> sRankNames = { "Fallo", "DeadM", "Hones", "Lones", "OldWo", "Updat" };
        constexpr int sUnrankedArchive = 6;

        int rankOf(std::string_view name)
        {
            for (std::size_t rank = 0; rank < sRankNames.size(); ++rank)
                if (name.find(sRankNames[rank]) != std::string_view::npos)
                    return static_cast<int>(rank);
            return sUnrankedArchive;
        }

        bool goesBefore(int existing, int added)
        {
            switch (existing)
            {
                case 0:
                case 1:
                    return true;
                case 2:
                    return added > 1;
                case 3:
                    return added > 2;
                case 4:
                case 5:
                    return added > 3;
                default:
                    return false;
            }
        }

        const std::vector<std::string>& retailList(ArchiveRules rules)
        {
            static const std::vector<std::string> newVegas = { "Fallout - Textures.bsa", "Fallout - Textures2.bsa",
                "Fallout - Meshes.bsa", "Fallout - Voices1.bsa", "Fallout - Sound.bsa", "Fallout - Misc.bsa" };
            static const std::vector<std::string> fallout3 = { "Fallout - Textures.bsa", "Fallout - Meshes.bsa",
                "Fallout - Voices.bsa", "Fallout - Sound.bsa", "Fallout - MenuVoices.bsa", "Fallout - Misc.bsa" };
            return rules == ArchiveRules::NewVegas ? newVegas : fallout3;
        }

        bool truthy(const std::optional<std::string>& value, bool fallback)
        {
            if (!value || value->empty())
                return fallback;
            return std::atoi(value->c_str()) != 0;
        }
    }

    ArchiveSettings ArchiveSettings::read(const Files::GameIni& ini, ArchiveRules rules)
    {
        ArchiveSettings settings;
        settings.mUseArchives = truthy(ini.get("Archive", "bUseArchives"), true);
        settings.mInvalidateOlderFiles = truthy(ini.get("Archive", "bInvalidateOlderFiles"), true);
        if (const auto list = ini.get("Archive", "sArchiveList"))
            settings.mArchiveList = Files::GameIni::splitList(*list);
        else
            settings.mArchiveList = retailList(rules);
        if (const auto file = ini.get("Archive", "SInvalidationFile"); file && !file->empty())
            settings.mInvalidationFile = *file;
        return settings;
    }

    std::vector<std::string> archivesToOpen(const ArchiveSettings& settings, ArchiveRules rules,
        const std::vector<std::string>& explicitArchives, const std::vector<std::string>& pluginArchives,
        const Files::Collections& collections)
    {
        std::vector<std::string> result;
        std::unordered_set<std::string> seen;
        const auto add = [&](const std::string& name) {
            if (seen.insert(Misc::StringUtils::lowerCase(name)).second)
                result.push_back(name);
        };

        if (settings.mUseArchives)
            for (const std::string& name : settings.mArchiveList)
            {
                // The games skip an archive they cannot open without a word
                if (collections.doesExist(name))
                    add(name);
                else
                    Log(Debug::Info) << "The archive " << name << " of the ini file is not in the data directories";
            }
        for (const std::string& name : explicitArchives)
            add(name);
        if (settings.mUseArchives)
        {
            if (rules == ArchiveRules::NewVegas && collections.doesExist("Update.bsa"))
                add("Update.bsa");
            for (const std::string& name : pluginArchives)
                add(name);
        }
        return result;
    }

    std::vector<std::string> orderArchives(const std::vector<std::string>& opened, ArchiveRules rules)
    {
        if (rules == ArchiveRules::Fallout3)
            return opened;

        std::vector<std::string> list;
        for (const std::string& name : opened)
        {
            const int rank = rankOf(name);
            if (rank == 0)
            {
                list.push_back(name);
                continue;
            }
            const auto before = std::find_if(list.begin(), list.end(),
                [rank](const std::string& existing) { return goesBefore(rankOf(existing), rank); });
            list.insert(before == list.end() ? list.begin() : before, name);
        }
        return list;
    }
}
