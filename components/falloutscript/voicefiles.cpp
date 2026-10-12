#include "voicefiles.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace FalloutScript
{
    namespace
    {
        constexpr std::string_view voiceFolder = "sound/voice/";
        constexpr std::array<std::string_view, 3> extensions = { "ogg", "wav", "mp3" };

        std::string lowerSlashes(std::string_view path)
        {
            std::string result(path);
            for (char& c : result)
                c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        }

        bool allDigits(std::string_view text, int (*is)(int))
        {
            return !text.empty() && std::all_of(text.begin(), text.end(), [is](char c) {
                return is(static_cast<unsigned char>(c)) != 0;
            });
        }

        std::uint32_t parseHex(std::string_view text)
        {
            std::uint32_t value = 0;
            for (const char c : text)
                value = value * 16
                    + static_cast<std::uint32_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : c - 'a' + 10);
            return value;
        }

        // The parts of a name that the games build and a file of another shape does not have
        struct Parts
        {
            std::string mPlugin;
            std::string mVoiceType;
            std::string mStem;
            std::string mText;
            std::string mExtension;
            std::uint32_t mResponse = 0;
            unsigned mNumber = 0;
        };

        bool split(std::string_view path, Parts& parts)
        {
            const std::string lower = lowerSlashes(path);
            std::string_view rest = lower;
            if (rest.substr(0, voiceFolder.size()) != voiceFolder)
                return false;
            rest.remove_prefix(voiceFolder.size());
            const std::size_t pluginEnd = rest.find('/');
            if (pluginEnd == std::string_view::npos || pluginEnd == 0)
                return false;
            parts.mPlugin = std::string(rest.substr(0, pluginEnd));
            rest.remove_prefix(pluginEnd + 1);
            const std::size_t voiceEnd = rest.find('/');
            if (voiceEnd == std::string_view::npos || voiceEnd == 0)
                return false;
            parts.mVoiceType = std::string(rest.substr(0, voiceEnd));
            rest.remove_prefix(voiceEnd + 1);
            if (rest.empty() || rest.find('/') != std::string_view::npos)
                return false;

            const std::size_t dot = rest.rfind('.');
            if (dot == std::string_view::npos || dot + 1 == rest.size())
                return false;
            parts.mExtension = std::string(rest.substr(dot + 1));
            const std::string_view stem = rest.substr(0, dot);
            parts.mStem = std::string(stem);

            // <text>_<8 hex digits>_<number>
            const std::size_t numberStart = stem.rfind('_');
            if (numberStart == std::string_view::npos || numberStart == 0)
                return false;
            const std::string_view number = stem.substr(numberStart + 1);
            if (number.size() > 3 || !allDigits(number, std::isdigit))
                return false;
            const std::size_t hexStart = stem.rfind('_', numberStart - 1);
            if (hexStart == std::string_view::npos || numberStart - hexStart - 1 != 8)
                return false;
            const std::string_view hex = stem.substr(hexStart + 1, 8);
            if (!allDigits(hex, std::isxdigit))
                return false;
            const unsigned value = static_cast<unsigned>(std::stoul(std::string(number)));
            if (value > 255)
                return false;
            parts.mText = std::string(stem.substr(0, hexStart));
            parts.mResponse = parseHex(hex);
            parts.mNumber = value;
            return true;
        }
    }

    bool parseVoiceFileName(std::string_view path, VoiceFileName& name)
    {
        Parts parts;
        if (!split(path, parts))
            return false;
        name.mPlugin = std::move(parts.mPlugin);
        name.mVoiceType = std::move(parts.mVoiceType);
        name.mText = std::move(parts.mText);
        name.mResponse = parts.mResponse;
        name.mNumber = parts.mNumber;
        name.mExtension = std::move(parts.mExtension);
        return true;
    }

    std::uint16_t VoiceIndex::intern(
        std::unordered_map<std::string, std::uint16_t>& table, std::vector<std::string>& names, const std::string& name)
    {
        const auto found = table.find(name);
        if (found != table.end())
            return found->second;
        const auto id = static_cast<std::uint16_t>(names.size());
        names.push_back(name);
        table.emplace(name, id);
        return id;
    }

    void VoiceIndex::add(std::string_view path)
    {
        Parts parts;
        if (!split(path, parts))
            return;
        const auto extension = std::find(extensions.begin(), extensions.end(), parts.mExtension);
        if (extension == extensions.end())
            return;
        // Fewer folders than a 16 bit number can hold; a game has a few hundred
        if (mPlugins.size() >= none - 1 || mVoiceTypes.size() >= none - 1)
            return;
        Key key;
        key.mPlugin = intern(mPluginIds, mPlugins, parts.mPlugin);
        key.mResponse = parts.mResponse & 0xFFFFFF;
        key.mNumber = static_cast<std::uint8_t>(parts.mNumber);
        Entry entry;
        entry.mVoiceType = intern(mVoiceTypeIds, mVoiceTypes, parts.mVoiceType);
        entry.mExtension = static_cast<std::uint8_t>(extension - extensions.begin());
        entry.mStem = std::move(parts.mStem);
        mFiles[key].push_back(std::move(entry));
        ++mCount;
    }

    void VoiceIndex::clear()
    {
        mPluginIds.clear();
        mPlugins.clear();
        mVoiceTypeIds.clear();
        mVoiceTypes.clear();
        mFiles.clear();
        mCount = 0;
    }

    std::string VoiceIndex::find(const std::vector<std::string>& plugins, std::string_view voiceType,
        std::uint32_t response, unsigned number) const
    {
        if (number > 255)
            return {};
        const std::string wantedVoice = lowerSlashes(voiceType);
        for (const std::string& plugin : plugins)
        {
            const auto pluginId = mPluginIds.find(lowerSlashes(plugin));
            if (pluginId == mPluginIds.end())
                continue;
            Key key;
            key.mPlugin = pluginId->second;
            key.mResponse = response & 0xFFFFFF;
            key.mNumber = static_cast<std::uint8_t>(number);
            const auto files = mFiles.find(key);
            if (files == mFiles.end())
                continue;
            // A speaker says a line with its own voice type: the file of another voice type is another person's (a
            // recording for the other sex, say). Only a speaker whose voice type is not known takes the first there is.
            // Then the sound format; the order the files were added breaks a tie.
            const Entry* best = nullptr;
            for (const Entry& entry : files->second)
            {
                if (!wantedVoice.empty() && mVoiceTypes[entry.mVoiceType] != wantedVoice)
                    continue;
                if (best == nullptr || entry.mExtension < best->mExtension)
                    best = &entry;
            }
            if (best == nullptr)
                continue;
            std::string path(voiceFolder);
            path += mPlugins[pluginId->second];
            path += '/';
            path += mVoiceTypes[best->mVoiceType];
            path += '/';
            path += best->mStem;
            path += '.';
            path += extensions[best->mExtension];
            return path;
        }
        return {};
    }
}
