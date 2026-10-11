#ifndef OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VOICEFILES_H
#define OPENFALLOUT_COMPONENTS_FALLOUTSCRIPT_VOICEFILES_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace FalloutScript
{
    /// What the name of a voice file says: sound/voice/<plugin>/<voice type>/<text>_<8 hex digits>_<n>.<extension>. The
    /// plugin is the file name of the plugin that has the response, the voice type the editor id of a VTYP record, the
    /// text a cut of the names of the quest and the topic, the digits the form id of the response with 00 for the
    /// index of its plugin, and the number the number of the line in the response.
    struct VoiceFileName
    {
        std::string mPlugin;
        std::string mVoiceType;
        std::string mText;
        std::uint32_t mResponse = 0;
        unsigned mNumber = 0;
        std::string mExtension;
    };

    /// Reads the name of a voice file, which is in the lower case of the file system of the games. False for a path
    /// that is not one of a voice file with such a name (a few files have other names and no response can name them).
    bool parseVoiceFileName(std::string_view path, VoiceFileName& name);

    /// The voice files that the data has, to find the one of a line of a response. A line is found by the plugin that
    /// has the response, the form id of the response and the number of the line, never by the text of its name (that is
    /// a cut of two other names). The voice type that the speaker has comes first when a response has the files of
    /// several; neither side is complete, so a line that has no file is not found and the caller says it without voice.
    class VoiceIndex
    {
    public:
        /// Takes the path if it is that of a voice file; anything else is left out. The sound formats come in the
        /// order ogg, wav, mp3 (the 40 wav of New Vegas have the ogg of the same line next to them).
        void add(std::string_view path);
        void clear();
        std::size_t size() const { return mCount; }

        /// The path of the file of the line, or empty. The plugins are those whose folder is looked in, in order: the
        /// one that has the response and the ones that can override it. `response` is the form id of the response, of
        /// which the lower 24 bits count. Only the files of the voice type are taken (a speaker whose voice type is
        /// not known, an empty one, takes the first there is).
        std::string find(const std::vector<std::string>& plugins, std::string_view voiceType, std::uint32_t response,
            unsigned number) const;

    private:
        struct Entry
        {
            std::uint16_t mVoiceType = 0;
            std::uint8_t mExtension = 0; // 0 ogg, 1 wav, 2 mp3
            std::string mStem; // the name of the file without its extension
        };
        struct Key
        {
            std::uint16_t mPlugin = 0;
            std::uint32_t mResponse = 0;
            std::uint8_t mNumber = 0;
            bool operator==(const Key& other) const
            {
                return mPlugin == other.mPlugin && mResponse == other.mResponse && mNumber == other.mNumber;
            }
        };
        struct KeyHash
        {
            std::size_t operator()(const Key& key) const
            {
                return (static_cast<std::size_t>(key.mResponse) << 16) ^ (static_cast<std::size_t>(key.mPlugin) << 4)
                    ^ key.mNumber;
            }
        };

        std::uint16_t intern(std::unordered_map<std::string, std::uint16_t>& table, std::vector<std::string>& names,
            const std::string& name);
        static const std::uint16_t none = 0xFFFF;

        std::unordered_map<std::string, std::uint16_t> mPluginIds;
        std::vector<std::string> mPlugins;
        std::unordered_map<std::string, std::uint16_t> mVoiceTypeIds;
        std::vector<std::string> mVoiceTypes;
        std::unordered_map<Key, std::vector<Entry>, KeyHash> mFiles;
        std::size_t mCount = 0;
    };
}

#endif
