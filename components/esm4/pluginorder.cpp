#include "pluginorder.hpp"

#include "common.hpp"
#include "reader.hpp"

#include <components/debug/debuglog.hpp>
#include <components/esm/format.hpp>
#include <components/files/collections.hpp>
#include <components/files/conversion.hpp>
#include <components/files/multidircollection.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/pathhelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ESM4
{
    namespace
    {
        constexpr std::size_t sNone = std::numeric_limits<std::size_t>::max();

        std::string trim(std::string_view text)
        {
            const char* spaces = " \t\r\n";
            const std::size_t first = text.find_first_not_of(spaces);
            if (first == std::string_view::npos)
                return {};
            const std::size_t last = text.find_last_not_of(spaces);
            return std::string(text.substr(first, last - first + 1));
        }

        bool isPluginName(std::string_view name)
        {
            const std::string_view extension = Misc::getFileExtension(name);
            return Misc::StringUtils::ciEqual(extension, "esm") || Misc::StringUtils::ciEqual(extension, "esp")
                || Misc::StringUtils::ciEqual(extension, "esl");
        }

        struct Orderer
        {
            const std::vector<PluginInfo>& mPlugins;
            std::unordered_map<std::string, std::size_t> mByName;
            std::vector<char> mState; // 0 not placed, 1 being placed, 2 placed
            std::vector<std::string> mReasons;
            std::vector<std::size_t> mOrder;
            std::vector<std::string> mProblems;
            std::vector<std::size_t> mChain;
            std::string_view mMissingMaster;

            Orderer(const std::vector<PluginInfo>& plugins, std::string_view missingMaster)
                : mPlugins(plugins)
                , mState(plugins.size(), 0)
                , mReasons(plugins.size())
                , mMissingMaster(missingMaster)
            {
            }

            void place(std::size_t index, std::size_t requester)
            {
                if (mState[index] == 2)
                    return;
                if (mState[index] == 1)
                {
                    // The chain from the first time this plugin was entered to the plugin that asks for it again
                    std::string chain;
                    auto from = std::find(mChain.begin(), mChain.end(), index);
                    for (auto it = from; it != mChain.end(); ++it)
                        chain += mPlugins[*it].mName + " needs ";
                    chain += mPlugins[index].mName;
                    mProblems.push_back("the masters depend on each other in a circle: " + chain);
                    return;
                }
                if (requester != sNone)
                    mReasons[index] = "it is a master of " + mPlugins[requester].mName;
                mState[index] = 1;
                mChain.push_back(index);
                for (const std::string& master : mPlugins[index].mMasters)
                {
                    const auto found = mByName.find(Misc::StringUtils::lowerCase(master));
                    if (found == mByName.end())
                        mProblems.push_back(
                            mPlugins[index].mName + " needs " + master + ", " + std::string(mMissingMaster));
                    else
                        place(found->second, index);
                }
                mChain.pop_back();
                mState[index] = 2;
                mOrder.push_back(index);
            }
        };
    }

    std::optional<PluginInfo> readPluginInfo(const std::filesystem::path& path, std::string name)
    {
        PluginInfo info;
        info.mName = std::move(name);
        const std::string fileName = Files::pathToUnicodeString(path.filename());

        {
            const auto stream = Files::openBinaryInputFileStream(path);
            if (!stream->is_open())
                throw std::runtime_error("Failed to open plugin " + fileName);
            if (ESM::readFormat(*stream) != ESM::Format::Tes4)
                return std::nullopt;
            // The TES4 record is its type, its data size and its flags
            stream->seekg(8);
            std::uint32_t flags = 0;
            stream->read(reinterpret_cast<char*>(&flags), sizeof(flags));
            if (!*stream)
                throw std::runtime_error(fileName + " ends inside its header");
            info.mMaster = (flags & Rec_ESM) != 0;
        }

        Reader reader(Files::openBinaryInputFileStream(path), path, nullptr, nullptr, true);
        for (const ESM::MasterData& master : reader.getGameFiles())
        {
            info.mMasters.push_back(master.name);
            info.mMasterSizes.push_back(master.size);
        }
        return info;
    }

    PluginOrder orderPlugins(const std::vector<PluginInfo>& plugins, std::string_view missingMaster)
    {
        Orderer orderer(plugins, missingMaster);

        if (plugins.size() > sMaxPlugins)
        {
            std::ostringstream message;
            message << "Cannot load " << plugins.size()
                    << " plugins: the top byte of a form ID names the plugin, so at " << "most " << sMaxPlugins
                    << " can load together";
            throw std::runtime_error(message.str());
        }

        for (std::size_t i = 0; i < plugins.size(); ++i)
            if (!orderer.mByName.emplace(Misc::StringUtils::lowerCase(plugins[i].mName), i).second)
                orderer.mProblems.push_back(plugins[i].mName + " is listed twice");

        // Masters first, each group in the order of the list. A master that sits after a plugin that is not one is the
        // case that moves.
        std::vector<std::size_t> sequence;
        std::vector<char> hopped(plugins.size(), 0);
        bool plainSeen = false;
        for (std::size_t i = 0; i < plugins.size(); ++i)
        {
            if (!plugins[i].mMaster)
                plainSeen = true;
            else if (plainSeen)
                hopped[i] = 1;
        }
        for (std::size_t i = 0; i < plugins.size(); ++i)
            if (plugins[i].mMaster)
                sequence.push_back(i);
        for (std::size_t i = 0; i < plugins.size(); ++i)
            if (!plugins[i].mMaster)
                sequence.push_back(i);

        for (const std::size_t i : sequence)
        {
            if (hopped[i])
                orderer.mReasons[i] = "it is flagged as a master";
            orderer.place(i, sNone);
        }

        if (!orderer.mProblems.empty())
        {
            std::string message = "The plugins cannot be loaded:";
            for (const std::string& problem : orderer.mProblems)
                message += "\n  " + problem;
            throw std::runtime_error(message);
        }

        PluginOrder result;
        result.mOrder = std::move(orderer.mOrder);
        for (std::size_t position = 0; position < result.mOrder.size(); ++position)
        {
            const std::size_t index = result.mOrder[position];
            if (index == position || orderer.mReasons[index].empty())
                continue;
            std::ostringstream line;
            line << plugins[index].mName << " moves from place " << index + 1 << " to place " << position + 1 << " of "
                 << plugins.size() << ": " << orderer.mReasons[index];
            result.mMoves.push_back(line.str());
        }
        return result;
    }

    std::vector<std::string> orderContentFiles(
        const std::vector<std::string>& content, const Files::Collections& collections, ContentOrder order)
    {
        struct Entry
        {
            PluginInfo mInfo;
            std::filesystem::path mPath;
        };

        std::vector<std::size_t> slots;
        std::vector<Entry> entries;
        std::unordered_map<std::string, std::size_t> byName;

        for (std::size_t i = 0; i < content.size(); ++i)
        {
            if (!isPluginName(content[i]))
                continue;
            const Files::MultiDirCollection& files = collections.getCollection(Misc::getFileExtension(content[i]));
            if (!files.doesExist(content[i]))
                throw std::runtime_error("Failed loading " + content[i] + ": the content file does not exist");
            std::filesystem::path path = files.getPath(content[i]);
            if (std::optional<PluginInfo> info = readPluginInfo(path, content[i]))
            {
                slots.push_back(i);
                byName.emplace(Misc::StringUtils::lowerCase(content[i]), entries.size());
                entries.push_back({ std::move(*info), std::move(path) });
            }
        }

        // The games activate the masters of an active plugin themselves, wherever in their Data folder they are
        for (std::size_t k = 0; k < entries.size(); ++k)
        {
            const std::vector<std::string> masters = entries[k].mInfo.mMasters;
            const std::string requester = entries[k].mInfo.mName;
            for (const std::string& master : masters)
            {
                if (!isPluginName(master) || byName.contains(Misc::StringUtils::lowerCase(master)))
                    continue;
                const Files::MultiDirCollection& files = collections.getCollection(Misc::getFileExtension(master));
                if (!files.doesExist(master))
                    continue;
                std::filesystem::path path = files.getPath(master);
                std::string name = Files::pathToUnicodeString(path.filename());
                if (std::optional<PluginInfo> info = readPluginInfo(path, name))
                {
                    Log(Debug::Info) << "Load order: " << name << " is added: it is a master of " << requester;
                    byName.emplace(Misc::StringUtils::lowerCase(master), entries.size());
                    entries.push_back({ std::move(*info), std::move(path) });
                }
            }
        }

        if (order == ContentOrder::FileTime)
        {
            std::vector<std::filesystem::file_time_type> times;
            for (const Entry& entry : entries)
            {
                std::error_code error;
                const auto time = std::filesystem::last_write_time(entry.mPath, error);
                times.push_back(error ? std::filesystem::file_time_type::min() : time);
            }
            std::vector<std::size_t> sorted(entries.size());
            std::iota(sorted.begin(), sorted.end(), std::size_t(0));
            std::stable_sort(sorted.begin(), sorted.end(), [&](std::size_t a, std::size_t b) {
                if (entries[a].mInfo.mMaster != entries[b].mInfo.mMaster)
                    return entries[a].mInfo.mMaster;
                return times[a] < times[b];
            });
            std::vector<Entry> reordered;
            for (const std::size_t index : sorted)
                reordered.push_back(std::move(entries[index]));
            entries = std::move(reordered);
            Log(Debug::Info) << "Load order: the plugins are sorted by the time their files were written";
        }

        std::vector<PluginInfo> plugins;
        for (const Entry& entry : entries)
            plugins.push_back(entry.mInfo);

        const PluginOrder sorted = orderPlugins(plugins, "which is neither listed nor in the data directories");
        for (const std::string& move : sorted.mMoves)
            Log(Debug::Info) << "Load order: " << move;

        std::vector<PluginInfo> loaded;
        std::vector<std::string> names;
        for (const std::size_t index : sorted.mOrder)
        {
            loaded.push_back(plugins[index]);
            names.push_back(plugins[index].mName);
        }

        std::unordered_map<std::string, std::filesystem::path> paths;
        for (const Entry& entry : entries)
            paths.emplace(Misc::StringUtils::lowerCase(entry.mInfo.mName), entry.mPath);
        for (const std::string& warning : masterSizeWarnings(loaded, [&](const std::string& name) {
                 const auto found = paths.find(Misc::StringUtils::lowerCase(name));
                 std::error_code error;
                 const std::uintmax_t size
                     = found == paths.end() ? std::uintmax_t(-1) : std::filesystem::file_size(found->second, error);
                 return error || found == paths.end() ? std::nullopt : std::optional<std::uint64_t>(size);
             }))
            Log(Debug::Warning) << warning;

        // The plugins fill the places the plugins of the list had; the masters that were added follow the last of them
        std::vector<std::string> result;
        std::size_t next = 0;
        std::size_t slot = 0;
        for (std::size_t i = 0; i < content.size(); ++i)
        {
            if (slot < slots.size() && slots[slot] == i)
            {
                ++slot;
                result.push_back(names[next++]);
                if (slot == slots.size())
                    while (next < names.size())
                        result.push_back(names[next++]);
            }
            else
                result.push_back(content[i]);
        }
        return result;
    }

    std::vector<std::string> dropMissingPlugins(
        const std::vector<std::string>& names, const Files::Collections& collections)
    {
        std::vector<std::string> result;
        for (const std::string& name : names)
        {
            if (isPluginName(name) && collections.getCollection(Misc::getFileExtension(name)).doesExist(name))
                result.push_back(name);
            else
                Log(Debug::Warning) << "Plugins file: " << name << " is not in the data directories, skipped";
        }
        return result;
    }

    std::vector<std::string> masterSizeWarnings(const std::vector<PluginInfo>& plugins,
        const std::function<std::optional<std::uint64_t>(const std::string&)>& sizeOf)
    {
        std::vector<std::string> result;
        for (const PluginInfo& plugin : plugins)
        {
            // The games check the plugins that are not masters
            if (plugin.mMaster)
                continue;
            for (std::size_t i = 0; i < plugin.mMasters.size() && i < plugin.mMasterSizes.size(); ++i)
            {
                const std::uint64_t expected = plugin.mMasterSizes[i];
                if (expected == 0)
                    continue;
                const std::optional<std::uint64_t> actual = sizeOf(plugin.mMasters[i]);
                if (actual && *actual != expected)
                    result.push_back(plugin.mName + " was made for another version of " + plugin.mMasters[i]
                        + ": it is " + std::to_string(*actual) + " bytes, the plugin expects "
                        + std::to_string(expected));
            }
        }
        return result;
    }

    std::vector<std::string> parsePluginsFile(std::string_view text)
    {
        // A byte order mark of UTF-8 starts some files
        if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0)
            text.remove_prefix(3);

        std::vector<std::string> marked;
        std::vector<std::string> all;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string line = trim(text.substr(start, end - start));
            start = end + 1;

            if (line.empty() || line.front() == '#')
                continue;
            const bool active = line.front() == '*';
            if (active)
                line = trim(std::string_view(line).substr(1));
            if (line.empty())
                continue;
            if (active)
                marked.push_back(line);
            all.push_back(std::move(line));
        }
        return marked.empty() ? all : marked;
    }
}
