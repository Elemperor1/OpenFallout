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

        struct Orderer
        {
            const std::vector<PluginInfo>& mPlugins;
            std::unordered_map<std::string, std::size_t> mByName;
            std::vector<char> mState; // 0 not placed, 1 being placed, 2 placed
            std::vector<std::string> mReasons;
            std::vector<std::size_t> mOrder;
            std::vector<std::string> mProblems;
            std::vector<std::size_t> mChain;

            explicit Orderer(const std::vector<PluginInfo>& plugins)
                : mPlugins(plugins)
                , mState(plugins.size(), 0)
                , mReasons(plugins.size())
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
                            mPlugins[index].mName + " needs " + master + ", which is not among the plugins to load");
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
            info.mMasters.push_back(master.name);
        return info;
    }

    PluginOrder orderPlugins(const std::vector<PluginInfo>& plugins)
    {
        Orderer orderer(plugins);

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
        const std::vector<std::string>& content, const Files::Collections& collections)
    {
        std::vector<std::size_t> slots;
        std::vector<PluginInfo> plugins;
        for (std::size_t i = 0; i < content.size(); ++i)
        {
            const std::string_view extension = Misc::getFileExtension(content[i]);
            if (!Misc::StringUtils::ciEqual(extension, "esm") && !Misc::StringUtils::ciEqual(extension, "esp")
                && !Misc::StringUtils::ciEqual(extension, "esl"))
                continue;
            const Files::MultiDirCollection& files = collections.getCollection(extension);
            if (!files.doesExist(content[i]))
                throw std::runtime_error("Failed loading " + content[i] + ": the content file does not exist");
            if (std::optional<PluginInfo> info = readPluginInfo(files.getPath(content[i]), content[i]))
            {
                slots.push_back(i);
                plugins.push_back(std::move(*info));
            }
        }

        const PluginOrder order = orderPlugins(plugins);
        for (const std::string& move : order.mMoves)
            Log(Debug::Info) << "Load order: " << move;

        std::vector<std::string> result = content;
        for (std::size_t position = 0; position < slots.size(); ++position)
            result[slots[position]] = content[slots[order.mOrder[position]]];
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
